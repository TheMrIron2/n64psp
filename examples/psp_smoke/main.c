#include "n64psp/bridge.h"
#include "n64psp/platform.h"
#include "n64psp/runtime.h"
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspkernel.h>
#include <stdio.h>

PSP_MODULE_INFO("n64psp_smoke", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

static int exit_callback(int arg1, int arg2, void *common) {
    (void)arg1;
    (void)arg2;
    (void)common;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp) {
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void) {
    SceUID thid = sceKernelCreateThread("update_thread", callback_thread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, 0);
    }
}

typedef struct psp_thread_case {
    OSMesgQueue *queue;
    OSMesg value;
} psp_thread_case;

static int psp_send_thread(void *userdata) {
    psp_thread_case *tc = (psp_thread_case *)userdata;
    return osSendMesg(tc->queue, tc->value, OS_MESG_BLOCK);
}

#if N64PSP_PSP_BENCHMARKS
typedef struct psp_pingpong_case {
    OSMesgQueue *request_queue;
    OSMesgQueue *ack_queue;
    int round_trips;
} psp_pingpong_case;

static void print_psp_queue_benchmark(const char *name, uint64_t ops, n64psp_time_us elapsed) {
    uint64_t ops_per_sec = elapsed ? (ops * 1000000ULL) / elapsed : 0;

    pspDebugScreenPrintf("%s: ops=%llu elapsed_us=%llu ops/s=%llu\n", name, (unsigned long long)ops,
                         (unsigned long long)elapsed, (unsigned long long)ops_per_sec);
}

static int run_psp_queue_benchmark(void) {
    enum { PAIRS = 100000 };
    OSMesg storage[16];
    OSMesgQueue queue;
    OSMesg msg = 0;
    n64psp_time_us start;
    n64psp_time_us end;
    int i;

    osCreateMesgQueue(&queue, storage, 1);
    start = n64psp_time_monotonic_us();
    for (i = 0; i < PAIRS; i++) {
        if (osSendMesg(&queue, (OSMesg)i, OS_MESG_NOBLOCK) != 0 ||
            osRecvMesg(&queue, &msg, OS_MESG_NOBLOCK) != 0 || msg != (OSMesg)i) {
            return 1;
        }
    }
    end = n64psp_time_monotonic_us();
    print_psp_queue_benchmark("queue bench cap=1", (uint64_t)PAIRS * 2ULL, end - start);

    osCreateMesgQueue(&queue, storage, 16);
    start = n64psp_time_monotonic_us();
    for (i = 0; i < PAIRS; i++) {
        if (osSendMesg(&queue, (OSMesg)i, OS_MESG_NOBLOCK) != 0 ||
            osRecvMesg(&queue, &msg, OS_MESG_NOBLOCK) != 0 || msg != (OSMesg)i) {
            return 1;
        }
    }
    end = n64psp_time_monotonic_us();
    print_psp_queue_benchmark("queue bench cap=16", (uint64_t)PAIRS * 2ULL, end - start);
    return 0;
}

static int psp_pingpong_thread(void *userdata) {
    psp_pingpong_case *pc = (psp_pingpong_case *)userdata;
    int i;

    for (i = 0; i < pc->round_trips; i++) {
        OSMesg msg = 0;
        if (osRecvMesg(pc->request_queue, &msg, OS_MESG_BLOCK) != 0 ||
            osSendMesg(pc->ack_queue, msg, OS_MESG_BLOCK) != 0) {
            return -1;
        }
    }
    return 0;
}

static int run_psp_pingpong_benchmark(const n64psp_platform_callbacks *platform) {
    enum { ROUND_TRIPS = 20000 };
    OSMesg request_storage[1];
    OSMesg ack_storage[1];
    OSMesgQueue request_queue;
    OSMesgQueue ack_queue;
    psp_pingpong_case pc;
    n64psp_platform_thread *thread = NULL;
    n64psp_time_us start;
    n64psp_time_us end;
    int code = -1;
    int i;

    osCreateMesgQueue(&request_queue, request_storage, 1);
    osCreateMesgQueue(&ack_queue, ack_storage, 1);
    pc.request_queue = &request_queue;
    pc.ack_queue = &ack_queue;
    pc.round_trips = ROUND_TRIPS;
    if (platform->thread_create(platform->userdata, "n64psp-pingpong", psp_pingpong_thread, &pc, 0x4000, 0x20,
                                &thread) != N64PSP_OK) {
        return 1;
    }
    n64psp_queue_reset_counters();
    start = n64psp_time_monotonic_us();
    for (i = 0; i < ROUND_TRIPS; i++) {
        OSMesg ack = 0;
        if (osSendMesg(&request_queue, (OSMesg)i, OS_MESG_BLOCK) != 0 ||
            osRecvMesg(&ack_queue, &ack, OS_MESG_BLOCK) != 0 || ack != (OSMesg)i) {
            return 1;
        }
    }
    if (platform->thread_join(platform->userdata, thread, &code) != N64PSP_OK || code != 0) {
        return 1;
    }
    platform->thread_destroy(platform->userdata, thread);
    end = n64psp_time_monotonic_us();
    print_psp_queue_benchmark("queue pingpong cap=1 two-thread roundtrip", (uint64_t)ROUND_TRIPS * 4ULL,
                              end - start);
    pspDebugScreenPrintf("queue pingpong round_trips=%d producer_to_consumer=request consumer_to_producer=ack\n",
                         ROUND_TRIPS);
    n64psp_queue_dump_counters("psp_pingpong");
    return 0;
}
#endif

int main(void) {
    pspDebugScreenInit();
    setup_callbacks();

    n64psp_platform_callbacks platform;
    n64psp_renderer_callbacks renderer;
    OSMesg storage[1];
    OSMesgQueue queue;
    OSMesg msg = 0;
    n64psp_platform_thread *thread = NULL;
    int thread_code = -1;
    uint8_t rdram_mem[8] = {0};
    n64psp_rdram rdram;
    uint8_t ucode[4] = {0};
    uint8_t data[4] = {0};
    OSTask task = {1, 0, ucode, sizeof(ucode), data, sizeof(data)};
    n64psp_task_record record;

    if (n64psp_platform_psp_get_callbacks(&platform) != N64PSP_OK ||
        n64psp_trace_backend_get_callbacks(&renderer) != N64PSP_OK ||
        n64psp_runtime_register_platform(&platform) != N64PSP_OK ||
        n64psp_runtime_register_renderer(&renderer) != N64PSP_OK || n64psp_runtime_init() != N64PSP_OK) {
        pspDebugScreenPrintf("runtime setup failed\n");
        sceKernelExitGame();
        return 1;
    }

    osCreateMesgQueue(&queue, storage, 1);
    if (osSendMesg(&queue, 11, OS_MESG_NOBLOCK) != 0 || osRecvMesg(&queue, &msg, OS_MESG_NOBLOCK) != 0 || msg != 11) {
        pspDebugScreenPrintf("nonblocking queue failed\n");
        sceKernelExitGame();
        return 2;
    }

    psp_thread_case tc = {&queue, 77};
    if (platform.thread_create(platform.userdata, "n64psp-send", psp_send_thread, &tc, 0x4000, 0x20, &thread) !=
        N64PSP_OK) {
        pspDebugScreenPrintf("thread create failed\n");
        sceKernelExitGame();
        return 3;
    }
    platform.sleep_us(platform.userdata, 10000);
    if (osRecvMesg(&queue, &msg, OS_MESG_BLOCK) != 0 || msg != 77 ||
        platform.thread_join(platform.userdata, thread, &thread_code) != N64PSP_OK || thread_code != 0) {
        pspDebugScreenPrintf("blocking queue failed\n");
        sceKernelExitGame();
        return 4;
    }
    platform.thread_destroy(platform.userdata, thread);

    pspDebugScreenPrintf("time=%llu us ticks/ms=%llu\n", (unsigned long long)n64psp_time_monotonic_us(),
                         (unsigned long long)n64psp_time_ms_to_ticks(1));
    if (n64psp_rdram_register(&rdram, rdram_mem, sizeof(rdram_mem), 0x80000000u) != N64PSP_OK ||
        n64psp_rdram_store_be32(&rdram, 0x80000000u, 0x11223344u) != N64PSP_OK ||
        n64psp_submit_task(&task, &record) != N64PSP_ERROR_UNSUPPORTED) {
        pspDebugScreenPrintf("bridge/task smoke failed\n");
        sceKernelExitGame();
        return 5;
    }
#if N64PSP_PSP_BENCHMARKS
    if (run_psp_queue_benchmark() != 0) {
        pspDebugScreenPrintf("queue benchmark failed\n");
        sceKernelExitGame();
        return 6;
    }
    if (run_psp_pingpong_benchmark(&platform) != 0) {
        pspDebugScreenPrintf("queue pingpong benchmark failed\n");
        sceKernelExitGame();
        return 7;
    }
#endif
    n64psp_runtime_shutdown();
    pspDebugScreenPrintf("n64psp PSP smoke passed\n");
    sceDisplayWaitVblankStart();
    sceKernelDelayThread(1000000);
    sceKernelExitGame();
    return 0;
}
