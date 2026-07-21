#include "tnl_internal.h"

#include "math_internal.h"

#include <math.h>
#include <string.h>

static void n64psp_tnl_load_vertex(
    n64psp_packed_vertex* vertex,
    const unsigned char* packed_vertices,
    size_t index
) {
    memcpy(vertex, packed_vertices + index * sizeof(*vertex), sizeof(*vertex));
}

static void n64psp_tnl_transform_vertex(
    n64psp_vec4f_pair* output,
    const n64psp_tnl_matrices* matrices,
    const n64psp_packed_vertex* vertex
) {
    n64psp_vec4f input;

    input.x = (float)vertex->position[0];
    input.y = (float)vertex->position[1];
    input.z = (float)vertex->position[2];
    input.w = 1.0f;
    n64psp_mat4f_transform_vec4_scalar(
        &output->first,
        &matrices->modelview,
        &input
    );
    n64psp_mat4f_transform_vec4_scalar(
        &output->second,
        &matrices->projection,
        &output->first
    );
}

static void n64psp_tnl_light_vertex(
    n64psp_vec4f* output,
    const n64psp_mat4f* normal_matrix,
    const n64psp_packed_vertex* vertex,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count
) {
    float nx = (float)vertex->attribute[0];
    float ny = (float)vertex->attribute[1];
    float nz = (float)vertex->attribute[2];
    float tx =
        (normal_matrix->m[0][0] * nx) +
        (normal_matrix->m[1][0] * ny) +
        (normal_matrix->m[2][0] * nz);
    float ty =
        (normal_matrix->m[0][1] * nx) +
        (normal_matrix->m[1][1] * ny) +
        (normal_matrix->m[2][1] * nz);
    float tz =
        (normal_matrix->m[0][2] * nx) +
        (normal_matrix->m[1][2] * ny) +
        (normal_matrix->m[2][2] * nz);
    float length_squared = (tx * tx) + (ty * ty) + (tz * tz);
    float r = ambient->x;
    float g = ambient->y;
    float b = ambient->z;
    size_t light_index;

    if (length_squared > 0.000001f) {
        float inverse_length = 1.0f / sqrtf(length_squared);

        tx *= inverse_length;
        ty *= inverse_length;
        tz *= inverse_length;
    }

    for (light_index = 0u; light_index < light_count; ++light_index) {
        const n64psp_directional_lightf* light = &lights[light_index];
        float dot =
            (tx * light->direction.x) +
            (ty * light->direction.y) +
            (tz * light->direction.z);

        if (dot > 0.0f) {
            r += light->color.x * dot;
            g += light->color.y * dot;
            b += light->color.z * dot;
        }
    }

    output->x = r;
    output->y = g;
    output->z = b;
    output->w = 0.0f;
}

void n64psp_tnl_transform_packed_batch_scalar(
    n64psp_vec4f_pair* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    size_t count
) {
    const unsigned char* input = (const unsigned char*)packed_vertices;
    size_t index;

    for (index = 0u; index < count; ++index) {
        n64psp_packed_vertex vertex;

        n64psp_tnl_load_vertex(&vertex, input, index);
        n64psp_tnl_transform_vertex(&output[index], matrices, &vertex);
    }
}

void n64psp_tnl_transform_light_packed_batch_scalar(
    n64psp_vec4f_pair* transform_output,
    n64psp_vec4f* lighting_output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    size_t count
) {
    const unsigned char* input = (const unsigned char*)packed_vertices;
    size_t index;

    for (index = 0u; index < count; ++index) {
        n64psp_packed_vertex vertex;

        n64psp_tnl_load_vertex(&vertex, input, index);
        n64psp_tnl_transform_vertex(&transform_output[index], matrices, &vertex);
        n64psp_tnl_light_vertex(
            &lighting_output[index],
            &matrices->modelview,
            &vertex,
            lights,
            ambient,
            light_count
        );
    }
}
