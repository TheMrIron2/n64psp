#include "n64psp/platform.h"
#include "n64psp/runtime.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define PAIRS 1000000
#define THREAD_PAIRS 250000

typedef struct bench_case {
    OSMesgQueue *queue;
    int count;
} bench_case;

static int setup_runtime(void) {
    n64psp_platform_callbacks platform;
    n64psp_renderer_callbacks renderer;

    if (n64psp_platform_host_get_callbacks(&platform) != N64PSP_OK ||
        n64psp_trace_backend_get_callbacks(&renderer) != N64PSP_OK ||
        n64psp_runtime_register_platform(&platform) != N64PSP_OK ||
        n64psp_runtime_register_renderer(&renderer) != N64PSP_OK || n64psp_runtime_init() != N64PSP_OK) {
        return 1;
    }
    return 0;
}

static void print_rate(const char *name, uint64_t ops, n64psp_time_us elapsed) {
    double seconds = elapsed ? (double)elapsed / 1000000.0 : 0.0;
    double rate = seconds > 0.0 ? (double)ops / seconds : 0.0;

    printf("%s: ops=%" PRIu64 " elapsed_us=%" PRIu64 " ops_per_sec=%.2f\n", name, ops, (uint64_t)elapsed, rate);
}

static int bench_pairs(const char *name, int capacity, int count) {
    OSMesg storage[64];
    OSMesgQueue queue;
    OSMesg out = 0;
    n64psp_time_us start;
    n64psp_time_us end;
    int i;

    if (capacity > (int)(sizeof(storage) / sizeof(storage[0]))) {
        return 1;
    }
    osCreateMesgQueue(&queue, storage, capacity);
    start = n64psp_time_monotonic_us();
    for (i = 0; i < count; i++) {
        if (osSendMesg(&queue, (OSMesg)i, OS_MESG_NOBLOCK) != 0 ||
            osRecvMesg(&queue, &out, OS_MESG_NOBLOCK) != 0 || out != (OSMesg)i) {
            return 1;
        }
    }
    end = n64psp_time_monotonic_us();
    print_rate(name, (uint64_t)(uint32_t)count * 2ULL, end - start);
    return 0;
}

static int producer_thread(void *userdata) {
    bench_case *bc = (bench_case *)userdata;
    int i;

    for (i = 0; i < bc->count; i++) {
        if (osSendMesg(bc->queue, (OSMesg)i, OS_MESG_BLOCK) != 0) {
            return -1;
        }
    }
    return 0;
}

static int consumer_thread(void *userdata) {
    bench_case *bc = (bench_case *)userdata;
    int i;

    for (i = 0; i < bc->count; i++) {
        OSMesg out = 0;
        if (osRecvMesg(bc->queue, &out, OS_MESG_BLOCK) != 0) {
            return -1;
        }
    }
    return 0;
}

static int bench_handoff(void) {
    n64psp_platform_callbacks platform;
    OSMesg storage[1];
    OSMesgQueue queue;
    bench_case prod;
    bench_case cons;
    n64psp_platform_thread *producer = NULL;
    n64psp_platform_thread *consumer = NULL;
    n64psp_time_us start;
    n64psp_time_us end;
    int code = -1;

    if (n64psp_platform_host_get_callbacks(&platform) != N64PSP_OK) {
        return 1;
    }
    osCreateMesgQueue(&queue, storage, 1);
    prod.queue = &queue;
    prod.count = THREAD_PAIRS;
    cons.queue = &queue;
    cons.count = THREAD_PAIRS;
    start = n64psp_time_monotonic_us();
    if (platform.thread_create(platform.userdata, "bench-prod", producer_thread, &prod, 0, 0, &producer) !=
            N64PSP_OK ||
        platform.thread_create(platform.userdata, "bench-cons", consumer_thread, &cons, 0, 0, &consumer) !=
            N64PSP_OK) {
        return 1;
    }
    if (platform.thread_join(platform.userdata, producer, &code) != N64PSP_OK || code != 0) {
        return 1;
    }
    platform.thread_destroy(platform.userdata, producer);
    if (platform.thread_join(platform.userdata, consumer, &code) != N64PSP_OK || code != 0) {
        return 1;
    }
    platform.thread_destroy(platform.userdata, consumer);
    end = n64psp_time_monotonic_us();
    print_rate("blocking handoff capacity=1", (uint64_t)THREAD_PAIRS * 2ULL, end - start);
    return 0;
}

static int independent_thread(void *userdata) {
    bench_case *bc = (bench_case *)userdata;
    OSMesg out = 0;
    int i;

    for (i = 0; i < bc->count; i++) {
        if (osSendMesg(bc->queue, (OSMesg)i, OS_MESG_NOBLOCK) != 0 ||
            osRecvMesg(bc->queue, &out, OS_MESG_NOBLOCK) != 0) {
            return -1;
        }
    }
    return 0;
}

static int bench_independent_queues(void) {
    n64psp_platform_callbacks platform;
    OSMesg storage[4][8];
    OSMesgQueue queues[4];
    bench_case cases[4];
    n64psp_platform_thread *threads[4] = {0};
    n64psp_time_us start;
    n64psp_time_us end;
    int code = -1;
    int i;

    if (n64psp_platform_host_get_callbacks(&platform) != N64PSP_OK) {
        return 1;
    }
    for (i = 0; i < 4; i++) {
        osCreateMesgQueue(&queues[i], storage[i], 8);
        cases[i].queue = &queues[i];
        cases[i].count = THREAD_PAIRS;
    }
    start = n64psp_time_monotonic_us();
    for (i = 0; i < 4; i++) {
        if (platform.thread_create(platform.userdata, "bench-independent", independent_thread, &cases[i], 0, 0,
                                   &threads[i]) != N64PSP_OK) {
            return 1;
        }
    }
    for (i = 0; i < 4; i++) {
        if (platform.thread_join(platform.userdata, threads[i], &code) != N64PSP_OK || code != 0) {
            return 1;
        }
        platform.thread_destroy(platform.userdata, threads[i]);
    }
    end = n64psp_time_monotonic_us();
    print_rate("four independent queues", (uint64_t)THREAD_PAIRS * 8ULL, end - start);
    return 0;
}

int main(void) {
    n64psp_queue_counters counters;

    if (setup_runtime() != 0) {
        puts("benchmark setup failed");
        return 1;
    }
    n64psp_queue_reset_counters();
    if (bench_pairs("uncontended pairs capacity=1", 1, PAIRS) != 0 ||
        bench_pairs("uncontended pairs capacity=64", 64, PAIRS) != 0 || bench_handoff() != 0 ||
        bench_independent_queues() != 0) {
        puts("benchmark failed");
        return 1;
    }
    n64psp_queue_get_counters(&counters);
    printf("queue_counters send=%" PRIu64 " jam=%" PRIu64 " recv=%" PRIu64 " uncontended=%" PRIu64
           " sender_blocks=%" PRIu64 " receiver_blocks=%" PRIu64 " sender_wakes=%" PRIu64
           " receiver_wakes=%" PRIu64 " retries=%" PRIu64 " spurious=%" PRIu64
           " failed_nonblock=%" PRIu64 " max_sender_waiters=%u max_receiver_waiters=%u\n",
           counters.send_calls, counters.jam_calls, counters.recv_calls, counters.uncontended_successes,
           counters.sender_blocks, counters.receiver_blocks, counters.sender_wake_signals,
           counters.receiver_wake_signals, counters.retry_wakeups, counters.spurious_wakeups,
           counters.failed_nonblocking, counters.max_sender_waiters, counters.max_receiver_waiters);
    n64psp_runtime_shutdown();
    return 0;
}
