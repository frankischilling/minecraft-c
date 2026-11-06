/* Actual World/Registry/Player construction; declared source method inputs.
 * No display, forged phase, entered flag, callback result or replacement Item. */
#define main mc_use_construction_fixture_main
#include "../../entity/player/test_construction.c"
#undef main

#include "block/registry/registry_internal.h"
#include "client/world/entity_renderer/internal.h"
#include "item/stack/source_internal.h"
#include "mc/client/controller/use.h"
#include "mc/client/world/held_item_tick.h"
#include "world/storage/generated_internal.h"

#include <xmmintrin.h>

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof result);
    return result;
}

static bool reset_controls(fixture *f, mc_item_stack *stack) {
    mc_client_render_classes *models = NULL;
    mc_client_render_engine_classes *graphics = NULL;
    CHECK(mc_client_render_classes_create(f->entities, f->profile, &models) == MC_CLIENT_MODEL_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    const mc_client_entity_renderer_services services = {models, graphics, f->profile, NULL};
    mc_client_held_item *owner = NULL;
    CHECK(mc_client_held_item_allocate(&services, NULL, &owner, NULL) ==
          MC_CLIENT_ENTITY_RENDERER_OK);
    /* Genuine RAW RA allocation, with declared source field inputs for these
     * literal method bodies. No completed constructor or GL claim is made. */
    CHECK(mc_item_stack_retain(stack) == MC_ITEM_STACK_OK);
    owner->source->b = owner->source->b_occurrence = stack;
    owner->source->d = -.25f;
    owner->source->g = 7;
    mc_client_entity_renderer_error error;
    for (unsigned int action = 0; action < 2; ++action) {
        const uint32_t input = action == 0 ? UINT32_C(0xffc12345) : UINT32_C(0x80000000);
        memcpy(&owner->source->c, &input, sizeof input);
        const unsigned int controls = _mm_getcsr();
        _mm_setcsr(controls | 0x8040u);
        const mc_client_entity_renderer_status status =
            mc_client_held_item_reset_equip(owner, (mc_client_held_item_equip_reset)action, &error);
        const unsigned int after = _mm_getcsr();
        _mm_setcsr(controls);
        CHECK(status == MC_CLIENT_ENTITY_RENDERER_OK && error.site.pc == 5 && error.ra_pc == 5 &&
              strcmp(error.site.class_name, "ra") == 0 &&
              strcmp(error.site.method, action == 0 ? "b" : "c") == 0 &&
              strcmp(error.site.descriptor, "()V") == 0 &&
              (after & 0xffc0u) == ((controls | 0x8040u) & 0xffc0u));
        mc_client_held_item_state state;
        CHECK(mc_client_held_item_inspect(owner, &state) && bits(state.ra_c) == 0 &&
              bits(state.ra_d) == bits(-.25f) && state.ra_b == stack && state.ra_g == 7 &&
              state.ra_last_store_pc == 2 && state.ra_phase == MC_CLIENT_ENTITY_RENDERER_RAW);
    }
    mc_client_held_item_tick_call *tick = NULL;
    mc_client_held_item_tick_error tick_error;
    owner->source->c = .75f; /* Declared input to the next actual RA.a prefix. */
    CHECK(mc_client_held_item_tick_begin(owner, &tick, &tick_error) ==
          MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL);
    /* The actual tick returned its null-App failure after d=c. Its retained
     * receipt is immutable while the later literal reset changes only c. */
    mc_client_held_item_tick_state tick_state;
    CHECK(mc_client_held_item_tick_inspect(tick, &tick_state) && tick_state.d_stored &&
          tick_state.status == MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL);
    const uint32_t before_d = bits(owner->source->d);
    CHECK(mc_client_held_item_reset_equip(owner, MC_CLIENT_HELD_ITEM_RESET_BLOCK_USE, &error) ==
              MC_CLIENT_ENTITY_RENDERER_OK &&
          bits(owner->source->c) == 0 && bits(owner->source->d) == before_d &&
          before_d == bits(.75f) && owner->source->b == stack);
    CHECK(mc_client_held_item_tick_inspect(tick, &tick_state) && tick_state.d_stored &&
          tick_state.status == MC_CLIENT_HELD_ITEM_TICK_SOURCE_NULL);
    CHECK(mc_client_held_item_tick_retire(&tick, &tick_error) == MC_CLIENT_HELD_ITEM_TICK_OK &&
          tick == NULL);
    CHECK(mc_client_held_item_reset_equip(NULL, MC_CLIENT_HELD_ITEM_RESET_AIR_USE, &error) ==
              MC_CLIENT_ENTITY_RENDERER_NULL_REFERENCE &&
          !error.native_policy && error.site.pc == 2 && strcmp(error.site.method, "c") == 0);
    CHECK(mc_client_held_item_retire(&owner, &error) == MC_CLIENT_ENTITY_RENDERER_OK &&
          owner == NULL);
    CHECK(mc_client_render_engine_classes_release(graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_render_classes_free(models) == MC_CLIENT_MODEL_OK);
    return true;
}

static bool controller_controls(fixture *f) {
    mc_client_singleplayer_controller *controller = NULL;
    CHECK(mc_client_singleplayer_controller_allocate(f->entities, f->profile, 0, &controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(controller, NULL, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_singleplayer_use_call *call = NULL;
    mc_singleplayer_use_error error;
    mc_singleplayer_use_state state;
    CHECK(mc_client_singleplayer_use_on_block_begin(controller, NULL, NULL, NULL, 3, 64, 5, 1,
                                                    &call,
                                                    &error) == MC_SINGLEPLAYER_USE_SOURCE_NULL);
    CHECK(mc_client_singleplayer_use_inspect(call, &state) && !state.returned && state.on_block &&
          state.source_pc == 7 && !state.block_read && !state.item_entered &&
          error.source_exception && !error.native_policy);
    CHECK(mc_client_singleplayer_controller_free(&controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_BUSY);
    CHECK(mc_client_singleplayer_use_retire(&call, &error) == MC_SINGLEPLAYER_USE_OK &&
          call == NULL);
    CHECK(mc_client_singleplayer_use_in_air_begin(controller, NULL, NULL, NULL, &call, &error) ==
          MC_SINGLEPLAYER_USE_SOURCE_NULL);
    CHECK(mc_client_singleplayer_use_inspect(call, &state) && state.source_pc == 1 &&
          !state.count_read && !state.item_entered && !state.returned);
    CHECK(mc_client_singleplayer_use_retire(&call, NULL) == MC_SINGLEPLAYER_USE_OK);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(acquisition != NULL && acquisition->cache->count == 0);
    /* The actual out-of-height FD return does not acquire a Chunk, initialize
     * another namespace or read nullable Player/Stack arguments. */
    CHECK(mc_client_singleplayer_use_on_block_begin(controller, NULL, f->world, NULL, 3, 200, 5, 1,
                                                    &call, &error) == MC_SINGLEPLAYER_USE_OK);
    CHECK(mc_client_singleplayer_use_inspect(call, &state) && state.returned && !state.result &&
          state.source_pc == 44 && state.block_read && state.clicked_id == 0 &&
          !state.activation_entered && !state.item_entered && acquisition->cache->count == 0);
    CHECK(mc_client_singleplayer_use_retire(&call, NULL) == MC_SINGLEPLAYER_USE_OK);
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 3, 2, 7) == MC_ITEM_STACK_OK);
    CHECK(mc_client_singleplayer_use_in_air_begin(controller, NULL, f->world, stack, &call,
                                                  &error) == MC_SINGLEPLAYER_USE_OK);
    CHECK(mc_client_singleplayer_use_inspect(call, &state) && state.returned && !state.result &&
          !state.on_block && state.source_pc == 79 && state.item_returned && state.same_stack &&
          state.count_read && state.count_compared && state.count_before == 2 &&
          state.count_after == 2 && state.result_stack == stack && !state.selected_slot_stored &&
          !state.zero_slot_cleared && acquisition->cache->count == 0);
    CHECK(mc_client_singleplayer_use_retire(&call, NULL) == MC_SINGLEPLAYER_USE_OK);
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL);
    blocks[((size_t)3 * 16u + 5u) * 128u + 64u] = 3;
    mc_world_chunk *chunk = NULL;
    CHECK(mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *chunk_state = mc_world_chunk_state(chunk);
    CHECK(chunk_state != NULL && mc_chunk_cache_publish(acquisition->cache, 0, 0, chunk_state));
    CHECK(mc_client_singleplayer_use_on_block_begin(controller, NULL, f->world, NULL, 3, 64, 5, 1,
                                                    &call, &error) == MC_SINGLEPLAYER_USE_OK);
    CHECK(mc_client_singleplayer_use_inspect(call, &state) && state.returned && !state.result &&
          state.clicked_id == 3 && state.activation_entered && state.activation_returned &&
          !state.activated && !state.item_entered && state.source_pc == 44);
    CHECK(mc_client_singleplayer_use_retire(&call, NULL) == MC_SINGLEPLAYER_USE_OK);
    CHECK(mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity));
    CHECK(mc_world_store_retire_chunk(f->store, chunk_state, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(reset_controls(f, stack));
    mc_item_stack_release(stack);
    CHECK(mc_client_singleplayer_controller_free(&controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("usage: use-controls RESOURCES EMPTY_WORLD_DIRECTORY\n", stderr);
        return 2;
    }
    mc_math_trig_init(&trig);
    fixture f = {0};
    if (!setup(&f, argv[1], argv[2]) || !controller_controls(&f) || !teardown(&f))
        return 1;
    printf("PASS: %u controller-use/RA-reset checks\n", checks);
    return 0;
}
