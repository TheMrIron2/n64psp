#ifndef N64PSP_RENDERER_H
#define N64PSP_RENDERER_H

#include "n64psp/result.h"
#include "n64psp/types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct n64psp_task_record {
    uint32_t type;
    uint32_t flags;
    uint32_t ucode_size;
    uint32_t data_size;
    n64psp_result result;
} n64psp_task_record;

typedef struct n64psp_renderer_callbacks {
    n64psp_result (*submit_task)(void *userdata, const OSTask *task, n64psp_task_record *out_record);
    void *userdata;
} n64psp_renderer_callbacks;

n64psp_result n64psp_trace_backend_get_callbacks(n64psp_renderer_callbacks *out_callbacks);
const n64psp_task_record *n64psp_trace_backend_records(size_t *out_count);
void n64psp_trace_backend_reset(void);

#ifdef __cplusplus
}
#endif

#endif
