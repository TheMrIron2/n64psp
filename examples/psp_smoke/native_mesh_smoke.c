#include <n64psp/native_mesh.h>
#include <n64psp/math.h>

static n64psp_mesh mesh;
static n64psp_mesh_packet packet;
static unsigned char packed[256] __attribute__((aligned(16)));
static const n64psp_mesh_vertex vertices[3] = {
    { { -1, -1, 0 }, 0, { 0, 0 }, { 255, 0, 0, 255 } },
    { { 1, -1, 0 }, 0, { 0, 0 }, { 0, 255, 0, 255 } },
    { { 0, 1, 0 }, 0, { 0, 0 }, { 0, 0, 255, 255 } }
};
static const void* resolve(void* user, uint32_t raw, size_t bytes, int* immutable) {
    (void) user;
    if (raw != 1 || bytes > sizeof(vertices)) return NULL;
    *immutable = 1;
    return vertices;
}
static int effect(void* user, const n64psp_mesh_command* command, n64psp_mesh_vertex_effect* output) {
    (void) user;
    return n64psp_mesh_command_effect(command, output);
}
int n64psp_psp_native_mesh_smoke(void) {
    {
        const float bounds[2][3]={{-1,-1,-1},{1,1,1}};
        n64psp_mat4f model={.m={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}};
        n64psp_mat4f projection={.m={{0.25f,0,0,0},{0,0.25f,0,0},{0,0,0.25f,0},{0,0,0,1}}};
        if (!n64psp_aabb_inside_clip(bounds,&model,&projection,1,0.001f,0.0005f) ||
            !n64psp_aabb_inside_clip_affine(bounds,&model,&projection,1,0.001f,0.0005f) ||
            n64psp_aabb_inside_clip(bounds,&model,&projection,3,0.001f,0.0005f) ||
            n64psp_aabb_inside_clip_affine(bounds,&model,&projection,3,0.001f,0.0005f)) return -1;
    }
    const n64psp_mesh_command words[] = {
        { 0x04000c00, 1 }, { 0xbf000000, 0x00000204 }, { 0xb8000000, 0 }
    };
    if (!n64psp_mesh_build(&mesh, words, 3, 1, resolve, NULL) || !mesh.valid ||
        mesh.index_count != 3 || mesh.indices[2] != 2 || mesh.final_slots[2] != 2) return -1;
    if (!n64psp_mesh_packet_pack(&packet, packed, sizeof(packed), &mesh) ||
        packet.span_count != 1 || packet.stream_count != 3 || packet.indices[2] != 2) return -1;
    {
        n64psp_mesh_span spans[2];
        if (n64psp_mesh_compile_spans(&packet, spans, 2) != 2 ||
            spans[0].count != 3 || spans[0].vertex_count != 3 || spans[1].count) return -1;
    }
    {
        const n64psp_mesh_command* continuation[] = { words };
        const uint32_t slots[] = {7,0};
        if (!n64psp_mesh_outputs_dead(continuation,0,slots,10,4,32,effect,NULL)) return -1;
        continuation[0] = words + 1;
        if (n64psp_mesh_outputs_dead(continuation,0,slots,10,4,32,effect,NULL)) return -1;
    }
    if (n64psp_mesh_build(&mesh, words, 3, 0, resolve, NULL) || mesh.valid) return -1;
    return 0;
}
