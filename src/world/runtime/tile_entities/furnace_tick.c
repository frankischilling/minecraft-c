#include "mc/world/runtime/tile_entities/furnace_tick.h"

#include "../../../item/stack/source_internal.h"
#include "../mutation/furnace/furnace_internal.h"
#include "../mutation/mutation_internal.h"
#include "../runtime_internal.h"
#include "tick_internal.h"
#include "tile_entities_internal.h"

#include "mc/core/java/java.h"

#include <stdlib.h>
#include <string.h>

struct mc_furnace_tick_work {
    mc_tile_entity *tile;
    mc_world_runtime *worlds[2];
    size_t world_count, receipt_count;
    mc_world_access_failure receipts[16];
    mc_world_mutation_error mutation;
    mc_live_tile_entity_error dirty;
    bool mutation_entered, dirty_entered, active;
};

static mc_world_storage_status fail(mc_furnace_tick_error *e, mc_world_storage_status status,
                                    const char *operation) {
    e->status = status;
    e->operation = operation;
    return status;
}

static bool tile_result(mc_furnace_tick_error *e, mc_tile_entity_status status) {
    if (status == MC_TILE_ENTITY_OK)
        return true;
    const mc_world_storage_status mapped = status == MC_TILE_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                                           : status == MC_TILE_ENTITY_ALLOCATION_FAILED
                                               ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                               : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    (void)fail(e, mapped,
               e->tile.operation != NULL ? e->tile.operation
                                         : "Furnace actual source Tile/Stack operation");
    return false;
}

static bool item_result(mc_furnace_tick_error *e, mc_item_stack_status status) {
    if (status == MC_ITEM_STACK_OK)
        return true;
    e->tile.status = MC_TILE_ENTITY_ITEM_FAILED;
    e->tile.item.status = status;
    return tile_result(e, MC_TILE_ENTITY_ITEM_FAILED);
}

static bool work(mc_furnace_tick_error *e, mc_tile_entity *tile) {
    if (e->work != NULL)
        return true;
    mc_furnace_tick_work *w = calloc(1, sizeof(*w));
    if (w == NULL) {
        (void)fail(e, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                   "Furnace reached World child diagnostics");
        return false;
    }
    const mc_tile_entity_status held = mc_tile_entity_retain(tile);
    if (held != MC_TILE_ENTITY_OK) {
        free(w);
        return tile_result(e, held);
    }
    w->tile = tile;
    w->active = true;
    mc_world_mutation_error_init(&w->mutation);
    mc_live_tile_entities_error_init(&w->dirty);
    e->work = w;
    return true;
}

static bool hold_world(mc_furnace_tick_error *e, mc_world_runtime *world) {
    if (world == NULL)
        return true;
    mc_furnace_tick_work *w = e->work;
    for (size_t i = 0; i < w->world_count; ++i)
        if (w->worlds[i] == world)
            return true;
    if (w->world_count == sizeof(w->worlds) / sizeof(*w->worlds)) {
        (void)fail(e, MC_WORLD_STORAGE_LIMIT, "Furnace evaluated World owner extent");
        return false;
    }
    if (!mc_world_runtime_binding_retain(world)) {
        (void)fail(e, MC_WORLD_STORAGE_BUSY, "Furnace evaluated World occurrence");
        return false;
    }
    w->worlds[w->world_count++] = world;
    return true;
}

static bool receipt(mc_furnace_tick_error *e, const mc_world_access_failure *value) {
    if (value->receipt == NULL)
        return true;
    mc_furnace_tick_work *w = e->work;
    for (size_t i = 0; i < w->receipt_count; ++i)
        if (w->receipts[i].receipt == value->receipt)
            return true;
    if (w->receipt_count == sizeof(w->receipts) / sizeof(*w->receipts)) {
        e->cleanup =
            (mc_world_storage_error){MC_WORLD_STORAGE_LIMIT, "Furnace retained callback extent", 0};
        return false;
    }
    const mc_world_storage_status status =
        mc_world_access_failure_retain(value, &w->receipts[w->receipt_count]);
    if (status != MC_WORLD_STORAGE_OK) {
        e->cleanup = (mc_world_storage_error){status, "Furnace callback producer occurrence", 0};
        return false;
    }
    ++w->receipt_count;
    return true;
}

static bool tile_receipts(mc_furnace_tick_error *e, const mc_live_tile_entity_error *t) {
    return receipt(e, &t->world_access_failure) &&
           receipt(e, &t->publication.world_access_failure) &&
           receipt(e, &t->batch.publication.world_access_failure);
}

static bool capture(mc_furnace_tick_error *e) {
    mc_furnace_tick_work *w = e->work;
    if (w == NULL)
        return true;
    if (w->dirty_entered && !tile_receipts(e, &w->dirty))
        return false;
    if (!w->mutation_entered)
        return true;
    const mc_world_mutation_error *m = &w->mutation;
    return receipt(e, &m->world_access_failure) &&
           receipt(e, &m->publication.world_access_failure) &&
           receipt(e, &m->batch.publication.world_access_failure) &&
           tile_receipts(e, &m->tile_entity) &&
           receipt(e, &m->nearby.batch.publication.world_access_failure) &&
           tile_receipts(e, &m->nearby.tile_entity) &&
           receipt(e, &m->power.access.batch.publication.world_access_failure) &&
           tile_receipts(e, &m->power.access.tile_entity) &&
           receipt(e, &m->rail_access.nearby.batch.publication.world_access_failure) &&
           tile_receipts(e, &m->rail_access.nearby.tile_entity);
}

bool mc_tile_entity_furnace_tick_retire(mc_furnace_tick_error *e) {
    if (e == NULL)
        return false;
    mc_furnace_tick_work *w = e->work;
    if (w != NULL) {
        if (w->active) {
            e->cleanup = (mc_world_storage_error){MC_WORLD_STORAGE_BUSY,
                                                  "Furnace active tick retirement", 0};
            return false;
        }
        if (!capture(e))
            return false;
        for (size_t i = 0; i < w->receipt_count; ++i)
            mc_world_access_failure_release(&w->receipts[i]);
        for (size_t i = 0; i < w->world_count; ++i)
            mc_world_runtime_binding_release(w->worlds[i]);
        mc_tile_entity_release(w->tile);
        free(w);
    }
    memset(e, 0, sizeof(*e));
    return true;
}

mc_world_storage_status mc_tile_entity_furnace_tick(mc_tile_entity *tile,
                                                    mc_furnace_tick_error *e) {
    if (e == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (e->work != NULL)
        return MC_WORLD_STORAGE_BUSY;
    memset(e, 0, sizeof(*e));
    mc_tile_entity_error_init(&e->tile);
    if (tile == NULL || tile->kind != MC_TILE_ENTITY_FURNACE || tile->phase != MC_TILE_ENTITY_READY)
        return fail(e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "Furnace actual held READY source owner");
    mc_tile_entity_status held = mc_tile_entity_retain(tile);
    if (!tile_result(e, held))
        return e->status;
    mc_registry_classes *registry = mc_live_tile_tick_code_registry(tile);
    bool dirty = false;
    const bool was_burning = tile->state.furnace.burn_remaining > 0;
    e->bytecode = 29;
    if (tile->state.furnace.burn_remaining > 0)
        tile->state.furnace.burn_remaining = mc_java_i32_sub(tile->state.furnace.burn_remaining, 1);
    e->bytecode = 36;
    mc_world_runtime *world = tile->world;
    if (world == NULL) {
        e->source_null = true;
        (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Furnace null World.B field receiver");
        goto leave;
    }
    if (!world->fields.B) {
        bool possible;
        if (tile->state.furnace.burn_remaining == 0) {
            e->bytecode = 50;
            if (!tile_result(e,
                             mc_tile_entity_furnace_can_smelt(tile, registry, &possible, &e->tile)))
                goto leave;
            if (possible) {
                e->bytecode = 64;
                if (tile->state.furnace.slots == NULL || tile->state.furnace.capacity != 3) {
                    (void)fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "Furnace actual fuel array");
                    goto leave;
                }
                int32_t duration;
                e->bytecode = 65;
                if (!tile_result(e,
                                 mc_tile_entity_furnace_fuel(registry, tile->state.furnace.slots[1],
                                                             &duration, &e->tile)))
                    goto leave;
                tile->state.furnace.burn_remaining = duration; /*69 before72. */
                tile->state.furnace.fuel_duration = duration;
                if (tile->state.furnace.burn_remaining > 0) {
                    dirty = true;
                    if (tile->state.furnace.slots[1] != NULL) {
                        int32_t count;
                        e->bytecode = 100;
                        if (!item_result(e, mc_item_stack_source_count_get(
                                                tile->state.furnace.slots[1], &count)))
                            goto leave;
                        e->bytecode = 105;
                        if (!item_result(
                                e, mc_item_stack_source_count_set(tile->state.furnace.slots[1],
                                                                  mc_java_i32_sub(count, 1))))
                            goto leave;
                        e->bytecode = 114;
                        if (!item_result(e, mc_item_stack_source_count_get(
                                                tile->state.furnace.slots[1], &count)))
                            goto leave;
                        if (count == 0) {
                            mc_item_stack *spent = tile->state.furnace.slots[1];
                            tile->state.furnace.slots[1] = NULL;
                            mc_item_stack_release(spent); /* Actual clear126, no bucket. */
                        }
                    }
                }
            }
        }
        e->bytecode = 128;
        if (tile->state.furnace.burn_remaining > 0) {
            e->bytecode = 135;
            if (!tile_result(e,
                             mc_tile_entity_furnace_can_smelt(tile, registry, &possible, &e->tile)))
                goto leave;
        } else
            possible = false;
        if (possible) {
            tile->state.furnace.cook_progress =
                mc_java_i32_add(tile->state.furnace.cook_progress, 1);
            if (tile->state.furnace.cook_progress == 200) {
                tile->state.furnace.cook_progress = 0; /*163 precedes actual K167. */
                e->bytecode = 167;
                if (!tile_result(e, mc_tile_entity_furnace_smelt(tile, registry, &e->tile)))
                    goto leave;
                dirty = true;
            }
        } else
            tile->state.furnace.cook_progress = 0;
        if (was_burning != (tile->state.furnace.burn_remaining > 0)) {
            dirty = true;
            const bool burning = tile->state.furnace.burn_remaining > 0;
            world = tile->world; /* Actual evaluated World211, before coordinates. */
            const int32_t x = tile->x, y = tile->y, z = tile->z;
            e->bytecode = 226;
            if (!work(e, tile) || !hold_world(e, world))
                goto leave;
            e->work->mutation_entered = true;
            e->mutation = &e->work->mutation;
            if (!mc_world_mutation_furnace_switch(registry, world, burning, x, y, z,
                                                  &e->work->mutation)) {
                e->source_null = e->mutation->furnace_source_null;
                (void)fail(e, e->mutation->status, e->mutation->operation);
                goto leave;
            }
        }
    }
    if (dirty) {
        e->bytecode = 234;
        if (!work(e, tile) || !hold_world(e, tile->world))
            goto leave;
        e->work->dirty_entered = true;
        e->dirty = &e->work->dirty;
        if (!tile_result(e, mc_tile_entity_dirty_detailed(tile, &e->work->dirty))) {
            if (e->dirty->status != MC_WORLD_STORAGE_OK)
                (void)fail(e, e->dirty->status, e->dirty->operation);
            goto leave;
        }
    }
    e->bytecode = 237;
    e->returned = true;
leave:
    mc_tile_entity_release(tile);
    if (e->work != NULL) {
        e->work->active = false;
        if (!capture(e) && e->status == MC_WORLD_STORAGE_OK)
            (void)fail(e, e->cleanup.status, e->cleanup.operation);
    }
    if (e->status == MC_WORLD_STORAGE_OK) {
        if (!mc_tile_entity_furnace_tick_retire(e))
            return fail(e, e->cleanup.status, e->cleanup.operation);
        mc_tile_entity_error_init(&e->tile);
        e->bytecode = 237;
        e->returned = true;
    }
    return e->status;
}
