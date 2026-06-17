#ifndef N64PSP_TYPES_H
#define N64PSP_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t OSMesg;

typedef struct OSMesgQueue {
    void *mtqueue;
    void *fullqueue;
    int validCount;
    int first;
    int msgCount;
    OSMesg *msg;
} OSMesgQueue;

typedef struct OSTask {
    uint32_t type;
    uint32_t flags;
    void *ucode;
    uint32_t ucode_size;
    void *data_ptr;
    uint32_t data_size;
} OSTask;

typedef uint32_t n64psp_n64_addr;
typedef uint64_t n64psp_time_us;
typedef uint64_t n64psp_ticks;

#define N64PSP_OS_MESG_NOBLOCK 0
#define N64PSP_OS_MESG_BLOCK 1
#define OS_MESG_NOBLOCK N64PSP_OS_MESG_NOBLOCK
#define OS_MESG_BLOCK N64PSP_OS_MESG_BLOCK

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define N64PSP_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
#define N64PSP_STATIC_ASSERT(cond, msg) typedef char n64psp_static_assertion_##__LINE__[(cond) ? 1 : -1]
#endif

N64PSP_STATIC_ASSERT(sizeof(n64psp_n64_addr) == 4, "N64 addresses are 32-bit");

#ifdef __cplusplus
}
#endif

#endif
