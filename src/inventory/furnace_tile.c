#include "../entity/player/tick_math_internal.h"
#include "internal.h"
#include "mc/entity/entity.h"
#include "mc/world/runtime/tile_entities/furnace.h"

typedef struct furnace_failure {
    struct furnace_failure *next;
    mc_live_tile_entity_error error;
    mc_world_access_failure owned[3];
} furnace_failure;

typedef struct furnace_tile {
    inv_budget budget;
    mc_tile_entity *tile;
    furnace_failure *failures;
} furnace_tile;

static mc_inventory_status finish(mc_inventory_error *e, mc_tile_entity_status status,
                                  const char *method, const char *desc, int32_t pc) {
    if (status == MC_TILE_ENTITY_OK)
        return MC_INVENTORY_OK;
    e->status = status == MC_TILE_ENTITY_INVALID_SLOT        ? MC_INVENTORY_INDEX_OUT_OF_BOUNDS
                : status == MC_TILE_ENTITY_LIMIT             ? MC_INVENTORY_LIMIT
                : status == MC_TILE_ENTITY_ALLOCATION_FAILED ? MC_INVENTORY_ALLOCATION_FAILED
                : status == MC_TILE_ENTITY_ITEM_FAILED       ? MC_INVENTORY_STACK_FAILED
                                                       : MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY;
    e->operation = "Furnace actual SK Inventory child";
    if (method != NULL)
        inv_frame(e, MC_REGISTRY_CLIENT, method[0] == 'y' ? "ow" : "sk",
                  method[0] == 'y' ? "ow" : "sk", method, desc, pc);
    if (status == MC_TILE_ENTITY_INVALID_SLOT) {
        e->source_exception = true;
        e->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
    }
    return e->status;
}

static furnace_failure *receipt(furnace_tile *o, mc_inventory_error *e) {
    furnace_failure *r = inv_alloc(&o->budget, sizeof(*r), e);
    if (r != NULL)
        mc_live_tile_entities_error_init(&r->error);
    return r;
}

static mc_inventory_status record(furnace_tile *o, furnace_failure *r, mc_tile_entity_status status,
                                  const char *method, const char *desc, int32_t pc,
                                  mc_inventory_error *e) {
    if (status == MC_TILE_ENTITY_OK) {
        inv_free(&o->budget, r);
        return MC_INVENTORY_OK;
    }
    r->next = o->failures;
    o->failures = r;
    const mc_world_access_failure *views[] = {&r->error.world_access_failure,
                                              &r->error.publication.world_access_failure,
                                              &r->error.batch.publication.world_access_failure};
    for (size_t i = 0; i < sizeof views / sizeof *views; ++i) {
        if (mc_world_access_failure_source_exception(views[i], &e->exception_class))
            e->source_exception = true;
        if (views[i]->receipt != NULL &&
            mc_world_access_failure_retain(views[i], &r->owned[i]) != MC_WORLD_STORAGE_OK) {
            e->status = MC_INVENTORY_LIMIT;
            e->operation = "Furnace failed callback producer occurrence capture";
            e->furnace = &r->error;
            return e->status;
        }
    }
    e->furnace = &r->error;
    e->mutation_reached = r->error.tile_entity_bytecode != 0;
    e->notification_reached = r->error.tile_entity_bytecode != 0;
    return finish(e, status, method, desc, pc);
}

static void end_calls(void *context) {
    furnace_tile *o = context;
    while (o->failures != NULL) {
        furnace_failure *r = o->failures;
        o->failures = r->next;
        for (size_t i = 0; i < sizeof r->owned / sizeof *r->owned; ++i)
            mc_world_access_failure_release(&r->owned[i]);
        inv_free(&o->budget, r);
    }
}

static void release(void *context) {
    furnace_tile *o = context;
    end_calls(context);
    mc_tile_entity_release(o->tile);
    inv_free(&o->budget, o);
}

static mc_inventory_status size(void *context, int32_t *out, mc_inventory_error *e) {
    return finish(e, mc_tile_entity_furnace_size(((furnace_tile *)context)->tile, out), "a", "()I",
                  4);
}

static mc_inventory_status borrow(void *context, int32_t index, const mc_item_stack **out,
                                  mc_inventory_error *e) {
    mc_item_stack *value = NULL;
    if (finish(e, mc_tile_entity_furnace_get(((furnace_tile *)context)->tile, index, &value), "f_",
               "(I)Liz;", 5) == MC_INVENTORY_OK) {
        *out = value;
        mc_item_stack_release(value);
    }
    return e->status;
}

static mc_inventory_status set(void *context, int32_t index, mc_item_stack *value,
                               mc_inventory_error *e) {
    return finish(e, mc_tile_entity_furnace_set(((furnace_tile *)context)->tile, index, value), "a",
                  "(ILiz;)V", 6);
}

static mc_inventory_status split(void *context, int32_t index, int32_t amount, mc_item_stack **out,
                                 mc_inventory_error *e) {
    return finish(
        e, mc_tile_entity_furnace_remove(((furnace_tile *)context)->tile, index, amount, out), "a",
        "(II)Liz;", 5);
}

static mc_inventory_status changed(void *context, mc_inventory_error *e) {
    furnace_tile *o = context;
    furnace_failure *r = receipt(o, e);
    if (r == NULL)
        return e->status;
    return record(o, r, mc_tile_entity_dirty_detailed(o->tile, &r->error), "y_", "()V", 24, e);
}

static mc_inventory_status usable(void *context, mc_entity *player, bool *out,
                                  mc_inventory_error *e) {
    furnace_tile *o = context;
    mc_tile_entity_snapshot tile;
    if (mc_tile_entity_get_snapshot(o->tile, &tile) != MC_TILE_ENTITY_OK)
        return finish(e, MC_TILE_ENTITY_INVALID_PHASE, "a_", "(Lgs;)Z", 4);
    if (tile.world == NULL) {
        e->status = MC_INVENTORY_NULL_REFERENCE;
        e->operation = "SK eligibility actual null World receiver";
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        inv_frame(e, MC_REGISTRY_CLIENT, "sk", "sk", "a_", "(Lgs;)Z", 16);
        return e->status;
    }
    furnace_failure *r = receipt(o, e);
    if (r == NULL)
        return e->status;
    mc_tile_entity *actual = NULL;
    if (mc_live_tile_entities_get(tile.world, tile.x, tile.y, tile.z, &actual, &r->error) !=
        MC_WORLD_STORAGE_OK)
        return record(o, r, MC_TILE_ENTITY_WORLD_FAILED, "a_", "(Lgs;)Z", 16, e);
    inv_free(&o->budget, r);
    const bool same = actual == o->tile;
    mc_tile_entity_release(actual);
    if (!same) {
        *out = false;
        return MC_INVENTORY_OK;
    }
    if (player == NULL) {
        e->status = MC_INVENTORY_NULL_REFERENCE;
        e->operation = "SK eligibility actual null Player distance receiver";
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        inv_frame(e, MC_REGISTRY_CLIENT, "sk", "sk", "a_", "(Lgs;)Z", 53);
        return e->status;
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        e->status = MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY;
        e->operation = "SK eligibility native strict FP environment";
        return e->status;
    }
    mc_entity_snapshot entity;
    if (mc_entity_get_snapshot(player, &entity) != MC_ENTITY_OK ||
        mc_tile_entity_get_snapshot(o->tile, &tile) != MC_TILE_ENTITY_OK)
        return finish(e, MC_TILE_ENTITY_STATE_ERROR, "a_", "(Lgs;)Z", 53);
    const double dx = at_sub(entity.base.x, at_add((double)tile.x, .5));
    const double dy = at_sub(entity.base.y, at_add((double)tile.y, .5));
    const double dz = at_sub(entity.base.z, at_add((double)tile.z, .5));
    const double distance = at_add(at_add(at_mul(dx, dx), at_mul(dy, dy)), at_mul(dz, dz));
    *out = !(distance > 64.0); /* Actual DCMPL59/IFLE65 admits NaN. */
    return MC_INVENTORY_OK;
}

static mc_inventory_status burning(void *context, bool *out, mc_inventory_error *e) {
    return finish(e, mc_tile_entity_furnace_burning(((furnace_tile *)context)->tile, out), "b",
                  "()Z", 1);
}

static mc_inventory_status cook_scale(void *context, int32_t scale, int32_t *out,
                                      mc_inventory_error *e) {
    return finish(e, mc_tile_entity_furnace_cook_scale(((furnace_tile *)context)->tile, scale, out),
                  "b", "(I)I", 1);
}

static mc_inventory_status burn_scale(void *context, int32_t scale, int32_t *out,
                                      mc_inventory_error *e) {
    return finish(e, mc_tile_entity_furnace_burn_scale(((furnace_tile *)context)->tile, scale, out),
                  "c", "(I)I", 1);
}

static mc_inventory_status counters(void *context, int32_t *cook, int32_t *burn, int32_t *fuel,
                                    mc_inventory_error *e) {
    mc_furnace_fields fields;
    if (finish(e, mc_tile_entity_furnace_fields_get(((furnace_tile *)context)->tile, &fields), NULL,
               NULL, -1) == MC_INVENTORY_OK) {
        *cook = fields.cook_progress;
        *burn = fields.burn_remaining;
        *fuel = fields.fuel_duration;
    }
    return e->status;
}

static mc_inventory_status progress(void *context, int32_t index, int32_t value,
                                    mc_inventory_error *e) {
    const mc_furnace_field field = index == 0   ? MC_FURNACE_COOK_PROGRESS
                                   : index == 1 ? MC_FURNACE_BURN_REMAINING
                                                : MC_FURNACE_FUEL_DURATION;
    return finish(e,
                  mc_tile_entity_furnace_field_set(((furnace_tile *)context)->tile, field, value),
                  NULL, NULL, -1);
}

static const inv_furnace_leaf leaf = {release,    end_calls, size,    borrow,  set,
                                      split,      changed,   usable,  burning, cook_scale,
                                      burn_scale, counters,  progress};

mc_inventory_status mc_inventory_furnace_from_tile(mc_registry_side side, mc_tile_entity *tile,
                                                   const mc_inventory_limits *limits,
                                                   mc_inventory_furnace **out,
                                                   mc_inventory_error *result) {
    mc_inventory_error e = {0};
    inv_budget budget;
    mc_tile_entity_snapshot state;
    if (side != MC_REGISTRY_CLIENT || tile == NULL || out == NULL || *out != NULL ||
        !inv_limits(limits, &budget) ||
        mc_tile_entity_get_snapshot(tile, &state) != MC_TILE_ENTITY_OK ||
        state.kind != MC_TILE_ENTITY_FURNACE) {
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
        e.operation = "Actual SK adapter native admission";
    } else {
        furnace_tile *o = inv_alloc(&budget, sizeof(*o), &e);
        if (o != NULL) {
            o->budget = budget;
            if (finish(&e, mc_tile_entity_retain(tile), NULL, NULL, -1) == MC_INVENTORY_OK) {
                o->tile = tile;
                if (inv_furnace_leaf_create(side, o, tile, &leaf, limits, out, &e) !=
                    MC_INVENTORY_OK)
                    release(o);
            } else
                inv_free(&o->budget, o);
        }
    }
    return inv_finish(result, &e);
}
