/* Genuine Player/App/World/Inventory constructors. Imported main is never run. */
#define main pd_unused_statistics_main
#include "../../client/statistics/test_player_dispatch.c"
#undef main

#include "core/math/execution_internal.h"
#include "entity/player/drop_execution_internal.h"
#include "entity/tick/execution_internal.h"
#include "inventory/internal.h"
#include "mc/entity/player/drop.h"
#include "mc/world/runtime/entities.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/runtime_internal.h"
#include "world/runtime/world_access_internal.h"
#include "world/storage/generated_internal.h"
#include "world/storage/objects_internal.h"

typedef struct pd_fixture {
    fixture game;
    mc_math_classes *math_owner;
    mc_math_execution math;
    mc_entity_tick_execution execution;
    mc_entity_tick_execution_world_token outer;
    mc_world_chunk *chunk;
    mc_world_store *fresh_store;
    mc_world_runtime *fresh_world;
    mc_world_provider *fresh_dimension;
    mc_player_drop_call *call;
    mc_player_drop_error error;
    mc_world_access *receiver;
    mc_world_access_receipt *receipt;
    mc_item_stack *stack;
    const char *mode;
    mc_world_runtime_execution_result owned;
    mc_entity_tick_execution_world_token extra[MC_ENTITY_TICK_EXECUTION_WORLDS], descendant;
    size_t callbacks, saved_bindings, extra_count;
    bool callback_valid, removed;
} pd_fixture;

static const mc_entity_tick_execution_site pd_site = {"native.player.drop", "run", "()V", 0};
static const mc_world_runtime_execution_code pd_code = {{"lm", "e", "()V", 6},
                                                        {"lm", "e", "()V", 33}};

static bool pd_is(const pd_fixture *s, const char *mode) {
    return s->mode != NULL && strcmp(s->mode, mode) == 0;
}

static bool pd_chunk(pd_fixture *s) {
    fixture *f = &s->game;
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL &&
          mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &s->chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    const mc_chunk_acquire_context *a = mc_world_runtime_acquisition(f->world);
    CHECK(a != NULL && mc_chunk_cache_publish(a->cache, 0, 0, mc_world_chunk_state(s->chunk)) &&
          mc_entity_set_position(f->player, .5, 64, .5) == MC_ENTITY_OK);
    return true;
}

static bool pd_stack(int32_t id, int32_t count, int32_t damage, mc_item_stack **out) {
    CHECK(mc_item_stack_allocate(out) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(*out, id, count, damage) == MC_ITEM_STACK_OK);
    return true;
}

static bool pd_statistic(fixture *f, size_t index, int32_t expected) {
    mc_registry_statistic_view view;
    int32_t actual = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app.registry, &view, NULL) == MC_REGISTRY_OK &&
          mc_client_stats_writer_value(f->app.stats_writer, view.named[index], &actual, NULL) ==
              MC_CLIENT_STATS_OK &&
          actual == expected);
    return true;
}

static bool pd_chunks_retire(pd_fixture *s) {
    fixture *f = &s->game;
    const mc_chunk_acquire_context *a = mc_world_runtime_acquisition(f->world);
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
          a != NULL && mc_chunk_pending_clear(a->pending) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_cache_init(a->cache, a->cache->entries, a->cache->capacity, a->cache->insertions,
                              a->cache->insertion_capacity));
    while (mc_world_store_chunk_count(f->store) != 0) {
        mc_chunk_state *chunk = NULL;
        CHECK(mc_world_store_chunk_at(f->store, 0, &chunk, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_store_retire_chunk(f->store, chunk, NULL) == MC_WORLD_STORAGE_OK);
    }
    s->chunk = NULL;
    return true;
}

static mc_world_storage_status pd_added(void *context, mc_world_runtime *world, mc_entity *item,
                                        mc_world_storage_error *cause,
                                        mc_world_access_failure *failure) {
    pd_fixture *s = context;
    ++s->callbacks;
    const bool fresh = pd_is(s, "fresh-world");
    mc_world_runtime *expected_world = fresh && s->callbacks == 2 ? s->fresh_world : s->game.world;
    s->callback_valid &= s->callbacks <= (fresh ? 2u : 1u) && world == expected_world &&
                         item != NULL && item->item.c == 40 && s->receiver->active_holds == 1 &&
                         item->base.bF && world->entity_membership.count == 1 &&
                         world->entity_membership.entries[0] == item &&
                         world->fields.b->count == 1 && world->fields.b->items[0] == item;
    if (s->call != NULL && s->mode != NULL) {
        mc_player_drop_execution_view view;
        mc_entity_tick_execution_world_token current;
        s->callback_valid &=
            mc_local_player_drop_execution_inspect(s->call, &view) &&
            view.execution == &s->execution && view.constructor_value == world &&
            view.publication_value == world && view.constructor_world.consumed &&
            view.publication_world.open &&
            mc_entity_tick_execution_current_world(&s->execution, &current) &&
            current.owner == view.publication_world.world.owner &&
            current.slot == view.publication_world.world.slot &&
            current.generation == view.publication_world.world.generation &&
            s->execution.leases[current.slot].phase == MC_WORLD_ACCESS_FAILURE_LEASE_ARMED &&
            mc_local_player_drop_retire(&s->call, NULL) == MC_PLAYER_DROP_BUSY;
    }
    if (!s->callback_valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Player drop bounded callback input", 0};
        return cause->status;
    }
    const bool source_failure =
        s->mode == NULL || pd_is(s, "lease-source") || pd_is(s, "callback-capture");
    if (!source_failure) {
        if (fresh && s->callbacks == 1)
            s->game.player->base.world = s->fresh_world; /* Actual reached callback input. */
        if (pd_is(s, "native-close")) {
            s->saved_bindings = world->persistent_bindings;
            world->persistent_bindings = 0; /* Documented native counter fault only. */
        }
        if (pd_is(s, "scope-drift"))
            return mc_entity_tick_execution_push_world(&s->execution, world, pd_site,
                                                       &s->descendant);
        return MC_WORLD_STORAGE_OK;
    }
    bool removed = false;
    mc_world_storage_status status =
        mc_world_runtime_remove_world_access(world, s->receiver, &removed, cause);
    if (status != MC_WORLD_STORAGE_OK || !removed)
        return status == MC_WORLD_STORAGE_OK ? MC_WORLD_STORAGE_UNSUPPORTED_STATE : status;
    s->removed = true;
    status = mc_world_access_receipt_create(s->receiver, NULL, &s->receipt, failure, cause);
    if (status != MC_WORLD_STORAGE_OK)
        return status;
    s->receipt->exception_class = "java/lang/IllegalStateException";
    *cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                      "Player drop actual Source callback failure", 57};
    if (pd_is(s, "callback-capture")) {
        s->saved_bindings = world->persistent_bindings;
        world->persistent_bindings = 0;
    }
    return cause->status;
}

static bool pd_control(pd_fixture *s, bool *passed) {
    fixture *f = &s->game;
    const bool missing = strstr(s->mode, "math-missing") != NULL;
    const bool limited = strstr(s->mode, "math-limit") != NULL;
    const bool selected = pd_is(s, "selected");
    const bool scatter = !pd_is(s, "normal") && !strstr(s->mode, "-normal");
    CHECK(pd_chunk(s) && pd_stack(3, selected ? 2 : 1, 7, &s->stack));
    if (!missing)
        CHECK(mc_math_classes_create(limited ? 1024 : SIZE_MAX, &s->math_owner, NULL) ==
              MC_MATH_CLASS_OK);
    mc_math_execution_init(&s->math, s->math_owner);
    mc_entity_tick_execution_init(&s->execution, &s->math);
    CHECK(mc_entity_tick_execution_push_world(&s->execution, f->world, pd_site, &s->outer) ==
          MC_WORLD_STORAGE_OK);
    s->callback_valid = true;
    if (pd_is(s, "preflight")) {
        for (unsigned int mode = 0; mode < 2; ++mode) {
            s->error = (mc_player_drop_error){.status = MC_PLAYER_DROP_LIMIT,
                                              .operation = "preserved Player drop error",
                                              .site = {"native.marker", "run", "()V", 203}};
            const mc_player_drop_error before = s->error;
            mc_player_drop_status raw = MC_PLAYER_DROP_WORLD;
            const size_t entities = f->app.entities->live_entities, holds = f->player->holds;
            CHECK(!mc_local_player_drop_stack_begin_in_execution(
                      mode == 0 ? NULL : &s->execution, mode == 1 ? NULL : f->app.world_classes,
                      pd_code, f->app.registry, f->player, s->stack, true, &s->call, &raw,
                      &s->error, &s->owned) &&
                  s->call == NULL && raw == MC_PLAYER_DROP_WORLD &&
                  s->error.status == before.status && s->error.operation == before.operation &&
                  s->error.site.pc == before.site.pc &&
                  s->owned.ownership_status != MC_WORLD_STORAGE_OK &&
                  f->app.entities->live_entities == entities && f->player->holds == holds &&
                  !s->math.selected && !s->math.failed && s->math.owned == NULL);
            s->owned = (mc_world_runtime_execution_result){0};
        }
        *passed = true;
    } else {
        const bool callback = pd_is(s, "lease-source") || pd_is(s, "callback-capture") ||
                              pd_is(s, "native-close") || pd_is(s, "scope-drift") ||
                              pd_is(s, "scatter") || pd_is(s, "normal") || selected;
        if (callback) {
            const mc_world_access_source_callbacks callbacks = {.entity_added = pd_added};
            CHECK(mc_world_access_source_create(&callbacks, s, &s->receiver, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  mc_world_runtime_add_world_access(f->world, s->receiver, NULL) ==
                      MC_WORLD_STORAGE_OK);
        }
        if (selected)
            CHECK(mc_inventory_player_set(f->player->local_player->fields.player.c, 0, s->stack,
                                          NULL) == MC_INVENTORY_OK);
        if (pd_is(s, "absent"))
            CHECK(mc_entity_set_position(f->player, 10000, 64, 10000) == MC_ENTITY_OK);
        if (pd_is(s, "scope-capacity"))
            while (s->extra_count + 1 < MC_ENTITY_TICK_EXECUTION_WORLDS) {
                CHECK(mc_entity_tick_execution_push_world(&s->execution, f->world, pd_site,
                                                          &s->extra[s->extra_count]) ==
                      MC_WORLD_STORAGE_OK);
                ++s->extra_count;
            }
        if (pd_is(s, "nullable-world") || pd_is(s, "null-stack"))
            f->player->base.world = NULL; /* Declared source input; native graph stays held. */
        mc_java_random expected = *f->player->random;
        mc_player_drop_status raw = MC_PLAYER_DROP_WORLD;
        const bool completed =
            selected ? mc_local_player_drop_selected_begin_in_execution(
                           &s->execution, f->app.world_classes, pd_code, f->app.registry, f->player,
                           &s->call, &raw, &s->error, &s->owned)
                     : mc_local_player_drop_stack_begin_in_execution(
                           &s->execution, f->app.world_classes, pd_code, f->app.registry, f->player,
                           pd_is(s, "null-stack") ? NULL : s->stack, scatter, &s->call, &raw,
                           &s->error, &s->owned);
        if (s->saved_bindings != 0)
            f->world->persistent_bindings = s->saved_bindings;
        if (pd_is(s, "nullable-world") || pd_is(s, "null-stack"))
            f->player->base.world = f->world; /* Teardown operand only; never resume the call. */
        mc_player_drop_state state;
        mc_player_drop_execution_view view;
        CHECK(mc_local_player_drop_inspect(s->call, &state) &&
              mc_local_player_drop_execution_inspect(s->call, &view) &&
              view.execution == &s->execution && view.result != &s->owned);
        if (pd_is(s, "null-stack"))
            CHECK(completed && raw == MC_PLAYER_DROP_OK && state.returned && state.source_pc == 4 &&
                  !state.item_allocated && !state.publication_entered &&
                  !state.statistic_returned && state.random_draws == 0 &&
                  !view.constructor_world.created && !view.publication_world.created &&
                  s->math.owned == NULL && !s->math.failed);
        else if (pd_is(s, "scope-capacity"))
            CHECK(!completed && raw == MC_PLAYER_DROP_OK && state.item_allocated &&
                  !state.item_constructed && state.item->phase == MC_ENTITY_RAW &&
                  state.source_pc == 10 && state.x == 0 && !view.constructor_world.created &&
                  !view.publication_world.created && state.random_draws == 0 && !s->math.failed &&
                  view.result->ownership_status == MC_WORLD_STORAGE_LIMIT);
        else if (missing || limited)
            CHECK(!completed && raw == MC_PLAYER_DROP_OK && s->error.status == MC_PLAYER_DROP_OK &&
                  state.item_constructed && state.pickup_delay_stored && !state.returned &&
                  !state.publication_entered && !state.statistic_returned &&
                  state.source_pc == (scatter ? 83u : 132u) &&
                  state.random_draws == (scatter ? 2u : 0u) && s->math.failed &&
                  s->math.first.caller.pc == (scatter ? 83 : 132) &&
                  strcmp(s->math.first.caller.class_name, "gs") == 0 &&
                  view.constructor_world.consumed && !view.publication_world.created &&
                  f->world->fields.b->count == 0);
        else if (pd_is(s, "nullable-world"))
            CHECK(!completed && raw == MC_PLAYER_DROP_NULL_REFERENCE && state.item_constructed &&
                  state.item->base.world == NULL && view.constructor_value == NULL &&
                  view.publication_value == NULL && !state.statistic_returned &&
                  s->error.source_exception && !s->error.native_policy && s->error.site.pc == 315 &&
                  s->error.child.pc == 5);
        else if (pd_is(s, "lease-source") || pd_is(s, "callback-capture")) {
            const mc_world_access_failure *protected = NULL;
            CHECK(!completed && raw == MC_PLAYER_DROP_WORLD && s->callback_valid &&
                  s->callbacks == 1 && s->removed && s->receipt->holds == 1 &&
                  s->error.source_exception && !s->error.native_policy && !state.returned &&
                  !state.statistic_returned && s->error.publication->cause.system_error == 57 &&
                  mc_world_access_failure_lease_inspect(
                      &s->execution.leases[view.publication_world.world.slot], &protected) &&
                  protected->receipt == s->receipt && s->receiver->active_holds == 1 &&
                  view.result->ownership_status == (pd_is(s, "callback-capture")
                                                        ? MC_WORLD_STORAGE_UNSUPPORTED_STATE
                                                        : MC_WORLD_STORAGE_OK) &&
                  mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY);
            protected = NULL;
        } else if (pd_is(s, "native-close") || pd_is(s, "scope-drift")) {
            const mc_player_drop_world_execution_view before = view.publication_world;
            mc_player_drop_call *const same = s->call;
            const mc_player_drop_status retire_status = mc_local_player_drop_retire(&s->call, NULL);
            CHECK(!completed && raw == MC_PLAYER_DROP_OK && s->error.status == MC_PLAYER_DROP_OK &&
                  !state.returned && state.inserted && state.publication_returned &&
                  !state.statistic_returned && state.source_pc == 315 && s->callbacks == 1 &&
                  before.open && before.close_attempted && before.locals_drained &&
                  !before.failed && !before.consumed &&
                  before.initial_close_status != MC_WORLD_STORAGE_OK &&
                  retire_status == MC_PLAYER_DROP_CLEANUP && s->call == same);
            if (pd_is(s, "scope-drift"))
                CHECK(mc_local_player_drop_native_close(s->call) == MC_WORLD_STORAGE_BUSY &&
                      mc_entity_tick_execution_pop_world(&s->execution, &s->descendant, true) ==
                          MC_WORLD_STORAGE_OK);
            else {
                const size_t bindings = f->world->persistent_bindings;
                f->world->persistent_bindings = 0;
                const mc_world_storage_status close = mc_local_player_drop_native_close(s->call);
                f->world->persistent_bindings = bindings;
                CHECK(close == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
                      mc_local_player_drop_execution_inspect(s->call, &view) &&
                      view.publication_world.open && !view.publication_world.consumed &&
                      view.publication_world.initial_close_status == before.initial_close_status);
            }
        } else {
            CHECK(completed && raw == MC_PLAYER_DROP_OK && state.returned &&
                  state.item_constructed && state.pickup_delay_stored &&
                  state.publication_returned && state.statistic_returned &&
                  state.inserted == !pd_is(s, "absent") && view.constructor_value == f->world &&
                  view.publication_value == f->world &&
                  state.source_pc == (selected ? 20u : 326u) && view.constructor_world.consumed &&
                  view.publication_world.consumed);
            if (pd_is(s, "absent"))
                CHECK(f->world->fields.b->count == 0 && !view.publication->get_returned);
            if (selected) {
                mc_item_stack_fields fields;
                CHECK(state.inventory_split_returned && state.stack != s->stack &&
                      f->player->local_player->fields.player.c->main_slots[0] == s->stack &&
                      mc_item_stack_fields_get(s->stack, &fields) == MC_ITEM_STACK_OK &&
                      fields.count == 1);
            }
        }
        for (size_t i = 0; i < state.random_draws; ++i)
            CHECK(mc_java_random_next_float(&expected) == state.draws[i]);
        CHECK(f->player->random->state == expected.state &&
              pd_statistic(f, 16, completed && !pd_is(s, "null-stack") ? 1 : 0) &&
              mc_local_player_drop_native_close(s->call) == MC_WORLD_STORAGE_OK &&
              mc_local_player_drop_execution_inspect(s->call, &view) &&
              !view.constructor_world.open && !view.publication_world.open &&
              view.completed == completed);
        if (completed) {
            mc_entity_tick_execution_world_token other, current;
            CHECK(mc_entity_tick_execution_push_world(&s->execution, NULL, pd_site, &other) ==
                      MC_WORLD_STORAGE_OK &&
                  mc_local_player_drop_native_close(s->call) == MC_WORLD_STORAGE_OK &&
                  mc_entity_tick_execution_current_world(&s->execution, &current) &&
                  current.owner == other.owner && current.slot == other.slot &&
                  current.generation == other.generation &&
                  mc_entity_tick_execution_pop_world(&s->execution, &other, false) ==
                      MC_WORLD_STORAGE_OK);
        }
        if (!missing && !limited && !pd_is(s, "null-stack") && !pd_is(s, "scope-capacity")) {
            mc_math_class_state math;
            CHECK(s->math.owned == s->math_owner && !s->math.failed &&
                  mc_math_classes_state(s->math_owner, &math, NULL) == MC_MATH_CLASS_OK &&
                  math.phase == MC_MATH_CLASS_READY && math.holds == 2);
        }
        const size_t callbacks_returned = s->callbacks;
        s->error = (mc_player_drop_error){0};
        state = (mc_player_drop_state){0};
        view = (mc_player_drop_execution_view){0};
        s->owned = (mc_world_runtime_execution_result){0};
        CHECK(mc_local_player_drop_retire(&s->call, NULL) == MC_PLAYER_DROP_OK && s->call == NULL &&
              s->callbacks == callbacks_returned && f->player->random->state == expected.state);
        if (s->receipt != NULL)
            CHECK(mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_BUSY);
        *passed = true;
    }
    while (s->extra_count != 0) {
        --s->extra_count;
        CHECK(mc_entity_tick_execution_pop_world(&s->execution, &s->extra[s->extra_count], true) ==
              MC_WORLD_STORAGE_OK);
    }
    CHECK(mc_entity_tick_execution_pop_world(&s->execution, &s->outer, true) ==
              MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
          mc_world_access_receipt_retire(&s->receipt, NULL) == MC_WORLD_STORAGE_OK &&
          mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK &&
          mc_math_classes_release(&s->math_owner, NULL) == MC_MATH_CLASS_OK);
    if (s->receiver != NULL) {
        if (!s->removed) {
            bool removed = false;
            CHECK(mc_world_runtime_remove_world_access(f->world, s->receiver, &removed, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  removed);
        }
        CHECK(mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_OK);
    }
    s->receiver = NULL;
    mc_item_stack_release(s->stack);
    s->stack = NULL;
    CHECK(pd_chunks_retire(s));
    return true;
}

static bool pd_fresh(pd_fixture *s, bool *passed) {
    fixture *f = &s->game;
    CHECK(pd_chunk(s) && pd_stack(3, 2, 7, &s->stack));
    uint8_t ids[256] = {0};
    mc_registry_block_view blocks;
    CHECK(mc_registry_classes_inspect_blocks(f->app.registry, &blocks, NULL) == MC_REGISTRY_OK);
    for (size_t i = 1; i < 256; ++i)
        ids[i] = blocks.m[i] == NULL ? 0 : (uint8_t)i;
    const mc_world_store_options options = {.session_token = 6382,
                                            .block_map = ids,
                                            .block_map_length = 256,
                                            .limits = mc_world_store_default_limits()};
    CHECK(mc_world_store_open("fresh-world", f->regions, &options, &s->fresh_store, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_provider_create(0, NULL, &s->fresh_dimension, NULL) == MC_WORLD_PROVIDER_OK);
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    limits.max_provider_chunks = 1;
    CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA,
                                          f->app.world_classes, &limits, &s->fresh_world,
                                          NULL) == MC_WORLD_STORAGE_OK);
    const mc_world_runtime_world_arguments arguments = {.store = s->fresh_store,
                                                        .name = f->name,
                                                        .seed = MC_PLAYER_STATISTICS_WORLD_SEED,
                                                        .dimension = s->fresh_dimension};
    CHECK(mc_world_runtime_construct_world(s->fresh_world, &arguments, NULL) ==
          MC_WORLD_STORAGE_OK);
    uint8_t *bytes = calloc(MC_CHUNK_BLOCKS, 1);
    mc_world_chunk *object = NULL;
    CHECK(bytes != NULL &&
          mc_world_store_create_generated(s->fresh_store, s->fresh_world, 0, 0, 0, &bytes,
                                          MC_CHUNK_BLOCKS, &object, NULL) == MC_WORLD_STORAGE_OK &&
          bytes == NULL);
    const mc_chunk_acquire_context *a = mc_world_runtime_acquisition(s->fresh_world);
    CHECK(a != NULL && mc_chunk_cache_publish(a->cache, 0, 0, mc_world_chunk_state(object)) &&
          mc_math_classes_create(SIZE_MAX, &s->math_owner, NULL) == MC_MATH_CLASS_OK);
    mc_math_execution_init(&s->math, s->math_owner);
    mc_entity_tick_execution_init(&s->execution, &s->math);
    CHECK(mc_entity_tick_execution_push_world(&s->execution, f->world, pd_site, &s->outer) ==
          MC_WORLD_STORAGE_OK);
    s->callback_valid = true;
    const mc_world_access_source_callbacks callbacks = {.entity_added = pd_added};
    CHECK(mc_world_access_source_create(&callbacks, s, &s->receiver, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->world, s->receiver, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(s->fresh_world, s->receiver, NULL) ==
              MC_WORLD_STORAGE_OK);
    /* Both actual Worlds share the held defining namespaces but own distinct
     * Stores, providers, generated chunks and membership lists. The first real
     * publication callback changes the next call's Player.World input; no hook
     * is fabricated between one drop's constructor and publication. */
    mc_entity *published[2] = {0};
    for (size_t pass = 0; pass < 2; ++pass) {
        mc_world_runtime *expected = pass == 0 ? f->world : s->fresh_world;
        mc_player_drop_status raw = MC_PLAYER_DROP_WORLD;
        CHECK(mc_local_player_drop_stack_begin_in_execution(
            &s->execution, f->app.world_classes, pd_code, f->app.registry, f->player, s->stack,
            true, &s->call, &raw, &s->error, &s->owned));
        mc_player_drop_state state;
        mc_player_drop_execution_view view;
        CHECK(raw == MC_PLAYER_DROP_OK && mc_local_player_drop_inspect(s->call, &state) &&
              mc_local_player_drop_execution_inspect(s->call, &view) && state.returned &&
              state.inserted && state.item->base.world == expected &&
              view.constructor_value == expected && view.publication_value == expected &&
              view.constructor_world.consumed && view.publication_world.consumed &&
              f->player->base.world == s->fresh_world && s->callback_valid &&
              s->callbacks == pass + 1);
        published[pass] = state.item;
        state = (mc_player_drop_state){0};
        view = (mc_player_drop_execution_view){0};
        s->error = (mc_player_drop_error){0};
        s->owned = (mc_world_runtime_execution_result){0};
        CHECK(mc_local_player_drop_native_close(s->call) == MC_WORLD_STORAGE_OK &&
              mc_local_player_drop_retire(&s->call, NULL) == MC_PLAYER_DROP_OK);
    }
    CHECK(f->world->entity_membership.count == 1 && s->fresh_world->entity_membership.count == 1 &&
          f->world->entity_membership.entries[0] == published[0] &&
          s->fresh_world->entity_membership.entries[0] == published[1] &&
          published[0] != published[1] && pd_statistic(f, 16, 2) && s->math.owned == s->math_owner);
    published[0] = published[1] = NULL; /* Expire membership borrows before terminal work. */
    f->player->base.world = f->world;   /* Declared teardown operand only. */
    CHECK(mc_entity_tick_execution_pop_world(&s->execution, &s->outer, false) ==
              MC_WORLD_STORAGE_OK &&
          mc_entity_tick_execution_retire(&s->execution) == MC_WORLD_STORAGE_OK &&
          mc_math_execution_retire(&s->math) == MC_MATH_CLASS_OK &&
          mc_math_classes_release(&s->math_owner, NULL) == MC_MATH_CLASS_OK);
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->world, s->receiver, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    removed = false;
    CHECK(mc_world_runtime_remove_world_access(s->fresh_world, s->receiver, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed && mc_world_access_free(s->receiver, NULL) == MC_WORLD_STORAGE_OK);
    s->receiver = NULL;
    mc_item_stack_release(s->stack);
    s->stack = NULL;
    CHECK(mc_world_runtime_retire_entity_containers(s->fresh_world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_chunk_pending_clear(a->pending) == MC_CHUNK_ACQUIRE_OK &&
          mc_chunk_cache_init(a->cache, a->cache->entries, a->cache->capacity, a->cache->insertions,
                              a->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(s->fresh_store, mc_world_chunk_state(object), NULL) ==
              MC_WORLD_STORAGE_OK);
    object = NULL;
    mc_world_provider_unbind(s->fresh_dimension);
    CHECK(mc_world_runtime_retire_world(s->fresh_world, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_free(s->fresh_world, NULL) == MC_WORLD_STORAGE_OK);
    s->fresh_world = NULL;
    mc_world_provider_release(s->fresh_dimension);
    s->fresh_dimension = NULL;
    CHECK(mc_world_store_free_checked(&s->fresh_store, NULL) == MC_WORLD_STORAGE_OK &&
          pd_chunks_retire(s));
    *passed = true;
    return true;
}

static bool pd_mode(const char *mode) {
    static const char *const modes[] = {"scatter",
                                        "normal",
                                        "selected",
                                        "null-stack",
                                        "nullable-world",
                                        "absent",
                                        "math-missing-scatter",
                                        "math-missing-normal",
                                        "math-limit-scatter",
                                        "math-limit-normal",
                                        "preflight",
                                        "lease-source",
                                        "callback-capture",
                                        "native-close",
                                        "scope-drift",
                                        "scope-capacity"};
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
        if (strcmp(mode, modes[i]) == 0)
            return true;
    return strcmp(mode, "fresh-world") == 0;
}

#ifndef MC_PLAYER_DROP_EXECUTION_MAIN
#define MC_PLAYER_DROP_EXECUTION_MAIN main
#endif
int MC_PLAYER_DROP_EXECUTION_MAIN(int argc, char **argv) {
    if (argc != 4 || !pd_mode(argv[1]) || CD(argv[3]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    printf("PLAYER_DROP_EXECUTION_BEGIN mode=%s\n", argv[1]);
    fflush(stdout);
    pd_fixture *s = calloc(1, sizeof(*s));
    bool passed = false;
    if (s != NULL)
        s->mode = argv[1];
    if (s == NULL || !setup(&s->game, argv[2], "normal", true) ||
        !(pd_is(s, "fresh-world") ? pd_fresh(s, &passed) : pd_control(s, &passed)) ||
        !teardown(&s->game))
        return 1;
    free(s);
    if (!passed)
        return 1;
    printf("PASS: %zu execution-owned Player drop checks;mode=%s\n", checks, argv[1]);
    return 0;
}
