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

static size_t mesh_align(size_t bytes) {
    return (bytes + 15) & ~(size_t) 15;
}

static int mesh_packet_layout(const n64psp_mesh* mesh, unsigned* vertices, unsigned* actions) {
    uint8_t seen[N64PSP_MESH_VERTEX_LIMIT] = {0};
    uint8_t used[N64PSP_MESH_VERTEX_LIMIT] = {0};
    unsigned i, indices = 0, span = 0;
    *vertices = *actions = 0;
    if (!mesh || !mesh->valid || !mesh->index_count || mesh->vertex_count > N64PSP_MESH_VERTEX_LIMIT ||
        mesh->index_count > N64PSP_MESH_INDEX_LIMIT || mesh->action_count > N64PSP_MESH_ACTION_LIMIT) return 0;
    for (i = 0; i < mesh->action_count; i++) {
        const n64psp_mesh_action* action = &mesh->actions[i];
        if (action->kind == N64PSP_MESH_MATERIAL) {
            (*actions)++;
            span = 0;
        } else if (action->kind == N64PSP_MESH_SPAN) {
            unsigned j;
            if (!action->count || action->count % 3 || action->first != indices ||
                action->first + action->count > mesh->index_count) return 0;
            if (!span) {
                (*actions)++;
                memset(seen, 0, sizeof(seen));
                span = 1;
            }
            for (j = 0; j < action->count; j++) {
                unsigned version = mesh->indices[action->first + j];
                if (version >= mesh->vertex_count) return 0;
                if (!seen[version]) { (*vertices)++; seen[version] = 1; }
                used[version] = 1;
            }
            indices += action->count;
        } else if (action->kind != N64PSP_MESH_LOAD) return 0;
    }
    if (indices != mesh->index_count) return 0;
    for (i = 0; i < N64PSP_MESH_SLOT_LIMIT; i++) {
        unsigned version = mesh->final_slots[i];
        if (version == N64PSP_MESH_EMPTY) continue;
        if (version >= mesh->vertex_count) return 0;
        if (!used[version]) { (*vertices)++; used[version] = 1; }
    }
    return 1;
}

size_t n64psp_mesh_packet_bytes(const n64psp_mesh* mesh) {
    unsigned vertices, actions;
    if (!mesh_packet_layout(mesh, &vertices, &actions)) return 0;
    return mesh_align(vertices * sizeof(n64psp_mesh_vertex)) +
           mesh_align(mesh->index_count * sizeof(uint16_t)) + actions * sizeof(n64psp_mesh_action);
}

int n64psp_mesh_packet_pack(n64psp_mesh_packet* packet, void* storage, size_t bytes, const n64psp_mesh* mesh) {
    uint16_t local[N64PSP_MESH_VERTEX_LIMIT], copies[N64PSP_MESH_VERTEX_LIMIT];
    unsigned i, vertices, actions;
    size_t required;
    uint8_t* cursor = storage;
    if (!packet) return 0;
    packet->valid = 0;
    if (!mesh_packet_layout(mesh, &vertices, &actions)) return 0;
    required = mesh_align(vertices * sizeof(n64psp_mesh_vertex)) +
               mesh_align(mesh->index_count * sizeof(uint16_t)) + actions * sizeof(n64psp_mesh_action);
    if (!storage || ((uintptr_t) storage & 15) || bytes < required) return 0;
    memset(local, 255, sizeof(local));
    memset(copies, 255, sizeof(copies));
    packet->vertex_count = packet->stream_count = packet->action_count = packet->span_count = 0;
    packet->loaded_count = mesh->vertex_count;
    packet->index_count = mesh->index_count;
    packet->command_count = mesh->command_count;
    packet->vertices = (n64psp_mesh_vertex*) cursor;
    cursor += mesh_align(vertices * sizeof(n64psp_mesh_vertex));
    packet->indices = (uint16_t*) cursor;
    cursor += mesh_align(mesh->index_count * sizeof(uint16_t));
    packet->actions = (n64psp_mesh_action*) cursor;
    for (i = 0; i < mesh->action_count; i++) {
        const n64psp_mesh_action* input = &mesh->actions[i];
        n64psp_mesh_action* action;
        unsigned j;
        if (input->kind == N64PSP_MESH_LOAD) continue;
        if (input->kind == N64PSP_MESH_MATERIAL) {
            packet->actions[packet->action_count++] = *input;
            continue;
        }
        if (packet->action_count && packet->actions[packet->action_count - 1].kind == N64PSP_MESH_SPAN) {
            action = &packet->actions[packet->action_count - 1];
        } else {
            action = &packet->actions[packet->action_count++];
            *action = *input;
            action->count = 0;
            action->w0 = packet->stream_count;
            action->w1 = 0;
            action->reserved = packet->stream_count;
            packet->span_count++;
            memset(local, 255, sizeof(local));
        }
        for (j = 0; j < input->count; j++) {
            unsigned version = mesh->indices[input->first + j];
            if (local[version] == N64PSP_MESH_EMPTY) {
                local[version] = (uint16_t) action->w1++;
                copies[version] = packet->vertex_count;
                packet->vertices[packet->vertex_count++] = mesh->vertices[version];
                packet->stream_count++;
            }
            packet->indices[input->first + j] = local[version];
        }
        action->count = (uint16_t) (action->count + input->count);
    }
    for (i = 0; i < N64PSP_MESH_SLOT_LIMIT; i++) {
        unsigned version = mesh->final_slots[i];
        packet->final_slots[i] = N64PSP_MESH_EMPTY;
        if (version == N64PSP_MESH_EMPTY) continue;
        if (copies[version] == N64PSP_MESH_EMPTY) {
            copies[version] = packet->vertex_count;
            packet->vertices[packet->vertex_count++] = mesh->vertices[version];
        }
        packet->final_slots[i] = copies[version];
    }
    if (packet->vertex_count != vertices || packet->action_count != actions) return 0;
    packet->valid = 1;
    return 1;
}

static int mesh_triangle_reads(uint32_t word, uint32_t slots[2]) {
    unsigned shift;
    for (shift = 0; shift < 24; shift += 8) {
        unsigned raw = (word >> shift) & 255, slot = raw / 2;
        if ((raw & 1) || slot >= 64) return 0;
        slots[slot / 32] |= 1u << (slot % 32);
    }
    return 1;
}

int n64psp_mesh_command_effect(const n64psp_mesh_command* command, n64psp_mesh_vertex_effect* effect) {
    unsigned op;
    if (!command || !effect) return 0;
    op = command->w0 >> 24;
    memset(effect, 0, sizeof(*effect));
    if (op == 4) {
        unsigned count = (command->w0 >> 10) & 63, slot = (command->w0 >> 17) & 127, i;
        if (!count || slot + count > 64) return 0;
        for (i = slot; i < slot + count; i++) effect->writes[i / 32] |= 1u << (i % 32);
        return 1;
    }
    if (op == 0xbf) return mesh_triangle_reads(command->w1, effect->reads);
    if (op == 0xb1) return mesh_triangle_reads(command->w0, effect->reads) && mesh_triangle_reads(command->w1, effect->reads);
    if (op == 0xbc && ((command->w0 >> 16) & 255) == 0x0c) return 0;
    return op == 0 || op == 1 || op == 3 || op == 0xb3 || op == 0xb4 ||
           (op >= 0xb6 && op <= 0xbd && op != 0xb8) || op == 0xe4 || op == 0xe5 ||
           (op >= 0xe6 && op != 0xf1);
}

int n64psp_mesh_outputs_dead(const n64psp_mesh_command* const* continuations, unsigned depth,
    const uint32_t slots[2], unsigned budget, unsigned max_depth, unsigned probe_limit,
    n64psp_mesh_effect_resolve resolve, void* user) {
    uint32_t remaining[2];
    const n64psp_mesh_command* cursor;
    unsigned probes = 0;
    if (!slots) return 0;
    remaining[0] = slots[0]; remaining[1] = slots[1];
    if (!remaining[0] && !remaining[1]) return 1;
    if (!continuations || !resolve || depth >= max_depth) return 0;
    cursor = continuations[depth];
    for (;;) {
        n64psp_mesh_vertex_effect effect;
        unsigned op;
        if (!cursor) {
            if (!depth) return 1;
            cursor = continuations[--depth];
            continue;
        }
        if (!budget || probes++ >= probe_limit) return 0;
        budget--;
        op = cursor->w0 >> 24;
        if (op == 0xb8) {
            cursor = NULL;
            continue;
        }
        if (op == 6 && depth + 1 >= max_depth) return 0;
        if (!resolve(user, cursor, &effect) || effect.commands > budget) return 0;
        budget -= effect.commands;
        if ((remaining[0] & effect.reads[0]) || (remaining[1] & effect.reads[1])) return 0;
        remaining[0] &= ~effect.writes[0];
        remaining[1] &= ~effect.writes[1];
        if (!remaining[0] && !remaining[1]) return 1;
        if (op == 6 && ((cursor->w0 >> 16) & 255) == 1) cursor = NULL;
        else cursor++;
    }
}
