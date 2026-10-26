#include "piston_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../collision/blocks_internal.h"
#include "../construction_internal.h"
#include "../mutation/mutation_internal.h"
#include "../runtime_internal.h"
#include "mc/block/registry/runtime.h"
#include "piston_list_internal.h"
#include "piston_math_internal.h"

#include <string.h>

static float float_value(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/* fcmpl(value,1)>=0 excludes every NaN, including either signaling sign.
 * The raw k->l store precedes this comparison and requires no FP operation. */
static bool at_least_one(uint32_t bits) {
    const uint32_t magnitude = bits & UINT32_C(0x7fffffff);
    return (bits & UINT32_C(0x80000000)) == 0 && magnitude >= UINT32_C(0x3f800000) &&
           magnitude <= UINT32_C(0x7f800000);
}

/* Initialize caller storage without a full tick-error return temporary. The
 * typed static template supplies C-defined zero/null values and the sentinel;
 * each canonical child initializer supplies its complete nested defaults. */
static void initialize_error(mc_piston_tick_error *out) {
    static const mc_piston_tick_error zero = {.entity_index = SIZE_MAX};
    memcpy(out, &zero, sizeof(*out));
    mc_world_mutation_error_init(&out->mutation);
    mc_live_tile_entities_error_init(&out->tile_entity);
    out->registry = out->mutation.registry;
    mc_world_nearby_error_init(&out->nearby);
    mc_world_collision_error_init(&out->collision);
    mc_entity_move_error_init(&out->movement);
}

mc_piston_tick_error mc_piston_tick_empty_error(void) {
    mc_piston_tick_error result;
    initialize_error(&result);
    return result;
}

static bool fail(mc_piston_tick_error *error, mc_world_storage_status status,
                 const char *operation) {
    error->status = status;
    error->operation = operation;
    return false;
}

static mc_world_storage_status entity_status(mc_entity_status status) {
    switch (status) {
        case MC_ENTITY_OK:
            return MC_WORLD_STORAGE_OK;
        case MC_ENTITY_LIMIT:
            return MC_WORLD_STORAGE_LIMIT;
        case MC_ENTITY_ALLOCATION_FAILED:
            return MC_WORLD_STORAGE_ALLOCATION_FAILED;
        case MC_ENTITY_BUSY:
            return MC_WORLD_STORAGE_BUSY;
        case MC_ENTITY_INVALID_ARGUMENT:
            return MC_WORLD_STORAGE_INVALID_ARGUMENT;
        default:
            return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    }
}

static bool floating(mc_piston_tick_error *error) {
    if (mc_piston_math_admitted())
        return true;
    error->tile_entity.tile_entity.status = MC_TILE_ENTITY_UNSUPPORTED_FP;
    return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Piston tick/push FP admission");
}

static bool registry_result(mc_piston_tick_error *error, mc_registry_status status,
                            const char *operation) {
    return status == MC_REGISTRY_OK ||
           fail(error, mc_world_runtime_registry_status(status), operation);
}

static bool list_result(mc_piston_tick_error *error, const mc_piston_list_error *lower) {
    if (lower->status == MC_WORLD_STORAGE_OK)
        return true;
    error->entity.status = lower->entity_status;
    error->entity.operation = lower->operation;
    error->entity_index = lower->index;
    return fail(error, lower->status, lower->operation);
}

static bool push_held(mc_tile_entity *tile_entity, mc_world_runtime_classes *code, float progress,
                      float amount, const mc_entity_move_limits *limits,
                      mc_piston_tick_error *error) {
    /* The actual first float subtraction precedes even Block.ad active use.
     * This World argument is neither inspected nor entered ahead of a getter
     * that may return without using it. */
    const bool extending = tile_entity->state.piston.extending;
    error->push_bytecode = extending ? 16u : 9u;
    if (!floating(error))
        return false;
    progress = extending ? mc_piston_float_subtract(1.0f, progress)
                         : mc_piston_float_subtract(progress, 1.0f);
    error->push_bytecode = 18;
    const mc_block *moving = NULL;
    if (!registry_result(
            error, mc_registry_block_static_get(code->registry, "ad", &moving, &error->registry),
            "Piston push live ad active use"))
        return false;

    mc_world_runtime *world = tile_entity->world;
    const int32_t x = tile_entity->x, y = tile_entity->y, z = tile_entity->z;
    const int32_t id = tile_entity->state.piston.block_id;
    const int32_t facing = tile_entity->state.piston.facing;
    mc_entity_bounds *bounds = NULL;
    mc_world_entity_list_view nearby = {0};
    bool completed = false;
    error->push_bytecode = 46;
    const mc_world_storage_status geometry = mc_world_collision_piston_shifted_box_in_classes_held(
        code, world, moving, x, y, z, id, progress, facing, &bounds, &error->collision);
    if (geometry != MC_WORLD_STORAGE_OK) {
        (void)fail(error, geometry, error->collision.nearby.query.operation);
        goto cleanup;
    }
    if (bounds == NULL) {
        completed = true;
        goto cleanup;
    }

    /* Geometry may synchronously rebind this TileEntity. The later World receiver
     * is loaded again, while its actual returned pooled box remains the same. */
    world = tile_entity->world;
    error->push_bytecode = 60;
    const mc_world_storage_status gathered =
        mc_world_runtime_nearby_entities(world, NULL, bounds, &nearby, &error->nearby);
    if (gathered != MC_WORLD_STORAGE_OK) {
        (void)fail(error, gathered, error->nearby.query.operation);
        goto cleanup;
    }
    error->push_bytecode = 67;
    size_t count;
    const mc_world_storage_status counted = mc_world_entity_list_view_count(&nearby, &count);
    if (counted != MC_WORLD_STORAGE_OK) {
        (void)fail(error, counted, "Piston nearby List isEmpty");
        goto cleanup;
    }
    if (count == 0) {
        completed = true;
        goto cleanup; /* Old shared m is deliberately not cleared here. */
    }

    mc_piston_list_error list;
    error->push_bytecode = 80;
    (void)mc_tile_entity_piston_entities_append(tile_entity, &nearby, &list);
    if (!list_result(error, &list))
        goto cleanup;
    mc_piston_entity_iterator iterator;
    error->push_bytecode = 89;
    (void)mc_tile_entity_piston_entities_iterator(tile_entity, &iterator, &list);
    if (!list_result(error, &list))
        goto cleanup;
    for (;;) {
        bool has_next;
        error->push_bytecode = 98;
        (void)mc_tile_entity_piston_entities_has_next(&iterator, &has_next, &list);
        if (!list_result(error, &list))
            goto cleanup;
        if (!has_next)
            break;
        error->entity_index = iterator.cursor;
        mc_entity *entity = NULL;
        error->push_bytecode = 108;
        (void)mc_tile_entity_piston_entities_next(&iterator, &entity, &list);
        if (!list_result(error, &list))
            goto cleanup;
        static const mc_registry_direction_field fields[3] = {
            MC_REGISTRY_DIRECTION_B, MC_REGISTRY_DIRECTION_C, MC_REGISTRY_DIRECTION_D};
        static const unsigned int pcs[3] = {121, 133, 145};
        double offsets[3];
        bool moved = true;
        for (size_t axis = 0; axis < 3; ++axis) {
            mc_registry_direction_view direction;
            error->push_bytecode = pcs[axis];
            if (!registry_result(error,
                                 mc_registry_direction_get(code->registry, fields[axis], &direction,
                                                           &error->registry),
                                 "Piston push Direction active use")) {
                moved = false;
                break;
            }
            /* Each getstatic precedes its own fresh TileEntity.facing read. */
            const int32_t actual_facing = tile_entity->state.piston.facing;
            int32_t component;
            error->push_bytecode = pcs[axis] + 7u;
            if (!registry_result(error,
                                 mc_registry_direction_at(&direction, actual_facing, &component,
                                                          &error->registry),
                                 "Piston push Direction index")) {
                moved = false;
                break;
            }
            error->push_bytecode = pcs[axis] + 9u;
            if (!floating(error)) {
                moved = false;
                break;
            }
            offsets[axis] = (double)mc_piston_float_multiply(amount, (float)component);
        }
        if (moved) {
            error->push_bytecode = 156;
            const mc_entity_status status = mc_entity_move(entity, offsets[0], offsets[1],
                                                           offsets[2], limits, &error->movement);
            if (status != MC_ENTITY_OK)
                moved = fail(error, entity_status(status), error->movement.operation);
        }
        mc_entity_release(entity);
        if (!moved)
            goto cleanup; /* Actual clear165 is absent on every failure. */
    }
    error->push_bytecode = 165;
    (void)mc_tile_entity_piston_entities_clear(tile_entity, &list);
    completed = list_result(error, &list);
cleanup:
    mc_world_entity_list_view_release(&nearby);
    if (bounds != NULL)
        (void)mc_entity_bounds_release(code->entities, bounds);
    return completed;
}

static bool tick_held(mc_tile_entity *tile_entity, mc_world_runtime_classes *code,
                      const mc_entity_move_limits *limits, mc_piston_tick_error *error) {
    tile_entity->state.piston.previous_progress_bits =
        tile_entity->state.piston.progress_bits; /* store5 */
    if (at_least_one(tile_entity->state.piston.previous_progress_bits)) {
        error->tick_bytecode = 21;
        if (!push_held(tile_entity, code, 1.0f, 0.25f, limits, error))
            return false;
        error->push_bytecode = 0;
        mc_world_runtime *world = tile_entity->world;
        const int32_t x = tile_entity->x, y = tile_entity->y, z = tile_entity->z;
        error->tick_bytecode = 40;
        const mc_world_storage_status removed =
            mc_live_tile_entities_remove(world, x, y, z, &error->tile_entity);
        if (removed != MC_WORLD_STORAGE_OK)
            return fail(error, removed, error->tile_entity.operation);
        error->tick_bytecode = 44;
        const mc_tile_entity_status invalidated = mc_tile_entity_set_invalid(tile_entity, true);
        if (invalidated != MC_TILE_ENTITY_OK) {
            error->tile_entity.tile_entity.status = invalidated;
            return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                        "Piston tick invalidation receiver");
        }
        int32_t id;
        error->tick_bytecode = 63;
        if (!mc_tile_entity_piston_current_block_held(tile_entity, &id, &error->mutation))
            return fail(error, error->mutation.status, error->mutation.operation);
        const mc_block *moving = NULL;
        error->tick_bytecode = 66;
        if (!registry_result(
                error,
                mc_registry_block_static_get(code->registry, "ad", &moving, &error->registry),
                "Piston tick live ad active use"))
            return false;
        error->tick_bytecode = 69;
        if (moving == NULL) {
            error->registry.status = MC_REGISTRY_NULL_REFERENCE;
            error->registry.message = "Piston tick live ad receiver is null";
            return fail(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, error->registry.message);
        }
        if (id != moving->fields.bn)
            return true;
        world = tile_entity->world;
        const int32_t restore_x = tile_entity->x, restore_y = tile_entity->y,
                      restore_z = tile_entity->z;
        const int32_t carried_id = tile_entity->state.piston.block_id;
        const int32_t carried_data = tile_entity->state.piston.block_data;
        bool ordinary;
        error->tick_bytecode = 99;
        const mc_world_storage_status restored =
            mc_world_runtime_block_metadata_set(world, restore_x, restore_y, restore_z, carried_id,
                                                carried_data, &ordinary, &error->mutation);
        return restored == MC_WORLD_STORAGE_OK ||
               fail(error, restored, error->mutation.operation); /* Discard pop102. */
    }

    error->tick_bytecode = 111;
    if (!floating(error))
        return false;
    const float next =
        mc_piston_float_add(float_value(tile_entity->state.piston.progress_bits), 0.5f);
    tile_entity->state.piston.progress_bits = float_bits(next); /* store112, even NaN. */
    if (at_least_one(tile_entity->state.piston.progress_bits))
        tile_entity->state.piston.progress_bits = UINT32_C(0x3f800000); /* store126 */
    if (!tile_entity->state.piston.extending)
        return true;
    const float current = float_value(tile_entity->state.piston.progress_bits);
    const float delta =
        mc_piston_float_subtract(float_value(tile_entity->state.piston.progress_bits),
                                 float_value(tile_entity->state.piston.previous_progress_bits));
    const float amount = mc_piston_float_add(delta, 0.0625f);
    error->tick_bytecode = 153;
    return push_held(tile_entity, code, current, amount, limits, error);
}

mc_world_storage_status mc_tile_entity_piston_tick(mc_tile_entity *tile_entity,
                                                   const mc_entity_move_limits *move_limits,
                                                   mc_piston_tick_error *error) {
    mc_piston_tick_error frame;
    initialize_error(&frame);
    bool retained = false;
    if (tile_entity == NULL) {
        frame.tile_entity.tile_entity.status = MC_TILE_ENTITY_INVALID_ARGUMENT;
        (void)fail(&frame, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Piston tick receiver");
    } else if (tile_entity->kind != MC_TILE_ENTITY_PISTON) {
        frame.tile_entity.tile_entity.status = MC_TILE_ENTITY_INVALID_KIND;
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Piston tick receiver kind");
    } else if (mc_tile_entity_piston_code_classes_held(tile_entity) == NULL) {
        frame.tile_entity.tile_entity.status = MC_TILE_ENTITY_INVALID_PHASE;
        (void)fail(&frame, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Piston tick receiver phase");
    } else {
        const mc_tile_entity_status held = mc_tile_entity_retain(tile_entity);
        if (held != MC_TILE_ENTITY_OK) {
            frame.tile_entity.tile_entity.status = held;
            (void)fail(&frame,
                       held == MC_TILE_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                                    : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Piston tick receiver retain");
        } else {
            retained = true;
            if (tick_held(tile_entity, mc_tile_entity_piston_code_classes_held(tile_entity),
                          move_limits, &frame))
                initialize_error(&frame);
        }
    }
    if (retained)
        mc_tile_entity_release(tile_entity);
    if (error != NULL)
        *error = frame;
    return frame.status;
}
