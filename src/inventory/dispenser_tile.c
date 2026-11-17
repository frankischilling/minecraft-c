#include "dispenser_internal.h"
#include "internal.h"

#include "../entity/player/tick_math_internal.h"
#include "mc/entity/entity.h"
#include "mc/world/runtime/registry_failure.h"
#include "mc/world/runtime/tile_entities/dispenser.h"

#include <string.h>

typedef struct dispenser_failure {
    struct dispenser_failure *next;
    mc_live_tile_entity_error detail;
    mc_world_access_failure captures[3];
    size_t count;
} dispenser_failure;

typedef struct dispenser_tile {
    inv_budget budget;
    size_t failure_count;
    uintptr_t diagnostic_owner_id;
    mc_tile_entity *tile;
    dispenser_failure *failures;
    mc_inventory_error cleanup;
} dispenser_tile;

static mc_inventory_status tile_result(mc_inventory_error *e, mc_tile_entity_status status,
                                       const char *method, const char *descriptor, int32_t pc) {
    if (status == MC_TILE_ENTITY_OK)
        return MC_INVENTORY_OK;
    e->status = status == MC_TILE_ENTITY_INVALID_SLOT        ? MC_INVENTORY_INDEX_OUT_OF_BOUNDS
                : status == MC_TILE_ENTITY_LIMIT             ? MC_INVENTORY_LIMIT
                : status == MC_TILE_ENTITY_ALLOCATION_FAILED ? MC_INVENTORY_ALLOCATION_FAILED
                : status == MC_TILE_ENTITY_ITEM_FAILED       ? MC_INVENTORY_STACK_FAILED
                                                       : MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY;
    e->operation = "Dispenser actual AZ Inventory child";
    if (method != NULL) {
        const char *declaring = strcmp(method, "y_") == 0 ? "ow" : "az";
        inv_frame(e, MC_REGISTRY_CLIENT, declaring, declaring, method, descriptor, pc);
    }
    if (status == MC_TILE_ENTITY_INVALID_SLOT) {
        e->source_exception = true;
        e->exception_class = "java/lang/ArrayIndexOutOfBoundsException";
    }
    return e->status;
}

static void cleanup_failure(dispenser_tile *o, mc_world_storage_status status,
                            const char *operation) {
    if (o->cleanup.status != MC_INVENTORY_OK)
        return;
    o->cleanup.status = status == MC_WORLD_STORAGE_LIMIT  ? MC_INVENTORY_LIMIT
                        : status == MC_WORLD_STORAGE_BUSY ? MC_INVENTORY_BUSY
                                                          : MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY;
    o->cleanup.operation = operation;
}

static void expire(mc_world_access_failure *f) {
    f->receiver = NULL;
    f->render_global = NULL;
    f->receipt = NULL;
}

static void capture(dispenser_tile *o, dispenser_failure *r, mc_world_access_failure *view,
                    mc_inventory_error *e) {
    const char *exception = NULL;
    const bool source = mc_world_access_failure_source_exception(view, &exception);
    if (source) {
        e->source_exception = true;
        e->exception_class = exception;
        e->exception_class_from_dispenser = view->receipt != NULL;
    }
    if (view->receipt == NULL) {
        if (view->receiver != NULL || view->render_global != NULL) {
            cleanup_failure(o, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                            "AZ child has no retainable producer receipt");
            expire(view);
            if (source)
                e->exception_class = NULL;
        }
        return;
    }
    for (size_t i = 0; i < r->count; ++i)
        if (r->captures[i].receipt == view->receipt)
            return;
    const mc_world_storage_status status =
        mc_world_access_failure_retain(view, &r->captures[r->count]);
    if (status == MC_WORLD_STORAGE_OK)
        ++r->count;
    else {
        cleanup_failure(o, status, "AZ actual failed child producer capture");
        expire(view);
        if (source)
            e->exception_class = NULL;
    }
}

static dispenser_failure *prepare(dispenser_tile *o, mc_inventory_error *e) {
    if (o->failure_count >= o->budget.max_slots) {
        e->status = MC_INVENTORY_LIMIT;
        e->operation = "AZ retained failed-call native count bound";
        return NULL;
    }
    dispenser_failure *r = inv_alloc(&o->budget, sizeof(*r), e);
    if (r != NULL)
        mc_live_tile_entities_error_init(&r->detail);
    return r;
}

static mc_inventory_status record(dispenser_tile *o, dispenser_failure *r,
                                  mc_tile_entity_status status, const char *method,
                                  const char *descriptor, int32_t pc, mc_inventory_error *e) {
    if (status == MC_TILE_ENTITY_OK) {
        inv_free(&o->budget, r);
        return MC_INVENTORY_OK;
    }
    r->next = o->failures;
    o->failures = r;
    ++o->failure_count;
    (void)tile_result(e, status, method, descriptor, pc);
    e->dispenser = &r->detail;
    e->dispenser_diagnostic_owner_id = o->diagnostic_owner_id;
    e->registry = r->detail.registry;
    e->registry_status = r->detail.registry.status;
    e->mutation_reached = r->detail.inventory_bytecode != 0;
    e->notification_reached = r->detail.tile_entity_bytecode != 0;
    capture(o, r, &r->detail.world_access_failure, e);
    capture(o, r, &r->detail.publication.world_access_failure, e);
    capture(o, r, &r->detail.batch.publication.world_access_failure, e);
    const mc_world_registry_failure *registry = &r->detail.registry_failure;
    if (mc_world_registry_failure_matches(registry, &r->detail.registry)) {
        e->source_exception = registry->kind == MC_WORLD_REGISTRY_FAILURE_SOURCE_EXCEPTION;
        e->source_error = registry->kind == MC_WORLD_REGISTRY_FAILURE_SOURCE_ERROR;
        e->exception_class = registry->exception_class;
        /* The matched Registry classifier supplies a static literal. The AZ
         * ledger still owns its actual child receipts, not this class name. */
        e->exception_class_from_dispenser = false;
    }
    return e->status;
}

static mc_inventory_status end_calls(void *context, mc_inventory_error *error) {
    dispenser_tile *o = context;
    mc_inventory_error e = {0};
    while (o->failures != NULL) {
        dispenser_failure *r = o->failures;
        expire(&r->detail.world_access_failure);
        expire(&r->detail.publication.world_access_failure);
        expire(&r->detail.batch.publication.world_access_failure);
        for (size_t i = 0; i < r->count; ++i) {
            mc_world_access_failure_release(&r->captures[i]);
            if (r->captures[i].receipt != NULL) {
                cleanup_failure(o, MC_WORLD_STORAGE_BUSY,
                                "AZ actual failed child producer release");
                e.status = MC_INVENTORY_BUSY;
                e.operation = "AZ actual failed child producer release";
                return inv_finish(error, &e);
            }
        }
        o->failures = r->next;
        --o->failure_count;
        inv_free(&o->budget, r);
    }
    return inv_finish(error, &e);
}

static void release(void *context) {
    dispenser_tile *o = context;
    if (end_calls(o, NULL) != MC_INVENTORY_OK)
        return;
    mc_tile_entity_release(o->tile);
    inv_free(&o->budget, o);
}

static mc_inventory_status size(void *context, int32_t *out, mc_inventory_error *e) {
    return tile_result(e, mc_tile_entity_dispenser_size(((dispenser_tile *)context)->tile, out),
                       "a", "()I", 0);
}

static mc_inventory_status borrow(void *context, int32_t index, const mc_item_stack **out,
                                  mc_inventory_error *e) {
    mc_item_stack *held = NULL;
    if (tile_result(e,
                    mc_tile_entity_dispenser_get(((dispenser_tile *)context)->tile, index, &held),
                    "f_", "(I)Liz;", 5) == MC_INVENTORY_OK) {
        *out = held;
        mc_item_stack_release(held);
    }
    return e->status;
}

static mc_inventory_status set(void *context, int32_t index, mc_item_stack *stack,
                               mc_inventory_error *e) {
    dispenser_tile *o = context;
    dispenser_failure *r = prepare(o, e);
    if (r == NULL)
        return e->status;
    const mc_tile_entity_status status =
        mc_tile_entity_dispenser_set_detailed(o->tile, index, stack, &r->detail);
    const int32_t pc =
        r->detail.inventory_bytecode == 0 ? 6 : (int32_t)r->detail.inventory_bytecode;
    return record(o, r, status, "a", "(ILiz;)V", pc, e);
}

static mc_inventory_status split(void *context, int32_t index, int32_t amount, mc_item_stack **out,
                                 mc_inventory_error *e) {
    dispenser_tile *o = context;
    dispenser_failure *r = prepare(o, e);
    if (r == NULL)
        return e->status;
    const mc_tile_entity_status status =
        mc_tile_entity_dispenser_remove_detailed(o->tile, index, amount, out, &r->detail);
    const int32_t pc =
        r->detail.inventory_bytecode == 0 ? 5 : (int32_t)r->detail.inventory_bytecode;
    return record(o, r, status, "a", "(II)Liz;", pc, e);
}

static mc_inventory_status changed(void *context, mc_inventory_error *e) {
    dispenser_tile *o = context;
    dispenser_failure *r = prepare(o, e);
    if (r == NULL)
        return e->status;
    const mc_tile_entity_status status = mc_tile_entity_dirty_detailed(o->tile, &r->detail);
    return record(o, r, status, "y_", "()V", 24, e);
}

static mc_inventory_status usable(void *context, mc_entity *player, bool *out,
                                  mc_inventory_error *e) {
    dispenser_tile *o = context;
    mc_tile_entity_snapshot tile;
    if (mc_tile_entity_get_snapshot(o->tile, &tile) != MC_TILE_ENTITY_OK)
        return tile_result(e, MC_TILE_ENTITY_INVALID_PHASE, "a_", "(Lgs;)Z", 4);
    if (tile.world == NULL) {
        e->status = MC_INVENTORY_NULL_REFERENCE;
        e->operation = "AZ eligibility actual null World receiver";
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        inv_frame(e, MC_REGISTRY_CLIENT, "az", "az", "a_", "(Lgs;)Z", 16);
        return e->status;
    }
    dispenser_failure *r = prepare(o, e);
    if (r == NULL)
        return e->status;
    mc_tile_entity *actual = NULL;
    if (mc_live_tile_entities_get(tile.world, tile.x, tile.y, tile.z, &actual, &r->detail) !=
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
        e->operation = "AZ eligibility actual null Player distance receiver";
        e->source_exception = true;
        e->exception_class = "java/lang/NullPointerException";
        inv_frame(e, MC_REGISTRY_CLIENT, "az", "az", "a_", "(Lgs;)Z", 53);
        return e->status;
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        e->status = MC_INVENTORY_UNIMPLEMENTED_DEPENDENCY;
        e->operation = "AZ eligibility native strict FP environment";
        return e->status;
    }
    mc_entity_snapshot entity;
    if (mc_entity_get_snapshot(player, &entity) != MC_ENTITY_OK ||
        mc_tile_entity_get_snapshot(o->tile, &tile) != MC_TILE_ENTITY_OK)
        return tile_result(e, MC_TILE_ENTITY_STATE_ERROR, "a_", "(Lgs;)Z", 53);
    const double dx = at_sub(entity.base.x, at_add((double)tile.x, .5));
    const double dy = at_sub(entity.base.y, at_add((double)tile.y, .5));
    const double dz = at_sub(entity.base.z, at_add((double)tile.z, .5));
    const double distance = at_add(at_add(at_mul(dx, dx), at_mul(dy, dy)), at_mul(dz, dz));
    *out = !(distance > 64.0); /* DCMPL59/IFLE65 also admits NaN. */
    return MC_INVENTORY_OK;
}

static size_t bytes(const void *context) {
    return ((const dispenser_tile *)context)->budget.used;
}

static const mc_inventory_error *cleanup(const void *context) {
    return &((const dispenser_tile *)context)->cleanup;
}

static mc_inventory_status admit(const void *context, mc_inventory_error *error) {
    const dispenser_tile *o = context;
    mc_inventory_error e = {0};
    mc_tile_entity_snapshot state;
    size_t total = 0, count = 0;
    if (o == NULL || o->tile == NULL || o->diagnostic_owner_id == 0 ||
        !inv_allocation_bytes(o, sizeof(*o), &total) ||
        mc_tile_entity_get_snapshot(o->tile, &state) != MC_TILE_ENTITY_OK ||
        state.kind != MC_TILE_ENTITY_DISPENSER || state.phase != MC_TILE_ENTITY_READY)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else {
        for (const dispenser_failure *r = o->failures; r != NULL; r = r->next) {
            size_t extent;
            if (++count > o->failure_count || r->count > 3 ||
                !inv_allocation_bytes(r, sizeof(*r), &extent) || extent > SIZE_MAX - total) {
                e.status = MC_INVENTORY_INVALID_ARGUMENT;
                break;
            }
            total += extent;
        }
        if (e.status == MC_INVENTORY_OK &&
            (count != o->failure_count || total != o->budget.used || total > o->budget.limit))
            e.status = MC_INVENTORY_INVALID_ARGUMENT;
    }
    return inv_finish(error, &e);
}

static const inv_dispenser_leaf leaf = {release, end_calls, size,  borrow,  set,  split,
                                        changed, usable,    bytes, cleanup, admit};

mc_inventory_status mc_inventory_dispenser_from_tile(mc_registry_side side, mc_tile_entity *tile,
                                                     const mc_inventory_limits *limits,
                                                     mc_inventory_dispenser **out,
                                                     mc_inventory_error *error) {
    mc_inventory_error e = {0};
    inv_budget budget;
    mc_tile_entity_snapshot state;
    if (side != MC_REGISTRY_CLIENT || tile == NULL || out == NULL || *out != NULL ||
        !inv_limits(limits, &budget) ||
        mc_tile_entity_get_snapshot(tile, &state) != MC_TILE_ENTITY_OK ||
        state.kind != MC_TILE_ENTITY_DISPENSER || state.phase != MC_TILE_ENTITY_READY) {
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
        e.operation = "Actual AZ fixed adapter native admission";
    } else {
        dispenser_tile *o = inv_alloc(&budget, sizeof(*o), &e);
        if (o != NULL) {
            o->budget = budget;
            if (tile_result(&e, mc_tile_entity_retain(tile), NULL, NULL, 0) == MC_INVENTORY_OK) {
                o->tile = tile;
                mc_inventory_dispenser *owner = NULL;
                if (inv_dispenser_leaf_create(side, o, tile, &leaf, limits, &owner, &e) ==
                    MC_INVENTORY_OK) {
                    mc_inventory_dispenser_state native = {0};
                    const bool inspected = mc_inventory_dispenser_read_state(owner, &native);
                    if (!inspected || native.allocation_bytes > budget.limit) {
                        (void)mc_inventory_dispenser_retire(&owner, NULL);
                        e.status = inspected ? MC_INVENTORY_LIMIT : MC_INVENTORY_INVALID_ARGUMENT;
                        e.operation = inspected ? "AZ complete adapter native allocation budget"
                                                : "AZ actual adapter native inspection";
                    } else {
                        o->diagnostic_owner_id = native.diagnostic_owner_id;
                        const size_t owner_bytes = native.allocation_bytes - o->budget.used;
                        o->budget.limit = budget.limit - owner_bytes;
                        *out = owner;
                    }
                } else
                    release(o);
            } else
                inv_free(&o->budget, o);
        }
    }
    return inv_finish(error, &e);
}
