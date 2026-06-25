#include <n64psp/math.h>

#include "math_internal.h"

#ifndef N64PSP_USE_VFPU
#define N64PSP_USE_VFPU 0
#endif

void n64psp_sincosf(
    float radians,
    float* out_sine,
    float* out_cosine
) {
#if defined(__PSP__) && N64PSP_USE_VFPU
    n64psp_sincosf_vfpu(radians, out_sine, out_cosine);
#else
    n64psp_sincosf_scalar(radians, out_sine, out_cosine);
#endif
}

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

void n64psp_mat4f_transform_vec4_2mat_batch(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
) {
#if defined(__PSP__) && N64PSP_USE_VFPU
    n64psp_mat4f_transform_vec4_2mat_batch_vfpu(
        output,
        first_matrix,
        second_matrix,
        input,
        count
    );
#else
    n64psp_mat4f_transform_vec4_2mat_batch_scalar(
        output,
        first_matrix,
        second_matrix,
        input,
        count
    );
#endif
}

void n64psp_mat4f_transform_vec4_chain2_batch(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
) {
#if defined(__PSP__) && N64PSP_USE_VFPU
    n64psp_mat4f_transform_vec4_chain2_batch_vfpu(
        output,
        first_matrix,
        second_matrix,
        input,
        count
    );
#else
    n64psp_mat4f_transform_vec4_chain2_batch_scalar(
        output,
        first_matrix,
        second_matrix,
        input,
        count
    );
#endif
}

void n64psp_mat4f_transform_vec4_precompose_2mat_batch_experimental(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
) {
#if !(defined(__PSP__) && N64PSP_USE_VFPU)
    n64psp_mat4f composed;
#endif

    if (count == 0u) {
        return;
    }

    /*
     * Experimental Daedalus-inspired path:
     *
     *     composed = second_matrix * first_matrix
     *     first    = first_matrix * input
     *     second   = composed * input
     *
     * This is mathematically equivalent to a two-transform chain under ideal
     * arithmetic, but it intentionally changes floating-point evaluation order.
     */
#if defined(__PSP__) && N64PSP_USE_VFPU
    n64psp_mat4f_transform_vec4_precompose_2mat_batch_vfpu(
        output,
        first_matrix,
        second_matrix,
        input,
        count
    );
#else
    n64psp_mat4f_mul_scalar(&composed, second_matrix, first_matrix);
    n64psp_mat4f_transform_vec4_2mat_batch_scalar(
        output,
        first_matrix,
        &composed,
        input,
        count
    );
#endif
}

#if defined(__PSP__) && N64PSP_USE_VFPU
void n64psp_mat4f_transform_vec4_precompose_2mat_batch_vfpu(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
) {
    n64psp_mat4f composed;

    if (count == 0u) {
        return;
    }

    n64psp_mat4f_mul_vfpu(&composed, second_matrix, first_matrix);
    n64psp_mat4f_transform_vec4_2mat_batch_vfpu(
        output,
        first_matrix,
        &composed,
        input,
        count
    );
}

void n64psp_mat4f_transform_vec4_chain2_batch_vfpu(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
) {
    n64psp_mat4f_transform_vec4_chain2_batch_vfpu_baseline(
        output,
        first_matrix,
        second_matrix,
        input,
        count
    );
}
#endif
