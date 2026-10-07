#include <n64psp/native_mesh.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static n64psp_mesh mesh;
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
int main(void) {
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
