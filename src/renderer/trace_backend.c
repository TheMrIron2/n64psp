#include "n64psp/renderer.h"
#include <string.h>

#define N64PSP_TRACE_RECORDS 32

static n64psp_task_record g_records[N64PSP_TRACE_RECORDS];
static size_t g_record_count;

static n64psp_result trace_submit(void *userdata, const OSTask *task, n64psp_task_record *out_record) {
    (void)userdata;
    if (!task) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_task_record record;
    record.type = task->type;
    record.flags = task->flags;
    record.ucode_size = task->ucode_size;
    record.data_size = task->data_size;
    record.result = N64PSP_ERROR_UNSUPPORTED;
    if (g_record_count < N64PSP_TRACE_RECORDS) {
        g_records[g_record_count++] = record;
    }
    if (out_record) {
        *out_record = record;
    }
    return N64PSP_ERROR_UNSUPPORTED;
}

n64psp_result n64psp_trace_backend_get_callbacks(n64psp_renderer_callbacks *out_callbacks) {
    if (!out_callbacks) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    out_callbacks->submit_task = trace_submit;
    out_callbacks->userdata = NULL;
    return N64PSP_OK;
}

const n64psp_task_record *n64psp_trace_backend_records(size_t *out_count) {
    if (out_count) {
        *out_count = g_record_count;
    }
    return g_records;
}

void n64psp_trace_backend_reset(void) {
    memset(g_records, 0, sizeof(g_records));
    g_record_count = 0;
}
