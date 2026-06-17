#ifndef N64PSP_RESULT_H
#define N64PSP_RESULT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum n64psp_result {
    N64PSP_OK = 0,
    N64PSP_ERROR_INVALID_ARGUMENT = -1,
    N64PSP_ERROR_INVALID_STATE = -2,
    N64PSP_ERROR_ALREADY_INITIALIZED = -3,
    N64PSP_ERROR_NOT_INITIALIZED = -4,
    N64PSP_ERROR_NO_MEMORY = -5,
    N64PSP_ERROR_QUEUE_FULL = -6,
    N64PSP_ERROR_QUEUE_EMPTY = -7,
    N64PSP_ERROR_TIMEOUT = -8,
    N64PSP_ERROR_OUT_OF_RANGE = -9,
    N64PSP_ERROR_UNSUPPORTED = -10,
    N64PSP_ERROR_PLATFORM = -11,
    N64PSP_ERROR_BUSY = -12,
} n64psp_result;

const char *n64psp_result_name(n64psp_result result);

#ifdef __cplusplus
}
#endif

#endif
