#ifndef N64PSP_MATH_INTERNAL_H
#define N64PSP_MATH_INTERNAL_H

#include <n64psp/math.h>

void n64psp_mat4f_mul_scalar(
    n64psp_mat4f* out,
    const n64psp_mat4f* a,
    const n64psp_mat4f* b
);

void n64psp_mat4f_transform_vec4_scalar(
    n64psp_vec4f* out,
    const n64psp_mat4f* matrix,
    const n64psp_vec4f* vector
);

void n64psp_mat4f_transform_vec4_2mat_batch_scalar(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
);

void n64psp_mat4f_transform_vec4_chain2_batch_scalar(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
);

#if defined(__PSP__) && N64PSP_USE_VFPU
void n64psp_mat4f_mul_vfpu(
    n64psp_mat4f* out,
    const n64psp_mat4f* a,
    const n64psp_mat4f* b
);

void n64psp_mat4f_transform_vec4_2mat_batch_vfpu(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
);

void n64psp_mat4f_transform_vec4_chain2_batch_vfpu(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
);
#endif

#endif
