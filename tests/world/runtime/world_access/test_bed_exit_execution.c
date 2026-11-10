/* Real SourceSaving/Kernel context; the imported observation main is not run. */
#include <inttypes.h>

#define MC_WORLD_BLOCK_SCALAR_MAIN be_unused_scalar_main
#include "test_block_scalar_execution.c"
#undef MC_WORLD_BLOCK_SCALAR_MAIN

#include "world/runtime/bed_spawn_execution_internal.h"

typedef struct be_fixture {
    bs_fixture scalar;
    mc_world_bed_spawn_error error;
    mc_world_bed_spawn_execution frame;
    mc_world_runtime_execution_result owned;
    mc_player_coordinates marker, *position;
    mc_world_storage_status raw;
    bool continued, callback_valid;
    size_t callbacks;
    mc_block **captured_m;
} be_fixture;

static mc_world_storage_status be_armed(void *context, mc_world_runtime *world, int32_t x,
                                        int32_t y, int32_t z, mc_tile_entity *tile,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    be_fixture *s = context;
    (void)cause;
    (void)failure;
    ++s->callbacks;
    mc_world_access_failure_lease *node =
        mc_entity_tick_execution_world_lease(&s->scalar.cq.execution);
    s->callback_valid = world == s->scalar.cq.source.game.worlds[0] && x == 8 && y == 64 &&
                        z == 8 && tile == NULL && node != NULL &&
                        node->phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED;
    s->continued =
        mc_world_bed_spawn_exit_in_execution(&s->frame, world, s->scalar.cq.source.game.registry[0],
                                             8, 64, 8, 0, NULL, &s->position, &s->error, &s->raw);
    printf("BED_EXIT_PROBE continued=%u position=%u\n", s->continued ? 1u : 0u,
           s->position != &s->marker ? 1u : 0u);
    fflush(stdout);
    return MC_WORLD_STORAGE_OK;
}

static int be_red(be_fixture *s) {
    fixture *f = &s->scalar.cq.source.game;
    int32_t id = -1;
    VERIFY(mc_world_runtime_block_get(f->worlds[0], 8, 64, 8, &id, NULL) == MC_WORLD_STORAGE_OK &&
           cq_remember(&s->scalar.cq) == 0);
    /* Authored cached terrain before the source invocation: first candidate
     * has Stone support and two air cells, so the legacy helper really exits. */
    memset(f->states[0]->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(f->states[0]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    f->states[0]->data->blocks[cell(7, 63, 7)] = 1;
    VERIFY(bs_begin(&s->scalar) == 0);
    s->position = &s->marker;
    s->raw = MC_WORLD_STORAGE_LIMIT;
    s->error.status = MC_WORLD_STORAGE_IO_ERROR;
    s->error.operation = "untouched owned bed error";
    s->error.pc = 193;
    mc_world_runtime_execution_result_init(&s->owned);
    mc_world_bed_spawn_execution_init(&s->frame, &s->scalar.cq.execution, f->classes[0], cq_code,
                                      bs_site, &s->owned);
    const mc_world_access_source_callbacks callbacks = {.tile_entity_dirty = be_armed};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK);
    const mc_world_storage_status status = mc_world_runtime_tile_entity_dirty_in_execution(
        &s->scalar.cq.execution, f->worlds[0], cq_code, 8, 64, 8, NULL, &s->scalar.cq.dirty,
        &s->scalar.cq.result);
    VERIFY(status == MC_WORLD_STORAGE_OK && s->callbacks == 1 && s->callback_valid);
    VERIFY(!s->continued && s->position == &s->marker && s->raw == MC_WORLD_STORAGE_LIMIT &&
           s->error.status == MC_WORLD_STORAGE_IO_ERROR && s->error.pc == 193 &&
           !s->error.helper_entered && s->error.event_count == 0 &&
           s->owned.ownership_status == MC_WORLD_STORAGE_BUSY);
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(f->worlds[0], s->scalar.cq.receiver, &removed,
                                                NULL) == MC_WORLD_STORAGE_OK &&
           removed && bs_end(&s->scalar, false) == 0 &&
           mc_world_access_free(s->scalar.cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->scalar.cq.receiver = NULL;
    return 0;
}

static int be_compare(be_fixture *s, int32_t skip) {
    fixture *f = &s->scalar.cq.source.game;
    mc_world_bed_spawn_error *expected = calloc(1, sizeof(*expected));
    mc_player_coordinates *position = NULL;
    VERIFY(expected != NULL &&
           mc_world_bed_spawn_exit(f->worlds[0], f->registry[0], 8, 64, 8, skip, NULL, &position,
                                   expected) == MC_WORLD_STORAGE_OK &&
           expected->returned && expected->retained_world_count == 0 && bs_begin(&s->scalar) == 0);
    mc_world_bed_spawn_execution_init(&s->frame, &s->scalar.cq.execution, f->classes[0], cq_code,
                                      bs_site, &s->owned);
    s->position = &s->marker;
    s->raw = MC_WORLD_STORAGE_LIMIT;
    VERIFY(mc_world_bed_spawn_exit_in_execution(&s->frame, f->worlds[0], f->registry[0], 8, 64, 8,
                                                skip, NULL, &s->position, &s->error, &s->raw) &&
           s->raw == MC_WORLD_STORAGE_OK && s->frame.completed && !s->frame.native_stopped &&
           s->error.returned && s->error.retained_world_count == 0 &&
           s->error.retained_events == expected->retained_events &&
           s->error.event_count == expected->event_count &&
           s->error.candidate_visits == expected->candidate_visits &&
           s->error.remaining_skip == expected->remaining_skip &&
           s->error.helper_pc == expected->helper_pc &&
           ((position == NULL && s->position == NULL) ||
            (position != NULL && s->position != NULL && s->position != &s->marker &&
             position->x == s->position->x && position->y == s->position->y &&
             position->z == s->position->z)));
    for (size_t i = 0; i < expected->retained_events; ++i) {
        const mc_world_bed_spawn_event *a = &expected->events[i], *b = &s->error.events[i];
        VERIFY(a->kind == b->kind && a->pc == b->pc && a->x == b->x && a->y == b->y &&
               a->z == b->z && a->result == b->result && a->completed && b->completed);
    }
    VERIFY(s->owned.ownership_status == MC_WORLD_STORAGE_OK && !cq_pending(&s->owned) &&
           !s->owned.selection_ready && f->worlds[0]->active_calls == 0);
    mc_world_bed_spawn_position_free(position);
    mc_world_bed_spawn_position_free(s->position);
    s->position = NULL;
    VERIFY(mc_world_bed_spawn_error_retire(expected) && mc_world_bed_spawn_error_retire(&s->error));
    free(expected);
    s->frame = (mc_world_bed_spawn_execution){0};
    s->owned = (mc_world_runtime_execution_result){0};
    VERIFY(bs_end(&s->scalar, false) == 0);
    return 0;
}

static int be_faults(be_fixture *s) {
    fixture *f = &s->scalar.cq.source.game;
    mc_world_chunk_nibble *original = f->chunks[0]->e;
    f->chunks[0]->e = NULL;
    VERIFY(bs_begin(&s->scalar) == 0);
    mc_world_bed_spawn_execution_init(&s->frame, &s->scalar.cq.execution, f->classes[0], cq_code,
                                      bs_site, &s->owned);
    s->position = &s->marker;
    s->raw = MC_WORLD_STORAGE_OK;
    VERIFY(!mc_world_bed_spawn_exit_in_execution(&s->frame, f->worlds[0], f->registry[0], 8, 64, 8,
                                                 0, NULL, &s->position, &s->error, &s->raw) &&
           s->raw == MC_WORLD_STORAGE_UNSUPPORTED_STATE && s->position == &s->marker &&
           s->error.source_exception && !s->error.native_policy && s->error.helper_pc == 4 &&
           strcmp(s->error.exception_class, "java/lang/NullPointerException") == 0 &&
           s->error.retained_world_count == 1 && f->worlds[0]->active_calls == 1 &&
           s->frame.scalar.source.source_exception && s->frame.scalar.source.world_pc == 68 &&
           s->frame.scalar.source.chunk_pc == 7 && s->frame.scalar.source.nibble_pc == 0 &&
           s->error.event_count == 1 && !s->error.events[0].completed &&
           s->owned.ownership_status == MC_WORLD_STORAGE_OK &&
           mc_world_bed_spawn_error_retire(&s->error) && f->worlds[0]->active_calls == 0);
    s->frame = (mc_world_bed_spawn_execution){0};
    s->owned = (mc_world_runtime_execution_result){0};
    VERIFY(bs_end(&s->scalar, true) == 0);
    /* Restore only the authored operand after checked native retirement. */
    f->chunks[0]->e = original;

    mc_entity_tick_execution_init(&s->scalar.cq.execution, NULL);
    VERIFY(mc_entity_tick_execution_push_world(&s->scalar.cq.execution, NULL, bs_site,
                                               &s->scalar.outer) == MC_WORLD_STORAGE_OK);
    mc_world_bed_spawn_execution_init(&s->frame, &s->scalar.cq.execution, f->classes[0], cq_code,
                                      bs_site, &s->owned);
    s->position = &s->marker;
    VERIFY(!mc_world_bed_spawn_exit_in_execution(&s->frame, NULL, f->registry[0], 8, 64, 8, 0, NULL,
                                                 &s->position, &s->error, &s->raw) &&
           s->frame.entered && !s->frame.scalar.entered &&
           s->raw == MC_WORLD_STORAGE_UNSUPPORTED_STATE && s->position == &s->marker &&
           s->error.helper_entered && s->error.helper_pc == 4 && s->error.source_exception &&
           !s->error.native_policy &&
           strcmp(s->error.exception_class, "java/lang/NullPointerException") == 0 &&
           s->error.event_count == 1 && !s->error.events[0].completed &&
           s->error.retained_world_count == 0 && s->owned.ownership_status == MC_WORLD_STORAGE_OK &&
           mc_world_bed_spawn_error_retire(&s->error));
    s->frame = (mc_world_bed_spawn_execution){0};
    s->owned = (mc_world_runtime_execution_result){0};
    VERIFY(bs_end(&s->scalar, true) == 0);
    return 0;
}

static mc_world_storage_status be_descendant(void *context, mc_world_runtime *world,
                                             mc_entity *entity, mc_world_storage_error *cause,
                                             mc_world_access_failure *failure) {
    (void)cause;
    (void)failure;
    be_fixture *s = context;
    ++s->callbacks;
    s->callback_valid = world == s->scalar.cq.source.game.worlds[0] && entity != NULL &&
                        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) && s->callbacks == 1 &&
                        s->frame.entered && s->frame.scalar.get_entered &&
                        !s->frame.scalar.get_returned;
    if (!s->callback_valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return mc_entity_tick_execution_push_world(&s->scalar.cq.execution, world, bs_site,
                                               &s->scalar.descendant);
}

static mc_world_storage_status be_support_descendant(void *context, mc_world_runtime *world,
                                                     mc_entity *entity,
                                                     mc_world_storage_error *cause,
                                                     mc_world_access_failure *failure) {
    (void)cause;
    (void)failure;
    be_fixture *s = context;
    ++s->callbacks;
    s->callback_valid = world == s->scalar.cq.source.game.worlds[0] && entity != NULL &&
                        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) && s->callbacks == 1 &&
                        s->frame.support.entered && s->frame.support.detail.error.bytecode == 7;
    if (!s->callback_valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    return mc_entity_tick_execution_push_world(&s->scalar.cq.execution, world, bs_site,
                                               &s->scalar.descendant);
}

static int be_pending(be_fixture *s) {
    fixture *f = &s->scalar.cq.source.game;
    VERIFY(bs_begin(&s->scalar) == 0);
    mc_world_bed_spawn_execution_init(&s->frame, &s->scalar.cq.execution, f->classes[0], cq_code,
                                      bs_site, &s->owned);
    const mc_world_access_source_callbacks callbacks = {.entity_added = be_descendant};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK);
    s->position = &s->marker;
    s->raw = MC_WORLD_STORAGE_LIMIT;
    const bool continued =
        mc_world_bed_spawn_exit_in_execution(&s->frame, f->worlds[0], f->registry[0], 8, 64, 8, 0,
                                             NULL, &s->position, &s->error, &s->raw);
    const int remembered = cq_remember(&s->scalar.cq);
    const bool expected =
        !continued && remembered == 0 && s->callback_valid && s->callbacks == 1 &&
        s->raw == MC_WORLD_STORAGE_BUSY && s->position == &s->marker && s->frame.native_stopped &&
        !s->frame.completed && s->frame.scalar.get_returned &&
        s->frame.scalar.get_status == MC_CHUNK_ACQUIRE_CALLBACK_FAILED &&
        !s->frame.scalar.completed && !s->error.returned && !s->error.result_present &&
        s->error.helper_pc == 4 && s->error.pass == -1 && s->error.table_identity == 0 &&
        s->error.row_identity == 0 && s->error.candidate_visits == 0 && s->error.event_count == 1 &&
        !s->error.events[0].completed && s->error.retained_world_count == 1 &&
        s->owned.ownership_status == MC_WORLD_STORAGE_BUSY && s->owned.root_pending;
    VERIFY(expected);
    size_t close_index = MC_ENTITY_TICK_EXECUTION_WORLDS, closes = 0;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (s->owned.closes[i].used) {
            close_index = i;
            ++closes;
        }
    VERIFY(closes == 1 && close_index < MC_ENTITY_TICK_EXECUTION_WORLDS);
    const mc_world_runtime_execution_close close = s->owned.closes[close_index];
    VERIFY(mc_world_runtime_execution_finish_pending(&s->scalar.cq.execution, &s->owned) ==
               MC_WORLD_STORAGE_BUSY &&
           mc_entity_tick_execution_pop_world(&s->scalar.cq.execution, &s->scalar.descendant,
                                              false) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_execution_retry_close(&s->scalar.cq.execution, &s->owned, &close) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_execution_finish_pending(&s->scalar.cq.execution, &s->owned) ==
               MC_WORLD_STORAGE_OK &&
           !cq_pending(&s->owned) && s->owned.selection_ready && s->position == &s->marker &&
           s->callbacks == 1 && !s->frame.completed && !s->error.events[0].completed &&
           !s->error.returned && mc_world_bed_spawn_error_retire(&s->error) &&
           f->worlds[0]->active_calls == 0);
    s->frame = (mc_world_bed_spawn_execution){0};
    s->owned = (mc_world_runtime_execution_result){0};
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(f->worlds[0], s->scalar.cq.receiver, &removed,
                                                NULL) == MC_WORLD_STORAGE_OK &&
           removed && bs_end(&s->scalar, true) == 0 &&
           mc_world_access_free(s->scalar.cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->scalar.cq.receiver = NULL;
    return 0;
}

static mc_world_storage_status be_replace_m(void *context, mc_world_runtime *world,
                                            mc_entity *entity, mc_world_storage_error *cause,
                                            mc_world_access_failure *failure) {
    (void)cause;
    (void)failure;
    be_fixture *s = context;
    fixture *f = &s->scalar.cq.source.game;
    ++s->callbacks;
    s->callback_valid = world == f->worlds[0] && entity != NULL && s->callbacks == 1 &&
                        s->frame.support.entered && s->frame.support.detail.error.bytecode == 7 &&
                        f->registry[0]->blocks.m == s->captured_m;
    if (!s->callback_valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    /* Authored synchronous Source reentry: the support caller must still use
     * the actual m reference captured before this acquiring GET7. */
    f->registry[0]->blocks.m = NULL;
    return MC_WORLD_STORAGE_OK;
}

static int be_support_capture(be_fixture *s) {
    fixture *f = &s->scalar.cq.source.game;
    mc_world_runtime_classes *code_classes = NULL;
    VERIFY(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &code_classes,
                                           NULL) == MC_WORLD_STORAGE_OK &&
           code_classes != f->worlds[0]->classes &&
           mc_world_runtime_classes_bind_registry(code_classes, f->registry[0], NULL) ==
               MC_REGISTRY_OK &&
           mc_world_runtime_classes_bind_entities(code_classes, f->entity_classes[0]) ==
               MC_ENTITY_OK &&
           bs_begin(&s->scalar) == 0);
    mc_world_solid_support_execution_init(&s->frame.support, &s->scalar.cq.execution, code_classes,
                                          cq_code, bs_site, &s->owned);
    s->captured_m = f->registry[0]->blocks.m;
    const mc_world_access_source_callbacks callbacks = {.entity_added = be_replace_m};
    VERIFY(s->captured_m != NULL &&
           mc_world_access_source_create(&callbacks, s, &s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK);
    bool answer = false;
    s->raw = MC_WORLD_STORAGE_LIMIT;
    VERIFY(mc_world_runtime_call_enter(f->worlds[0], NULL) == MC_WORLD_STORAGE_OK &&
           mc_world_solid_support_detailed_held_in_execution(&s->frame.support, f->worlds[0], 8, 64,
                                                             8, &answer, &s->raw) &&
           answer && s->raw == MC_WORLD_STORAGE_OK && s->frame.support.completed &&
           !s->frame.support.native_stopped && s->callback_valid && s->callbacks == 1 &&
           f->registry[0]->blocks.m == NULL && s->owned.ownership_status == MC_WORLD_STORAGE_OK &&
           !cq_pending(&s->owned) && !s->owned.selection_ready && cq_remember(&s->scalar.cq) == 0);
    mc_world_runtime_call_leave(f->worlds[0]);
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(f->worlds[0], s->scalar.cq.receiver, &removed,
                                                NULL) == MC_WORLD_STORAGE_OK &&
           removed);
    s->frame = (mc_world_bed_spawn_execution){0};
    s->owned = (mc_world_runtime_execution_result){0};
    VERIFY(bs_end(&s->scalar, false) == 0 &&
           mc_world_access_free(s->scalar.cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->scalar.cq.receiver = NULL;
    /* Only the authored between-call Registry input is restored, after the
     * actual entry and explicit context retirement both succeeded. */
    f->registry[0]->blocks.m = s->captured_m;
    VERIFY(mc_world_runtime_classes_free(code_classes, NULL) == MC_WORLD_STORAGE_OK);
    return 0;
}

static int be_support_pending(be_fixture *s) {
    fixture *f = &s->scalar.cq.source.game;
    VERIFY(bs_begin(&s->scalar) == 0);
    mc_world_solid_support_execution_init(&s->frame.support, &s->scalar.cq.execution, f->classes[0],
                                          cq_code, bs_site, &s->owned);
    const mc_world_access_source_callbacks callbacks = {.entity_added = be_support_descendant};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_call_enter(f->worlds[0], NULL) == MC_WORLD_STORAGE_OK);
    bool answer = false;
    s->raw = MC_WORLD_STORAGE_LIMIT;
    VERIFY(!mc_world_solid_support_detailed_held_in_execution(&s->frame.support, f->worlds[0], 8,
                                                              64, 8, &answer, &s->raw) &&
           cq_remember(&s->scalar.cq) == 0 && s->callback_valid && s->callbacks == 1 && !answer &&
           s->frame.support.native_stopped && !s->frame.support.completed &&
           s->frame.support.detail.error.bytecode == 7 &&
           s->frame.support.detail.error.registry.status == MC_REGISTRY_OK &&
           s->frame.support.detail.error.cause.status == MC_WORLD_STORAGE_OK &&
           s->owned.ownership_status == MC_WORLD_STORAGE_BUSY && s->owned.root_pending);
    size_t close_index = MC_ENTITY_TICK_EXECUTION_WORLDS, closes = 0;
    for (size_t i = 0; i < MC_ENTITY_TICK_EXECUTION_WORLDS; ++i)
        if (s->owned.closes[i].used) {
            close_index = i;
            ++closes;
        }
    VERIFY(closes == 1 && close_index < MC_ENTITY_TICK_EXECUTION_WORLDS);
    const mc_world_runtime_execution_close close = s->owned.closes[close_index];
    VERIFY(mc_world_runtime_execution_finish_pending(&s->scalar.cq.execution, &s->owned) ==
               MC_WORLD_STORAGE_BUSY &&
           mc_entity_tick_execution_pop_world(&s->scalar.cq.execution, &s->scalar.descendant,
                                              false) == MC_WORLD_STORAGE_OK &&
           mc_world_runtime_execution_retry_close(&s->scalar.cq.execution, &s->owned, &close) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_execution_finish_pending(&s->scalar.cq.execution, &s->owned) ==
               MC_WORLD_STORAGE_OK &&
           !cq_pending(&s->owned) && !answer && s->callbacks == 1 && !s->frame.support.completed &&
           s->frame.support.detail.error.bytecode == 7);
    mc_world_runtime_call_leave(f->worlds[0]);
    VERIFY(f->worlds[0]->active_calls == 0);
    s->frame = (mc_world_bed_spawn_execution){0};
    s->owned = (mc_world_runtime_execution_result){0};
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(f->worlds[0], s->scalar.cq.receiver, &removed,
                                                NULL) == MC_WORLD_STORAGE_OK &&
           removed && bs_end(&s->scalar, true) == 0 &&
           mc_world_access_free(s->scalar.cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->scalar.cq.receiver = NULL;
    return 0;
}

static mc_world_storage_status be_generation(void *context, mc_world_runtime *world,
                                             mc_entity *entity, mc_world_storage_error *cause,
                                             mc_world_access_failure *failure) {
    (void)cause;
    (void)failure;
    be_fixture *s = context;
    ++s->callbacks;
    mc_entity_tick_execution *execution = &s->scalar.cq.execution;
    mc_entity_tick_execution_world_slot *outer = &execution->world_slots[s->scalar.outer.slot];
    s->callback_valid = world == s->scalar.cq.source.game.worlds[0] && entity != NULL &&
                        mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) && s->callbacks == 1 &&
                        s->frame.scalar.get_entered && !s->frame.scalar.get_returned &&
                        outer->loader_open && outer->loader_slot < MC_ENTITY_TICK_EXECUTION_LOADERS;
    if (!s->callback_valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    s->scalar.root_loader = outer->loader_slot;
    mc_entity_tick_execution_loader_slot *loader = &execution->loader_slots[s->scalar.root_loader];
    s->scalar.reservation_generation = outer->selection_generation;
    s->callback_valid = loader->phase == MC_ENTITY_TICK_EXECUTION_LOADER_RESERVED &&
                        loader->generation == s->scalar.reservation_generation &&
                        s->scalar.reservation_generation < UINT64_MAX;
    if (!s->callback_valid)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    /* Native integrity control over the TWO actual reservation counters. The
     * genuine GET returns OK; only its original completion token becomes stale.
     * No Source field, reservation, loader result or selection is invented. */
    ++outer->selection_generation;
    ++loader->generation;
    s->scalar.projected = true;
    return MC_WORLD_STORAGE_OK;
}

static int be_raw_ok(be_fixture *s) {
    fixture *f = &s->scalar.cq.source.game;
    VERIFY(bs_begin(&s->scalar) == 0);
    mc_world_bed_spawn_execution_init(&s->frame, &s->scalar.cq.execution, f->classes[0], cq_code,
                                      bs_site, &s->owned);
    const mc_world_access_source_callbacks callbacks = {.entity_added = be_generation};
    VERIFY(mc_world_access_source_create(&callbacks, s, &s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK &&
           mc_world_runtime_add_world_access(f->worlds[0], s->scalar.cq.receiver, NULL) ==
               MC_WORLD_STORAGE_OK);
    s->position = &s->marker;
    s->raw = MC_WORLD_STORAGE_LIMIT;
    const bool continued =
        mc_world_bed_spawn_exit_in_execution(&s->frame, f->worlds[0], f->registry[0], 8, 64, 8, 0,
                                             NULL, &s->position, &s->error, &s->raw);
    /* Restore only the recorded native projection counters before observing or
     * draining them. This neither repairs Source state nor resumes the body. */
    if (s->scalar.projected) {
        s->scalar.cq.execution.world_slots[s->scalar.outer.slot].selection_generation =
            s->scalar.reservation_generation;
        s->scalar.cq.execution.loader_slots[s->scalar.root_loader].generation =
            s->scalar.reservation_generation;
        s->scalar.projected = false;
    }
    VERIFY(cq_remember(&s->scalar.cq) == 0 && !continued && s->callback_valid &&
           s->callbacks == 1 && s->raw == MC_WORLD_STORAGE_OK && s->position == &s->marker &&
           s->frame.native_stopped && !s->frame.completed && s->frame.scalar.get_returned &&
           s->frame.scalar.get_status == MC_CHUNK_ACQUIRE_OK &&
           !s->frame.scalar.source.source_exception && !s->error.returned &&
           !s->error.result_present && s->error.helper_pc == 4 && s->error.pass == -1 &&
           s->error.table_identity == 0 && s->error.row_identity == 0 &&
           s->error.candidate_visits == 0 && s->error.event_count == 1 &&
           !s->error.events[0].completed && s->error.retained_world_count == 1 &&
           s->owned.ownership_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
           s->owned.root_pending && s->owned.root_attempt.slot < MC_ENTITY_TICK_EXECUTION_LOADERS &&
           s->owned.pending[s->owned.root_attempt.slot].actual_status == MC_CHUNK_ACQUIRE_OK);
    VERIFY(mc_world_runtime_execution_finish_pending(&s->scalar.cq.execution, &s->owned) ==
               MC_WORLD_STORAGE_OK &&
           !cq_pending(&s->owned) && s->owned.selection_ready && s->position == &s->marker &&
           s->callbacks == 1 && !s->frame.completed && !s->error.events[0].completed &&
           !s->error.returned && mc_world_bed_spawn_error_retire(&s->error) &&
           f->worlds[0]->active_calls == 0);
    s->frame = (mc_world_bed_spawn_execution){0};
    s->owned = (mc_world_runtime_execution_result){0};
    bool removed = false;
    VERIFY(mc_world_runtime_remove_world_access(f->worlds[0], s->scalar.cq.receiver, &removed,
                                                NULL) == MC_WORLD_STORAGE_OK &&
           removed && bs_end(&s->scalar, true) == 0 &&
           mc_world_access_free(s->scalar.cq.receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->scalar.cq.receiver = NULL;
    return 0;
}

static int be_finish(be_fixture *s) {
    VERIFY(ss_stop(&s->scalar.cq.source) == 0 && cq_files(&s->scalar.cq, false) == 0 &&
           ss_finish(&s->scalar.cq.source) == 0);
    for (size_t i = 0; i < COUNT(s->scalar.cq.inputs); ++i)
        mc_ws_free_file_bytes(&s->scalar.cq.inputs[i]);
    free(s);
    return 0;
}

int main(int argc, char **argv) {
    VERIFY(argc == 6);
    be_fixture *s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->scalar.cq.mode = "admission";
    VERIFY(ss_setup(&s->scalar.cq.source, argv[1]) == 0 && cq_files(&s->scalar.cq, true) == 0 &&
           ss_open(&s->scalar.cq.source) == 0 && be_red(s) == 0 && be_compare(s, 0) == 0 &&
           be_compare(s, 1) == 0 && be_compare(s, 999) == 0 && be_faults(s) == 0 &&
           be_finish(s) == 0);
    s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->scalar.cq.mode = "open-descendant";
    VERIFY(ss_setup(&s->scalar.cq.source, argv[2]) == 0 && cq_files(&s->scalar.cq, true) == 0 &&
           ss_open(&s->scalar.cq.source) == 0 && be_pending(s) == 0 && be_finish(s) == 0);
    s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->scalar.cq.mode = "capture";
    VERIFY(ss_setup(&s->scalar.cq.source, argv[3]) == 0 && cq_files(&s->scalar.cq, true) == 0 &&
           ss_open(&s->scalar.cq.source) == 0 && be_support_capture(s) == 0 && be_finish(s) == 0);
    s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->scalar.cq.mode = "support-pending";
    VERIFY(ss_setup(&s->scalar.cq.source, argv[4]) == 0 && cq_files(&s->scalar.cq, true) == 0 &&
           ss_open(&s->scalar.cq.source) == 0 && be_support_pending(s) == 0 && be_finish(s) == 0);
    s = calloc(1, sizeof(*s));
    VERIFY(s != NULL);
    s->scalar.cq.mode = "raw-ok";
    VERIFY(ss_setup(&s->scalar.cq.source, argv[5]) == 0 && cq_files(&s->scalar.cq, true) == 0 &&
           ss_open(&s->scalar.cq.source) == 0 && be_raw_ok(s) == 0 && be_finish(s) == 0);
    printf("PASS: %u actual owned bed exit checks\n", checks);
    return 0;
}
