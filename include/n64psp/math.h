#ifndef N64PSP_MATH_H
#define N64PSP_MATH_H

#include <stddef.h>

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

#ifdef __cplusplus
}
#endif

#endif