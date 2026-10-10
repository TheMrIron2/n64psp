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
typedef struct {
    n64psp_mesh_vertex* vertices;
    uint16_t* indices;
    n64psp_mesh_action* actions;
    uint16_t final_slots[N64PSP_MESH_SLOT_LIMIT];
    uint16_t vertex_count, index_count, action_count, command_count, span_count, stream_count, loaded_count;
    uint8_t valid;
} n64psp_mesh_packet;
typedef const void* (*n64psp_mesh_resolve)(void* user, uint32_t address, size_t bytes, int* immutable);

// Inputs use native byte order and the caller owns all output storage
// The resolver must qualify each vertex range independently of command storage
int n64psp_mesh_build(n64psp_mesh* mesh, const n64psp_mesh_command* commands, size_t count,
                     int commands_immutable, n64psp_mesh_resolve resolve, void* user);
size_t n64psp_mesh_used_bytes(const n64psp_mesh* mesh);
size_t n64psp_mesh_packet_bytes(const n64psp_mesh* mesh);
// Storage must be aligned to 16 bytes and remain live while the packet is used
int n64psp_mesh_packet_pack(n64psp_mesh_packet* packet, void* storage, size_t bytes, const n64psp_mesh* mesh);

typedef struct {
    uint32_t reads[2], writes[2];
    unsigned commands;
} n64psp_mesh_vertex_effect;
typedef int (*n64psp_mesh_effect_resolve)(void* user, const n64psp_mesh_command* command,
                                        n64psp_mesh_vertex_effect* effect);
int n64psp_mesh_command_effect(const n64psp_mesh_command* command, n64psp_mesh_vertex_effect* effect);
// Continuations run from the task root to the current caller and NULL means return
int n64psp_mesh_outputs_dead(const n64psp_mesh_command* const* continuations, unsigned depth,
    const uint32_t slots[2], unsigned budget, unsigned max_depth, unsigned probe_limit,
    n64psp_mesh_effect_resolve resolve, void* user);

#ifdef __cplusplus
}
#endif

#endif
