#include "n64psp/platform.h"

#ifdef __PSP__
#include <pspdebug.h>
#include <pspkernel.h>
#include <psprtc.h>
#include <pspthreadman.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct n64psp_platform_sem {
    SceUID id;
};

struct n64psp_platform_mutex {
    SceUID id;
};

struct n64psp_platform_thread {
    SceUID id;
    n64psp_thread_entry entry;
    void *userdata;
    struct n64psp_platform_thread *start_arg;
    int result;
};

static n64psp_platform_psp_diag g_psp_diag;

static void psp_diag_reset_thread(n64psp_platform_thread *thread, void *userdata) {
    g_psp_diag.thread_create_raw = 0;
    g_psp_diag.thread_start_raw = 0;
    g_psp_diag.thread_wait_raw = 0;
    g_psp_diag.thread_delete_raw = 0;
    g_psp_diag.parent_thread_object = thread;
    g_psp_diag.parent_userdata = userdata;
    g_psp_diag.child_thread_object = NULL;
    g_psp_diag.child_userdata = NULL;
}

static void psp_log(void *userdata, const char *message) {
    (void)userdata;
    pspDebugScreenPrintf("%s\n", message);
    printf("%s\n", message);
}

static void psp_fatal(void *userdata, const char *message) {
    (void)userdata;
    pspDebugScreenPrintf("fatal: %s\n", message);
    printf("fatal: %s\n", message);
}

static n64psp_time_us psp_monotonic_us(void *userdata) {
    (void)userdata;
    u64 ticks = 0;
    u32 resolution = sceRtcGetTickResolution();
    if (sceRtcGetCurrentTick(&ticks) < 0) {
        return 0;
    }
    if (resolution == 0) {
        return 0;
    }
    return (n64psp_time_us)((ticks / resolution) * 1000000ULL + ((ticks % resolution) * 1000000ULL) / resolution);
}

static void psp_sleep_us(void *userdata, uint32_t us) {
    (void)userdata;
    sceKernelDelayThread((SceUInt)us);
}

static n64psp_result psp_sem_create(void *userdata, uint32_t initial, uint32_t maximum, n64psp_platform_sem **out_sem) {
    (void)userdata;
    if (!out_sem || initial > maximum || maximum == 0) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_platform_sem *sem = (n64psp_platform_sem *)malloc(sizeof(*sem));
    if (!sem) {
        return N64PSP_ERROR_NO_MEMORY;
    }
    sem->id = sceKernelCreateSema("n64psp-sem", 0, (int)initial, (int)maximum, NULL);
    g_psp_diag.sem_create_raw = sem->id;
    if (sem->id < 0) {
        free(sem);
        return N64PSP_ERROR_PLATFORM;
    }
    *out_sem = sem;
    return N64PSP_OK;
}

static n64psp_result psp_sem_wait(void *userdata, n64psp_platform_sem *sem) {
    (void)userdata;
    if (!sem) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    g_psp_diag.sem_wait_raw = sceKernelWaitSema(sem->id, 1, NULL);
    return g_psp_diag.sem_wait_raw < 0 ? N64PSP_ERROR_PLATFORM : N64PSP_OK;
}

static n64psp_result psp_sem_try_wait(void *userdata, n64psp_platform_sem *sem) {
    (void)userdata;
    if (!sem) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    g_psp_diag.sem_wait_raw = sceKernelPollSema(sem->id, 1);
    return g_psp_diag.sem_wait_raw < 0 ? N64PSP_ERROR_TIMEOUT : N64PSP_OK;
}

static n64psp_result psp_sem_post(void *userdata, n64psp_platform_sem *sem) {
    (void)userdata;
    if (!sem) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    g_psp_diag.sem_signal_raw = sceKernelSignalSema(sem->id, 1);
    return g_psp_diag.sem_signal_raw < 0 ? N64PSP_ERROR_PLATFORM : N64PSP_OK;
}

static void psp_sem_destroy(void *userdata, n64psp_platform_sem *sem) {
    (void)userdata;
    if (sem) {
        g_psp_diag.sem_delete_raw = sceKernelDeleteSema(sem->id);
        free(sem);
    }
}

static n64psp_result psp_mutex_create(void *userdata, n64psp_platform_mutex **out_mutex) {
    (void)userdata;
    if (!out_mutex) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_platform_mutex *mutex = (n64psp_platform_mutex *)malloc(sizeof(*mutex));
    if (!mutex) {
        return N64PSP_ERROR_NO_MEMORY;
    }
    mutex->id = sceKernelCreateSema("n64psp-mutex", 0, 1, 1, NULL);
    g_psp_diag.mutex_create_raw = mutex->id;
    if (mutex->id < 0) {
        free(mutex);
        return N64PSP_ERROR_PLATFORM;
    }
    *out_mutex = mutex;
    return N64PSP_OK;
}

static n64psp_result psp_mutex_lock(void *userdata, n64psp_platform_mutex *mutex) {
    (void)userdata;
    if (!mutex) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    g_psp_diag.mutex_lock_raw = sceKernelWaitSema(mutex->id, 1, NULL);
    return g_psp_diag.mutex_lock_raw < 0 ? N64PSP_ERROR_PLATFORM : N64PSP_OK;
}

static void psp_mutex_unlock(void *userdata, n64psp_platform_mutex *mutex) {
    (void)userdata;
    if (mutex) {
        g_psp_diag.mutex_unlock_raw = sceKernelSignalSema(mutex->id, 1);
    }
}

static void psp_mutex_destroy(void *userdata, n64psp_platform_mutex *mutex) {
    (void)userdata;
    if (mutex) {
        g_psp_diag.mutex_delete_raw = sceKernelDeleteSema(mutex->id);
        free(mutex);
    }
}

static int psp_thread_main(SceSize args, void *argp) {
    if (args != sizeof(n64psp_platform_thread *) || !argp) {
        sceKernelExitThread((int)N64PSP_ERROR_INVALID_ARGUMENT);
        return (int)N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_platform_thread *thread = *(n64psp_platform_thread **)argp;
    g_psp_diag.child_thread_object = thread;
    g_psp_diag.child_userdata = thread ? thread->userdata : NULL;
    thread->result = thread->entry(thread->userdata);
    sceKernelExitThread(thread->result);
    return thread->result;
}

static n64psp_result psp_thread_create(void *userdata, const char *name, n64psp_thread_entry entry,
                                       void *thread_userdata, uint32_t stack_size, int priority,
                                       n64psp_platform_thread **out_thread) {
    (void)userdata;
    if (!entry || !out_thread) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_platform_thread *thread = (n64psp_platform_thread *)malloc(sizeof(*thread));
    if (!thread) {
        return N64PSP_ERROR_NO_MEMORY;
    }
    thread->entry = entry;
    thread->userdata = thread_userdata;
    thread->start_arg = thread;
    thread->result = 0;
    psp_diag_reset_thread(thread, thread_userdata);
    thread->id = sceKernelCreateThread(name ? name : "n64psp-thread", psp_thread_main, priority,
                                       stack_size ? (int)stack_size : 0x4000, 0, NULL);
    g_psp_diag.thread_create_raw = thread->id;
    if (thread->id < 0) {
        free(thread);
        return N64PSP_ERROR_PLATFORM;
    }
    g_psp_diag.thread_start_raw = sceKernelStartThread(thread->id, sizeof(thread->start_arg), &thread->start_arg);
    if (g_psp_diag.thread_start_raw < 0) {
        sceKernelDeleteThread(thread->id);
        free(thread);
        return N64PSP_ERROR_PLATFORM;
    }
    *out_thread = thread;
    return N64PSP_OK;
}

static n64psp_result psp_thread_join(void *userdata, n64psp_platform_thread *thread, int *out_code) {
    (void)userdata;
    if (!thread) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    g_psp_diag.thread_wait_raw = sceKernelWaitThreadEnd(thread->id, NULL);
    if (g_psp_diag.thread_wait_raw < 0) {
        return N64PSP_ERROR_PLATFORM;
    }
    if (out_code) {
        *out_code = thread->result;
    }
    return N64PSP_OK;
}

static void psp_thread_destroy(void *userdata, n64psp_platform_thread *thread) {
    (void)userdata;
    if (thread) {
        g_psp_diag.thread_delete_raw = sceKernelDeleteThread(thread->id);
        free(thread);
    }
}

void n64psp_platform_psp_get_diag(n64psp_platform_psp_diag *out_diag) {
    if (out_diag) {
        *out_diag = g_psp_diag;
    }
}

void *n64psp_platform_psp_thread_object(n64psp_platform_thread *thread) {
    return thread;
}

void *n64psp_platform_psp_sem_object(n64psp_platform_sem *sem) {
    return sem;
}

n64psp_result n64psp_platform_psp_get_callbacks(n64psp_platform_callbacks *out_callbacks) {
    if (!out_callbacks) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    memset(out_callbacks, 0, sizeof(*out_callbacks));
    out_callbacks->log = psp_log;
    out_callbacks->fatal = psp_fatal;
    out_callbacks->monotonic_us = psp_monotonic_us;
    out_callbacks->sleep_us = psp_sleep_us;
    out_callbacks->sem_create = psp_sem_create;
    out_callbacks->sem_wait = psp_sem_wait;
    out_callbacks->sem_try_wait = psp_sem_try_wait;
    out_callbacks->sem_post = psp_sem_post;
    out_callbacks->sem_destroy = psp_sem_destroy;
    out_callbacks->mutex_create = psp_mutex_create;
    out_callbacks->mutex_lock = psp_mutex_lock;
    out_callbacks->mutex_unlock = psp_mutex_unlock;
    out_callbacks->mutex_destroy = psp_mutex_destroy;
    out_callbacks->thread_create = psp_thread_create;
    out_callbacks->thread_join = psp_thread_join;
    out_callbacks->thread_destroy = psp_thread_destroy;
    return N64PSP_OK;
}
#else
n64psp_result n64psp_platform_psp_get_callbacks(n64psp_platform_callbacks *out_callbacks) {
    (void)out_callbacks;
    return N64PSP_ERROR_UNSUPPORTED;
}
#endif
