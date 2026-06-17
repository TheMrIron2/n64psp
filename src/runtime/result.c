#include "n64psp/result.h"

const char *n64psp_result_name(n64psp_result result) {
    switch (result) {
    case N64PSP_OK:
        return "N64PSP_OK";
    case N64PSP_ERROR_INVALID_ARGUMENT:
        return "N64PSP_ERROR_INVALID_ARGUMENT";
    case N64PSP_ERROR_INVALID_STATE:
        return "N64PSP_ERROR_INVALID_STATE";
    case N64PSP_ERROR_ALREADY_INITIALIZED:
        return "N64PSP_ERROR_ALREADY_INITIALIZED";
    case N64PSP_ERROR_NOT_INITIALIZED:
        return "N64PSP_ERROR_NOT_INITIALIZED";
    case N64PSP_ERROR_NO_MEMORY:
        return "N64PSP_ERROR_NO_MEMORY";
    case N64PSP_ERROR_QUEUE_FULL:
        return "N64PSP_ERROR_QUEUE_FULL";
    case N64PSP_ERROR_QUEUE_EMPTY:
        return "N64PSP_ERROR_QUEUE_EMPTY";
    case N64PSP_ERROR_TIMEOUT:
        return "N64PSP_ERROR_TIMEOUT";
    case N64PSP_ERROR_OUT_OF_RANGE:
        return "N64PSP_ERROR_OUT_OF_RANGE";
    case N64PSP_ERROR_UNSUPPORTED:
        return "N64PSP_ERROR_UNSUPPORTED";
    case N64PSP_ERROR_PLATFORM:
        return "N64PSP_ERROR_PLATFORM";
    default:
        return "N64PSP_ERROR_UNKNOWN";
    }
}
