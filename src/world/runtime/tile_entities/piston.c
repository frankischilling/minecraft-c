#include "piston_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../collision/blocks_internal.h"
#include "../mutation/mutation_internal.h"
#include "mc/block/registry/runtime.h"
#include "tile_entities_internal.h"

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
static bool tile_entity_failure(mc_world_runtime *world, mc_world_mutation_error *error,
                                const mc_live_tile_entity_error *child) {
    error->tile_entity = *child;
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
    error->tile_entity = child->tile_entity;
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

static bool registry_failure(mc_tile_entity *tile_entity, mc_world_mutation_error *error,
                             const mc_registry_error *child, const char *operation) {
    error->registry = *child;
    error->status = mc_world_runtime_registry_status(child->status);
    error->operation = operation;
    error->cause = (mc_world_storage_error){error->status, operation, 0};
    /* Registry active use has no World receiver. A callback may have rebound
     * this TileEntity, including to NULL; diagnostics never dereference an old one. */
    mc_world_runtime *const world = tile_entity->world;
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
bool mc_tile_entity_piston_current_block_held(mc_tile_entity *tile_entity, int32_t *out,
                                              mc_world_mutation_error *error) {
    mc_world_runtime *const world = tile_entity->world;
    const int32_t x = tile_entity->x;
    const int32_t y = tile_entity->y;
    const int32_t z = tile_entity->z;
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

static bool finalize_held(mc_tile_entity *tile_entity, mc_world_mutation_error *error) {
    if (!previous_below_one(tile_entity->state.piston.previous_progress_bits))
        return true; /* Actual return98, with no class/World operation. */

    tile_entity->state.piston.progress_bits = UINT32_C(0x3f800000);          /* putfield13 */
    tile_entity->state.piston.previous_progress_bits = UINT32_C(0x3f800000); /* putfield16 */

    mc_world_runtime *world = tile_entity->world;
    int32_t x = tile_entity->x;
    int32_t y = tile_entity->y;
    int32_t z = tile_entity->z;
    mc_live_tile_entity_error removed = mc_live_tile_entities_empty_error();
    error->piston_finalize_bytecode = 35;
    if (mc_live_tile_entities_remove(world, x, y, z, &removed) != MC_WORLD_STORAGE_OK)
        return tile_entity_failure(world, error, &removed);

    error->piston_finalize_bytecode = 39;
    const mc_tile_entity_status invalidated = mc_tile_entity_set_invalid(tile_entity, true);
    if (invalidated != MC_TILE_ENTITY_OK) {
        error->tile_entity.tile_entity.status = invalidated;
        return mc_world_mutation_fail(NULL, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Piston finalizer invalidation receiver");
    }

    int32_t id = 0;
    error->piston_finalize_bytecode = 58;
    if (!mc_tile_entity_piston_current_block_held(tile_entity, &id, error))
        return false;

    /* getstatic61 follows the completed acquiring ID query. This is the TileEntity's
     * actual defining Registry, independently of replaceable source World. */
    error->piston_finalize_bytecode = 61;
    mc_registry_classes *const classes = mc_tile_entity_piston_registry_held(tile_entity);
    if (classes == NULL)
        return mc_world_mutation_fail(NULL, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Piston finalizer Registry binding");
    const mc_block *moving = NULL;
    mc_registry_error registry = mc_world_mutation_empty_registry();
    if (mc_registry_block_static_get(classes, "ad", &moving, &registry) != MC_REGISTRY_OK)
        return registry_failure(tile_entity, error, &registry,
                                "Piston finalizer live ad active use");
    error->piston_finalize_bytecode = 64;
    if (moving == NULL) {
        registry.status = MC_REGISTRY_NULL_REFERENCE;
        registry.message = "Piston finalizer live ad receiver is null";
        return registry_failure(tile_entity, error, &registry, registry.message);
    }
    if (id != moving->fields.bn)
        return true;

    /* Re-evaluate every source field after Registry initialization/reentry.
     * The World receiver precedes XYZ, carried ID and metadata argument reads. */
    world = tile_entity->world;
    x = tile_entity->x;
    y = tile_entity->y;
    z = tile_entity->z;
    const int32_t carried_id = tile_entity->state.piston.block_id;
    const int32_t carried_data = tile_entity->state.piston.block_data;
    bool ordinary = false;
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    if (mc_world_runtime_block_metadata_set(world, x, y, z, carried_id, carried_data, &ordinary,
                                            &lower) != MC_WORLD_STORAGE_OK) {
        finalize_nested(error, &lower, 94);
        return false;
    }
    return true; /* pop97 discards even an ordinary false; actual return98. */
}

mc_world_storage_status mc_tile_entity_piston_finalize(mc_tile_entity *tile_entity,
                                                       mc_world_mutation_error *error) {
    mc_world_mutation_error frame = mc_world_mutation_empty_error();
    bool retained = false;
    if (tile_entity == NULL) {
        frame.tile_entity.tile_entity.status = MC_TILE_ENTITY_INVALID_ARGUMENT;
        (void)mc_world_mutation_fail(NULL, &frame, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                     "Piston finalizer receiver");
    } else if (tile_entity->kind != MC_TILE_ENTITY_PISTON) {
        frame.tile_entity.tile_entity.status = MC_TILE_ENTITY_INVALID_KIND;
        (void)mc_world_mutation_fail(NULL, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "Piston finalizer receiver kind");
    } else if (tile_entity->phase != MC_TILE_ENTITY_READY || tile_entity->holds == 0 ||
               tile_entity->classes == NULL) {
        frame.tile_entity.tile_entity.status = MC_TILE_ENTITY_INVALID_PHASE;
        (void)mc_world_mutation_fail(NULL, &frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "Piston finalizer receiver phase");
    } else {
        const mc_tile_entity_status held = mc_tile_entity_retain(tile_entity);
        if (held != MC_TILE_ENTITY_OK) {
            frame.tile_entity.tile_entity.status = held;
            (void)mc_world_mutation_fail(NULL, &frame,
                                         held == MC_TILE_ENTITY_LIMIT
                                             ? MC_WORLD_STORAGE_LIMIT
                                             : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                         "Piston finalizer receiver retain");
        } else {
            retained = true;
            if (finalize_held(tile_entity, &frame))
                frame = mc_world_mutation_empty_error();
        }
    }
    if (retained)
        mc_tile_entity_release(tile_entity);
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
    mc_tile_entity *tile_entity = NULL;
    mc_live_tile_entity_error lookup = mc_live_tile_entities_empty_error();
    removed_at(block, 5, error);
    if (mc_live_tile_entities_get(world, x, y, z, &tile_entity, &lookup) != MC_WORLD_STORAGE_OK)
        return tile_entity_failure(world, error, &lookup);

    bool completed;
    if (tile_entity != NULL && tile_entity->kind == MC_TILE_ENTITY_PISTON) {
        mc_world_mutation_error lower = mc_world_mutation_empty_error();
        completed = mc_tile_entity_piston_finalize(tile_entity, &lower) == MC_WORLD_STORAGE_OK;
        if (!completed)
            removed_nested(block, 28, &lower, error);
    } else {
        /* Actual invokespecial rw/lb.b at40. Its invokespecial uu/na.b at6
         * is the complete return0 body; World remove14 executes afterwards.
         * No receiver substitution, factory, drop or alternate TileEntity is used. */
        removed_at(block, 40, error);
        mc_live_tile_entity_error lower = mc_live_tile_entities_empty_error();
        completed = mc_live_tile_entities_remove(world, x, y, z, &lower) == MC_WORLD_STORAGE_OK;
        if (!completed) {
            lower.caller_bytecode = 14;
            (void)tile_entity_failure(world, error, &lower);
            const bool client = block->classes->options.side == MC_REGISTRY_CLIENT;
            error->cause_hook_class = client ? "rw" : "lb";
            error->cause_hook_method = client ? "b(Lfd;III)V" : "b(Ldj;III)V";
            error->cause_hook_bytecode = 14;
        }
    }
    mc_tile_entity_release(tile_entity);
    if (completed) {
        error->hook_class = NULL;
        error->hook_method = NULL;
        error->hook_bytecode = 0;
        error->piston_removed_bytecode = 0;
    }
    return completed;
}

/* Complete ut/mz.b(World,IIII): the unused TileEntity getter still owns acquisition,
 * added-hook and invalid-Map cleanup effects. The enclosing caller holds the
 * actual World/Block; the getter returns its own nullable temporary TileEntity hold. */
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
        mc_tile_entity *tile_entity = NULL;
        mc_live_tile_entity_error lookup = mc_live_tile_entities_empty_error();
        error->hook_bytecode = 12;
        if (mc_live_tile_entities_get(world, x, y, z, &tile_entity, &lookup) != MC_WORLD_STORAGE_OK)
            return tile_entity_failure(world, error, &lookup);
        mc_tile_entity_release(tile_entity);
    }
    error->hook_class = NULL;
    error->hook_method = NULL;
    error->hook_bytecode = 0;
    return true; /* ifnonnull15 and either branch both reach return18. */
}
