#ifndef N64PSP_TNL_INTERNAL_H
#define N64PSP_TNL_INTERNAL_H

#include <n64psp/tnl.h>

void n64psp_tnl_transform_packed_batch_scalar(
    n64psp_vec4f_pair* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    size_t count
);

void n64psp_tnl_transform_light_packed_batch_scalar(
    n64psp_vec4f_pair* transform_output,
    n64psp_vec4f* lighting_output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    size_t count
);

void n64psp_tnl_transform_project_packed_batch_scalar(
    const n64psp_tnl_output_streams* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    int has_projection,
    size_t count
);

void n64psp_tnl_transform_project_light_packed_batch_scalar(
    const n64psp_tnl_output_streams* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    int has_projection,
    size_t count
);

#if defined(__PSP__) && N64PSP_USE_VFPU
void n64psp_tnl_transform_packed_batch_vfpu(
    n64psp_vec4f_pair* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    size_t count
);

void n64psp_tnl_transform_light_packed_batch_vfpu(
    n64psp_vec4f_pair* transform_output,
    n64psp_vec4f* lighting_output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    size_t count
);

void n64psp_tnl_transform_project_packed_batch_vfpu(
    const n64psp_tnl_output_streams* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    int has_projection,
    size_t count
);

void n64psp_tnl_transform_project_light_packed_batch_vfpu(
    const n64psp_tnl_output_streams* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    int has_projection,
    size_t count
);
#endif

#endif
