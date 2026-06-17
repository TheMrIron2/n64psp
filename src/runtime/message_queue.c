#include "n64psp/runtime.h"
#include <stddef.h>
#include <string.h>

const n64psp_platform_callbacks *n64psp__platform(void);

#define N64PSP_MAX_QUEUES 32

typedef struct queue_state {
    OSMesgQueue *queue;
    n64psp_platform_mutex *mutex;
    n64psp_platform_sem *items;
    n64psp_platform_sem *slots;
    int active;
} queue_state;

static queue_state g_queues[N64PSP_MAX_QUEUES];

static queue_state *find_state(OSMesgQueue *mq) {
    for (size_t i = 0; i < N64PSP_MAX_QUEUES; i++) {
        if (g_queues[i].active && g_queues[i].queue == mq) {
            return &g_queues[i];
        }
    }
    return NULL;
}

static queue_state *alloc_state(OSMesgQueue *mq) {
    queue_state *free_slot = NULL;
    for (size_t i = 0; i < N64PSP_MAX_QUEUES; i++) {
        if (g_queues[i].active && g_queues[i].queue == mq) {
            return &g_queues[i];
        }
        if (!g_queues[i].active && !free_slot) {
            free_slot = &g_queues[i];
        }
    }
    return free_slot;
}

void osCreateMesgQueue(OSMesgQueue *mq, OSMesg *msg, int count) {
    const n64psp_platform_callbacks *platform = n64psp__platform();
    if (!mq || !msg || count <= 0 || !platform) {
        return;
    }
    queue_state *state = alloc_state(mq);
    if (!state) {
        return;
    }
    if (state->active) {
        platform->sem_destroy(platform->userdata, state->items);
        platform->sem_destroy(platform->userdata, state->slots);
        platform->mutex_destroy(platform->userdata, state->mutex);
    }
    memset(state, 0, sizeof(*state));
    mq->mtqueue = NULL;
    mq->fullqueue = NULL;
    mq->msg = msg;
    mq->validCount = 0;
    mq->first = 0;
    mq->msgCount = count;
    if (platform->mutex_create(platform->userdata, &state->mutex) != N64PSP_OK ||
        platform->sem_create(platform->userdata, 0, (uint32_t)count, &state->items) != N64PSP_OK ||
        platform->sem_create(platform->userdata, (uint32_t)count, (uint32_t)count, &state->slots) != N64PSP_OK) {
        if (state->items) {
            platform->sem_destroy(platform->userdata, state->items);
        }
        if (state->slots) {
            platform->sem_destroy(platform->userdata, state->slots);
        }
        if (state->mutex) {
            platform->mutex_destroy(platform->userdata, state->mutex);
        }
        memset(state, 0, sizeof(*state));
        return;
    }
    state->queue = mq;
    state->active = 1;
}

static int send_common(OSMesgQueue *mq, OSMesg msg, int flag, int jam) {
    const n64psp_platform_callbacks *platform = n64psp__platform();
    queue_state *state = mq ? find_state(mq) : NULL;
    n64psp_result lock_result;
    n64psp_result post_result;
    if (!platform || !state || !mq->msg || mq->msgCount <= 0 || (flag != OS_MESG_BLOCK && flag != OS_MESG_NOBLOCK)) {
        return -1;
    }
    n64psp_result wait_result = flag == OS_MESG_BLOCK ? platform->sem_wait(platform->userdata, state->slots)
                                                      : platform->sem_try_wait(platform->userdata, state->slots);
    if (wait_result != N64PSP_OK) {
        return -1;
    }
    lock_result = platform->mutex_lock(platform->userdata, state->mutex);
    if (lock_result != N64PSP_OK) {
        platform->sem_post(platform->userdata, state->slots);
        return -1;
    }
    if (jam) {
        mq->first = (mq->first + mq->msgCount - 1) % mq->msgCount;
        mq->msg[mq->first] = msg;
    } else {
        int index = (mq->first + mq->validCount) % mq->msgCount;
        mq->msg[index] = msg;
    }
    mq->validCount++;
    platform->mutex_unlock(platform->userdata, state->mutex);
    post_result = platform->sem_post(platform->userdata, state->items);
    if (post_result != N64PSP_OK) {
        return -1;
    }
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
    queue_state *state = mq ? find_state(mq) : NULL;
    n64psp_result lock_result;
    n64psp_result post_result;
    if (!platform || !state || !mq->msg || mq->msgCount <= 0 || (flag != OS_MESG_BLOCK && flag != OS_MESG_NOBLOCK)) {
        return -1;
    }
    n64psp_result wait_result = flag == OS_MESG_BLOCK ? platform->sem_wait(platform->userdata, state->items)
                                                      : platform->sem_try_wait(platform->userdata, state->items);
    if (wait_result != N64PSP_OK) {
        return -1;
    }
    lock_result = platform->mutex_lock(platform->userdata, state->mutex);
    if (lock_result != N64PSP_OK) {
        platform->sem_post(platform->userdata, state->items);
        return -1;
    }
    OSMesg value = mq->msg[mq->first];
    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;
    platform->mutex_unlock(platform->userdata, state->mutex);
    post_result = platform->sem_post(platform->userdata, state->slots);
    if (post_result != N64PSP_OK) {
        return -1;
    }
    if (msg) {
        *msg = value;
    }
    return 0;
}

void n64psp__queues_reset(void) {
    const n64psp_platform_callbacks *platform = n64psp__platform();
    if (!platform) {
        memset(g_queues, 0, sizeof(g_queues));
        return;
    }
    for (size_t i = 0; i < N64PSP_MAX_QUEUES; i++) {
        if (g_queues[i].active) {
            platform->sem_destroy(platform->userdata, g_queues[i].items);
            platform->sem_destroy(platform->userdata, g_queues[i].slots);
            platform->mutex_destroy(platform->userdata, g_queues[i].mutex);
        }
    }
    memset(g_queues, 0, sizeof(g_queues));
}
