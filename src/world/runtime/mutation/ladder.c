#include "mutation_internal.h"

#include "../block_queries_internal.h"
#include "../collision/blocks_internal.h"
#include "mc/core/java/java.h"

#include <stdlib.h>

typedef union ladder_scratch {
    mc_world_collision_error value;
    mc_world_solid_support_detail solid;
    mc_world_mutation_error mutation;
} ladder_scratch;

typedef struct ladder_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    ladder_scratch *scratch;
} ladder_frame;

static void ladder_at(ladder_frame *f, unsigned int pc) {
    f->error->hook_class = "dp";
    f->error->hook_method = "b(Lfd;IIII)V";
    f->error->hook_bytecode = pc;
}

static bool ladder_workspace(ladder_frame *f, unsigned int pc) {
    ladder_at(f, pc);
    if (f->world == NULL) {
        f->error->ladder_source_null = true;
        return mc_world_mutation_fail(NULL, f->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Ladder actual null World receiver");
    }
    if (f->scratch == NULL)
        f->scratch = malloc(sizeof(*f->scratch));
    return f->scratch != NULL ||
           mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                  "Ladder reached child diagnostic allocation");
}

static void ladder_access(mc_world_mutation_error *e, const mc_world_nearby_error *child) {
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

static bool ladder_storage(ladder_frame *f, mc_world_storage_error cause) {
    f->error->status = cause.status;
    f->error->operation = cause.operation;
    f->error->cause = cause;
    f->world->last_error = cause;
    return false;
}

static bool ladder_metadata(ladder_frame *f, int32_t x, int32_t y, int32_t z, int32_t *out) {
    if (!ladder_workspace(f, 5))
        return false;
    mc_world_collision_error *child = &f->scratch->value;
    mc_world_collision_error_init(child);
    if (mc_world_collision_block_value_held(f->world, x, y, z, true, out, child) ==
        MC_WORLD_STORAGE_OK)
        return true;
    ladder_access(f->error, &child->nearby);
    f->error->registry = child->nearby.query.registry;
    return ladder_storage(f, child->nearby.query.cause);
}

static bool ladder_solid(ladder_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                         bool *out) {
    if (!ladder_workspace(f, pc))
        return false;
    mc_world_solid_support_detail *child = &f->scratch->solid;
    if (mc_world_solid_support_detailed_held(f->world, x, y, z, out, child) == MC_WORLD_STORAGE_OK)
        return true;
    ladder_access(f->error, &child->access);
    f->error->registry = child->error.registry;
    f->error->support_bytecode = child->error.bytecode;
    return ladder_storage(f, child->error.cause);
}

static bool ladder_nested(ladder_frame *f, unsigned int pc, const mc_world_mutation_error *child) {
    if (f->error != child)
        *f->error = *child;
    if (f->error->cause_hook_class == NULL) {
        f->error->cause_hook_class = f->error->hook_class;
        f->error->cause_hook_method = f->error->hook_method;
        f->error->cause_hook_bytecode = f->error->hook_bytecode;
    }
    ladder_at(f, pc);
    f->world->last_error = f->error->cause;
    return false;
}

static bool ladder_neighbor(ladder_frame *f, int32_t x, int32_t y, int32_t z) {
    int32_t metadata;
    if (!ladder_metadata(f, x, y, z, &metadata))
        return false;
    bool supported = false;
    /* The source compares the full metadata before each support call. Only
     * the matching orientation reaches World.h; no fallback support is read. */
    if (metadata >= 2 && metadata <= 5) {
        static const unsigned int pcs[4] = {26, 48, 70, 92};
        const int32_t sx = metadata == 4   ? mc_java_i32_add(x, 1)
                           : metadata == 5 ? mc_java_i32_sub(x, 1)
                                           : x;
        const int32_t sz = metadata == 2   ? mc_java_i32_add(z, 1)
                           : metadata == 3 ? mc_java_i32_sub(z, 1)
                                           : z;
        if (!ladder_solid(f, pcs[(size_t)metadata - 2u], sx, y, sz, &supported))
            return false;
    }
    if (!supported) {
        ladder_at(f, 114);
        if (!mc_world_mutation_drop(f->world, f->block, x, y, z, metadata, f->error))
            return ladder_nested(f, 114, f->error);
        if (!ladder_workspace(f, 123))
            return false;
        mc_world_mutation_error *child = &f->scratch->mutation;
        mc_world_mutation_error_init(child);
        bool ignored;
        if (mc_world_runtime_block_set(f->world, x, y, z, 0, &ignored, child) !=
            MC_WORLD_STORAGE_OK)
            return ladder_nested(f, 123, child);
        /* The ordinary setter Boolean is discarded at 126. */
    }
    /* The reached invokespecial135 is the literal base return0. */
    f->error->hook_class = f->error->hook_method = NULL;
    f->error->hook_bytecode = 0;
    return true;
}

bool mc_world_mutation_ladder_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, int32_t source_id,
                                       mc_world_mutation_error *error) {
    (void)source_id;
    ladder_frame frame = {world, block, error, NULL};
    const bool completed = ladder_neighbor(&frame, x, y, z);
    free(frame.scratch);
    return completed;
}
