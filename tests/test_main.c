#include "n64psp/bridge.h"
#include "n64psp/platform.h"
#include "n64psp/runtime.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #expr);                                   \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

#define ARRAY_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define TEST_MAX_QUEUES 32

typedef struct fault_platform {
    n64psp_platform_callbacks base;
    n64psp_platform_sem *sem_wait_hook;
    int sem_create_fail_after;
    int mutex_create_fail_after;
    int sem_post_fail_after;
    int sem_create_calls;
    int mutex_create_calls;
    int sem_post_calls;
    int fatal_count;
} fault_platform;

static int setup_runtime_with(const n64psp_platform_callbacks *platform_override) {
    n64psp_platform_callbacks platform;
    n64psp_renderer_callbacks renderer;

    if (platform_override) {
        platform = *platform_override;
    } else {
        CHECK(n64psp_platform_host_get_callbacks(&platform) == N64PSP_OK);
    }
    CHECK(n64psp_trace_backend_get_callbacks(&renderer) == N64PSP_OK);
    CHECK(n64psp_runtime_register_platform(&platform) == N64PSP_OK);
    CHECK(n64psp_runtime_register_renderer(&renderer) == N64PSP_OK);
    CHECK(n64psp_runtime_init() == N64PSP_OK);
    return 0;
}

static int reset_runtime_with(const n64psp_platform_callbacks *platform_override) {
    if (n64psp_runtime_is_initialized()) {
        CHECK(n64psp_runtime_shutdown() == N64PSP_OK);
    }
    return setup_runtime_with(platform_override);
}

static void fault_log(void *userdata, const char *message) {
    fault_platform *fp = (fault_platform *)userdata;
    fp->base.log(fp->base.userdata, message);
}

static void fault_fatal(void *userdata, const char *message) {
    fault_platform *fp = (fault_platform *)userdata;
    fp->fatal_count++;
    fp->base.fatal(fp->base.userdata, message);
}

static n64psp_time_us fault_monotonic_us(void *userdata) {
    fault_platform *fp = (fault_platform *)userdata;
    return fp->base.monotonic_us(fp->base.userdata);
}

static void fault_sleep_us(void *userdata, uint32_t us) {
    fault_platform *fp = (fault_platform *)userdata;
    fp->base.sleep_us(fp->base.userdata, us);
}

static n64psp_result fault_sem_create(void *userdata, uint32_t initial, uint32_t maximum,
                                      n64psp_platform_sem **out_sem) {
    fault_platform *fp = (fault_platform *)userdata;
    fp->sem_create_calls++;
    if (fp->sem_create_fail_after > 0 && fp->sem_create_calls >= fp->sem_create_fail_after) {
        return N64PSP_ERROR_PLATFORM;
    }
    return fp->base.sem_create(fp->base.userdata, initial, maximum, out_sem);
}

static n64psp_result fault_sem_wait(void *userdata, n64psp_platform_sem *sem) {
    fault_platform *fp = (fault_platform *)userdata;
    if (fp->sem_wait_hook) {
        fp->base.sem_post(fp->base.userdata, fp->sem_wait_hook);
    }
    return fp->base.sem_wait(fp->base.userdata, sem);
}

static n64psp_result fault_sem_try_wait(void *userdata, n64psp_platform_sem *sem) {
    fault_platform *fp = (fault_platform *)userdata;
    return fp->base.sem_try_wait(fp->base.userdata, sem);
}

static n64psp_result fault_sem_post(void *userdata, n64psp_platform_sem *sem) {
    fault_platform *fp = (fault_platform *)userdata;
    fp->sem_post_calls++;
    if (fp->sem_post_fail_after > 0 && fp->sem_post_calls == fp->sem_post_fail_after) {
        return N64PSP_ERROR_PLATFORM;
    }
    return fp->base.sem_post(fp->base.userdata, sem);
}

static void fault_sem_destroy(void *userdata, n64psp_platform_sem *sem) {
    fault_platform *fp = (fault_platform *)userdata;
    fp->base.sem_destroy(fp->base.userdata, sem);
}

static n64psp_result fault_mutex_create(void *userdata, n64psp_platform_mutex **out_mutex) {
    fault_platform *fp = (fault_platform *)userdata;
    fp->mutex_create_calls++;
    if (fp->mutex_create_fail_after > 0 && fp->mutex_create_calls >= fp->mutex_create_fail_after) {
        return N64PSP_ERROR_PLATFORM;
    }
    return fp->base.mutex_create(fp->base.userdata, out_mutex);
}

static n64psp_result fault_mutex_lock(void *userdata, n64psp_platform_mutex *mutex) {
    fault_platform *fp = (fault_platform *)userdata;
    return fp->base.mutex_lock(fp->base.userdata, mutex);
}

static void fault_mutex_unlock(void *userdata, n64psp_platform_mutex *mutex) {
    fault_platform *fp = (fault_platform *)userdata;
    fp->base.mutex_unlock(fp->base.userdata, mutex);
}

static void fault_mutex_destroy(void *userdata, n64psp_platform_mutex *mutex) {
    fault_platform *fp = (fault_platform *)userdata;
    fp->base.mutex_destroy(fp->base.userdata, mutex);
}

static n64psp_result fault_thread_create(void *userdata, const char *name, n64psp_thread_entry entry,
                                         void *thread_userdata, uint32_t stack_size, int priority,
                                         n64psp_platform_thread **out_thread) {
    fault_platform *fp = (fault_platform *)userdata;
    return fp->base.thread_create(fp->base.userdata, name, entry, thread_userdata, stack_size, priority, out_thread);
}

static n64psp_result fault_thread_join(void *userdata, n64psp_platform_thread *thread, int *out_code) {
    fault_platform *fp = (fault_platform *)userdata;
    return fp->base.thread_join(fp->base.userdata, thread, out_code);
}

static void fault_thread_destroy(void *userdata, n64psp_platform_thread *thread) {
    fault_platform *fp = (fault_platform *)userdata;
    fp->base.thread_destroy(fp->base.userdata, thread);
}

static int make_fault_platform(fault_platform *fp, n64psp_platform_callbacks *callbacks) {
    memset(fp, 0, sizeof(*fp));
    CHECK(n64psp_platform_host_get_callbacks(&fp->base) == N64PSP_OK);
    memset(callbacks, 0, sizeof(*callbacks));
    callbacks->log = fault_log;
    callbacks->fatal = fault_fatal;
    callbacks->monotonic_us = fault_monotonic_us;
    callbacks->sleep_us = fault_sleep_us;
    callbacks->sem_create = fault_sem_create;
    callbacks->sem_wait = fault_sem_wait;
    callbacks->sem_try_wait = fault_sem_try_wait;
    callbacks->sem_post = fault_sem_post;
    callbacks->sem_destroy = fault_sem_destroy;
    callbacks->mutex_create = fault_mutex_create;
    callbacks->mutex_lock = fault_mutex_lock;
    callbacks->mutex_unlock = fault_mutex_unlock;
    callbacks->mutex_destroy = fault_mutex_destroy;
    callbacks->thread_create = fault_thread_create;
    callbacks->thread_join = fault_thread_join;
    callbacks->thread_destroy = fault_thread_destroy;
    callbacks->userdata = fp;
    return 0;
}

static int test_fifo_and_jam(void) {
    OSMesg storage[4];
    OSMesgQueue q;
    OSMesg out = 0;

    osCreateMesgQueue(&q, storage, 4);
    CHECK(q.mtqueue == NULL && q.fullqueue == NULL);
    CHECK(q.validCount == 0 && q.msgCount == 4);
    CHECK(osSendMesg(&q, 1, OS_MESG_NOBLOCK) == 0);
    CHECK(osSendMesg(&q, 2, OS_MESG_NOBLOCK) == 0);
    CHECK(osJamMesg(&q, 99, OS_MESG_NOBLOCK) == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 99);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 1);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 2);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == -1);
    return 0;
}

static int test_os_queue_return_contract(void) {
    OSMesg storage[1];
    OSMesgQueue q;
    OSMesg out = 0;

    osCreateMesgQueue(&q, storage, 1);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == -1);
    CHECK(osSendMesg(&q, 1, OS_MESG_NOBLOCK) == 0);
    CHECK(osSendMesg(&q, 2, OS_MESG_NOBLOCK) == -1);
    CHECK(osJamMesg(&q, 3, OS_MESG_NOBLOCK) == -1);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 1);
    CHECK(osRecvMesg(NULL, &out, OS_MESG_NOBLOCK) == -1);
    CHECK(osSendMesg(NULL, 1, OS_MESG_NOBLOCK) == -1);
    CHECK(osJamMesg(NULL, 1, OS_MESG_NOBLOCK) == -1);
    CHECK(osRecvMesg(&q, &out, 99) == -1);
    CHECK(osSendMesg(&q, 1, 99) == -1);
    CHECK(osJamMesg(&q, 1, 99) == -1);
    CHECK(strcmp(n64psp_result_name(N64PSP_ERROR_BUSY), "N64PSP_ERROR_BUSY") == 0);
    return 0;
}

static int test_invalid_capacities(void) {
    OSMesg storage[1];
    OSMesgQueue q;

    memset(&q, 0, sizeof(q));
    osCreateMesgQueue(&q, storage, 0);
    CHECK(osSendMesg(&q, 1, OS_MESG_NOBLOCK) == -1);
    osCreateMesgQueue(&q, storage, -1);
    CHECK(osRecvMesg(&q, NULL, OS_MESG_NOBLOCK) == -1);
    return 0;
}

static int test_full_empty_wraparound(void) {
    OSMesg storage[2];
    OSMesgQueue q;
    OSMesg out = 0;
    int i;

    osCreateMesgQueue(&q, storage, 2);
    for (i = 0; i < 100; i++) {
        CHECK(osSendMesg(&q, (OSMesg)(i * 2), OS_MESG_NOBLOCK) == 0);
        CHECK(osSendMesg(&q, (OSMesg)(i * 2 + 1), OS_MESG_NOBLOCK) == 0);
        CHECK(osSendMesg(&q, 0xdead, OS_MESG_NOBLOCK) == -1);
        CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == (OSMesg)(i * 2));
        CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == (OSMesg)(i * 2 + 1));
        CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == -1);
    }
    return 0;
}

static int test_jam_ordering_cases(void) {
    OSMesg storage[3];
    OSMesg one_storage[1];
    OSMesgQueue q;
    OSMesgQueue one;
    OSMesg out = 0;

    osCreateMesgQueue(&q, storage, 3);
    CHECK(osJamMesg(&q, 10, OS_MESG_NOBLOCK) == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 10);

    CHECK(osSendMesg(&q, 1, OS_MESG_NOBLOCK) == 0);
    CHECK(osSendMesg(&q, 2, OS_MESG_NOBLOCK) == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 1);
    CHECK(osSendMesg(&q, 3, OS_MESG_NOBLOCK) == 0);
    CHECK(osJamMesg(&q, 99, OS_MESG_NOBLOCK) == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 99);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 2);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 3);

    osCreateMesgQueue(&one, one_storage, 1);
    CHECK(osJamMesg(&one, 7, OS_MESG_NOBLOCK) == 0);
    CHECK(osJamMesg(&one, 8, OS_MESG_NOBLOCK) == -1);
    CHECK(osRecvMesg(&one, &out, OS_MESG_NOBLOCK) == 0 && out == 7);
    return 0;
}

typedef struct thread_case {
    OSMesgQueue *q;
    OSMesg value;
} thread_case;

static int send_thread(void *userdata) {
    thread_case *tc = (thread_case *)userdata;
    return osSendMesg(tc->q, tc->value, OS_MESG_BLOCK);
}

static int jam_thread(void *userdata) {
    thread_case *tc = (thread_case *)userdata;
    return osJamMesg(tc->q, tc->value, OS_MESG_BLOCK);
}

static int recv_thread(void *userdata) {
    thread_case *tc = (thread_case *)userdata;
    return osRecvMesg(tc->q, &tc->value, OS_MESG_BLOCK);
}

static int test_blocking_receive_send_and_jam(void) {
    n64psp_platform_callbacks platform;
    OSMesg storage[1];
    OSMesgQueue q;
    OSMesg out = 0;
    n64psp_platform_thread *thread = NULL;
    int code = -1;

    CHECK(n64psp_platform_host_get_callbacks(&platform) == N64PSP_OK);
    osCreateMesgQueue(&q, storage, 1);

    thread_case recv_case = {&q, 0};
    CHECK(platform.thread_create(platform.userdata, "recv", recv_thread, &recv_case, 0, 0, &thread) == N64PSP_OK);
    platform.sleep_us(platform.userdata, 10000);
    CHECK(osSendMesg(&q, 123, OS_MESG_BLOCK) == 0);
    CHECK(platform.thread_join(platform.userdata, thread, &code) == N64PSP_OK);
    platform.thread_destroy(platform.userdata, thread);
    CHECK(code == 0 && recv_case.value == 123);

    CHECK(osSendMesg(&q, 7, OS_MESG_BLOCK) == 0);
    thread_case send_case = {&q, 8};
    CHECK(platform.thread_create(platform.userdata, "send", send_thread, &send_case, 0, 0, &thread) == N64PSP_OK);
    platform.sleep_us(platform.userdata, 10000);
    CHECK(osRecvMesg(&q, &out, OS_MESG_BLOCK) == 0 && out == 7);
    CHECK(platform.thread_join(platform.userdata, thread, &code) == N64PSP_OK);
    platform.thread_destroy(platform.userdata, thread);
    CHECK(code == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_BLOCK) == 0 && out == 8);

    CHECK(osSendMesg(&q, 1, OS_MESG_BLOCK) == 0);
    thread_case jam_case = {&q, 2};
    CHECK(platform.thread_create(platform.userdata, "jam", jam_thread, &jam_case, 0, 0, &thread) == N64PSP_OK);
    platform.sleep_us(platform.userdata, 10000);
    CHECK(osRecvMesg(&q, &out, OS_MESG_BLOCK) == 0 && out == 1);
    CHECK(platform.thread_join(platform.userdata, thread, &code) == N64PSP_OK);
    platform.thread_destroy(platform.userdata, thread);
    CHECK(code == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_BLOCK) == 0 && out == 2);
    return 0;
}

typedef struct churn_case {
    OSMesgQueue *queue;
    int id;
    int count;
    uint64_t sum;
} churn_case;

static int producer_thread(void *userdata) {
    churn_case *tc = (churn_case *)userdata;
    int i;

    for (i = 0; i < tc->count; i++) {
        OSMesg value = (OSMesg)(tc->id * 100000 + i);
        if (osSendMesg(tc->queue, value, OS_MESG_BLOCK) != 0) {
            return -1;
        }
        tc->sum += (uint64_t)value;
    }
    return 0;
}

static int consumer_thread(void *userdata) {
    churn_case *tc = (churn_case *)userdata;
    int i;

    for (i = 0; i < tc->count; i++) {
        OSMesg value = 0;
        if (osRecvMesg(tc->queue, &value, OS_MESG_BLOCK) != 0) {
            return -1;
        }
        tc->sum += (uint64_t)value;
    }
    return 0;
}

static int test_multiple_producers_consumers(void) {
    n64psp_platform_callbacks platform;
    OSMesg storage[8];
    OSMesgQueue q;
    n64psp_platform_thread *threads[4] = {0};
    churn_case cases[4];
    uint64_t produced = 0;
    uint64_t consumed = 0;
    int code = 0;
    int i;

    CHECK(n64psp_platform_host_get_callbacks(&platform) == N64PSP_OK);
    osCreateMesgQueue(&q, storage, ARRAY_COUNT(storage));
    memset(cases, 0, sizeof(cases));
    for (i = 0; i < 2; i++) {
        cases[i].queue = &q;
        cases[i].id = i + 1;
        cases[i].count = 256;
        CHECK(platform.thread_create(platform.userdata, "producer", producer_thread, &cases[i], 0, 0, &threads[i]) ==
              N64PSP_OK);
    }
    for (i = 2; i < 4; i++) {
        cases[i].queue = &q;
        cases[i].count = 256;
        CHECK(platform.thread_create(platform.userdata, "consumer", consumer_thread, &cases[i], 0, 0, &threads[i]) ==
              N64PSP_OK);
    }
    for (i = 0; i < 4; i++) {
        CHECK(platform.thread_join(platform.userdata, threads[i], &code) == N64PSP_OK);
        platform.thread_destroy(platform.userdata, threads[i]);
        CHECK(code == 0);
    }
    produced = cases[0].sum + cases[1].sum;
    consumed = cases[2].sum + cases[3].sum;
    CHECK(produced == consumed);
    return 0;
}

static int test_thousands_of_handoffs(void) {
    n64psp_platform_callbacks platform;
    OSMesg storage[1];
    OSMesgQueue q;
    n64psp_platform_thread *producer = NULL;
    n64psp_platform_thread *consumer = NULL;
    churn_case prod_case;
    churn_case cons_case;
    int code = -1;

    CHECK(n64psp_platform_host_get_callbacks(&platform) == N64PSP_OK);
    osCreateMesgQueue(&q, storage, 1);
    memset(&prod_case, 0, sizeof(prod_case));
    memset(&cons_case, 0, sizeof(cons_case));
    prod_case.queue = &q;
    prod_case.id = 5;
    prod_case.count = 3000;
    cons_case.queue = &q;
    cons_case.count = 3000;
    CHECK(platform.thread_create(platform.userdata, "producer", producer_thread, &prod_case, 0, 0, &producer) ==
          N64PSP_OK);
    CHECK(platform.thread_create(platform.userdata, "consumer", consumer_thread, &cons_case, 0, 0, &consumer) ==
          N64PSP_OK);
    CHECK(platform.thread_join(platform.userdata, producer, &code) == N64PSP_OK);
    platform.thread_destroy(platform.userdata, producer);
    CHECK(code == 0);
    CHECK(platform.thread_join(platform.userdata, consumer, &code) == N64PSP_OK);
    platform.thread_destroy(platform.userdata, consumer);
    CHECK(code == 0);
    CHECK(prod_case.sum == cons_case.sum);
    return 0;
}

static int test_registry_exhaustion(void) {
    OSMesg storage[TEST_MAX_QUEUES + 1][1];
    OSMesgQueue queues[TEST_MAX_QUEUES + 1];
    int i;

    CHECK(reset_runtime_with(NULL) == 0);
    memset(queues, 0, sizeof(queues));
    for (i = 0; i < TEST_MAX_QUEUES; i++) {
        osCreateMesgQueue(&queues[i], storage[i], 1);
        CHECK(osSendMesg(&queues[i], 1, OS_MESG_NOBLOCK) == 0);
    }
    osCreateMesgQueue(&queues[TEST_MAX_QUEUES], storage[TEST_MAX_QUEUES], 1);
    CHECK(osSendMesg(&queues[TEST_MAX_QUEUES], 1, OS_MESG_NOBLOCK) == -1);
    CHECK(reset_runtime_with(NULL) == 0);
    return 0;
}

static int test_reinit_and_address_reuse(void) {
    OSMesg storage_a[2];
    OSMesg storage_b[1];
    OSMesgQueue q;
    OSMesg out = 0;

    osCreateMesgQueue(&q, storage_a, 2);
    CHECK(osSendMesg(&q, 1, OS_MESG_NOBLOCK) == 0);
    osCreateMesgQueue(&q, storage_b, 1);
    CHECK(q.validCount == 0 && q.msgCount == 1 && q.msg == storage_b);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == -1);
    CHECK(osSendMesg(&q, 2, OS_MESG_NOBLOCK) == 0);
    CHECK(osSendMesg(&q, 3, OS_MESG_NOBLOCK) == -1);
    CHECK(reset_runtime_with(NULL) == 0);
    osCreateMesgQueue(&q, storage_a, 2);
    CHECK(osSendMesg(&q, 4, OS_MESG_NOBLOCK) == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 4);
    return 0;
}

static int wait_for_hook(fault_platform *fp) {
    return fp->base.sem_wait(fp->base.userdata, fp->sem_wait_hook) == N64PSP_OK ? 0 : 1;
}

static int test_reinit_and_shutdown_busy_waiters(void) {
    fault_platform fp;
    n64psp_platform_callbacks callbacks;
    OSMesg storage[1];
    OSMesg replacement[2];
    OSMesgQueue q;
    n64psp_platform_thread *thread = NULL;
    thread_case tc;
    int code = -1;
    OSMesg out = 0;

    CHECK(make_fault_platform(&fp, &callbacks) == 0);
    CHECK(reset_runtime_with(&callbacks) == 0);
    CHECK(fp.base.sem_create(fp.base.userdata, 0, 16, &fp.sem_wait_hook) == N64PSP_OK);
    osCreateMesgQueue(&q, storage, 1);
    tc.q = &q;
    tc.value = 0;
    CHECK(callbacks.thread_create(callbacks.userdata, "blocked-recv", recv_thread, &tc, 0, 0, &thread) == N64PSP_OK);
    CHECK(wait_for_hook(&fp) == 0);
    osCreateMesgQueue(&q, replacement, 2);
    CHECK(q.msg == storage && q.msgCount == 1);
    CHECK(n64psp_runtime_shutdown() == N64PSP_ERROR_BUSY);
    CHECK(n64psp_runtime_is_initialized());
    CHECK(osSendMesg(&q, 42, OS_MESG_NOBLOCK) == 0);
    CHECK(callbacks.thread_join(callbacks.userdata, thread, &code) == N64PSP_OK);
    callbacks.thread_destroy(callbacks.userdata, thread);
    CHECK(code == 0 && tc.value == 42);

    CHECK(osSendMesg(&q, 7, OS_MESG_NOBLOCK) == 0);
    tc.value = 8;
    CHECK(callbacks.thread_create(callbacks.userdata, "blocked-send", send_thread, &tc, 0, 0, &thread) == N64PSP_OK);
    CHECK(wait_for_hook(&fp) == 0);
    osCreateMesgQueue(&q, replacement, 2);
    CHECK(q.msg == storage && q.msgCount == 1);
    CHECK(n64psp_runtime_shutdown() == N64PSP_ERROR_BUSY);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 7);
    CHECK(callbacks.thread_join(callbacks.userdata, thread, &code) == N64PSP_OK);
    callbacks.thread_destroy(callbacks.userdata, thread);
    CHECK(code == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 8);
    fp.base.sem_destroy(fp.base.userdata, fp.sem_wait_hook);
    fp.sem_wait_hook = NULL;
    CHECK(n64psp_runtime_shutdown() == N64PSP_OK);
    CHECK(setup_runtime_with(NULL) == 0);
    return 0;
}

static int test_concurrent_create_and_lookup(void) {
    fault_platform fp;
    n64psp_platform_callbacks callbacks;
    OSMesg storage[2];
    OSMesgQueue q;
    n64psp_platform_thread *thread = NULL;
    thread_case tc;
    OSMesg out = 0;
    int code = -1;
    int i;

    CHECK(make_fault_platform(&fp, &callbacks) == 0);
    CHECK(reset_runtime_with(&callbacks) == 0);
    CHECK(fp.base.sem_create(fp.base.userdata, 0, 16, &fp.sem_wait_hook) == N64PSP_OK);
    osCreateMesgQueue(&q, storage, 2);
    tc.q = &q;
    tc.value = 0;
    CHECK(callbacks.thread_create(callbacks.userdata, "recv", recv_thread, &tc, 0, 0, &thread) == N64PSP_OK);
    CHECK(wait_for_hook(&fp) == 0);
    for (i = 0; i < 32; i++) {
        OSMesg replacement[2];
        osCreateMesgQueue(&q, replacement, 2);
        CHECK(q.msg == storage);
    }
    CHECK(osSendMesg(&q, 55, OS_MESG_NOBLOCK) == 0);
    CHECK(callbacks.thread_join(callbacks.userdata, thread, &code) == N64PSP_OK);
    callbacks.thread_destroy(callbacks.userdata, thread);
    CHECK(code == 0 && tc.value == 55);
    fp.base.sem_destroy(fp.base.userdata, fp.sem_wait_hook);
    fp.sem_wait_hook = NULL;
    osCreateMesgQueue(&q, storage, 2);
    CHECK(osSendMesg(&q, 56, OS_MESG_NOBLOCK) == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 56);
    CHECK(reset_runtime_with(NULL) == 0);
    return 0;
}

static int test_post_failure_rollbacks(void) {
    fault_platform fp;
    n64psp_platform_callbacks callbacks;
    OSMesg storage[1];
    OSMesgQueue q;
    OSMesg out = 0;

    CHECK(make_fault_platform(&fp, &callbacks) == 0);
    CHECK(reset_runtime_with(&callbacks) == 0);
    osCreateMesgQueue(&q, storage, 1);
    fp.sem_post_calls = 0;
    fp.sem_post_fail_after = 1;
    CHECK(osSendMesg(&q, 1, OS_MESG_NOBLOCK) == -1);
    fp.sem_post_fail_after = 0;
    CHECK(q.validCount == 0);
    CHECK(osSendMesg(&q, 2, OS_MESG_NOBLOCK) == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 2);

    CHECK(osSendMesg(&q, 3, OS_MESG_NOBLOCK) == 0);
    fp.sem_post_calls = 0;
    fp.sem_post_fail_after = 1;
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == -1);
    fp.sem_post_fail_after = 0;
    CHECK(q.validCount == 1);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 3);
    CHECK(reset_runtime_with(NULL) == 0);
    return 0;
}

static int test_partial_creation_failure(void) {
    fault_platform fp;
    n64psp_platform_callbacks callbacks;
    OSMesg storage[1];
    OSMesg replacement[2];
    OSMesgQueue q;
    OSMesg out = 0;

    CHECK(make_fault_platform(&fp, &callbacks) == 0);
    CHECK(reset_runtime_with(&callbacks) == 0);
    fp.sem_create_fail_after = 1;
    osCreateMesgQueue(&q, storage, 1);
    CHECK(osSendMesg(&q, 1, OS_MESG_NOBLOCK) == -1);
    fp.sem_create_fail_after = 0;
    osCreateMesgQueue(&q, storage, 1);
    CHECK(osSendMesg(&q, 2, OS_MESG_NOBLOCK) == 0);
    fp.sem_create_calls = 0;
    fp.sem_create_fail_after = 1;
    osCreateMesgQueue(&q, replacement, 2);
    fp.sem_create_fail_after = 0;
    CHECK(q.msg == storage && q.msgCount == 1);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 2);
    CHECK(reset_runtime_with(NULL) == 0);
    return 0;
}

static int test_time(void) {
    n64psp_time_us a = n64psp_time_monotonic_us();
    n64psp_time_us b = n64psp_time_monotonic_us();

    CHECK(b >= a);
    CHECK(n64psp_time_us_to_ticks(1000000) == N64PSP_TICKS_PER_SECOND);
    CHECK(n64psp_time_ms_to_ticks(1000) == N64PSP_TICKS_PER_SECOND);
    CHECK(n64psp_time_ticks_to_us(N64PSP_TICKS_PER_SECOND) == 1000000);
    CHECK(n64psp_time_us_to_ticks(UINT64_MAX) == UINT64_MAX);
    CHECK(n64psp_time_ticks_to_us(UINT64_MAX) != UINT64_MAX);
    CHECK(n64psp_time_us_to_ticks(UINT64_MAX / N64PSP_TICKS_PER_SECOND) != UINT64_MAX);
    CHECK(n64psp_time_after_eq(10, 9));
    return 0;
}

static n64psp_result bounded_pi_read(void *userdata, uint32_t offset, void *dst, size_t size) {
    return n64psp_rom_read((const n64psp_rom *)userdata, offset, dst, size);
}

static int test_bridge(void) {
    uint8_t mem[16] = {0};
    uint8_t rom[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    n64psp_rom rom_region;
    n64psp_platform_callbacks platform;
    n64psp_rdram rdram;
    uint32_t value = 0;
    uint64_t value64 = 0;
    uint8_t out[4] = {0};
    void *ptr = NULL;

    CHECK(n64psp_rdram_register(&rdram, mem, sizeof(mem), 0x80000000u) == N64PSP_OK);
    CHECK(n64psp_rdram_store_be32(&rdram, 0x80000000u, 0x12345678u) == N64PSP_OK);
    CHECK(n64psp_rdram_load_be32(&rdram, 0x80000000u, &value) == N64PSP_OK && value == 0x12345678u);
    CHECK(n64psp_rdram_store_be64(&rdram, 0x80000008u, 0x1122334455667788ULL) == N64PSP_OK);
    CHECK(n64psp_rdram_load_be64(&rdram, 0x80000008u, &value64) == N64PSP_OK && value64 == 0x1122334455667788ULL);
    CHECK(n64psp_rdram_store_be64(&rdram, 0x8000000cu, 0) == N64PSP_ERROR_OUT_OF_RANGE);
    CHECK(n64psp_rdram_translate(&rdram, 0x8000000fu, 1, &ptr) == N64PSP_OK);
    CHECK(n64psp_rdram_translate(&rdram, 0x80000010u, 1, &ptr) == N64PSP_ERROR_OUT_OF_RANGE);
    CHECK(n64psp_rdram_translate(&rdram, 0xffffffffu, 8, &ptr) == N64PSP_ERROR_OUT_OF_RANGE);
    CHECK(n64psp_platform_host_get_callbacks(&platform) == N64PSP_OK);
    CHECK(n64psp_rom_register(&rom_region, rom, sizeof(rom)) == N64PSP_OK);
    CHECK(n64psp_rom_read(&rom_region, 6, out, 2) == N64PSP_OK);
    CHECK(n64psp_rom_read(&rom_region, 7, out, 2) == N64PSP_ERROR_OUT_OF_RANGE);
    platform.pi_read = bounded_pi_read;
    platform.userdata = &rom_region;
    CHECK(reset_runtime_with(&platform) == 0);
    CHECK(n64psp_pi_read(2, out, 4) == N64PSP_OK);
    CHECK(out[0] == 3 && out[3] == 6);
    CHECK(n64psp_pi_read(7, out, 2) == N64PSP_ERROR_OUT_OF_RANGE);
    CHECK(reset_runtime_with(NULL) == 0);
    return 0;
}

static int test_task(void) {
    uint8_t ucode[4] = {0};
    uint8_t data[4] = {0};
    OSTask task = {1, 2, ucode, sizeof(ucode), data, sizeof(data)};
    n64psp_task_record record;

    CHECK(n64psp_submit_task(&task, &record) == N64PSP_ERROR_UNSUPPORTED);
    CHECK(record.type == 1 && record.result == N64PSP_ERROR_UNSUPPORTED);
    return 0;
}

int main(void) {
    CHECK(setup_runtime_with(NULL) == 0);
    CHECK(test_fifo_and_jam() == 0);
    CHECK(test_os_queue_return_contract() == 0);
    CHECK(test_invalid_capacities() == 0);
    CHECK(test_full_empty_wraparound() == 0);
    CHECK(test_jam_ordering_cases() == 0);
    CHECK(test_blocking_receive_send_and_jam() == 0);
    CHECK(test_multiple_producers_consumers() == 0);
    CHECK(test_thousands_of_handoffs() == 0);
    CHECK(test_registry_exhaustion() == 0);
    CHECK(test_reinit_and_address_reuse() == 0);
    CHECK(test_reinit_and_shutdown_busy_waiters() == 0);
    CHECK(test_concurrent_create_and_lookup() == 0);
    CHECK(test_post_failure_rollbacks() == 0);
    CHECK(test_partial_creation_failure() == 0);
    CHECK(test_time() == 0);
    CHECK(test_bridge() == 0);
    CHECK(test_task() == 0);
    CHECK(n64psp_runtime_shutdown() == N64PSP_OK);
    puts("n64psp tests passed");
    return 0;
}
