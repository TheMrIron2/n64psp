#include "lighting_internal.h"

#include <math.h>

void n64psp_directional_light_snorm8_batch_scalar(
    n64psp_vec4f* output_rgb,
    const n64psp_mat4f* normal_matrix,
    const n64psp_snorm8x4* normals,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    size_t count
) {
    size_t index;

    if (count == 0u) {
        return;
    }

    for (index = 0u; index < count; ++index) {
        float nx = (float)normals[index].x;
        float ny = (float)normals[index].y;
        float nz = (float)normals[index].z;
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

        output_rgb[index].x = r;
        output_rgb[index].y = g;
        output_rgb[index].z = b;
        output_rgb[index].w = 0.0f;
    }
}
