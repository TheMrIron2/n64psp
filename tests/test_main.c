#include "n64psp/bridge.h"
#include "n64psp/platform.h"
#include "n64psp/runtime.h"
#include <stdio.h>
#include <string.h>

#define CHECK(expr)                                                                                                    \
    do {                                                                                                               \
        if (!(expr)) {                                                                                                 \
            fprintf(stderr, "check failed: %s:%d: %s\n", __FILE__, __LINE__, #expr);                                   \
            return 1;                                                                                                  \
        }                                                                                                              \
    } while (0)

static int setup_runtime(void) {
    n64psp_platform_callbacks platform;
    n64psp_renderer_callbacks renderer;
    CHECK(n64psp_platform_host_get_callbacks(&platform) == N64PSP_OK);
    CHECK(n64psp_trace_backend_get_callbacks(&renderer) == N64PSP_OK);
    CHECK(n64psp_runtime_register_platform(&platform) == N64PSP_OK);
    CHECK(n64psp_runtime_register_renderer(&renderer) == N64PSP_OK);
    CHECK(n64psp_runtime_init() == N64PSP_OK);
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
    CHECK(q.validCount == 1);
    CHECK(osSendMesg(&q, 2, OS_MESG_NOBLOCK) == 0);
    CHECK(osJamMesg(&q, 99, OS_MESG_NOBLOCK) == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 99);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 1);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 2);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == N64PSP_ERROR_QUEUE_EMPTY);
    return 0;
}

static int test_queue_reinit_and_shutdown(void) {
    OSMesg storage_a[2];
    OSMesg storage_b[1];
    OSMesgQueue q;
    OSMesg out = 0;
    osCreateMesgQueue(&q, storage_a, 2);
    CHECK(osSendMesg(&q, 1, OS_MESG_NOBLOCK) == 0);
    osCreateMesgQueue(&q, storage_b, 1);
    CHECK(q.validCount == 0 && q.msgCount == 1 && q.msg == storage_b);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == N64PSP_ERROR_QUEUE_EMPTY);
    CHECK(osSendMesg(&q, 2, OS_MESG_NOBLOCK) == 0);
    CHECK(osSendMesg(&q, 3, OS_MESG_NOBLOCK) == N64PSP_ERROR_QUEUE_FULL);
    CHECK(n64psp_runtime_shutdown() == N64PSP_OK);
    CHECK(osSendMesg(&q, 4, OS_MESG_NOBLOCK) == N64PSP_ERROR_INVALID_ARGUMENT);
    return setup_runtime();
}

static int test_full_and_wrap(void) {
    OSMesg storage[2];
    OSMesgQueue q;
    OSMesg out = 0;
    osCreateMesgQueue(&q, storage, 2);
    CHECK(osSendMesg(&q, 1, OS_MESG_NOBLOCK) == 0);
    CHECK(osSendMesg(&q, 2, OS_MESG_NOBLOCK) == 0);
    CHECK(osSendMesg(&q, 3, OS_MESG_NOBLOCK) == N64PSP_ERROR_QUEUE_FULL);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 1);
    CHECK(osSendMesg(&q, 3, OS_MESG_NOBLOCK) == 0);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 2);
    CHECK(osRecvMesg(&q, &out, OS_MESG_NOBLOCK) == 0 && out == 3);
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

static int recv_thread(void *userdata) {
    thread_case *tc = (thread_case *)userdata;
    return osRecvMesg(tc->q, &tc->value, OS_MESG_BLOCK);
}

static int test_blocking_receive_and_send(void) {
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
    CHECK(n64psp_runtime_shutdown() == N64PSP_OK);
    CHECK(n64psp_runtime_register_platform(&platform) == N64PSP_OK);
    n64psp_renderer_callbacks renderer;
    CHECK(n64psp_trace_backend_get_callbacks(&renderer) == N64PSP_OK);
    CHECK(n64psp_runtime_register_renderer(&renderer) == N64PSP_OK);
    CHECK(n64psp_runtime_init() == N64PSP_OK);
    CHECK(n64psp_pi_read(2, out, 4) == N64PSP_OK);
    CHECK(out[0] == 3 && out[3] == 6);
    CHECK(n64psp_pi_read(7, out, 2) == N64PSP_ERROR_OUT_OF_RANGE);
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
    CHECK(setup_runtime() == 0);
    CHECK(test_fifo_and_jam() == 0);
    CHECK(test_queue_reinit_and_shutdown() == 0);
    CHECK(test_full_and_wrap() == 0);
    CHECK(test_blocking_receive_and_send() == 0);
    CHECK(test_time() == 0);
    CHECK(test_bridge() == 0);
    CHECK(test_task() == 0);
    CHECK(n64psp_runtime_shutdown() == N64PSP_OK);
    puts("n64psp tests passed");
    return 0;
}
