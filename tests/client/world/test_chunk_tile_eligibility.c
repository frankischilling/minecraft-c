#include "client/application_state.h"
#include "client/render/tile_entity_renderer/internal.h"
#include "client/render_engine/internal.h"
#include "client/world/source_chunk_compile_internal.h"
#include "mc/block/material/material.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/entities.h"

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
    mc_tile_entity *chest = NULL, *spawner = NULL;
    mc_source_dk_compile *call = NULL;
    mc_client_render_global_error error, cleanup_error;
    mc_client_tile_renderer_error tile_error;
    client_application app = {0};
    mc_client_render_global owner = {0};
    mc_client_render_list tiles = {0};
    mc_client_source_dk dk = {0};
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
          dk.tile_holds == NULL);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_source_dk_tile_eligibility_entered(&dk, spawner, &call, &error));
    CHECK(mc_source_dk_compile_inspect(call, &state) && state.tile_eligibility_returned &&
          state.tile_eligible && tiles.count == 1 && tiles.entries[0] == spawner &&
          dk.tile_holds != NULL && dk.tile_holds->tile == spawner);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    CHECK(mc_source_dk_tile_eligibility_entered(&dk, spawner, &call, &error) && tiles.count == 2 &&
          tiles.entries[1] == spawner);
    CHECK(mc_source_dk_compile_retire(&call, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    mc_tile_entity_release(spawner);
    spawner = NULL;
    mc_tile_entity_snapshot held;
    CHECK(mc_tile_entity_get_snapshot(dk.tile_holds->tile, &held) == MC_TILE_ENTITY_OK &&
          held.kind == MC_TILE_ENTITY_SPAWNER);
    CHECK(mc_source_dk_native_retire(&dk, &cleanup_error) == MC_CLIENT_RENDER_GLOBAL_OK);
    for (mc_source_dk_tile_hold *h = dk.tile_holds; h != NULL; h = h->next)
        CHECK(h->tile == NULL);
    CHECK(mc_client_tile_renderer_consumer_detach(&owner.tile_consumer, &tile_error) ==
          MC_CLIENT_TILE_RENDERER_OK);
    mc_tile_entity_release(chest);
    chest = NULL;
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
    mc_client_render_arena_free(&owner.arena);
    mc_tile_entity_release(chest);
    mc_tile_entity_release(spawner);
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
