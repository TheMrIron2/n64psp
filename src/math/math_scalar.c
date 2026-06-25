#include "math_internal.h"

#include <math.h>

void n64psp_sincosf_scalar(
    float radians,
    float* out_sine,
    float* out_cosine
) {
    const float sine = sinf(radians);
    const float cosine = cosf(radians);

    *out_sine = sine;
    *out_cosine = cosine;
}

void n64psp_mat4f_mul_scalar(
    n64psp_mat4f* out,
    const n64psp_mat4f* a,
    const n64psp_mat4f* b
) {
    n64psp_mat4f result;

    for (size_t column = 0; column < 4; ++column) {
        for (size_t row = 0; row < 4; ++row) {
            result.m[column][row] =
                a->m[0][row] * b->m[column][0] +
                a->m[1][row] * b->m[column][1] +
                a->m[2][row] * b->m[column][2] +
                a->m[3][row] * b->m[column][3];
        }
    }

    *out = result;
}

void n64psp_mat4f_transform_vec4_scalar(
    n64psp_vec4f* out,
    const n64psp_mat4f* matrix,
    const n64psp_vec4f* vector
) {
    const float x = vector->x;
    const float y = vector->y;
    const float z = vector->z;
    const float w = vector->w;

    n64psp_vec4f result;

    result.x =
        matrix->m[0][0] * x +
        matrix->m[1][0] * y +
        matrix->m[2][0] * z +
        matrix->m[3][0] * w;

    result.y =
        matrix->m[0][1] * x +
        matrix->m[1][1] * y +
        matrix->m[2][1] * z +
        matrix->m[3][1] * w;

    result.z =
        matrix->m[0][2] * x +
        matrix->m[1][2] * y +
        matrix->m[2][2] * z +
        matrix->m[3][2] * w;

    result.w =
        matrix->m[0][3] * x +
        matrix->m[1][3] * y +
        matrix->m[2][3] * z +
        matrix->m[3][3] * w;

    *out = result;
}

void n64psp_mat4f_transform_vec4_2mat_batch_scalar(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
) {
    size_t index;

    for (index = 0; index < count; ++index) {
        n64psp_mat4f_transform_vec4_scalar(
            &output[index].first,
            first_matrix,
            &input[index]
        );

        n64psp_mat4f_transform_vec4_scalar(
            &output[index].second,
            second_matrix,
            &input[index]
        );
    }
}

void n64psp_mat4f_transform_vec4_chain2_batch_scalar(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
) {
    size_t index;

    for (index = 0; index < count; ++index) {
        n64psp_mat4f_transform_vec4_scalar(
            &output[index].first,
            first_matrix,
            &input[index]
        );

        n64psp_mat4f_transform_vec4_scalar(
            &output[index].second,
            second_matrix,
            &output[index].first
        );
    }
}
