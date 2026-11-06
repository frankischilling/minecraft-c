/* Reuse genuine Registry/World/Player construction and checked retirement. */
#define main mc_mining_construction_fixture_main
#include "../../entity/player/test_construction.c"
#undef main

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/client/application_state.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/storage/generated_internal.h"
#include "mc/client/controller/mining.h"
#include "mc/entity/player/statistics.h"
#include "mc/inventory/mining.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/placement.h"

static bool lever_initial_hook_controls(fixture *f, mc_entity *player) {
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL && acquisition->cache->count == 0);
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL);
    blocks[((size_t)3 * 16u + 5u) * 128u + 64u] = 69;
    blocks[((size_t)3 * 16u + 5u) * 128u + 63u] = 1;
    mc_world_chunk *chunk = NULL;
    CHECK(mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    CHECK(state != NULL && mc_chunk_cache_publish(acquisition->cache, 0, 0, state));
    client_application *application = calloc(1, sizeof(*application));
    CHECK(application != NULL);
    application->active_world = f->world;
    application->h = player;
    application->registry = f->registry;
    mc_client_singleplayer_controller *controller = NULL;
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(controller, application, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_singleplayer_mining_call *call = NULL;
    mc_singleplayer_mining_error error;
    mc_singleplayer_mining_state result;
    int32_t metadata = -1;
    const size_t holds = player->holds;
    for (int32_t i = 0; i < 2; ++i) {
        CHECK(mc_client_singleplayer_mining_begin(controller, MC_SINGLEPLAYER_MINING_START, 3, 64,
                                                  5, 1, &call,
                                                  &error) == MC_SINGLEPLAYER_MINING_OK);
        CHECK(mc_client_singleplayer_mining_inspect(call, &result) && result.returned &&
              result.initial_hook_returned && result.block_id == 69 && result.source_pc == 113 &&
              !result.removal_returned && !result.progress_stored && error.activation != NULL &&
              error.activation->returned && error.activation->value &&
              error.activation->site.pc == 219 &&
              strcmp(error.initial_hook.class_name, "xr") == 0 &&
              strcmp(error.initial_hook.method, "b") == 0 && error.initial_hook.pc == 8 &&
              mc_world_runtime_metadata_get(f->world, 3, 64, 5, &metadata, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              metadata == (i == 0 ? 8 : 0) && player->holds == holds);
        CHECK(mc_client_singleplayer_controller_free(&controller, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_BUSY);
        CHECK(mc_client_singleplayer_mining_retire(&call, NULL) == MC_SINGLEPLAYER_MINING_OK &&
              call == NULL);
    }
    /* Missing actual WorldAccess fails after the metadata store. The real
     * controller receipt retains the XR child and its mutation prefix until
     * checked retirement; no direct wrapper result stands in for this call. */
    mc_world_reference_list *const accesses = f->world->fields.u;
    f->world->fields.u = NULL;
    const mc_singleplayer_mining_status failed_status = mc_client_singleplayer_mining_begin(
        controller, MC_SINGLEPLAYER_MINING_START, 3, 64, 5, 1, &call, &error);
    f->world->fields.u = accesses;
    CHECK(failed_status == MC_SINGLEPLAYER_MINING_WORLD && error.site.pc == 74 &&
          strcmp(error.site.class_name, "os") == 0 && strcmp(error.site.method, "a") == 0 &&
          error.initial_hook.pc == 8 && error.activation != NULL && !error.activation->returned &&
          error.activation->mutation != NULL &&
          error.activation->mutation->world_access_bytecode == 9 &&
          mc_client_singleplayer_mining_inspect(call, &result) && !result.returned &&
          !result.initial_hook_returned &&
          mc_world_runtime_metadata_get(f->world, 3, 64, 5, &metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          metadata == 8 && player->holds == holds);
    CHECK(mc_client_singleplayer_mining_retire(&call, NULL) == MC_SINGLEPLAYER_MINING_OK &&
          call == NULL);
    CHECK(mc_client_singleplayer_controller_free(&controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    free(application);
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool held_pickaxe_start(fixture *f, mc_entity *player) {
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL && acquisition->cache->count == 0);
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL);
    blocks[((size_t)3 * 16u + 5u) * 128u + 64u] = 1;
    mc_world_chunk *chunk = NULL;
    CHECK(mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    CHECK(state != NULL && mc_chunk_cache_publish(acquisition->cache, 0, 0, state));
    CHECK(mc_entity_set_position(player, 0.5, 64.0, 0.5) == MC_ENTITY_OK);
    player->base.aX = true;
    mc_item_stack *pickaxe = NULL;
    CHECK(mc_item_stack_allocate(&pickaxe) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(pickaxe, 270, 1, 0) == MC_ITEM_STACK_OK &&
          mc_inventory_player_set(player->local_player->fields.player.c, 0, pickaxe, NULL) ==
              MC_INVENTORY_OK);
    client_application *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    app->active_world = f->world;
    app->h = player;
    app->registry = f->registry;
    mc_client_singleplayer_controller *controller = NULL;
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(controller, app, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_singleplayer_mining_call *call = NULL;
    mc_singleplayer_mining_error error;
    mc_singleplayer_mining_state result;
    const size_t holds = player->holds;
    CHECK(mc_client_singleplayer_mining_begin(controller, MC_SINGLEPLAYER_MINING_START, 3, 64, 5, 1,
                                              &call, &error) == MC_SINGLEPLAYER_MINING_OK &&
          mc_client_singleplayer_mining_inspect(call, &result) && result.returned &&
          result.block_id == 1 && result.strength == 2.0f / 1.5f / 30.0f &&
          !result.removal_returned && !result.progress_stored && player->holds == holds);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(pickaxe, &fields) == MC_ITEM_STACK_OK && fields.id == 270 &&
          fields.count == 1 && fields.damage == 0 &&
          mc_client_singleplayer_mining_retire(&call, NULL) == MC_SINGLEPLAYER_MINING_OK &&
          mc_client_singleplayer_controller_free(&controller, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_inventory_player_set(player->local_player->fields.player.c, 0, NULL, NULL) ==
              MC_INVENTORY_OK);
    mc_item_stack_release(pickaxe);
    free(app);
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK);
    return true;
}

static bool inherited_harvest_controls(fixture *f, mc_entity *player) {
    client_application *app = calloc(1, sizeof(*app));
    CHECK(app != NULL);
    /* Component input uses the actual typed App fields read by OS. The real
     * Player was constructed with nullable Minecraft, so its later statistic
     * invocation supplies an observable source-null frontier after removal. */
    app->active_world = f->world;
    app->h = player;
    app->registry = f->registry;
    mc_client_singleplayer_controller *controller = NULL;
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(controller, app, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL && acquisition->cache->count == 0);
    /* Both owners use actual generated-Chunk constructors and authored block
     * bytes. Native cache publication is fixture input, not a replacement
     * World getter, mining body, auxiliary receiver or source action result. */
    const int32_t ids[] = {20, 17}; /* FK Glass and VG Log's UU superclass wrapper. */
    for (size_t i = 0; i < sizeof(ids) / sizeof(*ids); ++i) {
        uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
        CHECK(blocks != NULL);
        blocks[((size_t)3 * 16u + 5u) * 128u + 64u] = (uint8_t)ids[i];
        mc_world_chunk *chunk = NULL;
        CHECK(mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                              &chunk, NULL) == MC_WORLD_STORAGE_OK &&
              blocks == NULL && chunk != NULL);
        mc_chunk_state *state = mc_world_chunk_state(chunk);
        CHECK(state != NULL && mc_chunk_cache_publish(acquisition->cache, 0, 0, state));
        mc_singleplayer_mining_call *call = NULL;
        mc_singleplayer_mining_error error;
        mc_singleplayer_mining_state result;
        CHECK(mc_client_singleplayer_mining_begin(controller, MC_SINGLEPLAYER_MINING_DESTROY, 3, 64,
                                                  5, 1, &call,
                                                  &error) == MC_SINGLEPLAYER_MINING_PLAYER);
        CHECK(mc_client_singleplayer_mining_inspect(call, &result) && result.auxiliary_returned &&
              result.removal_returned && result.removed && result.harvest_allowed &&
              !result.statistic_returned && !result.drop_returned && !result.returned &&
              result.block_id == ids[i] && error.site.class_name != NULL &&
              strcmp(error.site.class_name, "uu") == 0 && error.site.pc == 10 &&
              error.statistic != NULL &&
              error.statistic->status == MC_PLAYER_STATISTIC_NULL_REFERENCE &&
              error.statistic->site.pc == 85);
        int32_t live = -1;
        CHECK(mc_world_runtime_block_get(f->world, 3, 64, 5, &live, NULL) == MC_WORLD_STORAGE_OK &&
              live == 0);
        CHECK(mc_client_singleplayer_mining_retire(&call, NULL) == MC_SINGLEPLAYER_MINING_OK &&
              call == NULL);
        CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                                  acquisition->cache->capacity, acquisition->cache->insertions,
                                  acquisition->cache->insertion_capacity));
        CHECK(mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_store_chunk_count(f->store) == 0);
    }
    CHECK(mc_client_singleplayer_controller_free(&controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    free(app);
    return true;
}

static bool mining_controls(fixture *f) {
    const mc_block *dirt = NULL, *bedrock = NULL;
    CHECK(mc_registry_block_get(f->registry, 3, &dirt, NULL) == MC_REGISTRY_OK && dirt != NULL);
    CHECK(mc_registry_block_get(f->registry, 7, &bedrock, NULL) == MC_REGISTRY_OK &&
          bedrock != NULL);
    float value = -7.0f;
    mc_player_mining_error player_error;
    CHECK(mc_block_player_strength(bedrock, NULL, &value, &player_error) == MC_ENTITY_OK &&
          value == 0.0f && player_error.returned && player_error.site.pc == 10);
    /* Actual UU.fcmpg5/ifge6 must reach the null Player invocation for both
     * quiet-NaN signs and either signed zero; it must not invent a zero return.
     * Use the real source hardness setter on the genuine Dirt singleton. */
    const float ordinary_hardness = dirt->fields.bo;
    const float ordinary_resistance = dirt->fields.bp;
    const uint32_t hardness_bits[] = {UINT32_C(0x7fc12345), UINT32_C(0xffc54321), UINT32_C(0),
                                      UINT32_C(0x80000000)};
    for (size_t i = 0; i < sizeof(hardness_bits) / sizeof(*hardness_bits); ++i) {
        float hardness;
        memcpy(&hardness, &hardness_bits[i], sizeof(hardness));
        mc_registry_block_hardness((mc_block *)dirt, hardness);
        value = -7.0f;
        CHECK(mc_block_player_strength(dirt, NULL, &value, &player_error) ==
                  MC_ENTITY_STATE_ERROR &&
              value == -7.0f && !player_error.returned && !player_error.native_policy &&
              player_error.site.pc == 13 && dirt->fields.bp == ordinary_resistance);
    }
    mc_registry_block_hardness((mc_block *)dirt, ordinary_hardness);
    CHECK(dirt->fields.bo == ordinary_hardness && dirt->fields.bp == ordinary_resistance);

    mc_player_session *session = NULL;
    mc_entity *player = NULL;
    CHECK(mc_player_session_create(f->sessions, NULL, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entities, &player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(player, f->world, NULL, session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    mc_player_session_retire(&session);
    mc_local_player_snapshot snapshot;
    CHECK(mc_local_player_get_snapshot(player, &snapshot) == MC_ENTITY_OK);
    const size_t holds = player->holds;
    bool allowed = false;
    CHECK(mc_local_player_can_harvest(player, dirt, &allowed, &player_error) == MC_ENTITY_OK &&
          allowed && player_error.harvest && player->holds == holds);
    mc_inventory_error inventory_error;
    CHECK(mc_inventory_player_mining_speed(snapshot.player.c, f->registry, dirt, &value,
                                           &inventory_error) == MC_INVENTORY_OK &&
          value == 1.0f);
    CHECK(mc_local_player_swing(player, &player_error) == MC_ENTITY_OK && player_error.returned &&
          player->local_player->fields.player.k == -1 && player->local_player->fields.player.j &&
          player->holds == holds);

    /* NULL App is a valid original controller constructor argument. Its first
     * progress call takes the coordinate-store branch without reading App;
     * repeating that coordinate reaches the real null read instead. */
    mc_client_singleplayer_controller *controller = NULL;
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_singleplayer_controller_construct(controller, NULL, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_singleplayer_mining_call *call = NULL;
    mc_singleplayer_mining_error error;
    mc_singleplayer_mining_state state;
    CHECK(mc_client_singleplayer_mining_begin(controller, MC_SINGLEPLAYER_MINING_PROGRESS, 12, 63,
                                              -4, 1, &call, &error) == MC_SINGLEPLAYER_MINING_OK);
    CHECK(mc_client_singleplayer_mining_inspect(call, &state) && state.returned &&
          state.changed_coordinate && !state.progress_stored && state.source_pc == 248);
    mc_singleplayer_controller_snapshot fields;
    CHECK(mc_client_singleplayer_controller_read(controller, &fields) && fields.c == 12 &&
          fields.d == 63 && fields.e == -4 && fields.f == 0.0f && fields.g == 0.0f &&
          fields.h == 0.0f && fields.native_holds == 2 && !fields.entered);
    CHECK(mc_client_singleplayer_controller_free(&controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_BUSY);
    CHECK(mc_client_singleplayer_mining_retire(&call, &error) == MC_SINGLEPLAYER_MINING_OK &&
          call == NULL);
    CHECK(mc_client_singleplayer_mining_begin(controller, MC_SINGLEPLAYER_MINING_PROGRESS, 12, 63,
                                              -4, 1, &call,
                                              &error) == MC_SINGLEPLAYER_MINING_SOURCE_NULL);
    CHECK(error.site.pc == 46 && !error.native_policy &&
          mc_client_singleplayer_mining_inspect(call, &state) && !state.returned &&
          !state.progress_stored && !state.changed_coordinate);
    CHECK(mc_client_singleplayer_mining_retire(&call, NULL) == MC_SINGLEPLAYER_MINING_OK &&
          call == NULL);
    CHECK(mc_client_singleplayer_controller_free(&controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(held_pickaxe_start(f, player));
    CHECK(inherited_harvest_controls(f, player));
    CHECK(lever_initial_hook_controls(f, player));
    mc_entity_release(player);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("usage: mining-controls RESOURCES EMPTY_WORLD_DIRECTORY\n", stderr);
        return 2;
    }
    mc_math_trig_init(&trig);
    fixture f = {0};
    if (!setup(&f, argv[1], argv[2]) || !mining_controls(&f) || !teardown(&f))
        return 1;
    printf("PASS: %u mining prefix/leaf/ownership checks\n", checks);
    return 0;
}
