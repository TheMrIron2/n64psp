#if N64PSP_USE_VFPU
#define N64PSP_MEMCPY_PSP_NAME n64psp_memcpy
#include "n64psp/detail/memory_psp_impl.h"
#else
#include "n64psp/memory.h"

void *n64psp_memcpy(void *dst, const void *src, size_t size) {
    unsigned char *out = (unsigned char *)dst;
    const unsigned char *in = (const unsigned char *)src;

    while (size > 0) {
        *out++ = *in++;
        size--;
    }

    return dst;
}
#endif
