#ifndef N64PSP_BRIDGE_H
#define N64PSP_BRIDGE_H

#include "n64psp/result.h"
#include "n64psp/types.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct n64psp_rdram {
    uint8_t *data;
    size_t size;
    n64psp_n64_addr base;
} n64psp_rdram;

typedef struct n64psp_rom {
    const uint8_t *data;
    size_t size;
} n64psp_rom;

n64psp_result n64psp_rdram_register(n64psp_rdram *rdram, void *data, size_t size, n64psp_n64_addr base);
n64psp_result n64psp_rdram_translate(n64psp_rdram *rdram, n64psp_n64_addr addr, size_t size, void **out_ptr);
n64psp_result n64psp_rdram_translate_const(const n64psp_rdram *rdram, n64psp_n64_addr addr, size_t size,
                                           const void **out_ptr);
n64psp_result n64psp_rdram_load_be16(const n64psp_rdram *rdram, n64psp_n64_addr addr, uint16_t *out_value);
n64psp_result n64psp_rdram_load_be32(const n64psp_rdram *rdram, n64psp_n64_addr addr, uint32_t *out_value);
n64psp_result n64psp_rdram_load_be64(const n64psp_rdram *rdram, n64psp_n64_addr addr, uint64_t *out_value);
n64psp_result n64psp_rdram_store_be16(n64psp_rdram *rdram, n64psp_n64_addr addr, uint16_t value);
n64psp_result n64psp_rdram_store_be32(n64psp_rdram *rdram, n64psp_n64_addr addr, uint32_t value);
n64psp_result n64psp_rdram_store_be64(n64psp_rdram *rdram, n64psp_n64_addr addr, uint64_t value);
n64psp_result n64psp_rom_register(n64psp_rom *rom, const void *data, size_t size);
n64psp_result n64psp_rom_read(const n64psp_rom *rom, uint32_t offset, void *dst, size_t size);
n64psp_result n64psp_pi_read(uint32_t offset, void *dst, size_t size);

#ifdef __cplusplus
}
#endif

#endif
