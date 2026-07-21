#include <n64psp/tnl.h>

#include "tnl_internal.h"

#ifndef N64PSP_USE_VFPU
#define N64PSP_USE_VFPU 0
#endif

void n64psp_tnl_transform_packed_batch(
    n64psp_vec4f_pair* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    size_t count
) {
#if defined(__PSP__) && N64PSP_USE_VFPU
    n64psp_tnl_transform_packed_batch_vfpu(
        output,
        matrices,
        packed_vertices,
        count
    );
#else
    n64psp_tnl_transform_packed_batch_scalar(
        output,
        matrices,
        packed_vertices,
        count
    );
#endif
}

void n64psp_tnl_transform_light_packed_batch(
    n64psp_vec4f_pair* transform_output,
    n64psp_vec4f* lighting_output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    size_t count
) {
#if defined(__PSP__) && N64PSP_USE_VFPU
    n64psp_tnl_transform_light_packed_batch_vfpu(
        transform_output,
        lighting_output,
        matrices,
        packed_vertices,
        lights,
        ambient,
        light_count,
        count
    );
#else
    n64psp_tnl_transform_light_packed_batch_scalar(
        transform_output,
        lighting_output,
        matrices,
        packed_vertices,
        lights,
        ambient,
        light_count,
        count
    );
#endif
}
