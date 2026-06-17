#include "n64psp/bridge.h"
#include "n64psp/runtime.h"
#include <string.h>

const n64psp_platform_callbacks *n64psp__platform(void);

n64psp_result n64psp_rdram_register(n64psp_rdram *rdram, void *data, size_t size, n64psp_n64_addr base) {
    if (!rdram || !data || size == 0) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    rdram->data = (uint8_t *)data;
    rdram->size = size;
    rdram->base = base;
    return N64PSP_OK;
}

static n64psp_result translate_common(const n64psp_rdram *rdram, n64psp_n64_addr addr, size_t size, const void **out) {
    if (!rdram || !rdram->data || !out) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    if (addr < rdram->base) {
        return N64PSP_ERROR_OUT_OF_RANGE;
    }
    uint64_t offset = (uint64_t)addr - (uint64_t)rdram->base;
    if (offset > rdram->size || size > rdram->size - (size_t)offset) {
        return N64PSP_ERROR_OUT_OF_RANGE;
    }
    *out = rdram->data + offset;
    return N64PSP_OK;
}

n64psp_result n64psp_rdram_translate(n64psp_rdram *rdram, n64psp_n64_addr addr, size_t size, void **out_ptr) {
    const void *ptr = NULL;
    n64psp_result result = translate_common(rdram, addr, size, &ptr);
    if (result == N64PSP_OK) {
        *out_ptr = (void *)ptr;
    }
    return result;
}

n64psp_result n64psp_rdram_translate_const(const n64psp_rdram *rdram, n64psp_n64_addr addr, size_t size,
                                           const void **out_ptr) {
    return translate_common(rdram, addr, size, out_ptr);
}

n64psp_result n64psp_rdram_load_be16(const n64psp_rdram *rdram, n64psp_n64_addr addr, uint16_t *out_value) {
    const uint8_t *p = NULL;
    if (!out_value) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_result r = translate_common(rdram, addr, 2, (const void **)&p);
    if (r != N64PSP_OK) {
        return r;
    }
    *out_value = (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
    return N64PSP_OK;
}

n64psp_result n64psp_rdram_load_be32(const n64psp_rdram *rdram, n64psp_n64_addr addr, uint32_t *out_value) {
    const uint8_t *p = NULL;
    if (!out_value) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_result r = translate_common(rdram, addr, 4, (const void **)&p);
    if (r != N64PSP_OK) {
        return r;
    }
    *out_value = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
    return N64PSP_OK;
}

n64psp_result n64psp_rdram_load_be64(const n64psp_rdram *rdram, n64psp_n64_addr addr, uint64_t *out_value) {
    const uint8_t *p = NULL;
    if (!out_value) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    n64psp_result r = translate_common(rdram, addr, 8, (const void **)&p);
    if (r != N64PSP_OK) {
        return r;
    }
    *out_value = ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
                 ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) | ((uint64_t)p[6] << 8) | p[7];
    return N64PSP_OK;
}

n64psp_result n64psp_rdram_store_be16(n64psp_rdram *rdram, n64psp_n64_addr addr, uint16_t value) {
    uint8_t *p = NULL;
    n64psp_result r = n64psp_rdram_translate(rdram, addr, 2, (void **)&p);
    if (r != N64PSP_OK) {
        return r;
    }
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
    return N64PSP_OK;
}

n64psp_result n64psp_rdram_store_be32(n64psp_rdram *rdram, n64psp_n64_addr addr, uint32_t value) {
    uint8_t *p = NULL;
    n64psp_result r = n64psp_rdram_translate(rdram, addr, 4, (void **)&p);
    if (r != N64PSP_OK) {
        return r;
    }
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
    return N64PSP_OK;
}

n64psp_result n64psp_rdram_store_be64(n64psp_rdram *rdram, n64psp_n64_addr addr, uint64_t value) {
    uint8_t *p = NULL;
    n64psp_result r = n64psp_rdram_translate(rdram, addr, 8, (void **)&p);
    if (r != N64PSP_OK) {
        return r;
    }
    p[0] = (uint8_t)(value >> 56);
    p[1] = (uint8_t)(value >> 48);
    p[2] = (uint8_t)(value >> 40);
    p[3] = (uint8_t)(value >> 32);
    p[4] = (uint8_t)(value >> 24);
    p[5] = (uint8_t)(value >> 16);
    p[6] = (uint8_t)(value >> 8);
    p[7] = (uint8_t)value;
    return N64PSP_OK;
}

n64psp_result n64psp_rom_register(n64psp_rom *rom, const void *data, size_t size) {
    if (!rom || !data || size == 0) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    rom->data = (const uint8_t *)data;
    rom->size = size;
    return N64PSP_OK;
}

n64psp_result n64psp_rom_read(const n64psp_rom *rom, uint32_t offset, void *dst, size_t size) {
    if (!rom || !rom->data || (!dst && size != 0)) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    if ((uint64_t)offset > rom->size || size > rom->size - (size_t)offset) {
        return N64PSP_ERROR_OUT_OF_RANGE;
    }
    if (size != 0) {
        memcpy(dst, rom->data + offset, size);
    }
    return N64PSP_OK;
}

n64psp_result n64psp_pi_read(uint32_t offset, void *dst, size_t size) {
    const n64psp_platform_callbacks *platform = n64psp__platform();
    if (!dst && size != 0) {
        return N64PSP_ERROR_INVALID_ARGUMENT;
    }
    if (!platform || !platform->pi_read) {
        return N64PSP_ERROR_UNSUPPORTED;
    }
    return platform->pi_read(platform->userdata, offset, dst, size);
}
