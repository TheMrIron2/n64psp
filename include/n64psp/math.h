#ifndef N64PSP_MATH_H
#define N64PSP_MATH_H

#if !defined(_SIZE_T) && \
    !defined(_SIZE_T_) && \
    !defined(_SIZE_T_DEF) && \
    !defined(_SIZE_T_DECLARED)
#include <stddef.h>
#endif
#include <n64psp/trig.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define N64PSP_ALIGN16 __attribute__((aligned(16)))
#else
#define N64PSP_ALIGN16
#endif

typedef struct N64PSP_ALIGN16 n64psp_vec4f {
    float x;
    float y;
    float z;
    float w;
} n64psp_vec4f;

typedef struct N64PSP_ALIGN16 n64psp_mat4f {
    float m[4][4];
} n64psp_mat4f;

typedef struct N64PSP_ALIGN16 n64psp_vec4f_pair {
    n64psp_vec4f first;
    n64psp_vec4f second;
} n64psp_vec4f_pair;

/*
 * Matrix storage convention
 * -------------------------
 *
 * The first index selects the input component/column.
 * The second index selects the output component/row.
 *
 * For v = { x, y, z, w }:
 *
 * result.x =
 *     matrix->m[0][0] * x +
 *     matrix->m[1][0] * y +
 *     matrix->m[2][0] * z +
 *     matrix->m[3][0] * w;
 *
 */

/*
 * Computes out = a * b.
 *
 * Applying out to a vector is equivalent to applying b first,
 * followed by a:
 *
 *     out(v) = a(b(v))
 *
 * Aliasing is supported:
 *
 *     out == a
 *     out == b
 *
 * Matrix objects must satisfy the alignment of n64psp_mat4f.
 */
void n64psp_mat4f_mul(
    n64psp_mat4f* out,
    const n64psp_mat4f* a,
    const n64psp_mat4f* b
);

void n64psp_mat4f_transform_vec4(
    n64psp_vec4f* out,
    const n64psp_mat4f* matrix,
    const n64psp_vec4f* vector
);

/*
 * Transforms each input vector independently through two matrices:
 *
 *     output[i].first  = first_matrix  * input[i]
 *     output[i].second = second_matrix * input[i]
 *
 * Both transformations consume the same original input vector.
 *
 * count == 0:
 *     Performs no memory access. All pointers may be NULL.
 *
 * count > 0:
 *     All pointers must be non-NULL.
 *     Matrices, input and output must be aligned to 16 bytes.
 *
 * Layout:
 *     Input elements are contiguous 16-byte n64psp_vec4f objects.
 *     Output elements are contiguous 32-byte n64psp_vec4f_pair objects.
 *
 * Overlap:
 *     Input and output ranges must not overlap.
 *     Matrix storage must not overlap input or output storage.
 *     first_matrix == second_matrix is permitted.
 *
 * Dispatch:
 *     Host builds use the scalar implementation.
 *     PSP builds with N64PSP_USE_VFPU=0 use the scalar implementation.
 *     PSP builds with N64PSP_USE_VFPU=1 use the VFPU implementation.
 *
 * Floating point:
 *     Scalar and VFPU results are semantically equivalent but are not
 *     required to be bit-identical.
 *
 * PSP callers selecting the VFPU path must execute on a thread with
 * PSP_THREAD_ATTR_VFPU.
 */
void n64psp_mat4f_transform_vec4_2mat_batch(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
);

/*
 * Transforms each input vector through a two-matrix chain:
 *
 *     output[i].first  = first_matrix  * input[i]
 *     output[i].second = second_matrix * output[i].first
 *
 * The first transformation consumes the original input vector. The second
 * transformation consumes the first transformed output, not the original
 * input. Matrices are not precomposed by this routine.
 *
 * count == 0:
 *     Performs no memory access. All pointers may be NULL.
 *
 * count > 0:
 *     All pointers must be non-NULL.
 *     Matrices, input and output must be aligned to 16 bytes.
 *
 * Layout:
 *     Input elements are contiguous 16-byte n64psp_vec4f objects.
 *     Output elements are contiguous 32-byte n64psp_vec4f_pair objects.
 *
 * Overlap:
 *     Input and output ranges must not overlap.
 *     Matrix storage must not overlap input or output storage.
 *     first_matrix == second_matrix is permitted.
 *
 * Dispatch:
 *     Host builds use the scalar implementation.
 *     PSP builds with N64PSP_USE_VFPU=0 use the scalar implementation.
 *     PSP builds with N64PSP_USE_VFPU=1 use the VFPU implementation.
 *
 * Floating point:
 *     The scalar implementation preserves the explicit two-transform
 *     arithmetic order. Scalar and VFPU results are semantically equivalent
 *     but are not required to be bit-identical.
 *
 * PSP callers selecting the VFPU path must execute on a thread with
 * PSP_THREAD_ATTR_VFPU.
 */
void n64psp_mat4f_transform_vec4_chain2_batch(
    n64psp_vec4f_pair* output,
    const n64psp_mat4f* first_matrix,
    const n64psp_mat4f* second_matrix,
    const n64psp_vec4f* input,
    size_t count
);

#ifdef __cplusplus
}
#endif

#endif
