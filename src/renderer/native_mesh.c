#include <n64psp/native_mesh.h>
#include <string.h>

static n64psp_mesh_action* mesh_action(n64psp_mesh* mesh, unsigned kind) {
    n64psp_mesh_action* action;
    if (mesh->action_count == N64PSP_MESH_ACTION_LIMIT) return NULL;
    action = &mesh->actions[mesh->action_count++];
    memset(action, 0, sizeof(*action));
    action->kind = (uint8_t) kind;
    return action;
}

static int mesh_triangle(n64psp_mesh* mesh, uint32_t word) {
    n64psp_mesh_action* action;
    unsigned i;
    uint16_t indices[3];
    for (i = 0; i < 3; i++) {
        unsigned raw = (word >> (16 - 8 * i)) & 255;
        unsigned slot = raw / 2;
        if ((raw & 1) || slot >= N64PSP_MESH_SLOT_LIMIT || mesh->final_slots[slot] == N64PSP_MESH_EMPTY) return 0;
        indices[i] = mesh->final_slots[slot];
    }
    if (mesh->index_count > N64PSP_MESH_INDEX_LIMIT - 3) return 0;
    if (mesh->action_count && mesh->actions[mesh->action_count - 1].kind == N64PSP_MESH_SPAN) {
        action = &mesh->actions[mesh->action_count - 1];
    } else {
        action = mesh_action(mesh, N64PSP_MESH_SPAN);
        if (!action) return 0;
        action->first = mesh->index_count;
        mesh->span_count++;
    }
    memcpy(&mesh->indices[mesh->index_count], indices, sizeof(indices));
    mesh->index_count = (uint16_t) (mesh->index_count + 3);
    action->count = (uint16_t) (action->count + 3);
    return 1;
}

int n64psp_mesh_build(n64psp_mesh* mesh, const n64psp_mesh_command* commands, size_t count,
                     int commands_immutable, n64psp_mesh_resolve resolve, void* user) {
    size_t i;
    if (!mesh) return 0;
    mesh->valid = 0;
    mesh->vertex_count = mesh->index_count = mesh->action_count = mesh->span_count = 0;
    mesh->command_count = 0;
    memset(mesh->final_slots, 255, sizeof(mesh->final_slots));
    if (!commands || !resolve || !commands_immutable || !count || count > N64PSP_MESH_COMMAND_LIMIT) return 0;
    for (i = 0; i < count; i++) {
        uint32_t w0 = commands[i].w0, w1 = commands[i].w1;
        unsigned op = w0 >> 24;
        n64psp_mesh_action* action;
        if (op == 4) {
            unsigned n = (w0 >> 10) & 63, slot = (w0 >> 17) & 127, j;
            int immutable = 0;
            const void* source;
            if (!n || slot + n > N64PSP_MESH_SLOT_LIMIT || n > (unsigned) N64PSP_MESH_VERTEX_LIMIT - mesh->vertex_count) return 0;
            source = resolve(user, w1, n * sizeof(n64psp_mesh_vertex), &immutable);
            if (!source || !immutable) return 0;
            action = mesh_action(mesh, N64PSP_MESH_LOAD);
            if (!action) return 0;
            action->first = mesh->vertex_count;
            action->count = (uint16_t) n;
            action->slot = (uint8_t) slot;
            memcpy(&mesh->vertices[mesh->vertex_count], source, n * sizeof(n64psp_mesh_vertex));
            for (j = 0; j < n; j++) {
                unsigned version = mesh->vertex_count + j;
                mesh->vertex_slots[version] = (uint8_t) (slot + j);
                mesh->final_slots[slot + j] = (uint16_t) version;
            }
            mesh->vertex_count = (uint16_t) (mesh->vertex_count + n);
        } else if (op == 0xbf) {
            if (!mesh_triangle(mesh, w1)) return 0;
        } else if (op == 0xb1) {
            if (!mesh_triangle(mesh, w0) || !mesh_triangle(mesh, w1)) return 0;
        } else if (op == 0xfd || op == 0xf5 || op == 0xf2) {
            if ((op == 0xf5 || op == 0xf2) && ((w1 >> 24) & 7)) continue;
            action = mesh_action(mesh, N64PSP_MESH_MATERIAL);
            if (!action) return 0;
            action->w0 = w0;
            action->w1 = w1;
        } else if (op == 0xb8) {
            if (i + 1 != count || w0 != 0xb8000000 || w1) return 0;
            mesh->command_count = (uint16_t) count;
            mesh->valid = 1;
            return 1;
        } else if (op != 0xe6 && op != 0xe7 && op != 0xe8 && op != 0xf3) {
            return 0;
        }
    }
    return 0;
}

size_t n64psp_mesh_used_bytes(const n64psp_mesh* mesh) {
    if (!mesh || !mesh->valid) return 0;
    return mesh->vertex_count * (sizeof(n64psp_mesh_vertex) + sizeof(uint8_t)) +
           mesh->index_count * sizeof(uint16_t) + mesh->action_count * sizeof(n64psp_mesh_action) +
           sizeof(mesh->final_slots) + 11;
}
