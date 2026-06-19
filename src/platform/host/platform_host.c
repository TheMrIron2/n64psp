#define _POSIX_C_SOURCE 200809L

#include "n64psp/platform.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct n64psp_platform_sem {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    uint32_t count;
    uint32_t max;
};

struct n64psp_platform_mutex {
    pthread_mutex_t mutex;
};

struct n64psp_platform_critical {
    pthread_mutex_t mutex;
};

struct n64psp_platform_thread {
    pthread_t thread;
    n64psp_thread_entry entry;
    void *userdata;
    int result;
};

static void host_log(void *userdata, const char *message) {
    (void)userdata;
    fprintf(stderr, "%s\n", message);
}

static void host_fatal(void *userdata, const char *message) {
    (void)userdata;
    fprintf(stderr, "fatal: %s\n", message);
}

static n64psp_time_us host_monotonic_us(void *userdata) {
    (void)userdata;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static void host_sleep_us(void *userdata, uint32_t us) {
    (void)userdata;
    struct timespec ts;
    ts.tv_sec = (time_t)(us / 1000000U);
    ts.tv_nsec = (long)(us % 1000000U) * 1000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
    }
}

static n64psp_result host_sem_create(void *userdata, uint32_t initial, uint32_t maximum,
                                     n64psp_platform_sem **out_sem) {
    (void)userdata;
    if (!out_sem || initial > maximum || maximum == 0) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_platform_sem *sem = (n64psp_platform_sem *)calloc(1, sizeof(*sem));
    if (!sem) {
        return N64PSP_ERROR_NO_MEMORY;
    }
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
    sem->count = initial;
    sem->max = maximum;
    *out_sem = sem;
    return N64PSP_OK;
}

static n64psp_result host_sem_wait(void *userdata, n64psp_platform_sem *sem) {
    (void)userdata;
    if (!sem) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    pthread_mutex_lock(&sem->mutex);
    while (sem->count == 0) {
        pthread_cond_wait(&sem->cond, &sem->mutex);
    }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
    return N64PSP_OK;
}

static n64psp_result host_sem_try_wait(void *userdata, n64psp_platform_sem *sem) {
    (void)userdata;
    if (!sem) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    pthread_mutex_lock(&sem->mutex);
    if (sem->count == 0) {
        pthread_mutex_unlock(&sem->mutex);
        return N64PSP_ERROR_TIMEOUT;
    }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
    return N64PSP_OK;
}

static n64psp_result host_sem_post(void *userdata, n64psp_platform_sem *sem) {
    (void)userdata;
    if (!sem) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    pthread_mutex_lock(&sem->mutex);
    if (sem->count >= sem->max) {
        pthread_mutex_unlock(&sem->mutex);
        return N64PSP_ERROR_INVALID_STATE;
    }
    sem->count++;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mutex);
    return N64PSP_OK;
}

static void host_sem_destroy(void *userdata, n64psp_platform_sem *sem) {
    (void)userdata;
    if (!sem) {
        return;
    }
    pthread_cond_destroy(&sem->cond);
    pthread_mutex_destroy(&sem->mutex);
    free(sem);
}

static n64psp_result host_mutex_create(void *userdata, n64psp_platform_mutex **out_mutex) {
    (void)userdata;
    if (!out_mutex) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_platform_mutex *mutex = (n64psp_platform_mutex *)calloc(1, sizeof(*mutex));
    if (!mutex) {
        return N64PSP_ERROR_NO_MEMORY;
    }
    pthread_mutex_init(&mutex->mutex, NULL);
    *out_mutex = mutex;
    return N64PSP_OK;
}

static n64psp_result host_mutex_lock(void *userdata, n64psp_platform_mutex *mutex) {
    (void)userdata;
    return mutex ? (pthread_mutex_lock(&mutex->mutex) == 0 ? N64PSP_OK : N64PSP_ERROR_PLATFORM)
                 : N64PSP_ERROR_INVALID_ARGUMENT;
}

static void host_mutex_unlock(void *userdata, n64psp_platform_mutex *mutex) {
    (void)userdata;
    if (mutex) {
        pthread_mutex_unlock(&mutex->mutex);
    }
}

static void host_mutex_destroy(void *userdata, n64psp_platform_mutex *mutex) {
    (void)userdata;
    if (mutex) {
        pthread_mutex_destroy(&mutex->mutex);
        free(mutex);
    }
}

static n64psp_result host_critical_create(void *userdata, n64psp_platform_critical **out_critical) {
    (void)userdata;
    if (!out_critical) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_platform_critical *critical = (n64psp_platform_critical *)calloc(1, sizeof(*critical));
    if (!critical) {
        return N64PSP_ERROR_NO_MEMORY;
    }
    pthread_mutex_init(&critical->mutex, NULL);
    *out_critical = critical;
    return N64PSP_OK;
}

static n64psp_result host_critical_enter(void *userdata, n64psp_platform_critical *critical, uintptr_t *out_state) {
    (void)userdata;
    if (!critical) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    if (out_state) {
        *out_state = 0;
    }
    return pthread_mutex_lock(&critical->mutex) == 0 ? N64PSP_OK : N64PSP_ERROR_PLATFORM;
}

static void host_critical_leave(void *userdata, n64psp_platform_critical *critical, uintptr_t state) {
    (void)userdata;
    (void)state;
    if (critical) {
        pthread_mutex_unlock(&critical->mutex);
    }
}

static void host_critical_destroy(void *userdata, n64psp_platform_critical *critical) {
    (void)userdata;
    if (critical) {
        pthread_mutex_destroy(&critical->mutex);
        free(critical);
    }
}

static void *host_thread_main(void *arg) {
    n64psp_platform_thread *thread = (n64psp_platform_thread *)arg;
    thread->result = thread->entry(thread->userdata);
    return NULL;
}

static n64psp_result host_thread_create(void *userdata, const char *name, n64psp_thread_entry entry,
                                        void *thread_userdata, uint32_t stack_size, int priority,
                                        n64psp_platform_thread **out_thread) {
    (void)userdata;
    (void)name;
    (void)stack_size;
    (void)priority;
    if (!entry || !out_thread) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_platform_thread *thread = (n64psp_platform_thread *)calloc(1, sizeof(*thread));
    if (!thread) {
        return N64PSP_ERROR_NO_MEMORY;
    }
    thread->entry = entry;
    thread->userdata = thread_userdata;
    if (pthread_create(&thread->thread, NULL, host_thread_main, thread) != 0) {
        free(thread);
        return N64PSP_ERROR_PLATFORM;
    }
    *out_thread = thread;
    return N64PSP_OK;
}

static n64psp_result host_thread_join(void *userdata, n64psp_platform_thread *thread, int *out_code) {
    (void)userdata;
    if (!thread) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    if (pthread_join(thread->thread, NULL) != 0) {
        return N64PSP_ERROR_PLATFORM;
    }
    if (out_code) {
        *out_code = thread->result;
    }
    return N64PSP_OK;
}

static void host_thread_destroy(void *userdata, n64psp_platform_thread *thread) {
    (void)userdata;
    free(thread);
}

n64psp_result n64psp_platform_host_get_callbacks(n64psp_platform_callbacks *out_callbacks) {
    if (!out_callbacks) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    memset(out_callbacks, 0, sizeof(*out_callbacks));
    out_callbacks->log = host_log;
    out_callbacks->fatal = host_fatal;
    out_callbacks->monotonic_us = host_monotonic_us;
    out_callbacks->sleep_us = host_sleep_us;
    out_callbacks->sem_create = host_sem_create;
    out_callbacks->sem_wait = host_sem_wait;
    out_callbacks->sem_try_wait = host_sem_try_wait;
    out_callbacks->sem_post = host_sem_post;
    out_callbacks->sem_destroy = host_sem_destroy;
    out_callbacks->mutex_create = host_mutex_create;
    out_callbacks->mutex_lock = host_mutex_lock;
    out_callbacks->mutex_unlock = host_mutex_unlock;
    out_callbacks->mutex_destroy = host_mutex_destroy;
    out_callbacks->critical_create = host_critical_create;
    out_callbacks->critical_enter = host_critical_enter;
    out_callbacks->critical_leave = host_critical_leave;
    out_callbacks->critical_destroy = host_critical_destroy;
    out_callbacks->thread_create = host_thread_create;
    out_callbacks->thread_join = host_thread_join;
    out_callbacks->thread_destroy = host_thread_destroy;
    return N64PSP_OK;
}
