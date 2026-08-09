#ifndef N64PSP_DETAIL_MEMORY_PSP_IMPL_H
#define N64PSP_DETAIL_MEMORY_PSP_IMPL_H

#include "n64psp/memory.h"

#include <stdint.h>

#ifndef N64PSP_MEMCPY_PSP_NAME
#error "N64PSP_MEMCPY_PSP_NAME must name the generated function"
#endif

#define N64PSP_MEMCPY_VFPU_THRESHOLD 128

void *N64PSP_MEMCPY_PSP_NAME(void *s1, const void *s2, size_t n) {
    unsigned char *su1 = (unsigned char *)s1;
    const unsigned char *su2 = (const unsigned char *)s2;

    if (n >= N64PSP_MEMCPY_VFPU_THRESHOLD) {
        while ((((uint32_t)su1) & 0xF) != 0) {
            *su1++ = *su2++;
            n--;
        }

        if ((((uint32_t)su2) & 0xF) == 0) {
            while (n >= 64) {
                __asm__ volatile(
                    ".set push\n"
                    ".set noreorder\n"
                    "lv.q c000, 0(%0)\n"
                    "lv.q c010, 16(%0)\n"
                    "lv.q c020, 32(%0)\n"
                    "lv.q c030, 48(%0)\n"
                    "sv.q c000, 0(%1)\n"
                    "sv.q c010, 16(%1)\n"
                    "sv.q c020, 32(%1)\n"
                    "sv.q c030, 48(%1)\n"
                    ".set pop\n"
                    :
                    : "r"(su2), "r"(su1)
                    : "memory");
                su1 += 64;
                su2 += 64;
                n -= 64;
            }

            while (n >= 16) {
                __asm__ volatile(
                    "lv.q c000, 0(%0)\n"
                    "sv.q c000, 0(%1)\n"
                    :
                    : "r"(su2), "r"(su1)
                    : "memory");
                su1 += 16;
                su2 += 16;
                n -= 16;
            }
        } else {
            while (n >= 64) {
                __asm__ volatile(
                    ".set push\n"
                    ".set noreorder\n"
                    "ulv.q c000, 0(%0)\n"
                    "ulv.q c010, 16(%0)\n"
                    "ulv.q c020, 32(%0)\n"
                    "ulv.q c030, 48(%0)\n"
                    "sv.q c000, 0(%1)\n"
                    "sv.q c010, 16(%1)\n"
                    "sv.q c020, 32(%1)\n"
                    "sv.q c030, 48(%1)\n"
                    ".set pop\n"
                    :
                    : "r"(su2), "r"(su1)
                    : "memory");
                su1 += 64;
                su2 += 64;
                n -= 64;
            }

            while (n >= 16) {
                __asm__ volatile(
                    "ulv.q c000, 0(%0)\n"
                    "sv.q c000, 0(%1)\n"
                    :
                    : "r"(su2), "r"(su1)
                    : "memory");
                su1 += 16;
                su2 += 16;
                n -= 16;
            }
        }
    }

    while (n > 0) {
        *su1 = *su2;
        su1++;
        su2++;
        n--;
    }

    return s1;
}

#undef N64PSP_MEMCPY_PSP_NAME

#endif
