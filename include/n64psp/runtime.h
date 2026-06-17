#ifndef N64PSP_RUNTIME_H
#define N64PSP_RUNTIME_H

#include "n64psp/platform.h"
#include "n64psp/renderer.h"
#include "n64psp/result.h"
#include "n64psp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define N64PSP_TICKS_PER_SECOND 46875000ULL

n64psp_result n64psp_runtime_register_platform(const n64psp_platform_callbacks *callbacks);
n64psp_result n64psp_runtime_register_renderer(const n64psp_renderer_callbacks *callbacks);
n64psp_result n64psp_runtime_init(void);
n64psp_result n64psp_runtime_shutdown(void);
int n64psp_runtime_is_initialized(void);
void n64psp_log(const char *message);
void n64psp_fatal(const char *message);

n64psp_time_us n64psp_time_monotonic_us(void);
n64psp_ticks n64psp_time_us_to_ticks(uint64_t us);
n64psp_ticks n64psp_time_ms_to_ticks(uint64_t ms);
uint64_t n64psp_time_ticks_to_us(n64psp_ticks ticks);
int n64psp_time_after_eq(n64psp_ticks a, n64psp_ticks b);

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, int count);
int osSendMesg(OSMesgQueue *mq, OSMesg msg, int flag);
int osJamMesg(OSMesgQueue *mq, OSMesg msg, int flag);
int osRecvMesg(OSMesgQueue *mq, OSMesg *msg, int flag);

n64psp_result n64psp_submit_task(const OSTask *task, n64psp_task_record *out_record);

#ifdef __cplusplus
}
#endif

#endif
