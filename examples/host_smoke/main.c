#include "n64psp/bridge.h"
#include "n64psp/platform.h"
#include "n64psp/runtime.h"
#include <stdio.h>

int main(void) {
    n64psp_platform_callbacks platform;
    n64psp_renderer_callbacks renderer;
    OSMesg storage[2];
    OSMesgQueue queue;
    OSMesg msg = 0;
    uint8_t rdram_mem[8] = {0};
    n64psp_rdram rdram;
    uint8_t ucode[4] = {0};
    uint8_t data[4] = {0};
    OSTask task = {1, 0, ucode, sizeof(ucode), data, sizeof(data)};
    n64psp_task_record record;

    if (n64psp_platform_host_get_callbacks(&platform) != N64PSP_OK ||
        n64psp_trace_backend_get_callbacks(&renderer) != N64PSP_OK ||
        n64psp_runtime_register_platform(&platform) != N64PSP_OK ||
        n64psp_runtime_register_renderer(&renderer) != N64PSP_OK || n64psp_runtime_init() != N64PSP_OK) {
        return 1;
    }

    osCreateMesgQueue(&queue, storage, 2);
    if (osSendMesg(&queue, 42, OS_MESG_BLOCK) != 0 || osRecvMesg(&queue, &msg, OS_MESG_BLOCK) != 0 || msg != 42) {
        return 2;
    }
    printf("ticks/ms=%llu\n", (unsigned long long)n64psp_time_ms_to_ticks(1));
    if (n64psp_rdram_register(&rdram, rdram_mem, sizeof(rdram_mem), 0x80000000u) != N64PSP_OK ||
        n64psp_rdram_store_be32(&rdram, 0x80000000u, 0xaabbccddu) != N64PSP_OK) {
        return 3;
    }
    if (n64psp_submit_task(&task, &record) != N64PSP_ERROR_UNSUPPORTED) {
        return 4;
    }
    if (n64psp_runtime_shutdown() != N64PSP_OK) {
        return 5;
    }
    puts("n64psp host smoke passed");
    return 0;
}
