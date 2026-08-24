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

typedef struct n64psp_texcoord_s10_5 {
    int16_t s;
    int16_t t;
} n64psp_texcoord_s10_5;

typedef enum n64psp_texgen_mode {
    N64PSP_TEXGEN_SPHERICAL,
    N64PSP_TEXGEN_LINEAR
} n64psp_texgen_mode;

typedef struct N64PSP_ALIGN16 n64psp_tnl_matrices {
    n64psp_mat4f modelview;
    n64psp_mat4f projection;
} n64psp_tnl_matrices;

typedef struct n64psp_tnl_output_streams {
    void* view;
    void* clip;
    void* projected;
    void* lighting;
    void* clip_code;
    void* valid;
    size_t vertex_stride;
    size_t lighting_stride;
} n64psp_tnl_output_streams;

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

/*
 * Transforms packed vertices directly into strided renderer-owned streams
 *
 * View and clip contain four floats, projected contains three floats and
 * clip_code and valid contain one uint32_t for each vertex
 * Vertex streams and vertex_stride must be 4-byte aligned
 * Matrices must be 16-byte aligned
 */
void n64psp_tnl_transform_project_packed_batch(
    const n64psp_tnl_output_streams* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    int has_projection,
    size_t count
);

/*
 * Transforms and lights packed vertices directly into strided streams
 *
 * Lighting contains four floats and advances by lighting_stride
 * Lighting, lights, ambient and lighting_stride must be 16-byte aligned
 * The unlit alignment and packed-vertex requirements also apply
 */
void n64psp_tnl_transform_project_light_packed_batch(
    const n64psp_tnl_output_streams* output,
    const n64psp_tnl_matrices* matrices,
    const void* packed_vertices,
    const n64psp_directional_lightf* lights,
    const n64psp_vec4f* ambient,
    size_t light_count,
    int has_projection,
    size_t count
);

/* Generates N64 S10.5 texture coordinates from modelview-transformed normals
 * Spherical maps signed X/Y while linear maps acos X/Y over pi */
void n64psp_texgen_snorm8_batch(
    n64psp_texcoord_s10_5* output,
    const n64psp_mat4f* modelview,
    const void* packed_vertices,
    n64psp_texgen_mode mode,
    size_t count
);

#ifdef __cplusplus
}
#endif

#endif
