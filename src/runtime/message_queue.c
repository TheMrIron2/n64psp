#include "n64psp/runtime.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

const n64psp_platform_callbacks *n64psp__platform(void);

#define N64PSP_MAX_QUEUES 32
#define N64PSP_WAIT_SEM_MAX 0x7fffffffU

#if defined(__GNUC__) || defined(__clang__)
#define N64PSP_ATOMIC_LOAD(ptr) __atomic_load_n((ptr), __ATOMIC_ACQUIRE)
#define N64PSP_ATOMIC_STORE(ptr, value) __atomic_store_n((ptr), (value), __ATOMIC_RELEASE)
#define N64PSP_ATOMIC_ADD(ptr, value) __atomic_add_fetch((ptr), (value), __ATOMIC_ACQ_REL)
#define N64PSP_ATOMIC_SUB(ptr, value) __atomic_sub_fetch((ptr), (value), __ATOMIC_ACQ_REL)
#else
#error "n64psp queue atomics require GCC/Clang __atomic builtins"
#endif

typedef struct queue_sync {
    n64psp_platform_critical *critical;
    n64psp_platform_sem *senders;
    n64psp_platform_sem *receivers;
} queue_sync;

typedef struct queue_state {
    OSMesgQueue *queue;
    queue_sync sync;
    unsigned long create_seq;
    uint32_t generation;
    uint32_t active_refs;
    uint32_t blocked_waiters;
    int sender_waiters;
    int receiver_waiters;
    int published;
    int accepting;
    int initializing;
} queue_state;

typedef struct queue_ref {
    queue_state *state;
    queue_sync sync;
    uint32_t generation;
} queue_ref;

typedef struct queue_counter_state {
    /*
     * PSP diagnostics use 32-bit atomic counters to avoid requiring 64-bit
     * atomic helper routines in the hot path. Public reads widen these values
     * to the uint64_t API shape; very long diagnostic runs may wrap.
     */
    uint32_t send_calls;
    uint32_t jam_calls;
    uint32_t recv_calls;
    uint32_t uncontended_successes;
    uint32_t sender_blocks;
    uint32_t receiver_blocks;
    uint32_t sender_wake_signals;
    uint32_t receiver_wake_signals;
    uint32_t retry_wakeups;
    uint32_t spurious_wakeups;
    uint32_t failed_nonblocking;
    uint32_t max_sender_waiters;
    uint32_t max_receiver_waiters;
} queue_counter_state;

static queue_state g_queues[N64PSP_MAX_QUEUES];
static n64psp_platform_mutex *g_registry_mutex;
static int g_registry_ready;
static uint32_t g_accepting_ops;
static unsigned long g_create_seq;
static queue_counter_state g_counters;

/*
 * Queue publication and lifetime invariants:
 *
 * - The registry mutex is used only to allocate/publish queue states, close
 *   admission for shutdown or reinitialisation, and destroy synchronization
 *   objects. It is not taken by ordinary successful send/jam/receive calls.
 * - Hot-path admission loads the global accepting flag, finds a published
 *   state, increments active_refs atomically, then revalidates global/state
 *   admission plus the state generation. If anything changed, the reference is
 *   dropped and the operation fails through the libultra-compatible `-1` path.
 * - Reinitialisation sets state accepting to zero before checking active_refs.
 *   Shutdown clears global admission before checking active_refs. A racing
 *   operation either owns a counted reference that makes the lifecycle action
 *   return BUSY, or it observes closed admission/generation mismatch and backs
 *   out before touching queue synchronization objects.
 * - Queue ring contents and waiter counts are protected by the per-queue
 *   critical section. The PSP backend implements that section with interrupt
 *   suspend/resume and it must never contain blocking calls, logging, heap
 *   allocation, or semaphore signal/wait operations.
 * - Sender/receiver semaphores are notification objects only. A waiter is
 *   counted under the queue critical section before it enters the kernel wait;
 *   a producer/consumer consumes exactly one waiter count before signaling.
 *   This permits signal-before-wait without accumulating stale wake tokens.
 */

static int valid_flag(int flag) {
    return flag == OS_MESG_BLOCK || flag == OS_MESG_NOBLOCK;
}

static void fatal_invariant(const char *message) {
    n64psp_fatal(message);
}

static void counter_inc(uint32_t *counter) {
#ifdef N64PSP_QUEUE_COUNTERS
    (void)N64PSP_ATOMIC_ADD(counter, 1);
#else
    (void)counter;
#endif
}

static void counter_max_u32(uint32_t *counter, uint32_t value) {
#ifdef N64PSP_QUEUE_COUNTERS
    uint32_t old_value = N64PSP_ATOMIC_LOAD(counter);

    while (value > old_value &&
           !__atomic_compare_exchange_n(counter, &old_value, value, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
    }
#else
    (void)counter;
    (void)value;
#endif
}

void n64psp_queue_reset_counters(void) {
#ifdef N64PSP_QUEUE_COUNTERS
    memset(&g_counters, 0, sizeof(g_counters));
#endif
}

void n64psp_queue_get_counters(n64psp_queue_counters *out_counters) {
    if (!out_counters) {
        return;
    }
#ifdef N64PSP_QUEUE_COUNTERS
    out_counters->send_calls = N64PSP_ATOMIC_LOAD(&g_counters.send_calls);
    out_counters->jam_calls = N64PSP_ATOMIC_LOAD(&g_counters.jam_calls);
    out_counters->recv_calls = N64PSP_ATOMIC_LOAD(&g_counters.recv_calls);
    out_counters->uncontended_successes = N64PSP_ATOMIC_LOAD(&g_counters.uncontended_successes);
    out_counters->sender_blocks = N64PSP_ATOMIC_LOAD(&g_counters.sender_blocks);
    out_counters->receiver_blocks = N64PSP_ATOMIC_LOAD(&g_counters.receiver_blocks);
    out_counters->sender_wake_signals = N64PSP_ATOMIC_LOAD(&g_counters.sender_wake_signals);
    out_counters->receiver_wake_signals = N64PSP_ATOMIC_LOAD(&g_counters.receiver_wake_signals);
    out_counters->retry_wakeups = N64PSP_ATOMIC_LOAD(&g_counters.retry_wakeups);
    out_counters->spurious_wakeups = N64PSP_ATOMIC_LOAD(&g_counters.spurious_wakeups);
    out_counters->failed_nonblocking = N64PSP_ATOMIC_LOAD(&g_counters.failed_nonblocking);
    out_counters->max_sender_waiters = N64PSP_ATOMIC_LOAD(&g_counters.max_sender_waiters);
    out_counters->max_receiver_waiters = N64PSP_ATOMIC_LOAD(&g_counters.max_receiver_waiters);
#else
    memset(out_counters, 0, sizeof(*out_counters));
#endif
}

void n64psp_queue_dump_counters(const char *label) {
    n64psp_queue_counters c;
    char line[384];

    n64psp_queue_get_counters(&c);
    snprintf(line, sizeof(line),
             "n64psp queue counters %s send=%llu recv=%llu jam=%llu uncontended=%llu sender_blocks=%llu "
             "receiver_blocks=%llu sender_wakes=%llu receiver_wakes=%llu retries=%llu spurious=%llu "
             "failed_nonblock=%llu max_sender_waiters=%u max_receiver_waiters=%u",
             label ? label : "", (unsigned long long)c.send_calls, (unsigned long long)c.recv_calls,
             (unsigned long long)c.jam_calls, (unsigned long long)c.uncontended_successes,
             (unsigned long long)c.sender_blocks, (unsigned long long)c.receiver_blocks,
             (unsigned long long)c.sender_wake_signals, (unsigned long long)c.receiver_wake_signals,
             (unsigned long long)c.retry_wakeups, (unsigned long long)c.spurious_wakeups,
             (unsigned long long)c.failed_nonblocking, (unsigned)c.max_sender_waiters,
             (unsigned)c.max_receiver_waiters);
    n64psp_log(line);
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

static n64psp_result queue_enter(const n64psp_platform_callbacks *platform, const queue_sync *sync,
                                 uintptr_t *out_state) {
    return platform->critical_enter(platform->userdata, sync->critical, out_state);
}

static void queue_leave(const n64psp_platform_callbacks *platform, const queue_sync *sync, uintptr_t state) {
    platform->critical_leave(platform->userdata, sync->critical, state);
}

static void destroy_sync(const n64psp_platform_callbacks *platform, queue_sync *sync) {
    if (sync->senders) {
        platform->sem_destroy(platform->userdata, sync->senders);
    }
    if (sync->receivers) {
        platform->sem_destroy(platform->userdata, sync->receivers);
    }
    if (sync->critical) {
        platform->critical_destroy(platform->userdata, sync->critical);
    }
    memset(sync, 0, sizeof(*sync));
}

static n64psp_result create_sync(const n64psp_platform_callbacks *platform, queue_sync *out_sync) {
    queue_sync sync;
    n64psp_result result;

    memset(&sync, 0, sizeof(sync));
    result = platform->critical_create(platform->userdata, &sync.critical);
    if (result != N64PSP_OK) {
        return result;
    }
    result = platform->sem_create(platform->userdata, 0, N64PSP_WAIT_SEM_MAX, &sync.senders);
    if (result != N64PSP_OK) {
        destroy_sync(platform, &sync);
        return result;
    }
    result = platform->sem_create(platform->userdata, 0, N64PSP_WAIT_SEM_MAX, &sync.receivers);
    if (result != N64PSP_OK) {
        destroy_sync(platform, &sync);
        return result;
    }
    *out_sync = sync;
    return N64PSP_OK;
}

static queue_state *find_state_hot(OSMesgQueue *mq) {
    size_t i;

    for (i = 0; i < N64PSP_MAX_QUEUES; i++) {
        if (N64PSP_ATOMIC_LOAD(&g_queues[i].published) && N64PSP_ATOMIC_LOAD(&g_queues[i].queue) == mq) {
            return &g_queues[i];
        }
    }
    return NULL;
}

static queue_state *alloc_state_locked(OSMesgQueue *mq) {
    queue_state *free_slot = NULL;
    size_t i;

    for (i = 0; i < N64PSP_MAX_QUEUES; i++) {
        if ((g_queues[i].published || g_queues[i].initializing) && g_queues[i].queue == mq) {
            return &g_queues[i];
        }
        if (!g_queues[i].published && !g_queues[i].initializing && !free_slot) {
            free_slot = &g_queues[i];
        }
    }
    return free_slot;
}

static uint32_t state_refs(const queue_state *state) {
    return N64PSP_ATOMIC_LOAD(&state->active_refs);
}

static int state_quiescent(const queue_state *state) {
    return state_refs(state) == 0 && N64PSP_ATOMIC_LOAD(&state->blocked_waiters) == 0;
}

static void log_create_rejected(const char *reason, OSMesgQueue *mq) {
    char message[160];

    snprintf(message, sizeof(message), "n64psp queue create rejected: %s queue=%p", reason, (void *)mq);
    n64psp_log(message);
}

static n64psp_result begin_queue_op(OSMesgQueue *mq, int flag, queue_ref *out_ref) {
    queue_state *state;
    uint32_t generation;

    memset(out_ref, 0, sizeof(*out_ref));
    if (!mq || !valid_flag(flag) || !N64PSP_ATOMIC_LOAD(&g_accepting_ops)) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }

    state = find_state_hot(mq);
    if (!state) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    generation = N64PSP_ATOMIC_LOAD(&state->generation);
    if (!N64PSP_ATOMIC_LOAD(&state->accepting)) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }

    (void)N64PSP_ATOMIC_ADD(&state->active_refs, 1);
    if (!N64PSP_ATOMIC_LOAD(&g_accepting_ops) || !N64PSP_ATOMIC_LOAD(&state->published) ||
        !N64PSP_ATOMIC_LOAD(&state->accepting) || N64PSP_ATOMIC_LOAD(&state->generation) != generation ||
        N64PSP_ATOMIC_LOAD(&state->queue) != mq || !mq->msg || mq->msgCount <= 0) {
        (void)N64PSP_ATOMIC_SUB(&state->active_refs, 1);
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }

    out_ref->state = state;
    out_ref->sync = state->sync;
    out_ref->generation = generation;
    return N64PSP_OK;
}

static void finish_queue_op(queue_ref *ref) {
    if (!ref->state || N64PSP_ATOMIC_SUB(&ref->state->active_refs, 1) == UINT32_MAX) {
        fatal_invariant("n64psp queue invariant: active reference underflow");
    }
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
    N64PSP_ATOMIC_STORE(&g_accepting_ops, 1);
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
    N64PSP_ATOMIC_STORE(&g_accepting_ops, 0);
    for (i = 0; i < N64PSP_MAX_QUEUES; i++) {
        if (g_queues[i].initializing || (g_queues[i].published && !state_quiescent(&g_queues[i]))) {
            N64PSP_ATOMIC_STORE(&g_accepting_ops, 1);
            registry_unlock(platform);
            return N64PSP_ERROR_BUSY;
        }
    }
    for (i = 0; i < N64PSP_MAX_QUEUES; i++) {
        if (g_queues[i].published) {
            to_destroy[destroy_count++] = g_queues[i].sync;
        }
        N64PSP_ATOMIC_STORE(&g_queues[i].accepting, 0);
        N64PSP_ATOMIC_STORE(&g_queues[i].published, 0);
        N64PSP_ATOMIC_STORE(&g_queues[i].queue, (OSMesgQueue *)NULL);
        g_queues[i].initializing = 0;
    }
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
    int was_published;
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
    if (!N64PSP_ATOMIC_LOAD(&g_accepting_ops)) {
        registry_unlock(platform);
        return N64PSP_ERROR_NOT_INITIALIZED;
    }
    state = alloc_state_locked(mq);
    if (!state) {
        registry_unlock(platform);
        log_create_rejected("registry exhausted", mq);
        return N64PSP_ERROR_NO_MEMORY;
    }
    if (state->initializing || (state->published && !state_quiescent(state))) {
        registry_unlock(platform);
        log_create_rejected("queue is busy", mq);
        return N64PSP_ERROR_BUSY;
    }
    was_published = state->published;
    N64PSP_ATOMIC_STORE(&state->queue, mq);
    state->initializing = 1;
    N64PSP_ATOMIC_STORE(&state->accepting, 0);
    registry_unlock(platform);

    result = create_sync(platform, &new_sync);
    if (result != N64PSP_OK) {
        if (registry_lock(platform) == N64PSP_OK) {
            state->initializing = 0;
            if (was_published) {
                N64PSP_ATOMIC_STORE(&state->accepting, 1);
            } else {
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
    state->sender_waiters = 0;
    state->receiver_waiters = 0;
    N64PSP_ATOMIC_STORE(&state->active_refs, 0);
    N64PSP_ATOMIC_STORE(&state->blocked_waiters, 0);
    N64PSP_ATOMIC_ADD(&state->generation, 1);
    N64PSP_ATOMIC_STORE(&state->published, 1);
    N64PSP_ATOMIC_STORE(&state->accepting, 1);
    state->initializing = 0;
    registry_unlock(platform);

    if (was_published) {
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

static void note_sender_waiter_count(queue_state *state) {
    counter_max_u32(&g_counters.max_sender_waiters, (uint32_t)state->sender_waiters);
}

static void note_receiver_waiter_count(queue_state *state) {
    counter_max_u32(&g_counters.max_receiver_waiters, (uint32_t)state->receiver_waiters);
}

static int post_waiter(const n64psp_platform_callbacks *platform, n64psp_platform_sem *sem, int wakes_sender) {
    n64psp_result result = platform->sem_post(platform->userdata, sem);

    if (result != N64PSP_OK) {
        fatal_invariant("n64psp queue invariant: failed to signal queue waiter");
        return -1;
    }
    counter_inc(wakes_sender ? &g_counters.sender_wake_signals : &g_counters.receiver_wake_signals);
    return 0;
}

static int send_common(OSMesgQueue *mq, OSMesg msg, int flag, int jam) {
    const n64psp_platform_callbacks *platform = n64psp__platform();
    queue_ref ref;
    uintptr_t critical_state = 0;
    n64psp_result result;
    int blocked_once = 0;
    int woke_without_progress = 0;

    counter_inc(jam ? &g_counters.jam_calls : &g_counters.send_calls);
    result = begin_queue_op(mq, flag, &ref);
    if (result != N64PSP_OK) {
        return -1;
    }

    for (;;) {
        int wake_receiver = 0;

        result = queue_enter(platform, &ref.sync, &critical_state);
        if (result != N64PSP_OK) {
            finish_queue_op(&ref);
            return -1;
        }
        if (mq->validCount < mq->msgCount) {
            if (jam) {
                mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
                mq->msg[mq->first] = msg;
            } else {
                int index = (mq->first + mq->validCount) % mq->msgCount;
                mq->msg[index] = msg;
            }
            mq->validCount++;
            if (ref.state->receiver_waiters > 0) {
                ref.state->receiver_waiters--;
                wake_receiver = 1;
            }
            queue_leave(platform, &ref.sync, critical_state);
            if (wake_receiver && post_waiter(platform, ref.sync.receivers, 0) != 0) {
                finish_queue_op(&ref);
                return -1;
            }
            if (!blocked_once) {
                counter_inc(&g_counters.uncontended_successes);
            }
            finish_queue_op(&ref);
            return 0;
        }
        if (flag == OS_MESG_NOBLOCK) {
            queue_leave(platform, &ref.sync, critical_state);
            counter_inc(&g_counters.failed_nonblocking);
            finish_queue_op(&ref);
            return -1;
        }
        if (woke_without_progress) {
            counter_inc(&g_counters.spurious_wakeups);
            woke_without_progress = 0;
        }
        ref.state->sender_waiters++;
        note_sender_waiter_count(ref.state);
        queue_leave(platform, &ref.sync, critical_state);

        if (!blocked_once) {
            counter_inc(&g_counters.sender_blocks);
            blocked_once = 1;
        }
        (void)N64PSP_ATOMIC_ADD(&ref.state->blocked_waiters, 1);
        result = platform->sem_wait(platform->userdata, ref.sync.senders);
        (void)N64PSP_ATOMIC_SUB(&ref.state->blocked_waiters, 1);
        if (result != N64PSP_OK) {
            fatal_invariant("n64psp queue invariant: sender wait failed");
            finish_queue_op(&ref);
            return -1;
        }
        counter_inc(&g_counters.retry_wakeups);
        woke_without_progress = 1;
    }
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
    uintptr_t critical_state = 0;
    n64psp_result result;
    int blocked_once = 0;
    int woke_without_progress = 0;

    counter_inc(&g_counters.recv_calls);
    result = begin_queue_op(mq, flag, &ref);
    if (result != N64PSP_OK) {
        return -1;
    }

    for (;;) {
        int wake_sender = 0;
        OSMesg value = 0;

        result = queue_enter(platform, &ref.sync, &critical_state);
        if (result != N64PSP_OK) {
            finish_queue_op(&ref);
            return -1;
        }
        if (mq->validCount > 0) {
            value = mq->msg[mq->first];
            mq->first = (mq->first + 1) % mq->msgCount;
            mq->validCount--;
            if (ref.state->sender_waiters > 0) {
                ref.state->sender_waiters--;
                wake_sender = 1;
            }
            queue_leave(platform, &ref.sync, critical_state);
            if (wake_sender && post_waiter(platform, ref.sync.senders, 1) != 0) {
                finish_queue_op(&ref);
                return -1;
            }
            if (msg) {
                *msg = value;
            }
            if (!blocked_once) {
                counter_inc(&g_counters.uncontended_successes);
            }
            finish_queue_op(&ref);
            return 0;
        }
        if (flag == OS_MESG_NOBLOCK) {
            queue_leave(platform, &ref.sync, critical_state);
            counter_inc(&g_counters.failed_nonblocking);
            finish_queue_op(&ref);
            return -1;
        }
        if (woke_without_progress) {
            counter_inc(&g_counters.spurious_wakeups);
            woke_without_progress = 0;
        }
        ref.state->receiver_waiters++;
        note_receiver_waiter_count(ref.state);
        queue_leave(platform, &ref.sync, critical_state);

        if (!blocked_once) {
            counter_inc(&g_counters.receiver_blocks);
            blocked_once = 1;
        }
        (void)N64PSP_ATOMIC_ADD(&ref.state->blocked_waiters, 1);
        result = platform->sem_wait(platform->userdata, ref.sync.receivers);
        (void)N64PSP_ATOMIC_SUB(&ref.state->blocked_waiters, 1);
        if (result != N64PSP_OK) {
            fatal_invariant("n64psp queue invariant: receiver wait failed");
            finish_queue_op(&ref);
            return -1;
        }
        counter_inc(&g_counters.retry_wakeups);
        woke_without_progress = 1;
    }
}
