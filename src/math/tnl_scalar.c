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

static uint32_t n64psp_tnl_clip_code(const n64psp_vec4f* clip) {
    uint32_t code = 0u;

    if (clip->x < -clip->w) {
        code |= 1u << 0;
    }
    if (clip->x > clip->w) {
        code |= 1u << 1;
    }
    if (clip->y < -clip->w) {
        code |= 1u << 2;
    }
    if (clip->y > clip->w) {
        code |= 1u << 3;
    }
    if (clip->z < -clip->w) {
        code |= 1u << 4;
    }
    if (clip->z > clip->w) {
        code |= 1u << 5;
    }
    return code;
}

static void n64psp_tnl_store_projected_vertex(
    const n64psp_tnl_output_streams* output,
    size_t index,
    const n64psp_vec4f_pair* transform,
    int has_projection
) {
    unsigned char* view = (unsigned char*)output->view + index * output->vertex_stride;
    unsigned char* clip = (unsigned char*)output->clip + index * output->vertex_stride;
    unsigned char* projected = (unsigned char*)output->projected + index * output->vertex_stride;
    unsigned char* clip_code = (unsigned char*)output->clip_code + index * output->vertex_stride;
    unsigned char* valid = (unsigned char*)output->valid + index * output->vertex_stride;
    n64psp_vec4f final_clip;
    float projected_xyz[3];
    uint32_t final_code;
    uint32_t final_valid;

    memcpy(view, &transform->first, sizeof(transform->first));

    if (has_projection) {
        if ((transform->second.w > -0.001f) &&
            (transform->second.w < 0.001f)) {
            final_valid = 0u;
            memcpy(valid, &final_valid, sizeof(final_valid));
            return;
        }

        final_clip = transform->second;
        projected_xyz[0] = final_clip.x / final_clip.w;
        projected_xyz[1] = final_clip.y / final_clip.w;
        projected_xyz[2] = final_clip.z / final_clip.w;
    } else {
        projected_xyz[0] = transform->first.x / 320.0f;
        projected_xyz[1] = -transform->first.y / 240.0f;
        projected_xyz[2] = transform->first.z / 4096.0f;
        final_clip.x = projected_xyz[0];
        final_clip.y = projected_xyz[1];
        final_clip.z = projected_xyz[2];
        final_clip.w = 1.0f;
    }

    final_code = n64psp_tnl_clip_code(&final_clip);
    final_valid = 1u;
    memcpy(clip, &final_clip, sizeof(final_clip));
    memcpy(projected, projected_xyz, sizeof(projected_xyz));
    memcpy(clip_code, &final_code, sizeof(final_code));
    memcpy(valid, &final_valid, sizeof(final_valid));
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

void n64psp_tnl_transform_project_packed_batch_scalar(
    const n64psp_tnl_output_streams* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    int has_projection,
    size_t count
) {
    const unsigned char* input = (const unsigned char*)packed_vertices;
    size_t index;

    for (index = 0u; index < count; ++index) {
        n64psp_packed_vertex vertex;
        n64psp_vec4f_pair transform;

        n64psp_tnl_load_vertex(&vertex, input, index);
        n64psp_tnl_transform_vertex(&transform, matrices, &vertex);
        n64psp_tnl_store_projected_vertex(
            output,
            index,
            &transform,
            has_projection
        );
    }
}

void n64psp_tnl_transform_project_light_packed_batch_scalar(
    const n64psp_tnl_output_streams* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    int has_projection,
    size_t count
) {
    const unsigned char* input = (const unsigned char*)packed_vertices;
    size_t index;

    for (index = 0u; index < count; ++index) {
        unsigned char* lighting =
            (unsigned char*)output->lighting + index * output->lighting_stride;
        n64psp_packed_vertex vertex;
        n64psp_vec4f_pair transform;
        n64psp_vec4f light;

        n64psp_tnl_load_vertex(&vertex, input, index);
        n64psp_tnl_transform_vertex(&transform, matrices, &vertex);
        n64psp_tnl_store_projected_vertex(
            output,
            index,
            &transform,
            has_projection
        );
        n64psp_tnl_light_vertex(
            &light,
            &matrices->modelview,
            &vertex,
            lights,
            ambient,
            light_count
        );
        memcpy(lighting, &light, sizeof(light));
    }
}

static int16_t n64psp_texgen_to_s10_5(float value) {
    float scaled = value * 65536.0f;

    if (scaled <= 0.0f) {
        return 0;
    }
    if (scaled >= 32767.0f) {
        return 32767;
    }
    return (int16_t)scaled;
}

void n64psp_texgen_snorm8_batch_scalar(
    n64psp_texcoord_s10_5* output,
    const n64psp_mat4f* modelview,
    const void* packed_vertices,
    n64psp_texgen_mode mode,
    size_t count
) {
    const unsigned char* input = (const unsigned char*)packed_vertices;
    const float inverse_two_pi = 0.15915494309189535f;
    float look_s_x;
    float look_s_y;
    float look_s_z;
    float look_t_x;
    float look_t_y;
    float look_t_z;
    float look_s_length_squared;
    float look_t_length_squared;
    size_t index;

    if (count == 0u) {
        return;
    }

    look_s_x = modelview->m[1][0];
    look_s_y = modelview->m[1][1];
    look_s_z = modelview->m[1][2];
    look_t_x = modelview->m[0][0];
    look_t_y = modelview->m[0][1];
    look_t_z = modelview->m[0][2];
    look_s_length_squared =
        look_s_x * look_s_x + look_s_y * look_s_y + look_s_z * look_s_z;
    look_t_length_squared =
        look_t_x * look_t_x + look_t_y * look_t_y + look_t_z * look_t_z;

    if (look_s_length_squared > 0.000001f) {
        float inverse_length = 1.0f / sqrtf(look_s_length_squared);

        look_s_x *= inverse_length;
        look_s_y *= inverse_length;
        look_s_z *= inverse_length;
    }
    if (look_t_length_squared > 0.000001f) {
        float inverse_length = 1.0f / sqrtf(look_t_length_squared);

        look_t_x *= inverse_length;
        look_t_y *= inverse_length;
        look_t_z *= inverse_length;
    }

    for (index = 0u; index < count; ++index) {
        n64psp_packed_vertex vertex;
        float dot_s;
        float dot_t;
        float s;
        float t;

        n64psp_tnl_load_vertex(&vertex, input, index);
        dot_s = (
            (float)vertex.attribute[0] * look_s_x +
            (float)vertex.attribute[1] * look_s_y +
            (float)vertex.attribute[2] * look_s_z
        ) / 127.0f;
        dot_t = (
            (float)vertex.attribute[0] * look_t_x +
            (float)vertex.attribute[1] * look_t_y +
            (float)vertex.attribute[2] * look_t_z
        ) / 127.0f;
        if (dot_s < -1.0f) {
            dot_s = -1.0f;
        } else if (dot_s > 1.0f) {
            dot_s = 1.0f;
        }
        if (dot_t < -1.0f) {
            dot_t = -1.0f;
        } else if (dot_t > 1.0f) {
            dot_t = 1.0f;
        }

        if (mode == N64PSP_TEXGEN_LINEAR) {
            s = acosf(-dot_s) * inverse_two_pi;
            t = acosf(-dot_t) * inverse_two_pi;
        } else {
            s = 0.25f * (1.0f + dot_s);
            t = 0.25f * (1.0f + dot_t);
        }
        output[index].s = n64psp_texgen_to_s10_5(s);
        output[index].t = n64psp_texgen_to_s10_5(t);
    }
}
