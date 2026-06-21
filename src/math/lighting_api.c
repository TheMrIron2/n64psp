#include <n64psp/lighting.h>

#include "lighting_internal.h"

#ifndef N64PSP_USE_VFPU
#define N64PSP_USE_VFPU 0
#endif

void n64psp_directional_light_snorm8_batch(
    n64psp_vec4f* output_rgb,
    const n64psp_mat4f* normal_matrix,
    const n64psp_snorm8x4* normals,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    size_t count
) {
#if defined(__PSP__) && N64PSP_USE_VFPU
    n64psp_directional_light_snorm8_batch_vfpu(
        output_rgb,
        normal_matrix,
        normals,
        lights,
        ambient,
        light_count,
        count
    );
#else
    n64psp_directional_light_snorm8_batch_scalar(
        output_rgb,
        normal_matrix,
        normals,
        lights,
        ambient,
        light_count,
        count
    );
#endif
}
