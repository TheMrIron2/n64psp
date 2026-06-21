#ifndef N64PSP_LIGHTING_H
#define N64PSP_LIGHTING_H

#include <n64psp/math.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct N64PSP_ALIGN16 n64psp_directional_lightf {
    n64psp_vec4f direction;
    n64psp_vec4f color;
} n64psp_directional_lightf;

typedef struct n64psp_snorm8x4 {
    int8_t x;
    int8_t y;
    int8_t z;
    int8_t w;
} n64psp_snorm8x4;

/*
 * Batched directional lighting for signed-byte XYZ normals.
 *
 * Semantics:
 *     normal.xyz is converted directly from signed bytes, normal.w is ignored.
 *     normal_matrix's upper-left 3x3 transforms the normal.
 *     The transformed normal is normalized only when dot(n, n) > 0.000001f.
 *     RGB starts at ambient.xyz, then adds light.color.xyz * max(dot, 0)
 *     for every supplied light. Light directions are assumed caller-prepared.
 *     Accumulated RGB is not clamped. output_rgb[index].w is set to 0.0f.
 *
 * count == 0:
 *     Returns immediately and dereferences no pointers. All pointers may be
 *     NULL.
 *
 * count > 0:
 *     output_rgb, normal_matrix, normals and ambient must be non-NULL and
 *     16-byte aligned. normals points to packed four-byte records.
 *     lights must be non-NULL and 16-byte aligned when light_count > 0.
 *     lights may be NULL when light_count == 0.
 *
 * Overlap:
 *     Input and output ranges must not overlap.
 *
 * Dispatch:
 *     Host builds use the scalar implementation.
 *     PSP builds with N64PSP_USE_VFPU=0 use the scalar implementation.
 *     PSP builds with N64PSP_USE_VFPU=1 use the PSP implementation.
 *
 * Floating point:
 *     Scalar and PSP results are expected to match within small absolute and
 *     relative tolerances, but bit identity is not part of the ABI.
 */
void n64psp_directional_light_snorm8_batch(
    n64psp_vec4f* output_rgb,
    const n64psp_mat4f* normal_matrix,
    const n64psp_snorm8x4* normals,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    size_t count
);

#ifdef __cplusplus
}
#endif

#endif
