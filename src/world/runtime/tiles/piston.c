#include "piston_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../collision/blocks_internal.h"
#include "../mutation/mutation_internal.h"
#include "mc/block/registry/runtime.h"
#include "tiles_internal.h"

/* Exact binary32 fcmpg(previous,1)<0. NaNs select +1, including signaling
 * representations; signed zero and every negative non-NaN are below 1. This
 * integer classification has no host arithmetic, NaN quieting or FP preflight. */
static bool previous_below_one(uint32_t bits) {
    const uint32_t magnitude = bits & UINT32_C(0x7fffffff);
    if (magnitude > UINT32_C(0x7f800000))
        return false;
    return (bits & UINT32_C(0x80000000)) != 0 || magnitude < UINT32_C(0x3f800000);
}

static bool storage_failure(mc_world_runtime *world, mc_world_mutation_error *error,
                            mc_world_storage_error cause) {
    error->status = cause.status;
    error->operation = cause.operation;
    error->cause = cause;
    if (world != NULL)
        world->last_error = cause;
    return false;
}

/* Preserve all per-attempt acquisition and typed descendants. These same
 * fields are used by ordinary mutation acquisition, rather than reconstructing
 * a cause from ambient World.last_error or a static operation string. */
static bool tile_failure(mc_world_runtime *world, mc_world_mutation_error *error,
                         const mc_live_tile_error *child) {
    error->tile = *child;
    error->registry = child->registry;
    error->entity = child->entity;
    error->publication = child->publication;
    error->batch = child->batch;
    error->activation_bytecode = child->activation_bytecode;
    error->activation_world_bytecode = child->activation_world_bytecode;
    error->load_bytecode = child->load_bytecode;
    error->load_builder_bytecode = child->load_builder_bytecode;
    error->load_entity = child->load_entity;
    error->provider_bytecode = child->provider_bytecode;
    return storage_failure(world, error, child->cause);
}

static bool query_failure(mc_world_runtime *world, mc_world_mutation_error *error,
                          const mc_world_nearby_error *child) {
    error->query = child->query;
    error->registry = child->query.registry;
    error->population = child->query.population;
    error->tile = child->tile;
    error->entity = child->entity;
    error->batch = child->batch;
    error->load_entity = child->load_entity;
    error->activation_bytecode = child->activation_bytecode;
    error->activation_world_bytecode = child->activation_world_bytecode;
    error->load_bytecode = child->load_bytecode;
    error->load_builder_bytecode = child->load_builder_bytecode;
    error->provider_bytecode = child->query.provider_bytecode;
    return storage_failure(world, error, child->query.cause);
}

static bool registry_failure(mc_tile *tile, mc_world_mutation_error *error,
                             const mc_registry_error *child, const char *operation) {
    error->registry = *child;
    error->status = mc_world_runtime_registry_status(child->status);
    error->operation = operation;
    error->cause = (mc_world_storage_error){error->status, operation, 0};
    /* Registry active use has no World receiver. A callback may have rebound
     * this Tile, including to NULL; diagnostics never dereference an old one. */
    mc_world_runtime *const world = tile->world;
    if (world != NULL) {
        world->last_registry_error = *child;
        world->last_error = error->cause;
    }
    return false;
}

static void finalize_nested(mc_world_mutation_error *error, const mc_world_mutation_error *child,
                            unsigned int pc) {
    const mc_world_mutation_error captured = *child;
    *error = captured;
    if (error->cause_piston_finalize_bytecode == 0)
        error->cause_piston_finalize_bytecode = captured.piston_finalize_bytecode;
    error->piston_finalize_bytecode = pc;
}

/* The actual bounded fd/dj.a(III)I implementation already supplies detailed
 * provider, concrete Chunk and activation causes. Enter precisely this newly
 * evaluated World receiver and keep its lease until the held call returns. */
bool mc_tile_piston_current_block_held(mc_tile *tile, int32_t *out,
                                       mc_world_mutation_error *error) {
    mc_world_runtime *const world = tile->world;
    const int32_t x = tile->x;
    const int32_t y = tile->y;
    const int32_t z = tile->z;
    mc_world_collision_error child = mc_world_collision_empty_error();
    if (world != NULL) {
        mc_world_storage_error cause = {0};
        if (mc_world_runtime_call_enter(world, &cause) != MC_WORLD_STORAGE_OK) {
            child.nearby.query.status = cause.status;
            child.nearby.query.operation = cause.operation;
            child.nearby.query.cause = cause;
            return query_failure(world, error, &child.nearby);
        }
    }
    const mc_world_storage_status status =
        mc_world_collision_block_value_held(world, x, y, z, false, out, &child);
    const bool completed =
        status == MC_WORLD_STORAGE_OK || query_failure(world, error, &child.nearby);
    if (world != NULL)
        mc_world_runtime_call_leave(world);
    return completed;
}

static bool finalize_held(mc_tile *tile, mc_world_mutation_error *error) {
    if (!previous_below_one(tile->state.piston.previous_progress_bits))
        return true; /* Actual return98, with no class/World operation. */

    tile->state.piston.progress_bits = UINT32_C(0x3f800000);          /* putfield13 */
    tile->state.piston.previous_progress_bits = UINT32_C(0x3f800000); /* putfield16 */

    mc_world_runtime *world = tile->world;
    int32_t x = tile->x;
    int32_t y = tile->y;
    int32_t z = tile->z;
    mc_live_tile_error removed = mc_live_tiles_empty_error();
    error->piston_finalize_bytecode = 35;
    if (mc_live_tiles_remove(world, x, y, z, &removed) != MC_WORLD_STORAGE_OK)
        return tile_failure(world, error, &removed);

    error->piston_finalize_bytecode = 39;
    const mc_tile_status invalidated = mc_tile_set_invalid(tile, true);
    if (invalidated != MC_TILE_OK) {
        error->tile.tile.status = invalidated;
        return mc_world_mutation_fail(NULL, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Piston finalizer invalidation receiver");
    }

    int32_t id = 0;
    error->piston_finalize_bytecode = 58;
    if (!mc_tile_piston_current_block_held(tile, &id, error))
        return false;

    /* getstatic61 follows the completed acquiring ID query. This is the Tile's
     * actual defining Registry, independently of replaceable source World. */
    error->piston_finalize_bytecode = 61;
    mc_registry_classes *const classes = mc_tile_piston_registry_held(tile);
    if (classes == NULL)
        return mc_world_mutation_fail(NULL, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Piston finalizer Registry binding");
    const mc_block *moving = NULL;
    mc_registry_error registry = mc_world_mutation_empty_registry();
    if (mc_registry_block_static_get(classes, "ad", &moving, &registry) != MC_REGISTRY_OK)
        return registry_failure(tile, error, &registry, "Piston finalizer live ad active use");
    error->piston_finalize_bytecode = 64;
    if (moving == NULL) {
        registry.status = MC_REGISTRY_NULL_REFERENCE;
        registry.message = "Piston finalizer live ad receiver is null";
        return registry_failure(tile, error, &registry, registry.message);
    }
    if (id != moving->fields.bn)
        return true;

    /* Re-evaluate every source field after Registry initialization/reentry.
     * The World receiver precedes XYZ, carried ID and metadata argument reads. */
    world = tile->world;
    x = tile->x;
    y = tile->y;
    z = tile->z;
    const int32_t carried_id = tile->state.piston.block_id;
    const int32_t carried_data = tile->state.piston.block_data;
    bool ordinary = false;
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    if (mc_world_runtime_block_metadata_set(world, x, y, z, carried_id, carried_data, &ordinary,
                                            &lower) != MC_WORLD_STORAGE_OK) {
        finalize_nested(error, &lower, 94);
        return false;
    }
    return true; /* pop97 discards even an ordinary false; actual return98. */
}

mc_world_storage_status mc_tile_piston_finalize(mc_tile *tile, mc_world_mutation_error *error) {
    mc_world_mutation_error frame = mc_world_mutation_empty_error();
    bool retained = false;
    if (tile == NULL) {
        frame.tile.tile.status = MC_TILE_INVALID_ARGUMENT;
        (void)mc_world_mutation_fail(NULL, &frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                     "Piston finalizer receiver");
    } else if (tile->kind != MC_TILE_PISTON) {
        frame.tile.tile.status = MC_TILE_INVALID_KIND;
        (void)mc_world_mutation_fail(NULL, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "Piston finalizer receiver kind");
    } else if (tile->phase != MC_TILE_READY || tile->holds == 0 || tile->classes == NULL) {
        frame.tile.tile.status = MC_TILE_INVALID_PHASE;
        (void)mc_world_mutation_fail(NULL, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "Piston finalizer receiver phase");
    } else {
        const mc_tile_status held = mc_tile_retain(tile);
        if (held != MC_TILE_OK) {
            frame.tile.tile.status = held;
            (void)mc_world_mutation_fail(NULL, &frame,
                                         held == MC_TILE_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                               : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Piston finalizer receiver retain");
        } else {
            retained = true;
            if (finalize_held(tile, &frame))
                frame = mc_world_mutation_empty_error();
        }
    }
    if (retained)
        mc_tile_release(tile);
    if (error != NULL)
        *error = frame;
    return frame.status;
}

static void removed_at(const mc_block *block, unsigned int pc, mc_world_mutation_error *error) {
    const bool client = block->classes->options.side == MC_REGISTRY_CLIENT;
    error->hook_class = client ? "ut" : "mz";
    error->hook_method = client ? "b(Lfd;III)V" : "b(Ldj;III)V";
    error->hook_bytecode = pc;
    error->piston_removed_bytecode = pc;
}

static void removed_nested(const mc_block *block, unsigned int pc,
                           const mc_world_mutation_error *child, mc_world_mutation_error *error) {
    const mc_world_mutation_error captured = *child;
    *error = captured;
    if (error->cause_piston_removed_bytecode == 0)
        error->cause_piston_removed_bytecode = captured.piston_removed_bytecode;
    if (error->cause_hook_class == NULL) {
        error->cause_hook_class = captured.hook_class;
        error->cause_hook_method = captured.hook_method;
        error->cause_hook_bytecode = captured.hook_bytecode;
    }
    removed_at(block, pc, error);
}

bool mc_world_mutation_piston_removed(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, mc_world_mutation_error *error) {
    mc_tile *tile = NULL;
    mc_live_tile_error lookup = mc_live_tiles_empty_error();
    removed_at(block, 5, error);
    if (mc_live_tiles_get(world, x, y, z, &tile, &lookup) != MC_WORLD_STORAGE_OK)
        return tile_failure(world, error, &lookup);

    bool completed;
    if (tile != NULL && tile->kind == MC_TILE_PISTON) {
        mc_world_mutation_error lower = mc_world_mutation_empty_error();
        completed = mc_tile_piston_finalize(tile, &lower) == MC_WORLD_STORAGE_OK;
        if (!completed)
            removed_nested(block, 28, &lower, error);
    } else {
        /* Actual invokespecial rw/lb.b at40. Its invokespecial uu/na.b at6
         * is the complete return0 body; World remove14 executes afterwards.
         * No receiver substitution, factory, drop or alternate Tile is used. */
        removed_at(block, 40, error);
        mc_live_tile_error lower = mc_live_tiles_empty_error();
        completed = mc_live_tiles_remove(world, x, y, z, &lower) == MC_WORLD_STORAGE_OK;
        if (!completed) {
            lower.caller_bytecode = 14;
            (void)tile_failure(world, error, &lower);
            const bool client = block->classes->options.side == MC_REGISTRY_CLIENT;
            error->cause_hook_class = client ? "rw" : "lb";
            error->cause_hook_method = client ? "b(Lfd;III)V" : "b(Ldj;III)V";
            error->cause_hook_bytecode = 14;
        }
    }
    mc_tile_release(tile);
    if (completed) {
        error->hook_class = NULL;
        error->hook_method = NULL;
        error->hook_bytecode = 0;
        error->piston_removed_bytecode = 0;
    }
    return completed;
}

/* Complete ut/mz.b(World,IIII): the unused Tile getter still owns acquisition,
 * added-hook and invalid-Map cleanup effects. The enclosing caller holds the
 * actual World/Block; the getter returns its own nullable temporary Tile hold. */
bool mc_world_mutation_piston_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                       int32_t y, int32_t z, int32_t source_id,
                                       mc_world_mutation_error *error) {
    (void)source_id;
    const bool client = block->classes->options.side == MC_REGISTRY_CLIENT;
    error->hook_class = client ? "ut" : "mz";
    error->hook_method = client ? "b(Lfd;IIII)V" : "b(Ldj;IIII)V";
    error->hook_bytecode = 1;
    if (world == NULL)
        return mc_world_mutation_fail(NULL, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Moving Piston neighbor World B receiver");
    if (!world->fields.B) {
        mc_tile *tile = NULL;
        mc_live_tile_error lookup = mc_live_tiles_empty_error();
        error->hook_bytecode = 12;
        if (mc_live_tiles_get(world, x, y, z, &tile, &lookup) != MC_WORLD_STORAGE_OK)
            return tile_failure(world, error, &lookup);
        mc_tile_release(tile);
    }
    error->hook_class = NULL;
    error->hook_method = NULL;
    error->hook_bytecode = 0;
    return true; /* ifnonnull15 and either branch both reach return18. */
}
