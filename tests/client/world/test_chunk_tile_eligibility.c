#include "client/application_state.h"
#include "client/render/tile_entity_renderer/internal.h"
#include "client/render_engine/internal.h"
#include "client/world/source_chunk_compile_internal.h"
#include "mc/block/material/material.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/entities.h"
#include "world/runtime/tile_entities/identity_set_internal.h"
#include "world/runtime/tile_entities/tile_entities_internal.h"

#include <stdio.h>
#include <string.h>

/* Execute the actual selected DK394..417 child over real LL/OW Class owners.
 * No N constructor, GL context, Chunk-cache query or Tile draw is claimed. */
static unsigned checks;
#define CHECK(v)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(v)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #v);                                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(void) {
    int result = 1;
    static mc_math_trig trig;
    mc_java_random_profile *profile = NULL;
    mc_materials *materials = NULL;
    mc_registry_classes *registry = NULL;
    mc_entity_classes *entities = NULL;
    mc_world_runtime_classes *worlds = NULL;
    mc_client_render_classes *models = NULL;
    mc_client_tile_renderer_classes *classes = NULL;
    mc_tile_entity *chest = NULL, *spawner = NULL, *second = NULL, *captured = NULL;
    mc_source_dk_compile *call = NULL;
    mc_client_render_global_error error, cleanup_error;
    mc_client_tile_renderer_error tile_error;
    client_application app = {0};
    mc_client_render_global owner = {0};
    mc_client_render_list tiles = {0};
    mc_client_render_list shared = {0}, other_tiles = {0};
    mc_client_source_dk dk = {0};
    mc_client_source_dk other_dk = {0};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&materials));
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    const mc_registry_options options = {
        .side = MC_REGISTRY_CLIENT, .random_profile = profile, .materials = materials};
    CHECK(mc_registry_classes_allocate(&options, &registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_math_trig_init(&trig));
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, &trig, &worlds, NULL) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(worlds, registry, NULL) == MC_REGISTRY_OK &&
          mc_world_runtime_classes_bind_entities(worlds, entities) == MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(entities, profile, &models) == MC_CLIENT_MODEL_OK);
    app.profile = profile;
    app.render_classes = models;
    app.world_classes = worlds;
    app.entities = entities;
    owner.fields.t = &app;
    owner.active = true; /* The genuine child borrows its caller's entered N guard. */
    owner.arena.options = mc_client_render_engine_default_options();
    dk.owner = &owner;
    dk.phase = MC_CLIENT_RENDER_GLOBAL_READY;
    dk.fields.B = &tiles;
    dk.tile_list_origin = &tiles; /* Explicit native List fixture identities. */
    owner.fields.a = owner.tile_list_origin = &shared;
    dk.fields.F = &shared;
    other_dk.owner = &owner;
    other_dk.phase = MC_CLIENT_RENDER_GLOBAL_READY;
    other_dk.fields.B = other_dk.tile_list_origin = &other_tiles;
    other_dk.fields.F = &shared;
    mc_client_chunk_compile_state state;

    /* NULL Tile still evaluates the real getstatic and initializes LL, then
     * returns false without reading B or inserting a Class-map key. */
    CHECK(mc_source_dk_tile_eligibility_entered(&dk, NULL, &call, &error));
    CHECK(mc_source_dk_compile_inspect(call, &state) && state.tile_eligibility_returned &&
          !state.tile_eligible && state.tile_eligibility_calls == 1 && state.source_pc == 417 &&
          owner.tile_consumer.attached && tiles.count == 0);
    classes = owner.tile_consumer.classes;
    mc_client_tile_renderer_state initial;
    CHECK(mc_client_tile_renderer_inspect(classes, &initial) &&
          initial.phase == MC_CLIENT_TILE_RENDERER_READY);
    CHECK(mc_client_tile_renderer_classes_retire(&classes, &tile_error) ==
              MC_CLIENT_TILE_RENDERER_BUSY &&
          classes != NULL);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_tile_entity_allocate_in_classes(worlds, MC_TILE_ENTITY_CHEST, &chest) ==
          MC_TILE_ENTITY_OK);
    const mc_tile_entity_class_literal *chest_class = NULL;
    CHECK(mc_tile_entity_get_class(chest, &chest_class) == MC_TILE_ENTITY_OK);

    /* Ordinary Map failure retains completed LL state and the reached child,
     * while caller cleanup releases only its own captured dispatcher/local. */
    const size_t class_limit = classes->options.class_bytes;
    classes->options.class_bytes = classes->bytes;
    const bool limited = mc_source_dk_tile_eligibility_entered(&dk, chest, &call, &error);
    classes->options.class_bytes = class_limit;
    CHECK(!limited && error.status == MC_CLIENT_RENDER_GLOBAL_LIMIT && error.source_pc == 399 &&
          !error.source_returned && error.tile_renderer != NULL &&
          error.tile_renderer->status == MC_CLIENT_TILE_RENDERER_LIMIT &&
          error.child.class_name != NULL && strcmp(error.child.class_name, "ll") == 0);
    mc_client_tile_renderer_state after_failure;
    CHECK(mc_client_tile_renderer_inspect(classes, &after_failure) &&
          after_failure.phase == MC_CLIENT_TILE_RENDERER_READY &&
          after_failure.entries == initial.entries);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);

    CHECK(mc_source_dk_tile_eligibility_entered(&dk, chest, &call, &error));
    CHECK(mc_source_dk_compile_inspect(call, &state) && state.tile_eligibility_returned &&
          !state.tile_eligible && tiles.count == 0);
    mc_client_tile_renderer_dispatcher *dispatcher = NULL;
    CHECK(mc_client_tile_renderer_static_get(classes, &dispatcher, &tile_error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    mc_client_tile_renderer_map_entry cached, repeated;
    CHECK(mc_client_tile_renderer_map_find(dispatcher, chest_class, &cached, &tile_error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          cached.present && cached.value == NULL);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    dk.fields.B = NULL;
    const bool chest_again = mc_source_dk_tile_eligibility_entered(&dk, chest, &call, &error);
    dk.fields.B = &tiles;
    CHECK(chest_again && mc_source_dk_compile_inspect(call, &state) && !state.tile_eligible &&
          mc_client_tile_renderer_map_find(dispatcher, chest_class, &repeated, &tile_error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          repeated.identity == cached.identity && repeated.value == NULL);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);

    /* The next invocation rereads the actual static field. An authored null
     * receiver fails at invoke399, despite this N's existing class association. */
    classes->a = NULL;
    const bool null_static = mc_source_dk_tile_eligibility_entered(&dk, chest, &call, &error);
    classes->a = dispatcher;
    CHECK(!null_static && error.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          error.source_pc == 399 && !error.native_policy && error.tile_renderer == NULL);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);

    /* Real registered Spawner Class is eligible, with no construction or draw
     * shortcut. B's occurrence survives retirement of the evaluated local19. */
    CHECK(mc_tile_entity_allocate_in_classes(worlds, MC_TILE_ENTITY_SPAWNER, &spawner) ==
          MC_TILE_ENTITY_OK);
    dk.fields.B = NULL;
    const bool null_list = mc_source_dk_tile_eligibility_entered(&dk, spawner, &call, &error);
    dk.fields.B = &tiles;
    CHECK(!null_list && error.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          error.source_pc == 411 && !error.native_policy);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    const size_t arena_limit = owner.arena.options.max_constructor_bytes;
    owner.arena.options.max_constructor_bytes =
        owner.arena.bytes + sizeof(render_allocation) + sizeof(mc_source_dk_tile_hold);
    const bool list_limited = mc_source_dk_tile_eligibility_entered(&dk, spawner, &call, &error);
    owner.arena.options.max_constructor_bytes = arena_limit;
    CHECK(!list_limited && error.status == MC_CLIENT_RENDER_GLOBAL_LIMIT &&
          error.source_pc == 411 && error.native_policy && tiles.count == 0 &&
          dk.tile_holds != NULL && dk.tile_holds->tile == NULL);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_source_dk_tile_eligibility_entered(&dk, spawner, &call, &error));
    CHECK(mc_source_dk_compile_inspect(call, &state) && state.tile_eligibility_returned &&
          state.tile_eligible && tiles.count == 1 && tiles.entries[0] == spawner &&
          dk.tile_holds != NULL && dk.tile_holds->tile == spawner);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_source_dk_tile_eligibility_entered(&dk, spawner, &call, &error) && tiles.count == 2 &&
          tiles.entries[1] == spawner);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);

    /* Selected original98..115 /394..417 /547..626 List/Set bodies over
     * explicitly registered native fixture Lists. No GL/ChunkCache execution
     * is claimed by these component controls. Identity uses the native profile. */
    CHECK(mc_source_dk_tile_lists_entered(&dk, NULL, 0, &call, &error) && tiles.count == 0 &&
          shared.count == 0);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    mc_tile_entity *duplicates[] = {spawner, spawner};
    CHECK(mc_source_dk_tile_lists_entered(&dk, duplicates, 2, &call, &error));
    CHECK(mc_source_dk_compile_inspect(call, &state) && state.returned && state.source_pc == 626 &&
          state.new_tile_set_count == 1 && state.old_tile_set_count == 0 &&
          state.shared_tiles_added == 1 && state.shared_tiles_removed == 0 && tiles.count == 2 &&
          shared.count == 1 && shared.entries[0] == spawner);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_render_global_tile_list_get(&owner, &shared, 0, &captured, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          captured == spawner);
    mc_tile_entity_release(captured);
    captured = NULL;
    const size_t stable_bytes = owner.arena.bytes, stable_allocations = owner.arena.allocations;
    for (size_t i = 0; i < 64; ++i) {
        const uint32_t previous_mod_count = shared.mod_count;
        CHECK(mc_source_dk_tile_lists_entered(&dk, duplicates, 2, &call, &error));
        CHECK(mc_source_dk_compile_inspect(call, &state) && state.new_tile_set_count == 0 &&
              state.old_tile_set_count == 0 && state.shared_tiles_added == 0 &&
              state.shared_tiles_removed == 0 && shared.count == 1 && tiles.count == 2 &&
              shared.mod_count == previous_mod_count + 1u);
        CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK &&
              owner.arena.bytes == stable_bytes && owner.arena.allocations == stable_allocations);
    }
    shared.entries[0] = &owner; /* Actual unproved object pointer, never a Tile cast. */
    CHECK(mc_render_global_tile_list_get(&owner, &shared, 0, &captured, &error) ==
              MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE &&
          captured == NULL && error.source_pc == 551 && error.native_policy);
    shared.entries[0] = spawner;
    CHECK(mc_render_global_tile_list_get(&owner, (mc_client_render_list *)&owner, 0, &captured,
                                         &error) == MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE &&
          captured == NULL && error.native_policy);
    CHECK(mc_render_global_tile_list_get(&owner, NULL, 0, &captured, &error) ==
              MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE &&
          captured == NULL && !error.native_policy);
    CHECK(mc_render_global_tile_list_get(&owner, &shared, -1, &captured, &error) ==
              MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX &&
          captured == NULL && !error.native_policy);

    CHECK(mc_tile_entity_allocate_in_classes(worlds, MC_TILE_ENTITY_SPAWNER, &second) ==
          MC_TILE_ENTITY_OK);
    mc_tile_entity *one_second[] = {second}, *one_spawner[] = {spawner};
    CHECK(mc_source_dk_tile_lists_entered(&other_dk, one_second, 1, &call, &error) &&
          shared.count == 2 && shared.entries[0] == spawner && shared.entries[1] == second);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_render_global_tile_list_get(&owner, &shared, 1, &captured, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          captured == second);
    mc_tile_entity_release(captured);
    captured = NULL;
    CHECK(mc_source_dk_tile_lists_entered(&dk, NULL, 0, &call, &error) && shared.count == 1 &&
          shared.entries[0] == second);
    CHECK(mc_source_dk_compile_inspect(call, &state) && state.shared_tiles_removed == 1);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_render_global_tile_list_get(&owner, &shared, 0, &captured, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          captured == second); /* Kept row's exact ledger index was compacted. */
    mc_tile_entity_release(captured);
    captured = NULL;
    CHECK(mc_source_dk_tile_lists_entered(&other_dk, NULL, 0, &call, &error) && shared.count == 0);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);

    /* Refused addAll584 publishes nothing; completed B/new Set effects remain.
     * Only native source-list capacity/arena quota are declared fault inputs. */
    void **saved_entries = shared.entries;
    const size_t saved_capacity = shared.capacity;
    shared.entries = NULL;
    shared.capacity = 0;
    owner.arena.options.max_constructor_bytes = owner.arena.bytes;
    const uint32_t before_refused_mod_count = shared.mod_count;
    const bool refused_add = mc_source_dk_tile_lists_entered(&dk, one_spawner, 1, &call, &error);
    owner.arena.options.max_constructor_bytes = arena_limit;
    CHECK(!refused_add && error.status == MC_CLIENT_RENDER_GLOBAL_LIMIT && error.source_pc == 584 &&
          error.native_policy && shared.count == 0 && tiles.count == 1 &&
          mc_source_dk_compile_inspect(call, &state) && state.new_tile_set_count == 1 &&
          state.shared_tiles_added == 0 && !state.returned &&
          shared.mod_count == before_refused_mod_count + 1u);
    shared.entries = saved_entries;
    shared.capacity = saved_capacity;
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_source_dk_tile_lists_entered(&dk, NULL, 0, &call, &error) && shared.count == 0);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);

    CHECK(mc_source_dk_tile_lists_entered(&dk, one_second, 1, &call, &error) && shared.count == 1);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    shared.entries[0] = &owner;
    const bool refused_remove = mc_source_dk_tile_lists_entered(&dk, one_spawner, 1, &call, &error);
    CHECK(!refused_remove && error.status == MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE &&
          error.source_pc == 608 && error.native_policy && shared.count == 2 &&
          shared.entries[0] == &owner && shared.entries[1] == spawner && tiles.count == 1 &&
          tiles.entries[0] == spawner && mc_source_dk_compile_inspect(call, &state) &&
          state.shared_tiles_added == 1 && state.shared_tiles_removed == 0 &&
          state.old_tile_set_count == 1 && !state.returned);
    shared.entries[0] = second;
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_render_global_tile_list_get(&owner, &shared, 0, &captured, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          captured == second);
    mc_tile_entity_release(captured);
    captured = NULL;
    /* A failed Source call is not repaired by retirement. Explicit caller-side
     * native List-clear input ends its two published occurrences for the next
     * independent component case; actual N.clear249 is integration-owned. */
    shared.count = 0;
    mc_render_global_tile_list_release(&owner);
    CHECK(mc_source_dk_tile_lists_entered(&dk, NULL, 0, &call, &error));
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    mc_tile_entity *two_new[] = {spawner, second};
    const uint32_t spawner_bin = (spawner->identity.hash ^ (spawner->identity.hash >> 16u)) & 15u;
    const uint32_t second_bin = (second->identity.hash ^ (second->identity.hash >> 16u)) & 15u;
    CHECK(mc_source_dk_tile_lists_entered(&dk, two_new, 2, &call, &error) && shared.count == 2 &&
          tiles.count == 2 && mc_source_dk_compile_inspect(call, &state) && state.returned &&
          state.source_pc == 626 && state.new_tile_set_count == 2 &&
          state.shared_tiles_added == 2 &&
          shared.entries[0] == (spawner_bin <= second_bin ? spawner : second) &&
          shared.entries[1] == (spawner_bin <= second_bin ? second : spawner));
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_source_dk_tile_lists_entered(&dk, NULL, 0, &call, &error) && shared.count == 0 &&
          mc_source_dk_compile_inspect(call, &state) && state.shared_tiles_removed == 2);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);

    /* Authored native invalid-cell input reaches contains608 only in the
     * other DK's F occurrence. The first completed removal is committed;
     * the throwing element and untouched suffix remain held and indexed. */
    CHECK(mc_source_dk_tile_lists_entered(&dk, one_spawner, 1, &call, &error));
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_source_dk_tile_lists_entered(&other_dk, one_second, 1, &call, &error) &&
          shared.count == 2 && shared.entries[0] == spawner && shared.entries[1] == second);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    const uint32_t saved_second_hash = second->identity.hash;
    const uint32_t before_tail_mod_count = shared.mod_count;
    second->identity.hash = UINT32_C(0x80000000);
    const bool refused_tail = mc_source_dk_tile_lists_entered(&dk, NULL, 0, &call, &error);
    second->identity.hash = saved_second_hash;
    CHECK(!refused_tail && error.source_pc == 608 && error.native_policy &&
          error.tile_identity_set != NULL &&
          error.tile_identity_set->tile_status == MC_TILE_ENTITY_STATE_ERROR && shared.count == 1 &&
          shared.entries[0] == second && shared.entries[1] == NULL &&
          shared.mod_count == before_tail_mod_count + 2u &&
          mc_source_dk_compile_inspect(call, &state) && state.shared_tiles_removed == 1 &&
          !state.returned);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_render_global_tile_list_get(&owner, &shared, 0, &captured, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          captured == second);
    mc_tile_entity_release(captured);
    captured = NULL;
    CHECK(mc_source_dk_tile_lists_entered(&other_dk, NULL, 0, &call, &error) && shared.count == 0);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_source_dk_tile_lists_entered(&dk, one_spawner, 1, &call, &error));
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);

    mc_tile_entity_snapshot held;
    CHECK(mc_tile_entity_get_snapshot(dk.tile_holds->tile, &held) == MC_TILE_ENTITY_OK &&
          held.kind == MC_TILE_ENTITY_SPAWNER);
    CHECK(mc_source_dk_native_retire(&dk, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    for (mc_source_dk_tile_hold *h = dk.tile_holds; h != NULL; h = h->next)
        CHECK(h->tile == NULL);
    CHECK(mc_source_dk_native_retire(&other_dk, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    /* Shared N row independently survives retirement of its originating DK. */
    CHECK(mc_render_global_tile_list_get(&owner, &shared, 0, &captured, &error) ==
              MC_CLIENT_RENDER_GLOBAL_OK &&
          captured == spawner);
    mc_tile_entity_release(captured);
    captured = NULL;
    mc_render_global_tile_list_release(&owner);
    for (mc_render_global_tile_occurrence *h = owner.tile_occurrences; h != NULL; h = h->next)
        CHECK(!h->active && h->tile == NULL);
    CHECK(mc_client_tile_renderer_consumer_detach(&owner.tile_consumer, &tile_error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    mc_tile_entity_release(chest);
    chest = NULL;
    mc_tile_entity_release(spawner);
    spawner = NULL;
    mc_tile_entity_release(second);
    second = NULL;
    CHECK(mc_client_tile_renderer_classes_retire(&classes, &tile_error) ==
              MC_CLIENT_TILE_RENDERER_OK &&
          classes == NULL);
    CHECK(mc_client_render_classes_free(models) == MC_CLIENT_MODEL_OK);
    models = NULL;
    CHECK(mc_world_runtime_classes_free(worlds, NULL) == MC_WORLD_STORAGE_OK);
    worlds = NULL;
    CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
    registry = NULL;
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_OK);
    entities = NULL;
    printf("DK Tile eligibility controls passed: %u\n", checks);
    result = 0;
cleanup:
    if (call != NULL)
        (void)mc_source_dk_compile_retire(&call, &cleanup_error);
    mc_source_dk_tile_occurrences_release(&dk, NULL);
    mc_source_dk_tile_occurrences_release(&other_dk, NULL);
    mc_render_global_tile_list_release(&owner);
    mc_client_render_arena_free(&owner.arena);
    mc_tile_entity_release(chest);
    mc_tile_entity_release(spawner);
    mc_tile_entity_release(second);
    mc_tile_entity_release(captured);
    if (owner.tile_consumer.attached)
        (void)mc_client_tile_renderer_consumer_detach(&owner.tile_consumer, &tile_error);
    if (classes != NULL)
        (void)mc_client_tile_renderer_classes_retire(&classes, &tile_error);
    if (models != NULL)
        (void)mc_client_render_classes_free(models);
    if (worlds != NULL)
        (void)mc_world_runtime_classes_free(worlds, NULL);
    if (registry != NULL)
        (void)mc_registry_classes_free(registry, NULL);
    if (entities != NULL)
        (void)mc_entity_classes_free(entities);
    mc_materials_destroy(materials);
    mc_java_random_profile_free(profile);
    return result;
}
