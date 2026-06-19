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

#endif