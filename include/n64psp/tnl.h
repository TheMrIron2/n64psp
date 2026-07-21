#ifndef N64PSP_TNL_H
#define N64PSP_TNL_H

#include <n64psp/lighting.h>
#include <n64psp/math.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct n64psp_packed_vertex {
    int16_t position[3];
    uint16_t flag;
    int16_t texcoord[2];
    int8_t attribute[3];
    uint8_t alpha;
} n64psp_packed_vertex;

typedef struct N64PSP_ALIGN16 n64psp_tnl_matrices {
    n64psp_mat4f modelview;
    n64psp_mat4f projection;
} n64psp_tnl_matrices;

/*
 * Transforms native-endian packed N64 vertices without an unpack pass
 *
 * packed_vertices uses contiguous 16-byte n64psp_packed_vertex records
 * output and matrices must be 16-byte aligned when count is non-zero
 */
void n64psp_tnl_transform_packed_batch(
    n64psp_vec4f_pair* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    size_t count
);

/*
 * Transforms and lights native-endian packed N64 vertices in one pass
 *
 * The lighting calculation matches n64psp_directional_light_snorm8_batch
 * using attribute XYZ as a signed-byte normal and ignoring alpha
 */
void n64psp_tnl_transform_light_packed_batch(
    n64psp_vec4f_pair* transform_output,
    n64psp_vec4f* lighting_output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    size_t count
);

#ifdef __cplusplus
}
#endif

#endif
