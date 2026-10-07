#ifndef N64PSP_NATIVE_MESH_H
#define N64PSP_NATIVE_MESH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define N64PSP_MESH_VERTEX_LIMIT 1024
#define N64PSP_MESH_INDEX_LIMIT 3072
#define N64PSP_MESH_ACTION_LIMIT 512
#define N64PSP_MESH_COMMAND_LIMIT 512
#define N64PSP_MESH_SLOT_LIMIT 64
#define N64PSP_MESH_EMPTY UINT16_MAX

typedef struct { uint32_t w0, w1; } n64psp_mesh_command;
typedef struct { int16_t position[3]; uint16_t flag; int16_t uv[2]; uint8_t color[4]; } n64psp_mesh_vertex;
enum { N64PSP_MESH_LOAD = 1, N64PSP_MESH_MATERIAL, N64PSP_MESH_SPAN };
typedef struct {
    uint32_t w0, w1;
    uint16_t first, count;
    uint8_t kind, slot;
    uint16_t reserved;
} n64psp_mesh_action;
typedef struct {
    n64psp_mesh_vertex vertices[N64PSP_MESH_VERTEX_LIMIT];
    uint16_t indices[N64PSP_MESH_INDEX_LIMIT];
    n64psp_mesh_action actions[N64PSP_MESH_ACTION_LIMIT];
    uint16_t final_slots[N64PSP_MESH_SLOT_LIMIT];
    uint8_t vertex_slots[N64PSP_MESH_VERTEX_LIMIT];
    uint16_t vertex_count, index_count, action_count, command_count, span_count;
    uint8_t valid;
} n64psp_mesh;
typedef const void* (*n64psp_mesh_resolve)(void* user, uint32_t address, size_t bytes, int* immutable);

// Inputs use native byte order and the caller owns all output storage
// The resolver must qualify each vertex range independently of command storage
int n64psp_mesh_build(n64psp_mesh* mesh, const n64psp_mesh_command* commands, size_t count,
                     int commands_immutable, n64psp_mesh_resolve resolve, void* user);
size_t n64psp_mesh_used_bytes(const n64psp_mesh* mesh);

#ifdef __cplusplus
}
#endif

#endif
