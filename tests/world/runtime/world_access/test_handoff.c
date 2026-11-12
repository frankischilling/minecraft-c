/* Prospective native ownership controls, NOT executed original observations.
 * This file requires the separately reviewed protected-handoff primitive.
 * Actual registration/next acquire every active right. Only native counters
 * receive temporary fault projections, restored BEFORE assertions. No source
 * pointer is restored to make cleanup succeed. */
int mc_handoff_unused_publication_fixture_main(int, char **);
#define main mc_handoff_unused_publication_fixture_main
#include "../test_entity_publication.c"
#undef main

#include "client/render_engine/image_buffer.h"
#include "client/world/render_global_access.h"
#include "client/world/render_global_internal.h"
#include "entity/tick/execution_internal.h"
#include "mc/entity/appearance.h"
#include "mc/entity/player/player.h"
#include "world/runtime/world_access_internal.h"
#include "world/runtime/world_access_lease_internal.h"

enum h_slot {
    H_CELL,
    H_RANGE,
    H_SOUND,
    H_PARTICLE,
    H_ADDED,
    H_REMOVED,
    H_SKY,
    H_TILE,
    H_AUX,
    H_SLOTS
};

enum h_mode {
    H_FAILURE,
    H_SUCCESS,
    H_NO_RECEIPT,
    H_FOREIGN,
    H_RAW_RECEIPT,
    H_BAD_RECEIVER,
    H_BAD_CHILD,
    H_FALSE_NULL
};

typedef struct h_receiver {
    mc_world_access *self, *foreign;
    mc_world_access_receipt *receipt, *second_receipt, *foreign_receipt;
    mc_world_access_failure foreign_view;
    mc_world_access_failure_lease *armed_node;
    mc_entity_tick_execution *execution;
    mc_world_storage_status reentrant_end;
    bool end_scope_preserved;
    mc_world_storage_status failure_status, free_during, armed_release, success_retire;
    mc_world_storage_error probe_cleanup;
    mc_world_runtime *last_world;
    enum h_mode mode;
    enum h_slot last_slot;
    size_t calls, original_holds, projected_holds;
    int32_t coordinates[6];
    double numbers[6];
    float volume, pitch;
    const mc_registry_string *name;
    mc_entity *entity;
    mc_tile_entity *tile;
    bool remove_self, removed, saturate, projected, armed_inspect, use_second;
} h_receiver;

typedef struct h_fixture {
    fixture *base;
    h_receiver receivers[2];
    mc_world_access_failure_lease nodes[2];
    mc_world_access_failure failure;
    mc_world_storage_error cause, cleanup;
    mc_world_storage_status status;
    mc_entity_tick_execution execution;
    bool execution_initialized;
    bool entered, next_returned, had_receiver, nodes_initialized;
    unsigned int failed_pc;
    mc_client_render_engine_classes *render_classes;
    mc_client_render_global *renderer;
    mc_client_render_global_error *render_cleanup;
    mc_player_session_classes *sessions;
    mc_player_session *session;
    mc_player_string *username;
    mc_entity *player;
} h_fixture;

static bool h_execution_error_equal(const mc_entity_tick_execution_error *a,
                                    const mc_entity_tick_execution_error *b) {
    return a->status == b->status && a->operation == b->operation &&
           a->site.class_name == b->site.class_name && a->site.method == b->site.method &&
           a->site.descriptor == b->site.descriptor && a->site.pc == b->site.pc;
}

static mc_world_storage_status h_observe(h_receiver *r, mc_world_runtime *world, enum h_slot slot,
                                         mc_world_storage_error *error,
                                         mc_world_access_failure *failure) {
    ++r->calls;
    r->last_world = world;
    r->last_slot = slot;
    if (r->remove_self) {
        bool removed = false;
        const mc_world_storage_status status =
            mc_world_runtime_remove_world_access(world, r->self, &removed, error);
        r->removed = status == MC_WORLD_STORAGE_OK && removed;
        if (!r->removed)
            return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
        r->free_during = mc_world_access_free(r->self, NULL);
    }
    if (r->armed_node != NULL) {
        const mc_world_access_failure *untouched = NULL;
        r->armed_inspect = mc_world_access_failure_lease_inspect(r->armed_node, &untouched);
        r->armed_release = mc_world_access_failure_lease_release(r->armed_node, &r->probe_cleanup);
    }
    if (r->execution != NULL) {
        mc_entity_tick_execution *execution = r->execution;
        mc_world_runtime *const actual_world = execution->world;
        const mc_entity_tick_execution_phase phase = execution->phase;
        const mc_entity_tick_execution_error first = execution->first;
        const mc_entity_tick_execution_error capture = execution->capture;
        mc_math_execution *const math = execution->math;
        const size_t current = execution->current_lease, count = execution->lease_count;
        const size_t active = r->self->active_holds, bindings = world->persistent_bindings;
        const bool held = execution->world_held, open = execution->world_open;
        const bool child_failed = execution->child_failed, pending = execution->capture_pending;
        r->reentrant_end = mc_entity_tick_execution_end_world(execution, false);
        r->end_scope_preserved =
            execution->world == actual_world && actual_world == world &&
            execution->phase == phase && phase == MC_ENTITY_TICK_EXECUTION_OPEN &&
            execution->math == math && execution->current_lease == current &&
            execution->lease_count == count && execution->world_held == held && held &&
            execution->world_open == open && open && execution->child_failed == child_failed &&
            execution->capture_pending == pending && r->self->active_holds == active &&
            world->persistent_bindings == bindings &&
            r->armed_node == &execution->leases[current] &&
            r->armed_node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
            h_execution_error_equal(&execution->first, &first) &&
            h_execution_error_equal(&execution->capture, &capture);
    }
    if (r->mode == H_FOREIGN) {
        *failure = r->foreign_view; /* Real foreign producer, not an adopted right. */
    } else if (r->mode == H_RAW_RECEIPT) {
        *failure = (mc_world_access_failure){.receiver = r->self,
                                             .receipt = (mc_world_access_receipt *)(uintptr_t)1};
    } else if (r->mode == H_FALSE_NULL) {
        *failure = (mc_world_access_failure){.source_null_receiver = true};
    } else if (r->mode != H_NO_RECEIPT) {
        mc_world_access_receipt **receipt_slot = r->use_second ? &r->second_receipt : &r->receipt;
        if (*receipt_slot != NULL)
            return MC_WORLD_STORAGE_BUSY;
        const mc_world_storage_status created =
            mc_world_access_receipt_create(r->self, NULL, receipt_slot, failure, error);
        if (created != MC_WORLD_STORAGE_OK)
            return created;
        if (r->mode == H_SUCCESS) {
            /* Real unpublished receipt retires while the current active right
             * still exists. No lease has been fabricated or counter patched. */
            *failure = (mc_world_access_failure){0};
            r->success_retire = mc_world_access_receipt_retire(receipt_slot, error);
            return r->success_retire;
        }
        (*receipt_slot)->exception_class = r->failure_status == MC_WORLD_STORAGE_IO_ERROR
                                               ? "java/lang/IllegalStateException"
                                               : NULL;
        if (r->mode == H_BAD_RECEIVER)
            failure->receiver = r->foreign;
        if (r->mode == H_BAD_CHILD)
            failure->render_global = (const mc_client_render_global_error *)(uintptr_t)1;
        if (r->saturate) {
            r->original_holds = (*receipt_slot)->holds;
            (*receipt_slot)->holds = SIZE_MAX; /* Declared native capture-counter fault only. */
            r->projected = true;
        }
    }
    *error = (mc_world_storage_error){r->failure_status, "handoff authored callback failure", 71};
    return r->failure_status;
}

static mc_world_storage_status h_cell(void *p, mc_world_runtime *w, int32_t x, int32_t y, int32_t z,
                                      mc_world_storage_error *e, mc_world_access_failure *f) {
    h_receiver *r = p;
    r->coordinates[0] = x;
    r->coordinates[1] = y;
    r->coordinates[2] = z;
    return h_observe(r, w, H_CELL, e, f);
}

static mc_world_storage_status h_range(void *p, mc_world_runtime *w, int32_t x0, int32_t y0,
                                       int32_t z0, int32_t x1, int32_t y1, int32_t z1,
                                       mc_world_storage_error *e, mc_world_access_failure *f) {
    h_receiver *r = p;
    const int32_t values[] = {x0, y0, z0, x1, y1, z1};
    memcpy(r->coordinates, values, sizeof(values));
    return h_observe(r, w, H_RANGE, e, f);
}

static mc_world_storage_status h_sound(void *p, mc_world_runtime *w, const mc_registry_string *name,
                                       double x, double y, double z, float volume, float pitch,
                                       mc_world_storage_error *e, mc_world_access_failure *f) {
    h_receiver *r = p;
    r->name = name;
    r->numbers[0] = x;
    r->numbers[1] = y;
    r->numbers[2] = z;
    r->volume = volume;
    r->pitch = pitch;
    return h_observe(r, w, H_SOUND, e, f);
}

static mc_world_storage_status h_particle(void *p, mc_world_runtime *w,
                                          const mc_registry_string *name, double x, double y,
                                          double z, double vx, double vy, double vz,
                                          mc_world_storage_error *e, mc_world_access_failure *f) {
    h_receiver *r = p;
    const double values[] = {x, y, z, vx, vy, vz};
    memcpy(r->numbers, values, sizeof(values));
    r->name = name;
    return h_observe(r, w, H_PARTICLE, e, f);
}

static mc_world_storage_status h_added(void *p, mc_world_runtime *w, mc_entity *entity,
                                       mc_world_storage_error *e, mc_world_access_failure *f) {
    h_receiver *r = p;
    r->entity = entity;
    return h_observe(r, w, H_ADDED, e, f);
}

static mc_world_storage_status h_removed(void *p, mc_world_runtime *w, mc_entity *entity,
                                         mc_world_storage_error *e, mc_world_access_failure *f) {
    h_receiver *r = p;
    r->entity = entity;
    return h_observe(r, w, H_REMOVED, e, f);
}

static mc_world_storage_status h_sky(void *p, mc_world_runtime *w, mc_world_storage_error *e,
                                     mc_world_access_failure *f) {
    return h_observe(p, w, H_SKY, e, f);
}

static mc_world_storage_status h_tile(void *p, mc_world_runtime *w, int32_t x, int32_t y, int32_t z,
                                      mc_tile_entity *tile, mc_world_storage_error *e,
                                      mc_world_access_failure *f) {
    h_receiver *r = p;
    r->coordinates[0] = x;
    r->coordinates[1] = y;
    r->coordinates[2] = z;
    r->tile = tile;
    return h_observe(r, w, H_TILE, e, f);
}

static mc_world_storage_status h_aux(void *p, mc_world_runtime *w, mc_entity *player,
                                     int32_t effect, int32_t x, int32_t y, int32_t z, int32_t data,
                                     mc_world_storage_error *e, mc_world_access_failure *f) {
    h_receiver *r = p;
    const int32_t values[] = {effect, x, y, z, data};
    memcpy(r->coordinates, values, sizeof(values));
    r->entity = player;
    return h_observe(r, w, H_AUX, e, f);
}

static const mc_world_access_source_callbacks h_callbacks = {.cell = h_cell,
                                                             .range = h_range,
                                                             .sound = h_sound,
                                                             .particle = h_particle,
                                                             .entity_added = h_added,
                                                             .entity_removed = h_removed,
                                                             .sky_changed = h_sky,
                                                             .tile_entity_dirty = h_tile,
                                                             .auxiliary = h_aux};

/* Actual low-level dispatch boundary, not a modeled whole World source method.
 * No assertion occurs after callback return until the caller restores an
 * explicitly projected native counter. The remaining actual right is released
 * once and the real World call is left on every entered path. */
static mc_world_storage_status h_invoke_body(h_fixture *h, enum h_slot slot,
                                             mc_world_access_failure_lease *node, bool legacy) {
    mc_world_runtime *w = h->base->worlds[0];
    mc_world_access *held = NULL;
    h->failure = (mc_world_access_failure){0};
    h->cause = (mc_world_storage_error){0};
    h->entered = false;
    h->next_returned = false;
    h->had_receiver = false;
    h->failed_pc = 0;
    h->status = mc_world_runtime_call_enter(w, &h->cause);
    if (h->status != MC_WORLD_STORAGE_OK)
        return h->status;
    h->entered = true;
    h->next_returned =
        mc_world_access_next(w, 0, 11, 22, 33, &held, &h->had_receiver, &h->failed_pc, &h->cause);
    if (!h->next_returned || !h->had_receiver) {
        h->status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        goto done;
    }
    if (legacy)
        switch (slot) {
            case H_CELL:
                h->status = mc_world_access_call_cell(held, w, 3, 64, -7, &h->cause, &h->failure);
                break;
            case H_RANGE:
                h->status =
                    mc_world_access_call_range(held, w, -3, 4, 5, 6, 7, 8, &h->cause, &h->failure);
                break;
            case H_SOUND:
                h->status = mc_world_access_call_sound(held, w, NULL, .25, -.5, 7.5, .75f, 1.25f,
                                                       &h->cause, &h->failure);
                break;
            case H_PARTICLE:
                h->status = mc_world_access_call_particle(held, w, NULL, .25, -.5, 7.5, 1, 2, 3,
                                                          &h->cause, &h->failure);
                break;
            case H_ADDED:
                h->status =
                    mc_world_access_call_entity_added(held, w, h->player, &h->cause, &h->failure);
                break;
            case H_REMOVED:
                h->status =
                    mc_world_access_call_entity_removed(held, w, h->player, &h->cause, &h->failure);
                break;
            case H_SKY:
                h->status = mc_world_access_call_sky_changed(held, w, &h->cause, &h->failure);
                break;
            case H_TILE:
                h->status = mc_world_access_call_tile_entity_dirty(held, w, 3, 64, -7, NULL,
                                                                   &h->cause, &h->failure);
                break;
            case H_AUX:
                h->status = mc_world_access_call_auxiliary(held, w, h->player, 9, 3, 64, -7, 17,
                                                           &h->cause, &h->failure);
                break;
            default:
                h->status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
                break;
        }
    else
        switch (slot) {
            case H_CELL:
                h->status = mc_world_access_call_cell_with_lease(&held, w, 3, 64, -7, node,
                                                                 &h->cause, &h->failure);
                break;
            case H_RANGE:
                h->status = mc_world_access_call_range_with_lease(&held, w, -3, 4, 5, 6, 7, 8, node,
                                                                  &h->cause, &h->failure);
                break;
            case H_SOUND:
                h->status = mc_world_access_call_sound_with_lease(
                    &held, w, NULL, .25, -.5, 7.5, .75f, 1.25f, node, &h->cause, &h->failure);
                break;
            case H_PARTICLE:
                h->status = mc_world_access_call_particle_with_lease(
                    &held, w, NULL, .25, -.5, 7.5, 1, 2, 3, node, &h->cause, &h->failure);
                break;
            case H_ADDED:
                h->status = mc_world_access_call_entity_added_with_lease(&held, w, h->player, node,
                                                                         &h->cause, &h->failure);
                break;
            case H_REMOVED:
                h->status = mc_world_access_call_entity_removed_with_lease(
                    &held, w, h->player, node, &h->cause, &h->failure);
                break;
            case H_SKY:
                h->status = mc_world_access_call_sky_changed_with_lease(&held, w, node, &h->cause,
                                                                        &h->failure);
                break;
            case H_TILE:
                h->status = mc_world_access_call_tile_entity_dirty_with_lease(
                    &held, w, 3, 64, -7, NULL, node, &h->cause, &h->failure);
                break;
            case H_AUX:
                h->status = mc_world_access_call_auxiliary_with_lease(
                    &held, w, h->player, 9, 3, 64, -7, 17, node, &h->cause, &h->failure);
                break;
            default:
                h->status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
                break;
        }
done:
    mc_world_access_release_held(held);
    mc_world_runtime_call_leave(w);
    return h->status;
}

static mc_world_storage_status h_invoke(h_fixture *h, enum h_slot slot,
                                        mc_world_access_failure_lease *node) {
    return h_invoke_body(h, slot, node, false);
}

static void h_restore_capture_fault(h_receiver *r) {
    if (r->projected) {
        r->projected_holds = r->receipt->holds;
        r->receipt->holds = r->original_holds;
        r->projected = false;
    }
}

static bool h_unsubscribe(h_fixture *h, mc_world_access *receiver) {
    bool removed = false;
    return mc_world_runtime_remove_world_access(h->base->worlds[0], receiver, &removed, NULL) ==
           MC_WORLD_STORAGE_OK;
}

static bool h_arguments(const h_fixture *h, const h_receiver *r, enum h_slot slot) {
    if (r->last_world != h->base->worlds[0] || r->last_slot != slot)
        return false;
    switch (slot) {
        case H_CELL:
            return r->coordinates[0] == 3 && r->coordinates[1] == 64 && r->coordinates[2] == -7;
        case H_RANGE: {
            const int32_t expected[] = {-3, 4, 5, 6, 7, 8};
            return memcmp(r->coordinates, expected, sizeof(expected)) == 0;
        }
        case H_SOUND:
            return r->name == NULL && r->numbers[0] == .25 && r->numbers[1] == -.5 &&
                   r->numbers[2] == 7.5 && r->volume == .75f && r->pitch == 1.25f;
        case H_PARTICLE: {
            const double expected[] = {.25, -.5, 7.5, 1, 2, 3};
            return r->name == NULL && memcmp(r->numbers, expected, sizeof(expected)) == 0;
        }
        case H_ADDED:
        case H_REMOVED:
            return r->entity == h->player;
        case H_SKY:
            return true;
        case H_TILE:
            return r->coordinates[0] == 3 && r->coordinates[1] == 64 && r->coordinates[2] == -7 &&
                   r->tile == NULL;
        case H_AUX: {
            const int32_t expected[] = {9, 3, 64, -7, 17};
            return r->entity == h->player &&
                   memcmp(r->coordinates, expected, sizeof(expected)) == 0;
        }
        default:
            return false;
    }
}

static bool h_generic(h_fixture *h) {
    bool ok = false;
    h_receiver *r = &h->receivers[0];
    mc_world_runtime *world = h->base->worlds[0];
    mc_world_access_failure_lease *node = &h->nodes[0];
    for (unsigned int index = 0; index < H_SLOTS; ++index) {
        const size_t before = r->calls;
        r->mode = H_FAILURE;
        r->failure_status = index == H_RANGE      ? MC_WORLD_STORAGE_LIMIT
                            : index == H_PARTICLE ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                                  : MC_WORLD_STORAGE_IO_ERROR;
        r->remove_self = true;
        r->removed = false;
        r->saturate = index == H_CELL;
        r->armed_node = node;
        CHECK(world->fields.u->count == 0 && r->receipt == NULL &&
              mc_world_runtime_add_world_access(world, r->self, NULL) == MC_WORLD_STORAGE_OK);
        const mc_world_storage_status status = h_invoke(h, (enum h_slot)index, node);
        h_restore_capture_fault(r); /* Before ANY assertion after the projected call. */
        const mc_world_storage_error first = h->cause;
        const mc_world_access_failure *view = NULL;
        CHECK(status == r->failure_status && r->calls == before + 1 &&
              r->last_slot == (enum h_slot)index && r->last_world == world && r->removed &&
              r->free_during == MC_WORLD_STORAGE_BUSY);
        CHECK(first.status == status && first.operation != NULL &&
              strcmp(first.operation, "handoff authored callback failure") == 0 &&
              first.system_error == 71);
        CHECK(!r->armed_inspect && r->armed_release == MC_WORLD_STORAGE_BUSY &&
              world->active_calls == 0 && world->fields.u->count == 0 &&
              r->self->active_holds == 1 && r->receipt->holds == 1);
        CHECK(!r->saturate || r->projected_holds == SIZE_MAX);
        CHECK(mc_world_access_failure_lease_inspect(node, &view) && view->receiver == r->self &&
              view->receipt == r->receipt && view->render_global == NULL &&
              h->failure.receipt == r->receipt);
        const char *exception = NULL;
        CHECK(mc_world_access_failure_source_exception(view, &exception) ==
              (r->failure_status == MC_WORLD_STORAGE_IO_ERROR));
        CHECK(r->failure_status == MC_WORLD_STORAGE_IO_ERROR
                  ? exception != NULL && strcmp(exception, "java/lang/IllegalStateException") == 0
                  : exception == NULL);
        CHECK(mc_world_access_receipt_retire(&r->receipt, &h->cleanup) == MC_WORLD_STORAGE_BUSY &&
              mc_world_access_free(r->self, NULL) == MC_WORLD_STORAGE_BUSY);
        if (index == H_CELL) {
            const size_t actual_active = r->self->active_holds;
            r->self->active_holds = 0; /* Native counter fault, no right is created. */
            const mc_world_storage_status refused =
                mc_world_access_failure_lease_release(node, &h->cleanup);
            r->self->active_holds = actual_active; /* Restore before assertions/producer calls. */
            CHECK(refused == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                  mc_world_access_failure_lease_inspect(node, &view) &&
                  view->receipt == r->receipt && r->self->active_holds == 1 &&
                  r->receipt->holds == 1);
            CHECK(mc_world_access_receipt_retire(&r->receipt, &h->cleanup) ==
                  MC_WORLD_STORAGE_BUSY);
        }
        h->failure = (mc_world_access_failure){0};
        view = NULL;
        CHECK(mc_world_access_failure_lease_release(node, &h->cleanup) == MC_WORLD_STORAGE_OK &&
              r->self->active_holds == 0);
        CHECK(mc_world_access_failure_lease_release(node, &h->cleanup) == MC_WORLD_STORAGE_OK &&
              r->calls == before + 1 &&
              mc_world_access_receipt_retire(&r->receipt, &h->cleanup) == MC_WORLD_STORAGE_OK &&
              r->receipt == NULL);
        CHECK(h->cause.status == first.status && h->cause.operation == first.operation &&
              h->cause.system_error == first.system_error);
    }
    ok = true;
cleanup:
    h_restore_capture_fault(r);
    return ok;
}

static bool h_success_and_legacy(h_fixture *h) {
    bool ok = false;
    h_receiver *r = &h->receivers[0];
    mc_world_runtime *world = h->base->worlds[0];
    for (unsigned int mode = 0; mode < 3; ++mode)
        for (unsigned int index = 0; index < H_SLOTS; ++index) {
            const size_t before = r->calls;
            r->mode = H_SUCCESS;
            r->remove_self = false;
            r->saturate = false;
            r->armed_node = mode == 0 ? &h->nodes[0] : NULL;
            CHECK(mc_world_runtime_add_world_access(world, r->self, NULL) == MC_WORLD_STORAGE_OK);
            CHECK(h_invoke_body(h, (enum h_slot)index, mode == 0 ? &h->nodes[0] : NULL,
                                mode == 2) == MC_WORLD_STORAGE_OK &&
                  r->success_retire == MC_WORLD_STORAGE_OK && r->receipt == NULL &&
                  r->self->active_holds == 0 && r->calls == before + 1);
            CHECK(h_arguments(h, r, (enum h_slot)index));
            const mc_world_access_failure *view = NULL;
            CHECK(!mc_world_access_failure_lease_inspect(&h->nodes[0], &view) && view == NULL &&
                  h_unsubscribe(h, r->self));
        }
    r->mode = H_NO_RECEIPT;
    r->failure_status = MC_WORLD_STORAGE_IO_ERROR;
    r->armed_node = &h->nodes[0];
    CHECK(mc_world_runtime_add_world_access(world, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          h_invoke(h, H_SOUND, &h->nodes[0]) == MC_WORLD_STORAGE_IO_ERROR && r->receipt == NULL &&
          r->self->active_holds == 0 && h->failure.receipt == NULL && h_unsubscribe(h, r->self));
    CHECK(mc_world_runtime_add_world_access(world, NULL, NULL) == MC_WORLD_STORAGE_OK &&
          h_invoke(h, H_SOUND, &h->nodes[0]) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          h->failure.source_null_receiver && h->failure.receiver == NULL &&
          h->failure.receipt == NULL);
    const char *exception = NULL;
    CHECK(mc_world_access_failure_source_exception(&h->failure, &exception) &&
          strcmp(exception, "java/lang/NullPointerException") == 0 && h_unsubscribe(h, NULL));
    h->failure = (mc_world_access_failure){0};
    ok = true;
cleanup:
    return ok;
}

/* Two real callback invocations on the SAME producer create distinct actual
 * receipts and transfer two independently acquired dispatch occurrences. The
 * second owner slot is pre-existing native fixture storage, not a repaired
 * source reference or a fabricated hold. */
static bool h_independent_nodes(h_fixture *h) {
    bool ok = false;
    h_receiver *r = &h->receivers[0];
    mc_world_runtime *world = h->base->worlds[0];
    r->mode = H_FAILURE;
    r->failure_status = MC_WORLD_STORAGE_IO_ERROR;
    r->remove_self = true;
    r->saturate = false;
    r->use_second = false;
    r->armed_node = &h->nodes[0];
    const size_t before = r->calls;
    CHECK(mc_world_runtime_add_world_access(world, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          h_invoke(h, H_SOUND, &h->nodes[0]) == MC_WORLD_STORAGE_IO_ERROR);
    const mc_world_access_failure *first = NULL, *second = NULL;
    CHECK(mc_world_access_failure_lease_inspect(&h->nodes[0], &first) &&
          first->receipt == r->receipt && r->self->active_holds == 1);
    r->use_second = true;
    r->armed_node = &h->nodes[1];
    CHECK(mc_world_runtime_add_world_access(world, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          h_invoke(h, H_SOUND, &h->nodes[1]) == MC_WORLD_STORAGE_IO_ERROR);
    CHECK(mc_world_access_failure_lease_inspect(&h->nodes[1], &second) &&
          second->receipt == r->second_receipt && r->receipt != r->second_receipt &&
          r->self->active_holds == 2 && r->calls == before + 2 && world->fields.u->count == 0);
    mc_world_access_failure_lease invalid_copy;
    memcpy(&invalid_copy, &h->nodes[0],
           sizeof(invalid_copy)); /* Explicit native invalid-copy fault. */
    const mc_world_access_failure sentinel = {0};
    const mc_world_access_failure *untouched = &sentinel;
    const bool copied_inspect = mc_world_access_failure_lease_inspect(&invalid_copy, &untouched);
    const mc_world_storage_status copied_release =
        mc_world_access_failure_lease_release(&invalid_copy, &h->cleanup);
    CHECK(!copied_inspect && untouched == &sentinel &&
          copied_release == MC_WORLD_STORAGE_INVALID_ARGUMENT && r->self->active_holds == 2);
    /* Clear the invalid diagnostic copy too; it never gained an owning right. */
    memset(&invalid_copy, 0, sizeof(invalid_copy));
    CHECK(mc_world_access_receipt_retire(&r->receipt, &h->cleanup) == MC_WORLD_STORAGE_BUSY &&
          mc_world_access_receipt_retire(&r->second_receipt, &h->cleanup) == MC_WORLD_STORAGE_BUSY);
    /* A fresh actual current occurrence cannot reuse a HELD node. Refusal
     * precedes source invocation; the dispatcher releases only the NEW right. */
    CHECK(mc_world_runtime_add_world_access(world, r->self, NULL) == MC_WORLD_STORAGE_OK &&
          h_invoke(h, H_SOUND, &h->nodes[0]) == MC_WORLD_STORAGE_BUSY && r->calls == before + 2 &&
          r->self->active_holds == 2 && h_unsubscribe(h, r->self));
    h->failure = (mc_world_access_failure){0};
    first = NULL;
    second = NULL;
    CHECK(mc_world_access_failure_lease_release(&h->nodes[0], &h->cleanup) == MC_WORLD_STORAGE_OK &&
          r->self->active_holds == 1 &&
          mc_world_access_receipt_retire(&r->receipt, &h->cleanup) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_access_failure_lease_inspect(&h->nodes[1], &second) &&
          second->receipt == r->second_receipt &&
          mc_world_access_receipt_retire(&r->second_receipt, &h->cleanup) == MC_WORLD_STORAGE_BUSY);
    second = NULL;
    CHECK(mc_world_access_failure_lease_release(&h->nodes[1], &h->cleanup) == MC_WORLD_STORAGE_OK &&
          r->self->active_holds == 0 &&
          mc_world_access_receipt_retire(&r->second_receipt, &h->cleanup) == MC_WORLD_STORAGE_OK &&
          r->calls == before + 2);
    r->use_second = false;
    ok = true;
cleanup:
    return ok;
}

/* Real successful callback reentry must not close its enclosing owner scope.
 * Complete actual dispatch/end/retire BEFORE checking the recorded refusal, so
 * owner1 fails after callback return and still cleans its genuine owned rights. */
static bool h_owner_reentry(h_fixture *h) {
    bool ok = false;
    mc_world_runtime *world = h->base->worlds[0];
    h_receiver *r = &h->receivers[0];
    mc_math_execution math;
    mc_math_execution_init(&math, NULL); /* Unused borrowed namespace, never activated. */
    mc_entity_tick_execution_init(&h->execution, &math);
    h->execution_initialized = true;
    const size_t bindings = world->persistent_bindings;
    CHECK(world->fields.u->count == 0 &&
          mc_entity_tick_execution_begin_world(
              &h->execution, world,
              (mc_entity_tick_execution_site){"native-control", "owner-reentry", "()V", 1}) ==
              MC_WORLD_STORAGE_OK);
    mc_world_access_failure_lease *const node = mc_entity_tick_execution_world_lease(&h->execution);
    CHECK(node != NULL && world->persistent_bindings == bindings + 1);
    r->mode = H_SUCCESS;
    r->remove_self = false;
    r->saturate = false;
    r->use_second = false;
    r->armed_node = node;
    r->execution = &h->execution;
    const size_t before = r->calls;
    CHECK(mc_world_runtime_add_world_access(world, r->self, NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_storage_status status = h_invoke(h, H_SOUND, node);
    r->execution = NULL;
    r->armed_node = NULL;
    const mc_world_storage_status ended = mc_entity_tick_execution_end_world(&h->execution, false);
    const mc_world_storage_status retired = mc_entity_tick_execution_retire(&h->execution);
    CHECK(status == MC_WORLD_STORAGE_OK && r->calls == before + 1 &&
          r->success_retire == MC_WORLD_STORAGE_OK && r->receipt == NULL &&
          r->reentrant_end == MC_WORLD_STORAGE_BUSY && r->end_scope_preserved &&
          ended == MC_WORLD_STORAGE_OK && retired == MC_WORLD_STORAGE_OK &&
          h->execution.phase == MC_ENTITY_TICK_EXECUTION_RETIRED && h->execution.world == NULL &&
          !h->execution.world_open && !h->execution.world_held &&
          h->execution.current_lease == SIZE_MAX && h->execution.lease_count == 0 &&
          h->execution.first.status == MC_WORLD_STORAGE_OK &&
          world->persistent_bindings == bindings && world->active_calls == 0 &&
          r->self->active_holds == 0 && math.owned == NULL && !math.failed &&
          h_unsubscribe(h, r->self));
    ok = true;
cleanup:
    r->execution = NULL;
    r->armed_node = NULL;
    return ok;
}

static bool h_bad_producers(h_fixture *h) {
    bool ok = false;
    h_receiver *r = &h->receivers[0], *other = &h->receivers[1];
    mc_world_runtime *world = h->base->worlds[0];
    CHECK(mc_world_access_receipt_create(other->self, NULL, &other->receipt, &r->foreign_view,
                                         &h->cleanup) == MC_WORLD_STORAGE_OK);
    r->foreign = other->self;
    r->foreign_receipt = other->receipt;

    const enum h_mode modes[] = {H_FOREIGN, H_RAW_RECEIPT, H_BAD_RECEIVER, H_BAD_CHILD,
                                 H_FALSE_NULL};

    for (size_t i = 0; i < COUNT(modes); ++i) {
        const size_t before = r->calls;
        r->mode = modes[i];
        r->remove_self = false;
        r->saturate = false;
        r->armed_node = &h->nodes[0];
        r->failure_status = MC_WORLD_STORAGE_IO_ERROR;
        CHECK(mc_world_runtime_add_world_access(world, r->self, NULL) == MC_WORLD_STORAGE_OK);
        CHECK(h_invoke(h, H_SOUND, &h->nodes[0]) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              r->calls == before + 1 && r->self->active_holds == 0 && h->failure.receiver == NULL &&
              h->failure.receipt == NULL && h->failure.render_global == NULL &&
              !h->failure.source_null_receiver);
        CHECK(h->nodes[0].callback_status == MC_WORLD_STORAGE_IO_ERROR &&
              h->nodes[0].callback_error.status == MC_WORLD_STORAGE_IO_ERROR &&
              h->nodes[0].callback_error.operation != NULL &&
              strcmp(h->nodes[0].callback_error.operation, "handoff authored callback failure") ==
                  0 &&
              h->nodes[0].callback_error.system_error == 71);
        CHECK(other->receipt->holds == 1 && other->self->active_holds == 0 &&
              other->receipt == r->foreign_receipt);
        const mc_world_access_failure *view = NULL;
        CHECK(!mc_world_access_failure_lease_inspect(&h->nodes[0], &view) && view == NULL &&
              h_unsubscribe(h, r->self));
        CHECK(mc_world_access_receipt_retire(&r->receipt, &h->cleanup) == MC_WORLD_STORAGE_OK &&
              r->receipt == NULL);
    }
    r->foreign_view = (mc_world_access_failure){0};
    r->foreign_receipt = NULL;
    CHECK(mc_world_access_receipt_retire(&other->receipt, &h->cleanup) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool h_render_global(h_fixture *h) {
    bool ok = false;
    fixture *f = h->base;
    mc_world_runtime *world = f->worlds[0];
    const uint16_t name[] = {'H', 'a', 'n', 'd', 'o', 'f', 'f'};
    CHECK(world->fields.u->count == 0 &&
          mc_client_render_engine_classes_create(NULL, &h->render_classes) ==
              MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_global_allocate(h->render_classes, NULL, &h->renderer,
                                           h->render_cleanup) == MC_CLIENT_RENDER_GLOBAL_OK &&
          mc_render_global_access_create(h->renderer, &h->renderer->access, h->render_cleanup) ==
              MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_registry_classes_initialize_statistics(f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &h->sessions,
                                           NULL) == MC_PLAYER_OK);
    CHECK(mc_player_string_create(name, COUNT(name), &h->username) == MC_PLAYER_OK &&
          mc_player_session_create(h->sessions, h->username, NULL, &h->session, NULL) ==
              MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], &h->player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(h->player, world, NULL, h->session, 0, NULL, NULL) ==
              MC_PLAYER_OK);
    const size_t bindings = world->persistent_bindings;
    CHECK(mc_render_global_access_source_add(h->renderer->access, world, h->render_cleanup) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          world->fields.u->count == 1);
    mc_world_access *identity = world->world_accesses.entries[0];
    CHECK(h_invoke(h, H_ADDED, &h->nodes[0]) == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          identity->active_holds == 1 && world->active_calls == 0);
    const mc_world_access_failure *view = NULL;
    CHECK(mc_world_access_failure_lease_inspect(&h->nodes[0], &view) &&
          view->receiver == identity && view->receipt != NULL && view->render_global != NULL &&
          view->render_global->source_pc == 26 &&
          view->render_global->status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          mc_entity_cloak_url(h->player) != NULL);
    const char *exception = NULL;
    CHECK(mc_world_access_failure_source_exception(view, &exception) && exception != NULL &&
          strcmp(exception, "java/lang/NullPointerException") == 0);
    mc_render_global_callback_state callback;
    mc_client_image_buffer_state processor;
    CHECK(mc_render_global_access_callback_inspect(h->renderer->access, view, &callback) &&
          callback.processors[0] != NULL && callback.processors[1] == NULL &&
          callback.images[0] == NULL && callback.images[1] == NULL);
    const mc_client_image_buffer *actual_processor = callback.processors[0];
    CHECK(mc_client_image_buffer_inspect(actual_processor, &processor) &&
          processor.native_holds == 1 && processor.last_source_pc == 4 && processor.a == NULL &&
          processor.b == 0 && processor.c == 0 && processor.array_owner == NULL &&
          processor.pending_candidate == NULL);
    /* Saturate only a native counter on the actual N-produced receipt. The
     * pre-handoff saturated case is covered by h_generic's real callback;
     * there is no interception or substituted callback in this N case. */
    const size_t original_holds = view->receipt->holds;
    mc_world_access_failure unacquired = {0};
    view->receipt->holds = SIZE_MAX;
    const mc_world_storage_status capture = mc_world_access_failure_retain(view, &unacquired);
    const size_t projected = view->receipt->holds;
    view->receipt->holds = original_holds; /* Before assertions/retirement. */
    CHECK(capture == MC_WORLD_STORAGE_LIMIT && projected == SIZE_MAX && original_holds == 1 &&
          unacquired.receipt == NULL && unacquired.receiver == NULL);
    const mc_world_storage_error first = h->cause;
    CHECK(mc_render_global_access_detach_all(h->renderer->access, h->render_cleanup) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          world->fields.u->count == 1 && world->persistent_bindings == bindings + 1 &&
          identity->active_holds == 1);
    CHECK(mc_render_global_access_callback_inspect(h->renderer->access, view, &callback) &&
          callback.processors[0] == actual_processor &&
          mc_client_image_buffer_inspect(actual_processor, &processor) &&
          processor.native_holds == 1);
    const size_t active = identity->active_holds;
    identity->active_holds = 0; /* Declared counter fault, then restore before any assertion. */
    const mc_world_storage_status refused =
        mc_world_access_failure_lease_release(&h->nodes[0], &h->cleanup);
    identity->active_holds = active;
    CHECK(refused == MC_WORLD_STORAGE_UNSUPPORTED_STATE && identity->active_holds == 1 &&
          mc_world_access_failure_lease_inspect(&h->nodes[0], &view));
    CHECK(mc_render_global_access_detach_all(h->renderer->access, h->render_cleanup) ==
              MC_CLIENT_RENDER_GLOBAL_BUSY &&
          mc_client_image_buffer_inspect(actual_processor, &processor) &&
          processor.native_holds == 1);
    /* These are borrowed diagnostic copies, not source pointer repairs. */
    h->failure = (mc_world_access_failure){0};
    view = NULL;
    callback = (mc_render_global_callback_state){0};
    actual_processor = NULL;
    processor = (mc_client_image_buffer_state){0};
    CHECK(mc_world_access_failure_lease_release(&h->nodes[0], &h->cleanup) == MC_WORLD_STORAGE_OK &&
          identity->active_holds == 0);
    CHECK(mc_render_global_access_detach_all(h->renderer->access, h->render_cleanup) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          world->fields.u->count == 0 && world->persistent_bindings == bindings);
    CHECK(h->cause.status == first.status && h->cause.operation == first.operation &&
          h->cause.system_error == first.system_error);
    CHECK(mc_render_global_access_retire(&h->renderer->access, h->render_cleanup) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          h->renderer->access == NULL);
    ok = true;
cleanup:
    return ok;
}

static bool h_setup(h_fixture *h, const char *root) {
    bool ok = false;
    fixture *f = h->base;
    const uint16_t title[] = {'H', 'a', 'n', 'd', 'o', 'f', 'f'};
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK &&
              mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
                  MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
    }
    CHECK(open_store(f, 0, root) && create_world(f, 0));
    for (size_t i = 0; i < COUNT(h->nodes); ++i)
        mc_world_access_failure_lease_init(&h->nodes[i]);
    h->nodes_initialized = true;
    for (size_t i = 0; i < COUNT(h->receivers); ++i)
        CHECK(mc_world_access_source_create(&h_callbacks, &h->receivers[i], &h->receivers[i].self,
                                            NULL) == MC_WORLD_STORAGE_OK);
    ok = true;
cleanup:
    return ok;
}

static bool h_cleanup(h_fixture *h) {
    if (h == NULL)
        return true;
    for (size_t i = 0; i < COUNT(h->receivers); ++i)
        h_restore_capture_fault(&h->receivers[i]);
    h->failure = (mc_world_access_failure){0};
    if (h->execution_initialized) {
        for (size_t i = 0; i < COUNT(h->receivers); ++i)
            h->receivers[i].execution = NULL;
        if (h->execution.world_open &&
            mc_entity_tick_execution_end_world(&h->execution, true) != MC_WORLD_STORAGE_OK)
            return false;
        if (mc_entity_tick_execution_retire(&h->execution) != MC_WORLD_STORAGE_OK)
            return false;
    }
    for (size_t i = 0; i < COUNT(h->nodes); ++i) {
        /* setup may have stopped before explicit node initialization. */
        if (!h->nodes_initialized)
            continue;
        if (mc_world_access_failure_lease_release(&h->nodes[i], &h->cleanup) != MC_WORLD_STORAGE_OK)
            return false;
    }
    for (size_t i = 0; i < COUNT(h->receivers); ++i) {
        h_receiver *r = &h->receivers[i];
        r->foreign_view = (mc_world_access_failure){0};
        if (r->self != NULL && h->base != NULL && h->base->worlds[0] != NULL &&
            !h_unsubscribe(h, r->self))
            return false;
        if (mc_world_access_receipt_retire(&r->receipt, &h->cleanup) != MC_WORLD_STORAGE_OK)
            return false;
        if (mc_world_access_receipt_retire(&r->second_receipt, &h->cleanup) != MC_WORLD_STORAGE_OK)
            return false;
        if (mc_world_access_free(r->self, &h->cleanup) != MC_WORLD_STORAGE_OK)
            return false;
        r->self = NULL;
    }
    if (h->renderer != NULL && h->renderer->access != NULL) {
        if (mc_render_global_access_detach_all(h->renderer->access, h->render_cleanup) !=
                MC_CLIENT_RENDER_GLOBAL_OK ||
            mc_render_global_access_retire(&h->renderer->access, h->render_cleanup) !=
                MC_CLIENT_RENDER_GLOBAL_OK)
            return false;
    }
    if (h->renderer != NULL && mc_client_render_global_release(&h->renderer, h->render_cleanup) !=
                                   MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    if (h->render_classes != NULL &&
        mc_client_render_engine_classes_release(h->render_classes) != MC_CLIENT_RENDER_ENGINE_OK)
        return false;
    h->render_classes = NULL;
    mc_entity_release(h->player);
    h->player = NULL;
    mc_player_session_retire(&h->session);
    mc_player_string_release(h->username);
    h->username = NULL;
    if (mc_player_session_classes_free(&h->sessions, NULL) != MC_PLAYER_OK)
        return false;
    if (!cleanup_fixture(h->base))
        return false;
    h->base = NULL;
    free(h->render_cleanup);
    h->render_cleanup = NULL;
    free(h);
    return true;
}

int main(int argc, char **argv) {
    int result = 1;
    h_fixture *h = NULL;
    char root[8192];
    if (argc != 3 || strcmp(argv[1], "world-access") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    h = calloc(1, sizeof(*h));
    CHECK(h != NULL);
    h->base = calloc(1, sizeof(*h->base));
    h->render_cleanup = calloc(1, sizeof(*h->render_cleanup));
    CHECK(h->base != NULL && h->render_cleanup != NULL && h_setup(h, root));
    CHECK(h_generic(h) && h_success_and_legacy(h) && h_independent_nodes(h) && h_owner_reentry(h) &&
          h_bad_producers(h) && h_render_global(h));
    result = 0;
cleanup:
    if (!h_cleanup(h)) {
        fputs("WorldAccess handoff cleanup refused; actual remaining owners retained\n", stderr);
        return 1;
    }
    if (result == 0)
        printf("PASS: %u protected WorldAccess handoff checks\n", checks);
    return result;
}
