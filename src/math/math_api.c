#include <n64psp/math.h>

#include "math_internal.h"

#ifndef N64PSP_USE_VFPU
#define N64PSP_USE_VFPU 0
#endif

void n64psp_mat4f_mul(
    n64psp_mat4f* out,
    const n64psp_mat4f* a,
    const n64psp_mat4f* b
) {
#if defined(__PSP__) && N64PSP_USE_VFPU
    n64psp_mat4f_mul_vfpu(out, a, b);
#else
    n64psp_mat4f_mul_scalar(out, a, b);
#endif
}

// matrix must not overlap out or vector
void n64psp_mat4f_transform_vec4(
    n64psp_vec4f* out,
    const n64psp_mat4f* matrix,
    const n64psp_vec4f* vector
) {
    // this establishes transform semantics before later batch VFPU interface is introduced
    n64psp_mat4f_transform_vec4_scalar(out, matrix, vector);
}