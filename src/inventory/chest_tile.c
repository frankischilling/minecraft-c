#include "../entity/player/tick_math_internal.h"
#include "chest_internal.h"
#include "mc/core/math/math.h"
#include "mc/entity/entity.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

typedef struct chest_failure {
    struct chest_failure *next;
    mc_live_tile_entity_error error;
    mc_world_access_failure owned[3];
} chest_failure;

typedef struct chest_tile {
    inv_budget budget;
    mc_tile_entity *tile;
    chest_failure *failures;
} chest_tile;

static mc_inventory_status status(mc_tile_entity_status value) {
    switch (value) {
        case MC_TILE_ENTITY_OK:
            return MC_INVENTORY_OK;
        case MC_TILE_ENTITY_INVALID_SLOT:
            return MC_INVENTORY_INDEX_OUT_OF_BOUNDS;
        case MC_TILE_ENTITY_ALLOCATION_FAILED:
            return MC_INVENTORY_ALLOCATION_FAILED;
        case MC_TILE_ENTITY_LIMIT:
            return MC_INVENTORY_LIMIT;
        case MC_TILE_ENTITY_ITEM_FAILED:
            return MC_INVENTORY_STACK_FAILED;
        default:
            return MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY;
    }
}

static mc_inventory_status finish(mc_inventory_error *e, mc_tile_entity_status value,
                                  const char *method, const char *desc, int32_t pc) {
    e->status = status(value);
    if (e->status != MC_INVENTORY_OK) {
        e->operation = "Chest actual JS Inventory child";
        if (method != NULL) {
            const bool dirty = method[0] == 'y';
            inv_frame(e, MC_REGISTRY_CLIENT, dirty ? "ow" : "js", dirty ? "ow" : "js", method, desc,
                      pc);
        }
        if (value == MC_TILE_ENTITY_INVALID_SLOT) {
            e->source_exception = true;
            e->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
        }
    }
    return e->status;
}

static chest_failure *receipt(chest_tile *o, mc_inventory_error *e) {
    chest_failure *r = inv_alloc(&o->budget, sizeof(*r), e);
    if (r != NULL)
        mc_live_tile_entities_error_init(&r->error);
    return r;
}

static mc_inventory_status record(chest_tile *o, chest_failure *r, mc_tile_entity_status value,
                                  const char *method, const char *desc, int32_t pc,
                                  mc_inventory_error *e) {
    if (value == MC_TILE_ENTITY_OK) {
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
            e->operation = "Chest failed callback producer occurrence capture";
            e->chest = &r->error;
            return e->status;
        }
    }
    e->chest = &r->error;
    e->mutation_reached = r->error.inventory_bytecode != 0 || r->error.tile_entity_bytecode != 0;
    e->notification_reached = r->error.tile_entity_bytecode != 0;
    return finish(e, value, method, desc, pc);
}

static void end_calls(void *context) {
    chest_tile *o = context;
    while (o->failures != NULL) {
        chest_failure *r = o->failures;
        o->failures = r->next;
        for (size_t i = 0; i < sizeof r->owned / sizeof *r->owned; ++i)
            mc_world_access_failure_release(&r->owned[i]);
        inv_free(&o->budget, r);
    }
}

static void release(void *context) {
    chest_tile *o = context;
    end_calls(context);
    mc_tile_entity_release(o->tile);
    inv_free(&o->budget, o);
}

static mc_inventory_status size(void *context, int32_t *out, mc_inventory_error *e) {
    chest_tile *o = context;
    return finish(e, mc_tile_entity_chest_size(o->tile, out), "a", "()I", 2);
}

static mc_inventory_status name(void *context, const char **out, mc_inventory_error *e) {
    (void)context;
    (void)e;
    *out = "Chest"; /* JS.c literal. */
    return MC_INVENTORY_OK;
}

static mc_inventory_status limit(void *context, int32_t *out, mc_inventory_error *e) {
    chest_tile *o = context;
    return finish(e, mc_tile_entity_chest_limit(o->tile, out), "d", "()I", 2);
}

static mc_inventory_status borrow(void *context, int32_t index, const mc_item_stack **out,
                                  mc_inventory_error *e) {
    chest_tile *o = context;
    mc_item_stack *value = NULL;
    const mc_tile_entity_status s = mc_tile_entity_chest_get(o->tile, index, &value);
    if (finish(e, s, "f_", "(I)Liz;", 5) == MC_INVENTORY_OK) {
        *out = value;
        mc_item_stack_release(value); /* The actual slot still owns the borrow. */
    }
    return e->status;
}

static mc_inventory_status set(void *context, int32_t index, mc_item_stack *value,
                               mc_inventory_error *e) {
    chest_tile *o = context;
    chest_failure *r = receipt(o, e);
    if (r == NULL)
        return e->status;
    const mc_tile_entity_status s =
        mc_tile_entity_chest_set_detailed(o->tile, index, value, &r->error);
    return record(o, r, s, "a", "(ILiz;)V", s == MC_TILE_ENTITY_INVALID_SLOT ? 6 : 31, e);
}

static mc_inventory_status split(void *context, int32_t index, int32_t amount, mc_item_stack **out,
                                 mc_inventory_error *e) {
    chest_tile *o = context;
    chest_failure *r = receipt(o, e);
    if (r == NULL)
        return e->status;
    const mc_tile_entity_status s =
        mc_tile_entity_chest_remove_detailed(o->tile, index, amount, out, &r->error);
    return record(o, r, s, "a", "(II)Liz;",
                  s == MC_TILE_ENTITY_INVALID_SLOT ? 5 : (int32_t)r->error.inventory_bytecode, e);
}

static mc_inventory_status changed(void *context, mc_inventory_error *e) {
    chest_tile *o = context;
    chest_failure *r = receipt(o, e);
    if (r == NULL)
        return e->status;
    const mc_tile_entity_status s = mc_tile_entity_dirty_detailed(o->tile, &r->error);
    return record(o, r, s, "y_", "()V", 24, e);
}

static mc_inventory_status usable(void *context, mc_entity *player, bool *out,
                                  mc_inventory_error *e) {
    chest_tile *o = context;
    mc_tile_entity_snapshot tile;
    if (mc_tile_entity_get_snapshot(o->tile, &tile) != MC_TILE_ENTITY_OK)
        return finish(e, MC_TILE_ENTITY_INVALID_PHASE, "a_", "(Lgs;)Z", 4);
    if (tile.world == NULL) {
        e->status = MC_INVENTORY_NULL_REFERENCE;
        e->operation = "JS eligibility actual null World receiver";
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        inv_frame(e, MC_REGISTRY_CLIENT, "js", "js", "a_", "(Lgs;)Z", 16);
        return e->status;
    }
    chest_failure *r = receipt(o, e);
    if (r == NULL)
        return e->status;
    mc_tile_entity *actual = NULL;
    const mc_world_storage_status world =
        mc_live_tile_entities_get(tile.world, tile.x, tile.y, tile.z, &actual, &r->error);
    if (world != MC_WORLD_STORAGE_OK)
        return record(o, r, MC_TILE_ENTITY_WORLD_FAILED, "a_", "(Lgs;)Z", 16, e);
    inv_free(&o->budget, r);
    const bool same = actual == o->tile;
    mc_tile_entity_release(actual);
    if (!same) {
        *out = false; /* A null Player is not used on this branch. */
        return MC_INVENTORY_OK;
    }
    if (player == NULL) {
        e->status = MC_INVENTORY_NULL_REFERENCE;
        e->operation = "JS eligibility actual null Player distance receiver";
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        inv_frame(e, MC_REGISTRY_CLIENT, "js", "js", "a_", "(Lgs;)Z", 53);
        return e->status;
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        e->status = MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY;
        e->operation = "JS eligibility native strict FP environment";
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
    *out = distance <= 64.0; /* JS uses DCMPL: NaN is not eligible. */
    return MC_INVENTORY_OK;
}

static const inv_chest_leaf leaves = {release, size,  name,    limit,  borrow,
                                      set,     split, changed, usable, end_calls};

mc_inventory_status mc_inventory_chest_from_tile(mc_registry_side side, mc_tile_entity *tile,
                                                 const mc_inventory_limits *limits,
                                                 mc_inventory_chest **out,
                                                 mc_inventory_error *result) {
    mc_inventory_error e = {0};
    inv_budget budget;
    mc_tile_entity_snapshot state;
    if (side != MC_REGISTRY_CLIENT || tile == NULL || out == NULL || *out != NULL ||
        !inv_limits(limits, &budget) ||
        mc_tile_entity_get_snapshot(tile, &state) != MC_TILE_ENTITY_OK ||
        state.kind != MC_TILE_ENTITY_CHEST) {
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
        e.operation = "Actual JS adapter native admission";
    } else {
        chest_tile *o = inv_alloc(&budget, sizeof(*o), &e);
        if (o != NULL) {
            o->budget = budget;
            if (finish(&e, mc_tile_entity_retain(tile), NULL, NULL, -1) == MC_INVENTORY_OK) {
                o->tile = tile;
                if (inv_chest_leaf_create(side, o, tile, &leaves, limits, out, &e) !=
                    MC_INVENTORY_OK)
                    release(o);
            } else
                inv_free(&o->budget, o);
        }
    }
    return inv_finish(result, &e);
}
