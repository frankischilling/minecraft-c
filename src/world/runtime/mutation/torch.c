#include "mutation_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../block_queries_internal.h"
#include "../collision/blocks_internal.h"
#include "../torch_support_internal.h"
#include "mc/core/java/java.h"

#include <stdlib.h>

typedef enum torch_method {
    TORCH_CAN_PLACE,
    TORCH_FLOOR,
    TORCH_SURVIVE,
    TORCH_ADDED,
    TORCH_NEIGHBOR,
    TORCH_TICK
} torch_method;

typedef struct torch_frame {
    mc_world_runtime *world;
    const mc_block *block;
    mc_world_mutation_error *error;
    torch_method method;
} torch_frame;

static void torch_at(const torch_frame *f, unsigned int pc) {
    mc_world_mutation_error *e = f->error;
    e->hook_class = "vm";
    e->hook_bytecode = pc;
    switch (f->method) {
        case TORCH_CAN_PLACE:
            e->hook_method = "a(Lfd;III)Z";
            e->torch_can_place_bytecode = pc;
            break;
        case TORCH_FLOOR:
            e->hook_method = "h(Lfd;III)Z";
            e->torch_floor_bytecode = pc;
            break;
        case TORCH_SURVIVE:
            e->hook_method = "i(Lfd;III)Z";
            e->torch_survive_bytecode = pc;
            break;
        case TORCH_ADDED:
            e->hook_method = "c(Lfd;III)V";
            e->torch_added_bytecode = pc;
            break;
        case TORCH_NEIGHBOR:
            e->hook_method = "b(Lfd;IIII)V";
            e->torch_neighbor_bytecode = pc;
            break;
        case TORCH_TICK:
            e->hook_method = "a(Lfd;IIILjava/util/Random;)V";
            e->torch_tick_bytecode = pc;
            break;
    }
}

static bool torch_done(mc_world_mutation_error *e) {
    e->hook_class = e->hook_method = NULL;
    e->hook_bytecode = 0;
    e->torch_can_place_bytecode = e->torch_floor_bytecode = e->torch_survive_bytecode = 0;
    e->torch_added_bytecode = e->torch_neighbor_bytecode = e->torch_tick_bytecode = 0;
    return true;
}

static bool torch_storage(const torch_frame *f, unsigned int pc, mc_world_storage_error cause) {
    torch_at(f, pc);
    f->error->status = cause.status;
    f->error->operation = cause.operation;
    f->error->cause = cause;
    if (f->world != NULL)
        f->world->last_error = cause;
    return false;
}

static bool torch_null(const torch_frame *f, unsigned int pc) {
    torch_at(f, pc);
    f->error->torch_source_null = true;
    return mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                  "Torch actual World null receiver");
}

/* Current caller and deepest concrete cause are a diagnostic projection, not
 * a limit on source recursion. Notifying children use reached heap scratch. */
static bool torch_nested(const torch_frame *f, unsigned int pc,
                         const mc_world_mutation_error *lower) {
    if (f->error != lower)
        *f->error = *lower;
    if (f->error->cause_hook_class == NULL) {
        f->error->cause_hook_class = f->error->hook_class;
        f->error->cause_hook_method = f->error->hook_method;
        f->error->cause_hook_bytecode = f->error->hook_bytecode;
    }
    torch_at(f, pc);
    if (f->world != NULL)
        f->world->last_error = f->error->cause;
    return false;
}

static void torch_access(mc_world_mutation_error *e, const mc_world_nearby_error *child) {
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

static bool torch_value(const torch_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                        bool metadata, int32_t *out) {
    torch_at(f, pc);
    if (f->world == NULL)
        return torch_null(f, pc);
    mc_world_collision_error *child = malloc(sizeof(*child));
    if (child == NULL)
        return mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                      "Torch reached value diagnostic allocation");
    *child = mc_world_collision_empty_error();
    const mc_world_storage_status status =
        mc_world_collision_block_value_held(f->world, x, y, z, metadata, out, child);
    if (status != MC_WORLD_STORAGE_OK) {
        torch_access(f->error, &child->nearby);
        f->error->registry = child->nearby.query.registry;
        (void)torch_storage(f, pc, child->nearby.query.cause);
    }
    free(child);
    return status == MC_WORLD_STORAGE_OK;
}

static bool torch_solid(const torch_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                        bool *out) {
    torch_at(f, pc);
    if (f->world == NULL)
        return torch_null(f, pc);
    mc_world_solid_support_detail *child = malloc(sizeof(*child));
    if (child == NULL)
        return mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                      "Torch reached support diagnostic allocation");
    const mc_world_storage_status status =
        mc_world_solid_support_detailed_held(f->world, x, y, z, out, child);
    if (status != MC_WORLD_STORAGE_OK) {
        torch_access(f->error, &child->access);
        f->error->support_bytecode = child->error.bytecode;
        f->error->registry = child->error.registry;
        (void)torch_storage(f, pc, child->error.cause);
    }
    free(child);
    return status == MC_WORLD_STORAGE_OK;
}

bool mc_world_mutation_torch_floor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                   int32_t y, int32_t z, bool *out,
                                   mc_world_mutation_error *error) {
    const torch_frame f = {world, block, error, TORCH_FLOOR};
    bool value;
    if (!torch_solid(&f, 5, x, y, z, &value))
        return false;
    if (!value) {
        int32_t id;
        if (!torch_value(&f, 16, x, y, z, false, &id))
            return false;
        mc_world_torch_fence_error child;
        const mc_registry_status status =
            mc_world_torch_floor_fence(block->classes, id, &value, &child);
        torch_at(&f, child.pc);
        if (status != MC_REGISTRY_OK)
            return mc_world_mutation_registry_fail(world, error, &child.registry,
                                                   "Torch floor live Fence field");
    }
    *out = value;
    return torch_done(error);
}

bool mc_world_mutation_torch_can_place(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, bool *out,
                                       mc_world_mutation_error *error) {
    const torch_frame f = {world, block, error, TORCH_CAN_PLACE};
    const unsigned int pcs[] = {7, 22, 37, 52};
    const int32_t xs[] = {mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x};
    const int32_t zs[] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1)};
    bool value = false;
    for (size_t i = 0; i < 4; ++i) {
        if (!torch_solid(&f, pcs[i], xs[i], y, zs[i], &value))
            return false;
        if (value)
            break;
    }
    if (!value &&
        !mc_world_mutation_torch_floor(world, block, x, mc_java_i32_sub(y, 1), z, &value, error))
        return torch_nested(&f, 68, error);
    *out = value;
    return torch_done(error);
}

static mc_world_mutation_error *torch_workspace(const torch_frame *f, unsigned int pc) {
    torch_at(f, pc);
    mc_world_mutation_error *child = malloc(sizeof(*child));
    if (child == NULL)
        (void)mc_world_mutation_fail(f->world, f->error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                     "Torch notifying child diagnostic allocation");
    else
        mc_world_mutation_error_init(child);
    return child;
}

static bool torch_child(const torch_frame *f, unsigned int pc, mc_world_mutation_error *child,
                        bool completed) {
    if (!completed)
        (void)torch_nested(f, pc, child);
    free(child);
    return completed;
}

static bool torch_remove(const torch_frame *f, unsigned int metadata_pc, unsigned int drop_pc,
                         unsigned int remove_pc, int32_t x, int32_t y, int32_t z) {
    int32_t metadata;
    if (!torch_value(f, metadata_pc, x, y, z, true, &metadata))
        return false;
    torch_at(f, drop_pc);
    if (!mc_world_mutation_drop(f->world, f->block, x, y, z, metadata, f->error))
        return torch_nested(f, drop_pc, f->error);
    mc_world_mutation_error *child = torch_workspace(f, remove_pc);
    if (child == NULL)
        return false;
    bool ignored;
    return torch_child(f, remove_pc, child,
                       mc_world_runtime_block_set(f->world, x, y, z, 0, &ignored, child) ==
                           MC_WORLD_STORAGE_OK);
}

bool mc_world_mutation_torch_survive(mc_world_runtime *world, const mc_block *block, int32_t x,
                                     int32_t y, int32_t z, bool *out,
                                     mc_world_mutation_error *error) {
    const torch_frame f = {world, block, error, TORCH_SURVIVE};
    bool supported;
    if (!mc_world_mutation_torch_can_place(world, block, x, y, z, &supported, error))
        return torch_nested(&f, 6, error);
    if (!supported && !torch_remove(&f, 23, 26, 35, x, y, z))
        return false;
    *out = supported;
    return torch_done(error);
}

static bool torch_metadata(const torch_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                           int32_t metadata) {
    mc_world_mutation_error *child = torch_workspace(f, pc);
    if (child == NULL)
        return false;
    return torch_child(f, pc, child,
                       mc_world_runtime_metadata_set(f->world, x, y, z, metadata, child) ==
                           MC_WORLD_STORAGE_OK);
}

bool mc_world_mutation_torch_added(mc_world_runtime *world, const mc_block *block, int32_t x,
                                   int32_t y, int32_t z, mc_world_mutation_error *error) {
    const torch_frame f = {world, block, error, TORCH_ADDED};
    const unsigned int reads[] = {7, 32, 57, 82};
    const unsigned int writes[] = {19, 44, 69, 94};
    const int32_t xs[] = {mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x};
    const int32_t zs[] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1)};
    bool supported = false;
    for (size_t i = 0; i < 4; ++i) {
        if (!torch_solid(&f, reads[i], xs[i], y, zs[i], &supported))
            return false;
        if (supported) {
            if (!torch_metadata(&f, writes[i], x, y, z, (int32_t)i + 1))
                return false;
            break;
        }
    }
    if (!supported) {
        if (!mc_world_mutation_torch_floor(world, block, x, mc_java_i32_sub(y, 1), z, &supported,
                                           error))
            return torch_nested(&f, 108, error);
        if (supported && !torch_metadata(&f, 120, x, y, z, 5))
            return false;
    }
    bool ignored;
    if (!mc_world_mutation_torch_survive(world, block, x, y, z, &ignored, error))
        return torch_nested(&f, 129, error);
    return torch_done(error);
}

bool mc_world_mutation_torch_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, int32_t source_id,
                                      mc_world_mutation_error *error) {
    (void)source_id;
    const torch_frame f = {world, block, error, TORCH_NEIGHBOR};
    bool survives;
    if (!mc_world_mutation_torch_survive(world, block, x, y, z, &survives, error))
        return torch_nested(&f, 6, error);
    if (!survives)
        return torch_done(error);
    int32_t metadata;
    if (!torch_value(&f, 17, x, y, z, true, &metadata))
        return false;
    const unsigned int pcs[] = {32, 54, 76, 98};
    const int32_t xs[] = {mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1), x, x};
    const int32_t zs[] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1)};
    bool detached = false;
    for (size_t i = 0; i < 4; ++i) {
        bool supported;
        if (!torch_solid(&f, pcs[i], xs[i], y, zs[i], &supported))
            return false;
        if (!supported && metadata == (int32_t)i + 1)
            detached = true;
    }
    bool floor;
    if (!mc_world_mutation_torch_floor(world, block, x, mc_java_i32_sub(y, 1), z, &floor, error))
        return torch_nested(&f, 121, error);
    if (!floor && metadata == 5)
        detached = true;
    if (detached && !torch_remove(&f, 152, 155, 164, x, y, z))
        return false;
    return torch_done(error);
}

bool mc_world_mutation_torch_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                  int32_t y, int32_t z, mc_java_random *random,
                                  mc_world_mutation_error *error) {
    (void)random; /* invokespecial UU tick8 is empty; VM never draws Random. */
    const torch_frame f = {world, block, error, TORCH_TICK};
    int32_t metadata;
    if (!torch_value(&f, 16, x, y, z, true, &metadata))
        return false;
    if (metadata == 0 && !mc_world_mutation_torch_added(world, block, x, y, z, error))
        return torch_nested(&f, 28, error);
    return torch_done(error);
}
