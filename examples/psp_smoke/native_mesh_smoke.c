#include <n64psp/native_mesh.h>

static n64psp_mesh mesh;
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
int n64psp_psp_native_mesh_smoke(void) {
    const n64psp_mesh_command words[] = {
        { 0x04000c00, 1 }, { 0xbf000000, 0x00000204 }, { 0xb8000000, 0 }
    };
    if (!n64psp_mesh_build(&mesh, words, 3, 1, resolve, NULL) || !mesh.valid ||
        mesh.index_count != 3 || mesh.indices[2] != 2 || mesh.final_slots[2] != 2) return -1;
    if (n64psp_mesh_build(&mesh, words, 3, 0, resolve, NULL) || mesh.valid) return -1;
    return 0;
}
