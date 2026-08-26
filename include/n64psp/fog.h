#ifndef N64PSP_FOG_H
#define N64PSP_FOG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct n64psp_fog_coefficients {
    int16_t multiplier;
    int16_t offset;
} n64psp_fog_coefficients;

uint8_t n64psp_fog_alpha(const n64psp_fog_coefficients* coefficients, float clip_z, float clip_w);
uint8_t n64psp_fog_alpha_lerp(uint8_t from, uint8_t to, float amount);

#ifdef __cplusplus
}
#endif

#endif
