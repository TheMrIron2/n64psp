#include <n64psp/tnl.h>

#include "tnl_internal.h"

#if defined(__PSP__)
#include <stddef.h>

typedef char n64psp_tnl_output_streams_size_check[
    (sizeof(n64psp_tnl_output_streams) == 32u) ? 1 : -1
];
typedef char n64psp_tnl_output_streams_stride_offset_check[
    (offsetof(n64psp_tnl_output_streams, vertex_stride) == 24u) ? 1 : -1
];
#endif

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

void n64psp_tnl_transform_project_packed_batch(
    const n64psp_tnl_output_streams* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    int has_projection,
    size_t count
) {
#if defined(__PSP__) && N64PSP_USE_VFPU
    n64psp_tnl_transform_project_packed_batch_vfpu(
        output,
        matrices,
        packed_vertices,
        has_projection,
        count
    );
#else
    n64psp_tnl_transform_project_packed_batch_scalar(
        output,
        matrices,
        packed_vertices,
        has_projection,
        count
    );
#endif
}

void n64psp_tnl_transform_project_light_packed_batch(
    const n64psp_tnl_output_streams* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    int has_projection,
    size_t count
) {
#if defined(__PSP__) && N64PSP_USE_VFPU
    n64psp_tnl_transform_project_light_packed_batch_vfpu(
        output,
        matrices,
        packed_vertices,
        lights,
        ambient,
        light_count,
        has_projection,
        count
    );
#else
    n64psp_tnl_transform_project_light_packed_batch_scalar(
        output,
        matrices,
        packed_vertices,
        lights,
        ambient,
        light_count,
        has_projection,
        count
    );
#endif
}

void n64psp_texgen_snorm8_batch(
    n64psp_texcoord_s10_5* output,
    const n64psp_mat4f* modelview,
    const void* packed_vertices,
    n64psp_texgen_mode mode,
    size_t count
) {
    n64psp_texgen_snorm8_batch_scalar(
        output,
        modelview,
        packed_vertices,
        mode,
        count
    );
}
