/* Real App/statistics/World/Player owners, without entering its test main. */
#define main mc_mining_tools_unused_statistics_main
#include "../statistics/test_player_dispatch.c"
#undef main

#include "../../../src/world/runtime/runtime_internal.h"
#include "../../../src/world/storage/generated_internal.h"
#include "mc/client/controller/mining.h"
#include "mc/item/mining/destroy.h"

typedef struct mining_profile {
    int32_t id, count, damage, after_count, after_damage, broken;
    bool clear, harvest;
} mining_profile;

static bool exercise(fixture *f, int profile) {
    static const mining_profile profiles[] = {
        {270, 1, 0, 1, 1, 0, false, true},   {270, 1, 59, 0, 0, 1, true, true},
        {270, 2, 59, 1, 0, 1, false, true},  {270, 0, 0, 0, 1, 0, true, true},
        {270, -3, 0, -3, 1, 0, false, true}, {268, 1, 59, 0, 0, 1, true, false}};
    CHECK(profile >= 0 && (size_t)profile < sizeof(profiles) / sizeof(*profiles));
    const mining_profile *p = &profiles[profile];
    uint8_t *blocks = calloc(MC_CHUNK_BLOCKS, 1);
    CHECK(blocks != NULL);
    blocks[((size_t)3 * 16u + 5u) * 128u + 64u] = 1;
    mc_world_chunk *chunk = NULL;
    CHECK(mc_world_store_create_generated(f->store, f->world, 0, 0, 0, &blocks, MC_CHUNK_BLOCKS,
                                          &chunk, NULL) == MC_WORLD_STORAGE_OK &&
          blocks == NULL);
    mc_chunk_state *state = mc_world_chunk_state(chunk);
    const mc_chunk_acquire_context *acquisition = mc_world_runtime_acquisition(f->world);
    CHECK(state != NULL && acquisition != NULL &&
          mc_chunk_cache_publish(acquisition->cache, 0, 0, state));
    f->app.h = f->player;
    f->app.active_world = f->world;
    CHECK(mc_entity_set_position(f->player, 0.5, 64.0, 0.5) == MC_ENTITY_OK);
    f->player->base.aX = true;
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, p->id, p->count, p->damage) == MC_ITEM_STACK_OK &&
          mc_inventory_player_set(f->player->local_player->fields.player.c, 0, stack, NULL) ==
              MC_INVENTORY_OK);
    const mc_statistic *used = f->app.registry->statistics.E[p->id];
    const mc_statistic *broken = f->app.registry->statistics.F[p->id];
    const mc_statistic *mined = f->app.registry->statistics.C[1];
    int32_t used_before, broken_before, mined_before;
    CHECK(mc_client_stats_writer_value(f->app.stats_writer, used, &used_before, NULL) ==
              MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(f->app.stats_writer, broken, &broken_before, NULL) ==
              MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(f->app.stats_writer, mined, &mined_before, NULL) ==
              MC_CLIENT_STATS_OK);
    mc_client_singleplayer_controller *controller = NULL;
    CHECK(mc_client_singleplayer_controller_allocate(f->app.entities, f->app.profile, 0,
                                                     &controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK &&
          mc_client_singleplayer_controller_construct(controller, &f->app, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_singleplayer_mining_call *call = NULL;
    mc_singleplayer_mining_error error;
    mc_singleplayer_mining_state result;
    const mc_singleplayer_mining_status status = mc_client_singleplayer_mining_begin(
        controller, MC_SINGLEPLAYER_MINING_DESTROY, 3, 64, 5, 1, &call, &error);
    if (status != MC_SINGLEPLAYER_MINING_OK)
        fprintf(stderr, "held-tool destruction: %s at %s.%s:%u\n", error.operation,
                error.site.class_name, error.site.method, error.site.pc);
    CHECK(status == MC_SINGLEPLAYER_MINING_OK &&
          mc_client_singleplayer_mining_inspect(call, &result) && result.returned &&
          result.removed && result.tool_returned && result.item_used_returned &&
          result.harvest_allowed == p->harvest && result.exhausted_stack == p->clear &&
          result.clear_returned == p->clear && result.destroy_callback_returned == p->clear &&
          result.statistic_returned == p->harvest && result.drop_returned == p->harvest);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.id == p->id &&
          fields.count == p->after_count && fields.damage == p->after_damage);
    const mc_item_stack *selected = NULL;
    CHECK(mc_inventory_player_borrow(f->player->local_player->fields.player.c, 0, &selected,
                                     NULL) == MC_INVENTORY_OK &&
          selected == (p->clear ? NULL : stack));
    int32_t block_after, used_after, broken_after, mined_after;
    CHECK(mc_world_runtime_block_get(f->world, 3, 64, 5, &block_after, NULL) ==
              MC_WORLD_STORAGE_OK &&
          block_after == 0 &&
          mc_client_stats_writer_value(f->app.stats_writer, used, &used_after, NULL) ==
              MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(f->app.stats_writer, broken, &broken_after, NULL) ==
              MC_CLIENT_STATS_OK &&
          mc_client_stats_writer_value(f->app.stats_writer, mined, &mined_after, NULL) ==
              MC_CLIENT_STATS_OK &&
          used_after == used_before + 1 && broken_after == broken_before + p->broken &&
          mined_after == mined_before + (p->harvest ? 1 : 0));
    const size_t items = f->world->entity_membership.count;
    CHECK(items == (p->harvest ? 1u : 0u));
    if (items != 0) {
        mc_entity *drop = f->world->entity_membership.entries[0];
        mc_item_stack_fields payload;
        CHECK(mc_entity_is_kind(drop, MC_ENTITY_CLASS_KIND_ITEM) &&
              mc_item_stack_fields_get(drop->stack, &payload) == MC_ITEM_STACK_OK &&
              payload.id == 4 && payload.count == 1 && payload.damage == 0);
    }
    printf("CONTROLLER_DESTROY\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%zu\n", profile, p->id,
           p->count, p->damage, fields.count, fields.damage, selected == NULL ? 1 : 0,
           used_after - used_before, broken_after - broken_before, mined_after - mined_before,
           items);
    CHECK(mc_client_singleplayer_mining_retire(&call, NULL) == MC_SINGLEPLAYER_MINING_OK &&
          mc_client_singleplayer_controller_free(&controller, NULL) ==
              MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_item_stack_release(stack);
    f->app.h = NULL;
    f->app.active_world = NULL;
    mc_entity_release(f->player);
    f->player = NULL;
    /* This fixture explicitly published its authored Chunk into the exported
     * native cache. End genuine Entity memberships first, then withdraw that
     * fixture-owned publication and retire the same Chunk through the store.
     * Ordinary World retirement correctly refuses a nonempty cache. */
    CHECK(mc_world_runtime_retire_entity_containers(f->world, NULL) == MC_WORLD_STORAGE_OK &&
          acquisition->cache->count == 1 && acquisition->cache->insertion_count == 1 &&
          mc_chunk_cache_init(acquisition->cache, acquisition->cache->entries,
                              acquisition->cache->capacity, acquisition->cache->insertions,
                              acquisition->cache->insertion_capacity) &&
          mc_world_store_retire_chunk(f->store, state, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_store_chunk_count(f->store) == 0);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 4 || CD(argv[2]) != 0 || !mc_math_trig_init(&trig))
        return 2;
    char *end = NULL;
    const long profile = strtol(argv[3], &end, 10);
    if (*end != '\0' || profile < 0 || profile > 5)
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    if (f == NULL || !setup(f, argv[1], "mining-tools", true) || !exercise(f, (int)profile) ||
        !teardown(f))
        return 1;
    free(f);
    printf("PASS: %zu held-tool controller destruction checks\n", checks);
    return 0;
}
