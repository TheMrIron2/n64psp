#include "n64psp/runtime.h"
#include <inttypes.h>
#include <stddef.h>
#include <string.h>

static n64psp_platform_callbacks g_platform;
static n64psp_renderer_callbacks g_renderer;
static int g_has_platform;
static int g_has_renderer;
static int g_initialized;

void n64psp__queues_reset(void);

static int platform_valid(const n64psp_platform_callbacks *cb) {
    return cb && cb->log && cb->fatal && cb->monotonic_us && cb->sleep_us && cb->sem_create && cb->sem_wait &&
           cb->sem_try_wait && cb->sem_post && cb->sem_destroy && cb->mutex_create && cb->mutex_lock &&
           cb->mutex_unlock && cb->mutex_destroy && cb->thread_create && cb->thread_join && cb->thread_destroy;
}

n64psp_result n64psp_runtime_register_platform(const n64psp_platform_callbacks *callbacks) {
    if (g_initialized) {
        return N64PSP_ERROR_INVALID_STATE;
    }
    if (!platform_valid(callbacks)) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    g_platform = *callbacks;
    g_has_platform = 1;
    return N64PSP_OK;
}

n64psp_result n64psp_runtime_register_renderer(const n64psp_renderer_callbacks *callbacks) {
    if (g_initialized) {
        return N64PSP_ERROR_INVALID_STATE;
    }
    if (!callbacks || !callbacks->submit_task) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    g_renderer = *callbacks;
    g_has_renderer = 1;
    return N64PSP_OK;
}

n64psp_result n64psp_runtime_init(void) {
    if (g_initialized) {
        return N64PSP_ERROR_ALREADY_INITIALIZED;
    }
    if (!g_has_platform || !g_has_renderer) {
        return N64PSP_ERROR_INVALID_STATE;
    }
    g_initialized = 1;
    n64psp_log("n64psp runtime initialized");
    return N64PSP_OK;
}

n64psp_result n64psp_runtime_shutdown(void) {
    if (!g_initialized) {
        return N64PSP_ERROR_NOT_INITIALIZED;
    }
    n64psp__queues_reset();
    g_initialized = 0;
    n64psp_log("n64psp runtime shut down");
    return N64PSP_OK;
}

int n64psp_runtime_is_initialized(void) {
    return g_initialized;
}

void n64psp_log(const char *message) {
    if (g_has_platform && g_platform.log) {
        g_platform.log(g_platform.userdata, message ? message : "(null)");
    }
}

void n64psp_fatal(const char *message) {
    if (g_has_platform && g_platform.fatal) {
        g_platform.fatal(g_platform.userdata, message ? message : "(null)");
    }
}

n64psp_time_us n64psp_time_monotonic_us(void) {
    if (!g_has_platform || !g_platform.monotonic_us) {
        return 0;
    }
    return g_platform.monotonic_us(g_platform.userdata);
}

n64psp_ticks n64psp_time_us_to_ticks(uint64_t us) {
    const uint64_t hz = N64PSP_TICKS_PER_SECOND;
    uint64_t whole = us / 1000000ULL;
    uint64_t rem_us = us % 1000000ULL;
    if (whole > UINT64_MAX / hz) {
        return UINT64_MAX;
    }
    uint64_t ticks = whole * hz;
    uint64_t rem_ticks = (rem_us * hz + 999999ULL) / 1000000ULL;
    if (ticks > UINT64_MAX - rem_ticks) {
        return UINT64_MAX;
    }
    return ticks + rem_ticks;
}

n64psp_ticks n64psp_time_ms_to_ticks(uint64_t ms) {
    if (ms > UINT64_MAX / 1000ULL) {
        return UINT64_MAX;
    }
    return n64psp_time_us_to_ticks(ms * 1000ULL);
}

uint64_t n64psp_time_ticks_to_us(n64psp_ticks ticks) {
    uint64_t whole = ticks / N64PSP_TICKS_PER_SECOND;
    uint64_t rem_ticks = ticks % N64PSP_TICKS_PER_SECOND;
    if (whole > UINT64_MAX / 1000000ULL) {
        return UINT64_MAX;
    }
    uint64_t us = whole * 1000000ULL;
    uint64_t rem_us = (rem_ticks * 1000000ULL) / N64PSP_TICKS_PER_SECOND;
    if (us > UINT64_MAX - rem_us) {
        return UINT64_MAX;
    }
    return us + rem_us;
}

int n64psp_time_after_eq(n64psp_ticks a, n64psp_ticks b) {
    return (int64_t)(a - b) >= 0;
}

n64psp_result n64psp_submit_task(const OSTask *task, n64psp_task_record *out_record) {
    n64psp_task_record local;
    if (!g_initialized || !g_has_renderer) {
        return N64PSP_ERROR_NOT_INITIALIZED;
    }
    if (!task || (!task->ucode && task->ucode_size != 0) || (!task->data_ptr && task->data_size != 0)) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    memset(&local, 0, sizeof(local));
    n64psp_result result = g_renderer.submit_task(g_renderer.userdata, task, &local);
    local.result = result;
    if (out_record) {
        *out_record = local;
    }
    return result;
}

const n64psp_platform_callbacks *n64psp__platform(void) {
    return g_has_platform ? &g_platform : NULL;
}
