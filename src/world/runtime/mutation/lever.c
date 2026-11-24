#include "mutation_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../block_queries_internal.h"
#include "../collision/blocks_internal.h"
#include "mc/core/java/java.h"

#include <stdlib.h>

typedef union lever_scratch {
    mc_world_solid_support_detail solid;
    mc_world_collision_error value;
    mc_world_mutation_error mutation;
} lever_scratch;

typedef struct lever_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    const char *method;
    lever_scratch **scratch;
} lever_frame;

static void lever_at(const lever_frame *f, unsigned int pc) {
    f->error->hook_class = "xr";
    f->error->hook_method = f->method;
    f->error->hook_bytecode = pc;
}

static bool lever_done(const lever_frame *f) {
    f->error->hook_class = f->error->hook_method = NULL;
    f->error->hook_bytecode = 0;
    return true;
}

static lever_scratch *lever_workspace(const lever_frame *f, unsigned int pc) {
    lever_at(f, pc);
    if (*f->scratch == NULL)
        *f->scratch = malloc(sizeof(**f->scratch));
    if (*f->scratch == NULL)
        (void)mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                     "Lever reached child diagnostic allocation");
    return *f->scratch;
}

static bool lever_storage(const lever_frame *f, unsigned int pc, mc_world_storage_error cause) {
    lever_at(f, pc);
    f->error->status = cause.status;
    f->error->operation = cause.operation;
    f->error->cause = cause;
    if (f->world != NULL)
        f->world->last_error = cause;
    return false;
}

static bool lever_nested(const lever_frame *f, unsigned int pc,
                         const mc_world_mutation_error *lower) {
    if (f->error != lower)
        *f->error = *lower;
    if (f->error->cause_hook_class == NULL) {
        f->error->cause_hook_class = f->error->hook_class;
        f->error->cause_hook_method = f->error->hook_method;
        f->error->cause_hook_bytecode = f->error->hook_bytecode;
    }
    lever_at(f, pc);
    if (f->world != NULL)
        f->world->last_error = f->error->cause;
    return false;
}

static void lever_access(mc_world_mutation_error *e, const mc_world_nearby_error *child) {
    e->nearby = *child;
    e->query = child->query;
    e->population = child->query.population;
    e->entity = child->entity;
    e->tile_entity = child->tile_entity;
    e->batch = child->batch;
    e->load_entity = child->load_entity;
    e->activation_bytecode = child->activation_bytecode;
    e->activation_world_bytecode = child->activation_world_bytecode;
    e->load_bytecode = child->load_bytecode;
    e->load_builder_bytecode = child->load_builder_bytecode;
}

static bool lever_solid(const lever_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                        bool *out) {
    lever_at(f, pc);
    if (f->world == NULL) {
        f->error->lever_source_null = true;
        return mc_world_mutation_fail(NULL, f->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Lever actual null World support call");
    }
    lever_scratch *scratch = lever_workspace(f, pc);
    if (scratch == NULL)
        return false;
    mc_world_solid_support_detail *child = &scratch->solid;
    if (mc_world_solid_support_detailed_held(f->world, x, y, z, out, child) == MC_WORLD_STORAGE_OK)
        return true;
    lever_access(f->error, &child->access);
    f->error->support_bytecode = child->error.bytecode;
    f->error->registry = child->error.registry;
    return lever_storage(f, pc, child->error.cause);
}

static bool lever_metadata(const lever_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                           int32_t *out) {
    lever_at(f, pc);
    if (f->world == NULL) {
        f->error->lever_source_null = true;
        return mc_world_mutation_fail(NULL, f->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Lever actual null World metadata call");
    }
    lever_scratch *scratch = lever_workspace(f, pc);
    if (scratch == NULL)
        return false;
    mc_world_collision_error *child = &scratch->value;
    *child = mc_world_collision_empty_error();
    if (mc_world_collision_block_value_held(f->world, x, y, z, true, out, child) ==
        MC_WORLD_STORAGE_OK)
        return true;
    lever_access(f->error, &child->nearby);
    f->error->registry = child->nearby.query.registry;
    return lever_storage(f, pc, child->nearby.query.cause);
}

static bool lever_can_place(const lever_frame *caller, int32_t x, int32_t y, int32_t z, bool *out) {
    lever_frame f = *caller;
    f.method = "a(Lfd;III)Z";
    static const unsigned int pcs[5] = {7, 22, 37, 52, 67};
    const int32_t xs[5] = {mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x, x};
    const int32_t ys[5] = {y, y, y, y, mc_java_i32_sub(y, 1)};
    const int32_t zs[5] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1), z};
    bool supported = false;
    for (size_t i = 0; i < 5; ++i) {
        if (!lever_solid(&f, pcs[i], xs[i], ys[i], zs[i], &supported))
            return false;
        if (supported)
            break;
    }
    *out = supported;
    return lever_done(&f);
}

static bool lever_remove(const lever_frame *f, unsigned int metadata_pc, unsigned int drop_pc,
                         unsigned int setter_pc, int32_t x, int32_t y, int32_t z) {
    int32_t metadata;
    if (!lever_metadata(f, metadata_pc, x, y, z, &metadata))
        return false;
    lever_at(f, drop_pc);
    if (!mc_world_mutation_drop(f->world, f->block, x, y, z, metadata, f->error))
        return lever_nested(f, drop_pc, f->error);
    lever_scratch *scratch = lever_workspace(f, setter_pc);
    if (scratch == NULL)
        return false;
    mc_world_mutation_error *child = &scratch->mutation;
    mc_world_mutation_error_init(child);
    bool ignored;
    if (mc_world_runtime_block_set(f->world, x, y, z, 0, &ignored, child) != MC_WORLD_STORAGE_OK)
        return lever_nested(f, setter_pc, child);
    return true;
}

static bool lever_survive(const lever_frame *caller, int32_t x, int32_t y, int32_t z, bool *out) {
    lever_frame f = *caller;
    f.method = "h(Lfd;III)Z";
    bool supported;
    if (!lever_can_place(&f, x, y, z, &supported))
        return lever_nested(&f, 6, f.error);
    if (!supported && !lever_remove(&f, 23, 26, 35, x, y, z))
        return false;
    *out = supported;
    return lever_done(&f);
}

bool mc_world_mutation_lever_can_place(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, bool *out,
                                       mc_world_mutation_error *error) {
    lever_scratch *scratch = NULL;
    const lever_frame f = {world, block, error, "a(Lfd;III)Z", &scratch};
    const bool completed = lever_can_place(&f, x, y, z, out);
    free(scratch);
    return completed;
}

bool mc_world_mutation_lever_survive(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, bool *out,
                                     mc_world_mutation_error *error) {
    lever_scratch *scratch = NULL;
    const lever_frame f = {world, block, error, "h(Lfd;III)Z", &scratch};
    const bool completed = lever_survive(&f, x, y, z, out);
    free(scratch);
    return completed;
}

static bool lever_neighbor(const lever_frame *f, int32_t x, int32_t y, int32_t z) {
    bool supported;
    if (!lever_survive(f, x, y, z, &supported))
        return lever_nested(f, 6, f->error);
    if (!supported)
        return lever_done(f);
    int32_t metadata;
    if (!lever_metadata(f, 17, x, y, z, &metadata))
        return false;
    const int32_t orientation = metadata & 7;
    static const unsigned int pcs[6] = {35, 57, 79, 101, 123, 145};
    const int32_t xs[6] = {mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x, x, x};
    const int32_t ys[6] = {y, y, y, y, mc_java_i32_sub(y, 1), mc_java_i32_sub(y, 1)};
    const int32_t zs[6] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1), z, z};
    bool detached = false;
    /* Every support query is reached, including both floor calls. The source
     * checks orientation after each query and never exits this sequence early. */
    for (size_t i = 0; i < 6; ++i) {
        if (!lever_solid(f, pcs[i], xs[i], ys[i], zs[i], &supported))
            return false;
        if (!supported && orientation == (int32_t)i + 1)
            detached = true;
    }
    if (detached && !lever_remove(f, 177, 180, 189, x, y, z))
        return false;
    return lever_done(f);
}

bool mc_world_mutation_lever_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, int32_t source_id,
                                      mc_world_mutation_error *error) {
    (void)source_id;
    lever_scratch *scratch = NULL;
    const lever_frame f = {world, block, error, "b(Lfd;IIII)V", &scratch};
    const bool completed = lever_neighbor(&f, x, y, z);
    free(scratch);
    return completed;
}

static bool lever_notify(const lever_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z) {
    lever_scratch *scratch = lever_workspace(f, pc);
    if (scratch == NULL)
        return false;
    mc_world_mutation_error *child = &scratch->mutation;
    mc_world_mutation_error_init(child);
    if (!mc_world_mutation_neighbors(f->world, x, y, z, f->block->fields.bn, child))
        return lever_nested(f, pc, child);
    return true;
}

static bool lever_removed(const lever_frame *f, int32_t x, int32_t y, int32_t z) {
    int32_t metadata;
    if (!lever_metadata(f, 5, x, y, z, &metadata))
        return false;
    if ((metadata & 8) > 0) {
        if (!lever_notify(f, 27, x, y, z))
            return false;
        const int32_t orientation = metadata & 7;
        const unsigned int pc = orientation == 1   ? 54u
                                : orientation == 2 ? 77u
                                : orientation == 3 ? 100u
                                : orientation == 4 ? 123u
                                                   : 140u;
        const int32_t nx = orientation == 1   ? mc_java_i32_sub(x, 1)
                           : orientation == 2 ? mc_java_i32_add(x, 1)
                                              : x;
        const int32_t ny = orientation >= 1 && orientation <= 4 ? y : mc_java_i32_sub(y, 1);
        const int32_t nz = orientation == 3   ? mc_java_i32_sub(z, 1)
                           : orientation == 4 ? mc_java_i32_add(z, 1)
                                              : z;
        if (!lever_notify(f, pc, nx, ny, nz))
            return false;
    }
    /* invokespecial149 selects the proved empty uu.b(World,III), return0. */
    return lever_done(f);
}

bool mc_world_mutation_lever_removed(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, mc_world_mutation_error *error) {
    lever_scratch *scratch = NULL;
    const lever_frame f = {world, block, error, "b(Lfd;III)V", &scratch};
    const bool completed = lever_removed(&f, x, y, z);
    free(scratch);
    return completed;
}
