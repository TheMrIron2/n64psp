#ifndef N64PSP_PLATFORM_H
#define N64PSP_PLATFORM_H

#include "n64psp/result.h"
#include "n64psp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct n64psp_platform_sem n64psp_platform_sem;
typedef struct n64psp_platform_mutex n64psp_platform_mutex;
typedef struct n64psp_platform_critical n64psp_platform_critical;
typedef struct n64psp_platform_thread n64psp_platform_thread;

typedef int (*n64psp_thread_entry)(void *userdata);

typedef struct n64psp_platform_callbacks {
    void (*log)(void *userdata, const char *message);
    void (*fatal)(void *userdata, const char *message);
    n64psp_time_us (*monotonic_us)(void *userdata);
    void (*sleep_us)(void *userdata, uint32_t us);

    n64psp_result (*sem_create)(void *userdata, uint32_t initial, uint32_t maximum, n64psp_platform_sem **out_sem);
    n64psp_result (*sem_wait)(void *userdata, n64psp_platform_sem *sem);
    n64psp_result (*sem_try_wait)(void *userdata, n64psp_platform_sem *sem);
    n64psp_result (*sem_post)(void *userdata, n64psp_platform_sem *sem);
    void (*sem_destroy)(void *userdata, n64psp_platform_sem *sem);

    n64psp_result (*mutex_create)(void *userdata, n64psp_platform_mutex **out_mutex);
    n64psp_result (*mutex_lock)(void *userdata, n64psp_platform_mutex *mutex);
    void (*mutex_unlock)(void *userdata, n64psp_platform_mutex *mutex);
    void (*mutex_destroy)(void *userdata, n64psp_platform_mutex *mutex);

    n64psp_result (*critical_create)(void *userdata, n64psp_platform_critical **out_critical);
    n64psp_result (*critical_enter)(void *userdata, n64psp_platform_critical *critical, uintptr_t *out_state);
    void (*critical_leave)(void *userdata, n64psp_platform_critical *critical, uintptr_t state);
    void (*critical_destroy)(void *userdata, n64psp_platform_critical *critical);

    n64psp_result (*thread_create)(void *userdata, const char *name, n64psp_thread_entry entry, void *thread_userdata,
                                   uint32_t stack_size, int priority, n64psp_platform_thread **out_thread);
    n64psp_result (*thread_join)(void *userdata, n64psp_platform_thread *thread, int *out_code);
    void (*thread_destroy)(void *userdata, n64psp_platform_thread *thread);

    n64psp_result (*pi_read)(void *userdata, uint32_t offset, void *dst, size_t size);
    void *userdata;
} n64psp_platform_callbacks;

n64psp_result n64psp_platform_host_get_callbacks(n64psp_platform_callbacks *out_callbacks);
n64psp_result n64psp_platform_psp_get_callbacks(n64psp_platform_callbacks *out_callbacks);

#ifdef __PSP__
typedef struct n64psp_platform_psp_diag {
    int sem_create_raw;
    int sem_wait_raw;
    int sem_signal_raw;
    int sem_delete_raw;
    int mutex_create_raw;
    int mutex_lock_raw;
    int mutex_unlock_raw;
    int mutex_delete_raw;
    int thread_create_raw;
    int thread_start_raw;
    int thread_wait_raw;
    int thread_delete_raw;
    void *parent_thread_object;
    void *parent_userdata;
    void *child_thread_object;
    void *child_userdata;
} n64psp_platform_psp_diag;

void n64psp_platform_psp_get_diag(n64psp_platform_psp_diag *out_diag);
void *n64psp_platform_psp_thread_object(n64psp_platform_thread *thread);
void *n64psp_platform_psp_sem_object(n64psp_platform_sem *sem);
#endif

#ifdef __cplusplus
}
#endif

#endif
