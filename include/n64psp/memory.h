#ifndef N64PSP_MEMORY_H
#define N64PSP_MEMORY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PSP callers must run on a VFPU enabled thread */
void *n64psp_memcpy(void *dst, const void *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif
