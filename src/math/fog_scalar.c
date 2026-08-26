#include <n64psp/fog.h>

uint8_t n64psp_fog_alpha(const n64psp_fog_coefficients* coefficients, float clip_z, float clip_w) {
    float fog;

    if ((coefficients == 0) || (clip_w <= 0.0f)) {
        return 0;
    }

    fog = ((clip_z / clip_w) * (float) coefficients->multiplier) + (float) coefficients->offset;
    if (fog <= 0.0f) {
        return 0;
    }
    if (fog >= 255.0f) {
        return 255;
    }
    return (uint8_t) fog;
}

uint8_t n64psp_fog_alpha_lerp(uint8_t from, uint8_t to, float amount) {
    float fog = (float) from + (((float) to - (float) from) * amount);

    if (fog <= 0.0f) {
        return 0;
    }
    if (fog >= 255.0f) {
        return 255;
    }
    return (uint8_t) fog;
}
