#include "mutation_internal.h"
#include "portal_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../collision/blocks_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"

#include <stdlib.h>

typedef union portal_scratch {
    mc_world_collision_error value;
    mc_world_mutation_error mutation;
} portal_scratch;

typedef struct portal_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    const char *method;
    portal_scratch *scratch;
} portal_frame;

static void portal_at(const portal_frame *f, unsigned int pc) {
    f->error->hook_class = "ak";
    f->error->hook_method = f->method;
    f->error->hook_bytecode = pc;
}

static bool portal_done(const portal_frame *f) {
    f->error->hook_class = f->error->hook_method = NULL;
    f->error->hook_bytecode = 0;
    return true;
}

static bool portal_fail(const portal_frame *f, unsigned int pc, const char *operation) {
    portal_at(f, pc);
    return mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  operation);
}

static bool portal_entry(const portal_frame *f) {
    if (f->error == NULL)
        return false;
    if (f->block == NULL || f->block->classes == NULL || f->block->kind != MC_BLOCK_PORTAL ||
        f->block->classes->options.side != MC_REGISTRY_CLIENT)
        return portal_fail(f, 0, "Portal actual client concrete receiver");
    return true;
}

/* Diagnostic storage is lazy and per invocation, so synchronous collapse does
 * not keep a large by-value mutation graph on each native stack frame. */
static portal_scratch *portal_workspace(portal_frame *f, unsigned int pc) {
    portal_at(f, pc);
    if (f->scratch == NULL)
        f->scratch = malloc(sizeof(*f->scratch));
    if (f->scratch == NULL)
        (void)mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                     "Portal reached child diagnostic allocation");
    return f->scratch;
}

static bool portal_get(portal_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                       int32_t *out) {
    portal_at(f, pc);
    if (f->world == NULL)
        return portal_fail(f, pc, "Portal actual World block-read receiver");
    portal_scratch *scratch = portal_workspace(f, pc);
    if (scratch == NULL)
        return false;
    mc_world_collision_error *child = &scratch->value;
    mc_world_collision_error_init(child);
    if (mc_world_collision_block_value_held(f->world, x, y, z, false, out, child) ==
        MC_WORLD_STORAGE_OK)
        return true;
    const mc_world_nearby_error *lower = &child->nearby;
    f->error->nearby = *lower;
    f->error->query = lower->query;
    f->error->population = lower->query.population;
    f->error->entity = lower->entity;
    f->error->tile_entity = lower->tile_entity;
    f->error->batch = lower->batch;
    f->error->load_entity = lower->load_entity;
    f->error->activation_bytecode = lower->activation_bytecode;
    f->error->activation_world_bytecode = lower->activation_world_bytecode;
    f->error->load_bytecode = lower->load_bytecode;
    f->error->load_builder_bytecode = lower->load_builder_bytecode;
    f->error->registry = lower->query.registry;
    f->error->cause = lower->query.cause;
    f->error->status = lower->query.cause.status;
    f->error->operation = lower->query.cause.operation;
    f->world->last_error = lower->query.cause;
    return false;
}

static bool portal_static_id(const portal_frame *f, const char *name, unsigned int load_pc,
                             unsigned int field_pc, int32_t *out) {
    const mc_block *selected = NULL;
    portal_at(f, load_pc);
    if (mc_registry_block_static_get(f->block->classes, name, &selected, &f->error->registry) !=
        MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(f->world, f->error, &f->error->registry,
                                               "Portal live Block static");
    portal_at(f, field_pc);
    if (selected == NULL)
        return portal_fail(f, field_pc, "Portal live Block static field receiver");
    *out = selected->fields.bn;
    return true;
}

static bool portal_static_matches(const portal_frame *f, int32_t actual, const char *name,
                                  unsigned int load_pc, unsigned int field_pc, bool *out) {
    int32_t expected = 0;
    if (!portal_static_id(f, name, load_pc, field_pc, &expected))
        return false;
    *out = actual == expected;
    return true;
}

static bool portal_set(portal_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                       int32_t id) {
    portal_scratch *scratch = portal_workspace(f, pc);
    if (scratch == NULL)
        return false;
    mc_world_mutation_error *child = &scratch->mutation;
    mc_world_mutation_error_init(child);
    bool ignored;
    if (mc_world_runtime_block_set(f->world, x, y, z, id, &ignored, child) == MC_WORLD_STORAGE_OK)
        return true;
    *f->error = *child;
    if (f->error->cause_hook_class == NULL) {
        f->error->cause_hook_class = f->error->hook_class;
        f->error->cause_hook_method = f->error->hook_method;
        f->error->cause_hook_bytecode = f->error->hook_bytecode;
    }
    portal_at(f, pc);
    return false;
}

/* AK.a_: exactly one horizontal axis, the asymmetric air-only anchor shift,
 * then the 4x5 frame scan with the four corners omitted. */
static bool portal_create(portal_frame *f, int32_t x, int32_t y, int32_t z, bool *out) {
    int32_t id;
    bool matches;
    if (!portal_get(f, 13, mc_java_i32_sub(x, 1), y, z, &id) ||
        !portal_static_matches(f, id, "aq", 16, 19, &matches))
        return false;
    if (!matches && (!portal_get(f, 32, mc_java_i32_add(x, 1), y, z, &id) ||
                     !portal_static_matches(f, id, "aq", 35, 38, &matches)))
        return false;
    const int32_t dx = matches ? 1 : 0;
    if (!portal_get(f, 54, x, y, mc_java_i32_sub(z, 1), &id) ||
        !portal_static_matches(f, id, "aq", 57, 60, &matches))
        return false;
    if (!matches && (!portal_get(f, 76, x, y, mc_java_i32_add(z, 1), &id) ||
                     !portal_static_matches(f, id, "aq", 79, 82, &matches)))
        return false;
    const int32_t dz = matches ? 1 : 0;
    if (dx == dz) {
        *out = false;
        return portal_done(f);
    }
    if (!portal_get(f, 108, mc_java_i32_sub(x, dx), y, mc_java_i32_sub(z, dz), &id))
        return false;
    if (id == 0) {
        x = mc_java_i32_sub(x, dx);
        z = mc_java_i32_sub(z, dz);
    }
    for (int32_t u = -1; u <= 2; ++u) {
        for (int32_t v = -1; v <= 3; ++v) {
            const bool side = u == -1 || u == 2;
            const bool cap = v == -1 || v == 3;
            if (side && cap)
                continue;
            const int32_t px = mc_java_i32_add(x, mc_java_i32_mul(dx, u));
            const int32_t pz = mc_java_i32_add(z, mc_java_i32_mul(dz, u));
            if (!portal_get(f, 222, px, mc_java_i32_add(y, v), pz, &id))
                return false;
            if (side || cap) {
                if (!portal_static_matches(f, id, "aq", 234, 237, &matches))
                    return false;
            } else if (id == 0)
                matches = true;
            else if (!portal_static_matches(f, id, "as", 252, 255, &matches))
                return false;
            if (!matches) {
                *out = false;
                return portal_done(f);
            }
        }
    }
    portal_at(f, 277);
    f->world->fields.o = true;
    for (int32_t u = 0; u < 2; ++u) {
        for (int32_t v = 0; v < 3; ++v) {
            int32_t portal_id = 0;
            if (!portal_static_id(f, "bf", 318, 321, &portal_id) ||
                !portal_set(f, 324, mc_java_i32_add(x, mc_java_i32_mul(dx, u)),
                            mc_java_i32_add(y, v), mc_java_i32_add(z, mc_java_i32_mul(dz, u)),
                            portal_id))
                return false;
        }
    }
    portal_at(f, 342);
    f->world->fields.o = false;
    *out = true;
    return portal_done(f);
}

static bool portal_remove(portal_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z) {
    return portal_set(f, pc, x, y, z, 0) && portal_done(f);
}

/* AK.b never uses source_id. A failed column/side check removes only this
 * reached cell. Its notifying setter performs the real recursive collapse. */
static bool portal_neighbor(portal_frame *f, int32_t x, int32_t y, int32_t z) {
    int32_t id;
    if (!portal_get(f, 13, mc_java_i32_sub(x, 1), y, z, &id))
        return false;
    bool along_x = id == f->block->fields.bn;
    if (!along_x) {
        if (!portal_get(f, 30, mc_java_i32_add(x, 1), y, z, &id))
            return false;
        along_x = id == f->block->fields.bn;
    }
    const int32_t dx = along_x ? 1 : 0, dz = along_x ? 0 : 1;
    int32_t base_y = y;
    for (;;) {
        if (!portal_get(f, 57, x, mc_java_i32_sub(base_y, 1), z, &id))
            return false;
        if (id != f->block->fields.bn)
            break;
        base_y = mc_java_i32_sub(base_y, 1);
    }
    bool matches;
    if (!portal_get(f, 81, x, mc_java_i32_sub(base_y, 1), z, &id) ||
        !portal_static_matches(f, id, "aq", 84, 87, &matches))
        return false;
    if (!matches)
        return portal_remove(f, 99, x, y, z);
    int32_t height = 1;
    while (height < 4) {
        if (!portal_get(f, 122, x, mc_java_i32_add(base_y, height), z, &id))
            return false;
        if (id != f->block->fields.bn)
            break;
        ++height;
    }
    if (height == 3) {
        if (!portal_get(f, 153, x, mc_java_i32_add(base_y, height), z, &id) ||
            !portal_static_matches(f, id, "aq", 156, 159, &matches))
            return false;
    } else
        matches = false;
    if (!matches)
        return portal_remove(f, 171, x, y, z);
    if (!portal_get(f, 183, mc_java_i32_sub(x, 1), y, z, &id))
        return false;
    bool has_x = id == f->block->fields.bn;
    if (!has_x) {
        if (!portal_get(f, 200, mc_java_i32_add(x, 1), y, z, &id))
            return false;
        has_x = id == f->block->fields.bn;
    }
    if (!portal_get(f, 224, x, y, mc_java_i32_sub(z, 1), &id))
        return false;
    bool has_z = id == f->block->fields.bn;
    if (!has_z) {
        if (!portal_get(f, 241, x, y, mc_java_i32_add(z, 1), &id))
            return false;
        has_z = id == f->block->fields.bn;
    }
    if (has_x && has_z)
        return portal_remove(f, 274, x, y, z);
    if (!portal_get(f, 290, mc_java_i32_add(x, dx), y, mc_java_i32_add(z, dz), &id) ||
        !portal_static_matches(f, id, "aq", 293, 296, &matches))
        return false;
    bool valid = false;
    if (matches) {
        if (!portal_get(f, 313, mc_java_i32_sub(x, dx), y, mc_java_i32_sub(z, dz), &id))
            return false;
        valid = id == f->block->fields.bn;
    }
    if (!valid) {
        if (!portal_get(f, 334, mc_java_i32_sub(x, dx), y, mc_java_i32_sub(z, dz), &id) ||
            !portal_static_matches(f, id, "aq", 337, 340, &matches))
            return false;
        if (matches) {
            if (!portal_get(f, 357, mc_java_i32_add(x, dx), y, mc_java_i32_add(z, dz), &id))
                return false;
            valid = id == f->block->fields.bn;
        }
    }
    return valid ? portal_done(f) : portal_remove(f, 373, x, y, z);
}

bool mc_world_mutation_portal_create(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, bool *out,
                                     mc_world_mutation_error *error) {
    portal_frame f = {.world = world, .block = block, .error = error, .method = "a_(Lfd;III)Z"};
    if (!portal_entry(&f))
        return false;
    if (out == NULL)
        return portal_fail(&f, 0, "Portal creation ordinary output");
    const bool completed = portal_create(&f, x, y, z, out);
    free(f.scratch);
    return completed;
}

bool mc_world_mutation_portal_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, int32_t source_id,
                                       mc_world_mutation_error *error) {
    (void)source_id;
    portal_frame f = {.world = world, .block = block, .error = error, .method = "b(Lfd;IIII)V"};
    if (!portal_entry(&f))
        return false;
    const bool completed = portal_neighbor(&f, x, y, z);
    free(f.scratch);
    return completed;
}
