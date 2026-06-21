#ifndef N64PSP_LIGHTING_INTERNAL_H
#define N64PSP_LIGHTING_INTERNAL_H

#include <n64psp/lighting.h>

void n64psp_directional_light_snorm8_batch_scalar(
    n64psp_vec4f* output_rgb,
    const n64psp_mat4f* normal_matrix,
    const n64psp_snorm8x4* normals,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    size_t count
);

#if defined(__PSP__) && N64PSP_USE_VFPU
void n64psp_directional_light_snorm8_batch_vfpu(
    n64psp_vec4f* output_rgb,
    const n64psp_mat4f* normal_matrix,
    const n64psp_snorm8x4* normals,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    size_t count
);
#endif

#endif
