#include "n64psp/runtime.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const n64psp_platform_callbacks *n64psp__platform(void);

#define N64PSP_MAX_QUEUES 32

typedef struct queue_sync {
    n64psp_platform_mutex *mutex;
    n64psp_platform_sem *items;
    n64psp_platform_sem *slots;
} queue_sync;

typedef struct queue_state {
    OSMesgQueue *queue;
    queue_sync sync;
    unsigned long create_seq;
    uint32_t active_ops;
    uint32_t blocked_waiters;
    int active;
    int initializing;
} queue_state;

typedef struct queue_ref {
    queue_state *state;
    queue_sync sync;
    int blocked;
} queue_ref;

static queue_state g_queues[N64PSP_MAX_QUEUES];
static n64psp_platform_mutex *g_registry_mutex;
static int g_registry_ready;
static int g_accepting_ops;
static int g_shutdown_started;
static unsigned long g_create_seq;

/*
 * Queue lifetime lock order:
 *
 * 1. The registry mutex protects queue metadata, active operation counts,
 *    blocked waiter counts, queue registration, reinitialisation, and reset.
 * 2. A queue operation takes an active reference under the registry mutex,
 *    then drops the registry mutex before waiting on any queue semaphore.
 * 3. Queue data mutation is protected only by the individual queue mutex.
 * 4. The registry mutex is never held while blocking on a queue semaphore or
 *    while holding a queue mutex.
 *
 * The active reference keeps the synchronization objects reachable while the
 * operation is outside the registry lock. Shutdown and reinitialisation only
 * destroy queue synchronization objects after proving active_ops and
 * blocked_waiters are both zero.
 */

static int valid_flag(int flag) {
    return flag == OS_MESG_BLOCK || flag == OS_MESG_NOBLOCK;
}

static void fatal_invariant(const char *message) {
    n64psp_fatal(message);
}

static n64psp_result registry_lock(const n64psp_platform_callbacks *platform) {
    if (!platform || !g_registry_ready || !g_registry_mutex) {
        return N64PSP_ERROR_NOT_INITIALIZED;
    }
    return platform->mutex_lock(platform->userdata, g_registry_mutex);
}

static void registry_unlock(const n64psp_platform_callbacks *platform) {
    platform->mutex_unlock(platform->userdata, g_registry_mutex);
}

static void destroy_sync(const n64psp_platform_callbacks *platform, queue_sync *sync) {
    if (sync->items) {
        platform->sem_destroy(platform->userdata, sync->items);
    }
    if (sync->slots) {
        platform->sem_destroy(platform->userdata, sync->slots);
    }
    if (sync->mutex) {
        platform->mutex_destroy(platform->userdata, sync->mutex);
    }
    memset(sync, 0, sizeof(*sync));
}

static n64psp_result create_sync(const n64psp_platform_callbacks *platform, int count, queue_sync *out_sync) {
    queue_sync sync;
    n64psp_result result;

    memset(&sync, 0, sizeof(sync));
    result = platform->mutex_create(platform->userdata, &sync.mutex);
    if (result != N64PSP_OK) {
        return result;
    }
    result = platform->sem_create(platform->userdata, 0, (uint32_t)count, &sync.items);
    if (result != N64PSP_OK) {
        destroy_sync(platform, &sync);
        return result;
    }
    result = platform->sem_create(platform->userdata, (uint32_t)count, (uint32_t)count, &sync.slots);
    if (result != N64PSP_OK) {
        destroy_sync(platform, &sync);
        return result;
    }
    *out_sync = sync;
    return N64PSP_OK;
}

static queue_state *find_state_locked(OSMesgQueue *mq) {
    size_t i;

    for (i = 0; i < N64PSP_MAX_QUEUES; i++) {
        if ((g_queues[i].active || g_queues[i].initializing) && g_queues[i].queue == mq) {
            return &g_queues[i];
        }
    }
    return NULL;
}

static queue_state *alloc_state_locked(OSMesgQueue *mq) {
    queue_state *free_slot = NULL;
    size_t i;

    for (i = 0; i < N64PSP_MAX_QUEUES; i++) {
        if ((g_queues[i].active || g_queues[i].initializing) && g_queues[i].queue == mq) {
            return &g_queues[i];
        }
        if (!g_queues[i].active && !g_queues[i].initializing && !free_slot) {
            free_slot = &g_queues[i];
        }
    }
    return free_slot;
}

static int state_quiescent(const queue_state *state) {
    return state->active_ops == 0 && state->blocked_waiters == 0;
}

static void log_create_rejected(const char *reason, OSMesgQueue *mq) {
    char message[160];

    snprintf(message, sizeof(message), "n64psp queue create rejected: %s queue=%p", reason, (void *)mq);
    n64psp_log(message);
}

static n64psp_result begin_queue_op(OSMesgQueue *mq, int flag, int waits, queue_ref *out_ref) {
    const n64psp_platform_callbacks *platform = n64psp__platform();
    queue_state *state;
    n64psp_result result;

    memset(out_ref, 0, sizeof(*out_ref));
    if (!mq || !valid_flag(flag)) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    result = registry_lock(platform);
    if (result != N64PSP_OK) {
        return result;
    }
    if (!g_accepting_ops || g_shutdown_started) {
        registry_unlock(platform);
        return N64PSP_ERROR_NOT_INITIALIZED;
    }
    state = find_state_locked(mq);
    if (!state || !state->active || state->initializing || !mq->msg || mq->msgCount <= 0) {
        registry_unlock(platform);
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    state->active_ops++;
    if (waits && flag == OS_MESG_BLOCK) {
        state->blocked_waiters++;
        out_ref->blocked = 1;
    }
    out_ref->state = state;
    out_ref->sync = state->sync;
    registry_unlock(platform);
    return N64PSP_OK;
}

static void finish_queue_op(queue_ref *ref) {
    const n64psp_platform_callbacks *platform = n64psp__platform();

    if (!ref->state || registry_lock(platform) != N64PSP_OK) {
        fatal_invariant("n64psp queue invariant: failed to finish queue operation");
        return;
    }
    if (ref->blocked) {
        if (ref->state->blocked_waiters == 0) {
            fatal_invariant("n64psp queue invariant: blocked waiter underflow");
        } else {
            ref->state->blocked_waiters--;
        }
    }
    if (ref->state->active_ops == 0) {
        fatal_invariant("n64psp queue invariant: active operation underflow");
    } else {
        ref->state->active_ops--;
    }
    registry_unlock(platform);
    memset(ref, 0, sizeof(*ref));
}

n64psp_result n64psp__queues_init(void) {
    const n64psp_platform_callbacks *platform = n64psp__platform();
    n64psp_result result;

    if (!platform) {
        return N64PSP_ERROR_NOT_INITIALIZED;
    }
    if (!g_registry_ready) {
        result = platform->mutex_create(platform->userdata, &g_registry_mutex);
        if (result != N64PSP_OK) {
            return result;
        }
        g_registry_ready = 1;
    }
    result = registry_lock(platform);
    if (result != N64PSP_OK) {
        return result;
    }
    memset(g_queues, 0, sizeof(g_queues));
    g_accepting_ops = 1;
    g_shutdown_started = 0;
    registry_unlock(platform);
    return N64PSP_OK;
}

n64psp_result n64psp__queues_shutdown(void) {
    const n64psp_platform_callbacks *platform = n64psp__platform();
    queue_sync to_destroy[N64PSP_MAX_QUEUES];
    size_t destroy_count = 0;
    size_t i;
    n64psp_result result;

    memset(to_destroy, 0, sizeof(to_destroy));
    result = registry_lock(platform);
    if (result != N64PSP_OK) {
        return result;
    }
    g_shutdown_started = 1;
    for (i = 0; i < N64PSP_MAX_QUEUES; i++) {
        if (g_queues[i].active && !state_quiescent(&g_queues[i])) {
            g_shutdown_started = 0;
            registry_unlock(platform);
            return N64PSP_ERROR_BUSY;
        }
    }
    g_accepting_ops = 0;
    for (i = 0; i < N64PSP_MAX_QUEUES; i++) {
        if (g_queues[i].active) {
            to_destroy[destroy_count++] = g_queues[i].sync;
        }
    }
    memset(g_queues, 0, sizeof(g_queues));
    g_shutdown_started = 0;
    registry_unlock(platform);

    for (i = 0; i < destroy_count; i++) {
        destroy_sync(platform, &to_destroy[i]);
    }
    return N64PSP_OK;
}

static n64psp_result create_queue_internal(OSMesgQueue *mq, OSMesg *msg, int count) {
    const n64psp_platform_callbacks *platform = n64psp__platform();
    queue_state *state;
    queue_sync new_sync;
    queue_sync old_sync;
    int was_active;
    n64psp_result result;

    memset(&new_sync, 0, sizeof(new_sync));
    memset(&old_sync, 0, sizeof(old_sync));
    if (!platform || !mq || !msg || count <= 0) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }

    result = registry_lock(platform);
    if (result != N64PSP_OK) {
        return result;
    }
    if (!g_accepting_ops || g_shutdown_started) {
        registry_unlock(platform);
        return N64PSP_ERROR_NOT_INITIALIZED;
    }
    state = alloc_state_locked(mq);
    if (!state) {
        registry_unlock(platform);
        log_create_rejected("registry exhausted", mq);
        return N64PSP_ERROR_NO_MEMORY;
    }
    if (state->active && !state_quiescent(state)) {
        registry_unlock(platform);
        log_create_rejected("queue is busy", mq);
        return N64PSP_ERROR_BUSY;
    }
    was_active = state->active;
    state->queue = mq;
    state->initializing = 1;
    registry_unlock(platform);

    result = create_sync(platform, count, &new_sync);
    if (result != N64PSP_OK) {
        if (registry_lock(platform) == N64PSP_OK) {
            state->initializing = 0;
            if (!was_active) {
                memset(state, 0, sizeof(*state));
            }
            registry_unlock(platform);
        } else {
            fatal_invariant("n64psp queue invariant: failed to recover after queue create failure");
        }
        return result;
    }

    result = registry_lock(platform);
    if (result != N64PSP_OK) {
        destroy_sync(platform, &new_sync);
        fatal_invariant("n64psp queue invariant: failed to publish created queue");
        return result;
    }
    old_sync = state->sync;
    mq->mtqueue = NULL;
    mq->fullqueue = NULL;
    mq->msg = msg;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    state->sync = new_sync;
    state->create_seq = ++g_create_seq;
    state->active_ops = 0;
    state->blocked_waiters = 0;
    state->active = 1;
    state->initializing = 0;
    registry_unlock(platform);

    if (was_active) {
        destroy_sync(platform, &old_sync);
    }
    return N64PSP_OK;
}

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, int count) {
    n64psp_result result = create_queue_internal(mq, msg, count);

    if (result != N64PSP_OK) {
        char message[160];

        snprintf(message, sizeof(message), "n64psp osCreateMesgQueue failed: %s queue=%p count=%d",
                 n64psp_result_name(result), (void *)mq, count);
        n64psp_log(message);
    }
}

static int send_common(OSMesgQueue *mq, OSMesg msg, int flag, int jam) {
    const n64psp_platform_callbacks *platform = n64psp__platform();
    queue_ref ref;
    n64psp_result result;
    int old_first = 0;
    int index = 0;

    result = begin_queue_op(mq, flag, 1, &ref);
    if (result != N64PSP_OK) {
        return -1;
    }
    result = flag == OS_MESG_BLOCK ? platform->sem_wait(platform->userdata, ref.sync.slots)
                                   : platform->sem_try_wait(platform->userdata, ref.sync.slots);
    if (result != N64PSP_OK) {
        finish_queue_op(&ref);
        return -1;
    }
    result = platform->mutex_lock(platform->userdata, ref.sync.mutex);
    if (result != N64PSP_OK) {
        if (platform->sem_post(platform->userdata, ref.sync.slots) != N64PSP_OK) {
            fatal_invariant("n64psp queue invariant: failed to restore reserved slot");
        }
        finish_queue_op(&ref);
        return -1;
    }

    old_first = mq->first;
    if (jam) {
        mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
        mq->msg[mq->first] = msg;
    } else {
        index = (mq->first + mq->validCount) % mq->msgCount;
        mq->msg[index] = msg;
    }
    mq->validCount++;

    result = platform->sem_post(platform->userdata, ref.sync.items);
    if (result != N64PSP_OK) {
        mq->validCount--;
        if (jam) {
            mq->first = old_first;
        }
        if (platform->sem_post(platform->userdata, ref.sync.slots) != N64PSP_OK) {
            fatal_invariant("n64psp queue invariant: failed to restore slot after send rollback");
        }
        platform->mutex_unlock(platform->userdata, ref.sync.mutex);
        finish_queue_op(&ref);
        return -1;
    }
    platform->mutex_unlock(platform->userdata, ref.sync.mutex);
    finish_queue_op(&ref);
    return 0;
}

int osSendMesg(OSMesgQueue *mq, OSMesg msg, int flag) {
    return send_common(mq, msg, flag, 0);
}

int osJamMesg(OSMesgQueue *mq, OSMesg msg, int flag) {
    return send_common(mq, msg, flag, 1);
}

int osRecvMesg(OSMesgQueue *mq, OSMesg *msg, int flag) {
    const n64psp_platform_callbacks *platform = n64psp__platform();
    queue_ref ref;
    n64psp_result result;
    OSMesg value;
    int old_first;

    result = begin_queue_op(mq, flag, 1, &ref);
    if (result != N64PSP_OK) {
        return -1;
    }
    result = flag == OS_MESG_BLOCK ? platform->sem_wait(platform->userdata, ref.sync.items)
                                   : platform->sem_try_wait(platform->userdata, ref.sync.items);
    if (result != N64PSP_OK) {
        finish_queue_op(&ref);
        return -1;
    }
    result = platform->mutex_lock(platform->userdata, ref.sync.mutex);
    if (result != N64PSP_OK) {
        if (platform->sem_post(platform->userdata, ref.sync.items) != N64PSP_OK) {
            fatal_invariant("n64psp queue invariant: failed to restore reserved item");
        }
        finish_queue_op(&ref);
        return -1;
    }

    old_first = mq->first;
    value = mq->msg[mq->first];
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;

    result = platform->sem_post(platform->userdata, ref.sync.slots);
    if (result != N64PSP_OK) {
        mq->first = old_first;
        mq->validCount++;
        if (platform->sem_post(platform->userdata, ref.sync.items) != N64PSP_OK) {
            fatal_invariant("n64psp queue invariant: failed to restore item after receive rollback");
        }
        platform->mutex_unlock(platform->userdata, ref.sync.mutex);
        finish_queue_op(&ref);
        return -1;
    }
    if (msg) {
        *msg = value;
    }
    platform->mutex_unlock(platform->userdata, ref.sync.mutex);
    finish_queue_op(&ref);
    return 0;
}
