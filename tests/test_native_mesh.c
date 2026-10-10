#include <n64psp/native_mesh.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static n64psp_mesh mesh;
static n64psp_mesh_packet packet;
static unsigned char packed[32768] __attribute__((aligned(16)));
static n64psp_mesh_vertex vertices[64];
static int payload_immutable = 1;
static const void* resolve(void* user, uint32_t raw, size_t bytes, int* immutable) {
    (void) user;
    if (raw != 1 || bytes > sizeof(vertices)) return NULL;
    *immutable = payload_immutable;
    return vertices;
}
static int build(const n64psp_mesh_command* words, size_t count) {
    return n64psp_mesh_build(&mesh, words, count, 1, resolve, NULL);
}
static int effect_resolve(void* user, const n64psp_mesh_command* command, n64psp_mesh_vertex_effect* effect) {
    (void) user;
    if (command->w0 >> 24 == 6) {
        *effect = (n64psp_mesh_vertex_effect) { {0,0}, {command->w1 == 1 ? 7u : 1u,0}, 3 };
        return command->w1 == 1 || command->w1 == 2;
    }
    if (command->w0 >> 24 == 4 && command->w1 != 1) return 0;
    return n64psp_mesh_command_effect(command, effect);
}
static void test_continuations(void) {
    const uint32_t slots[2] = {7,0};
    const n64psp_mesh_command overwrite[] = {{0x04000c00,1},{0xbf000000,0x00000204},{0xb8000000,0}};
    const n64psp_mesh_command read[] = {{0xbf000000,0x00000204},{0xb8000000,0}};
    const n64psp_mesh_command end[] = {{0xb8000000,0}};
    const n64psp_mesh_command child[] = {{0x06000000,1},{0xb8000000,0}};
    const n64psp_mesh_command unknown[] = {{0x06000000,3},{0xb8000000,0}};
    const n64psp_mesh_command opaque[] = {{0xbe000000,0},{0xb8000000,0}};
    const n64psp_mesh_command partial[] = {{0x04000400,1},{0xbf000000,0x00000204},{0xb8000000,0}};
    const n64psp_mesh_command nopush[] = {{0x06010000,2},{0x04000c00,1},{0xb8000000,0}};
    const n64psp_mesh_command* continuations[2] = {overwrite,end};
    n64psp_mesh_vertex_effect effect;
    assert(n64psp_mesh_outputs_dead(continuations,0,slots,100,4,32,effect_resolve,NULL));
    continuations[0]=read;
    assert(!n64psp_mesh_outputs_dead(continuations,0,slots,100,4,32,effect_resolve,NULL));
    assert(!n64psp_mesh_outputs_dead(continuations,1,slots,100,4,32,effect_resolve,NULL));
    continuations[0]=end;
    assert(n64psp_mesh_outputs_dead(continuations,1,slots,100,4,32,effect_resolve,NULL));
    continuations[0]=child;
    assert(n64psp_mesh_outputs_dead(continuations,0,slots,4,4,32,effect_resolve,NULL));
    assert(!n64psp_mesh_outputs_dead(continuations,0,slots,3,4,32,effect_resolve,NULL));
    assert(!n64psp_mesh_outputs_dead(continuations,0,slots,4,1,32,effect_resolve,NULL));
    continuations[0]=unknown;
    assert(!n64psp_mesh_outputs_dead(continuations,0,slots,100,4,32,effect_resolve,NULL));
    continuations[0]=opaque;
    assert(!n64psp_mesh_outputs_dead(continuations,0,slots,100,4,32,effect_resolve,NULL));
    continuations[0]=partial;
    assert(!n64psp_mesh_outputs_dead(continuations,0,slots,100,4,32,effect_resolve,NULL));
    continuations[0]=overwrite;
    assert(!n64psp_mesh_outputs_dead(continuations,0,slots,0,4,32,effect_resolve,NULL));
    assert(!n64psp_mesh_outputs_dead(continuations,0,slots,100,4,0,effect_resolve,NULL));
    continuations[0]=read;continuations[1]=nopush;
    assert(!n64psp_mesh_outputs_dead(continuations,1,slots,100,4,32,effect_resolve,NULL));
    assert(n64psp_mesh_command_effect(&overwrite[0],&effect) && effect.writes[0]==7);
    assert(!n64psp_mesh_command_effect(NULL,&effect));
    assert(!n64psp_mesh_outputs_dead(continuations,0,NULL,100,4,32,effect_resolve,NULL));
}
int main(void) {
    test_continuations();
    n64psp_mesh_command words[] = {
        { 0x04000c00, 1 }, { 0xbf000000, 0x00000204 },
        { 0x04000c00, 1 }, { 0xbf000000, 0x00000204 }, { 0xb8000000, 0 }
    };
    n64psp_mesh_command oversized[N64PSP_MESH_COMMAND_LIMIT + 1];
    n64psp_mesh_command changed[5];
    unsigned i;
    vertices[0].position[0] = 123;
    assert(build(words, 5) && mesh.valid);
    assert(mesh.vertex_count == 6 && mesh.index_count == 6 && mesh.span_count == 2);
    assert(mesh.indices[0] == 0 && mesh.indices[3] == 3 && mesh.final_slots[0] == 3);
    assert(n64psp_mesh_packet_bytes(&mesh) == 128);
    assert(n64psp_mesh_packet_pack(&packet, packed, sizeof(packed), &mesh));
    assert(packet.valid && packet.span_count == 1 && packet.action_count == 1 && packet.stream_count == 6);
    assert(packet.indices[0] == 0 && packet.indices[3] == 3 && packet.final_slots[0] == 3);
    assert(packet.actions[0].count == 6 && packet.actions[0].w0 == 0 && packet.actions[0].w1 == 6);
    assert(!n64psp_mesh_packet_pack(&packet, packed + 1, sizeof(packed) - 1, &mesh) && !packet.valid);
    assert(!n64psp_mesh_packet_pack(&packet, packed, 127, &mesh));
    assert(!n64psp_mesh_packet_pack(NULL, packed, sizeof(packed), &mesh));
    assert(!n64psp_mesh_packet_bytes(NULL));
    {
        n64psp_mesh_command sparse[] = {
            { 0x04000c00, 1 }, { 0xbf000000, 0x00000204 },
            { 0x04000c00, 1 }, { 0x04000c00, 1 },
            { 0xbf000000, 0x00000204 }, { 0xb8000000, 0 }
        };
        assert(build(sparse, 6));
        assert(n64psp_mesh_packet_pack(&packet, packed, sizeof(packed), &mesh));
        assert(packet.loaded_count == 9 && packet.stream_count == 6 && packet.vertex_count == 6);
        assert(packet.indices[3] == 3 && packet.final_slots[0] == 3);
        sparse[0].w0 = 0x04001000;
        sparse[2].w0 = 0xb8000000; sparse[2].w1 = 0;
        assert(build(sparse, 3));
        assert(n64psp_mesh_packet_pack(&packet, packed, sizeof(packed), &mesh));
        assert(packet.stream_count == 3 && packet.vertex_count == 4 && packet.final_slots[3] == 3);
    }
    {
        n64psp_mesh_command material[] = {
            { 0x04000c00, 1 }, { 0xbf000000, 0x00000204 },
            { 0xfd100000, 2 }, { 0x04000c00, 1 }, { 0xbf000000, 0x00000204 }, { 0xb8000000, 0 }
        };
        assert(build(material, 6));
        assert(n64psp_mesh_packet_pack(&packet, packed, sizeof(packed), &mesh));
        assert(packet.span_count == 2 && packet.action_count == 3);
        assert(packet.actions[1].kind == N64PSP_MESH_MATERIAL && packet.actions[1].w1 == 2);
        assert(packet.actions[2].w0 == 3 && packet.actions[2].reserved == 3 && packet.indices[3] == 0);
    }
    for (i = 0; i < 22; i++) {
        oversized[2*i] = words[0];
        oversized[2*i+1] = words[1];
    }
    oversized[44] = words[4];
    assert(build(oversized, 45));
    assert(n64psp_mesh_packet_pack(&packet, packed, sizeof(packed), &mesh));
    assert(packet.span_count == 1 && packet.stream_count == 66 && packet.actions[0].w1 == 66);
    assert(packet.indices[65] == 65 && packet.final_slots[2] == 65);
    mesh.indices[0] = UINT16_MAX;
    assert(!n64psp_mesh_packet_pack(&packet, packed, sizeof(packed), &mesh) && !packet.valid);
    assert(build(words, 5));
    vertices[0].position[0] = 456;
    assert(mesh.vertices[0].position[0] == 123 && mesh.vertices[3].position[0] == 123);
    payload_immutable = 0;
    assert(!build(words, 5) && !mesh.valid);
    payload_immutable = 1;
    assert(!n64psp_mesh_build(&mesh, words, 5, 0, resolve, NULL) && !mesh.valid);
    assert(!n64psp_mesh_build(&mesh, words, 5, 1, NULL, NULL));
    assert(!n64psp_mesh_build(&mesh, NULL, 5, 1, resolve, NULL));
    assert(!n64psp_mesh_build(NULL, words, 5, 1, resolve, NULL));
    assert(!build(words, 0) && !build(words, 4));
    assert(!build(words, 1) && !build(words + 1, 4));
    for (i = 0; i < 7; i++) {
        memcpy(changed, words, sizeof(words));
        switch (i) {
            case 0: changed[0].w0 = 0x04000000; break;
            case 1: changed[0].w0 |= 63 << 17; break;
            case 2: changed[0].w1 = 2; break;
            case 3: changed[1].w1 = 0x00010204; break;
            case 4: changed[1].w1 = 0x007e0204; break;
            case 5: changed[4].w1 = 1; break;
            case 6: changed[1].w0 = 0x06000000; break;
        }
        assert(!build(changed, 5) && !mesh.valid && !n64psp_mesh_used_bytes(&mesh));
    }
    memset(oversized, 0, sizeof(oversized));
    assert(!build(oversized, N64PSP_MESH_COMMAND_LIMIT + 1));
    for (i = 0; i < 17; i++) {
        oversized[i].w0 = 0x04000000 | (63 << 10);
        oversized[i].w1 = 1;
    }
    oversized[17].w0 = 0xb8000000;
    assert(!build(oversized, 18) && !mesh.valid);
    printf("retained mesh bounds, immutability, copied payload and slot-version checks passed\n");
    return 0;
}
