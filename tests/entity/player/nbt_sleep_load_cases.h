/* Authored detailed fd.a(gs) caller controls. Each case owns a fresh genuine
 * World and unpublished Player. No graphical Load/Respawn call is claimed. */
#include "mc/world/runtime/loading/player.h"

static bool nbt_sleep_load_case(fixture *f, client_application *app, unsigned int scenario) {
    bool ok = false, player_world_changed = false;
    nbt_metadata_fixture m = {0};
    mc_world_runtime *const app_world = app->active_world;
    mc_world_info_player *saved = NULL, *current = NULL;
    mc_world_player_load_error *error = calloc(1, sizeof(*error));
    nbt_input *input = calloc(1, sizeof(*input));
    const unsigned int registry_entered = app->registry->entered;
    char path[8192];
    uint8_t map[256];
    for (size_t i = 0; i < COUNT(map); ++i)
        map[i] = (uint8_t)i;
    const int length = snprintf(path, sizeof(path), "%s/../sleep-load-%u",
                                mc_world_store_root(f->stores[0]), scenario);
    const mc_world_store_options options = {.session_token = 501u + scenario,
                                            .block_map = map,
                                            .block_map_length = sizeof(map),
                                            .limits = mc_world_store_default_limits()};
    static const uint16_t name[] = {'S', 'l', 'e', 'e', 'p'};
    CHECK(error != NULL && input != NULL && scenario < 4 && length > 0 &&
          (size_t)length < sizeof(path) &&
          mc_world_store_open(path, f->regions, &options, &m.store, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_info_name_copy((mc_nbt_string){name, COUNT(name)}, SIZE_MAX, &m.name) ==
              MC_WORLD_INFO_OK &&
          nbt_metadata_construct(f, &m));
    app->active_world = m.world;
    CHECK(mc_local_player_allocate(app->entities, &m.player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(m.player, m.world, app, app->session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          nbt_prepare(app, m.player, "sleep", input, &error->nbt));
    /* A missing Bed is the declared native caller input. Query and publication
     * still use the actual normal Chunk and real provider/cache. */
    mc_chunk_state *chunk = NULL;
    CHECK(mc_world_runtime_get(m.world, 0, 0, &chunk) == MC_CHUNK_ACQUIRE_OK && chunk != NULL &&
          !chunk->blank && chunk->data != NULL);
    chunk->data->blocks[boxes_index(8, 80, 8)] = 0;
    m.world->fields.J = true;
    const mc_nbt_tag *tag = nbt_make_input(input, "sleep");
    CHECK(tag != NULL &&
          (scenario != 3 || nbt_scalar(input, &input->nodes[0], "Score", MC_NBT_SHORT, 123)) &&
          mc_world_info_player_copy(tag, NULL, &saved, NULL) == MC_WORLD_INFO_OK &&
          mc_world_info_player_set(m.world->fields.x, saved) == MC_WORLD_INFO_OK &&
          m.world->fields.d->count == 0 && m.world->fields.b->count == 0);
    mc_player_fields *fields = &m.player->local_player->fields.player;
    const size_t holds = m.player->holds, borrows = app->registry->runtime_borrows,
                 bindings = m.world->persistent_bindings;
    const mc_java_random actor_random = *m.player->random, world_random = *m.world->fields.r;
    if (scenario == 2) {
        m.player->base.world = NULL; /* Declared actual source receiver fault. */
        player_world_changed = true;
    }
    const mc_world_storage_status status =
        mc_world_runtime_load_player(m.world, m.player, scenario == 0 ? NULL : error);
    CHECK(m.world->active_calls == 0 && m.world->local_leases == 0 &&
          nbt_random_equal(&actor_random, m.player->random) &&
          nbt_random_equal(&world_random, m.world->fields.r));
    if (scenario == 1) {
        CHECK(status == MC_WORLD_STORAGE_OK && error->saved_data_present && error->nbt_returned &&
              error->info_cleared && error->publication_returned && error->source_returned &&
              error->bytecode == 91 && !error->source_exception && !error->source_error &&
              !error->caught_exception && !error->native_policy && error->nbt.sleep_work == NULL &&
              error->nbt.sleep == NULL && !fields->u && fields->private_a == 0 &&
              fields->g == 123 && fields->v != NULL && fields->v->x == 8 && fields->v->y == 80 &&
              fields->v->z == 8 && fields->private_b != NULL && fields->private_b->x == 5 &&
              fields->private_b->y == 66 && fields->private_b->z == -7 &&
              fields->private_b != fields->v && !m.world->fields.J &&
              m.world->fields.d->count == 1 && m.world->fields.d->items[0] == m.player &&
              m.world->fields.b->count == 1 && m.world->fields.b->items[0] == m.player &&
              mc_world_info_player_get(m.world->fields.x, &current) == MC_WORLD_INFO_OK &&
              current == NULL && app->registry->runtime_borrows == borrows &&
              mc_world_player_load_error_retire(error));
    } else {
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              mc_world_info_player_get(m.world->fields.x, &current) == MC_WORLD_INFO_OK &&
              current == saved && m.world->fields.d->count == 0 && m.world->fields.b->count == 0 &&
              fields->v != NULL && fields->v->x == 8 && fields->v->y == 80 && fields->v->z == 8 &&
              fields->g == 900);
        if (scenario == 0) {
            CHECK(fields->u && fields->private_a == 23 && m.world->fields.J &&
                  fields->private_b != NULL && fields->private_b->x == -7 &&
                  fields->private_b->y == 65 && fields->private_b->z == 9 &&
                  m.player->holds == holds && app->registry->runtime_borrows == borrows &&
                  m.world->persistent_bindings == bindings);
        } else {
            CHECK(error->bytecode == 14 && error->catch_bytecode == 88 && error->source_exception &&
                  error->caught_exception && error->print_stack_trace_pending &&
                  !error->source_returned && !error->nbt_returned && !error->info_cleared &&
                  !error->publication_returned && !error->native_policy && !error->source_error &&
                  error->nbt.sleep_work != NULL && error->nbt.base_bytecode == 311 &&
                  app->registry->runtime_borrows == borrows + 1);
            if (scenario == 2) {
                CHECK(m.player->base.world == NULL && fields->u && fields->private_a == 23 &&
                      m.world->fields.J && error->nbt.status == MC_ENTITY_NBT_NULL_REFERENCE &&
                      error->nbt.player_bytecode == 93 && error->nbt.local_player_bytecode == 2 &&
                      error->nbt.sleep != NULL && error->nbt.sleep->source_exception &&
                      !error->nbt.sleep->native_policy && !error->nbt.sleep->source_error &&
                      error->nbt.sleep->site.pc == 48 &&
                      strcmp(error->exception_class, "java/lang/NullPointerException") == 0);
                m.player->base.world = m.world;
                player_world_changed = false;
            } else {
                CHECK(!fields->u && fields->private_a == 0 && !m.world->fields.J &&
                      fields->private_b != NULL && fields->private_b->x == 5 &&
                      fields->private_b->y == 66 && fields->private_b->z == -7 &&
                      error->nbt.status == MC_ENTITY_NBT_WRONG_TAG &&
                      error->nbt.player_bytecode == 152 && error->nbt.local_player_bytecode == 9 &&
                      error->nbt.field != NULL && strcmp(error->nbt.field, "Score") == 0 &&
                      error->nbt.sleep == NULL &&
                      strcmp(error->exception_class, "java/lang/ClassCastException") == 0);
            }
            const mc_world_storage_status first_status = error->status;
            const char *const first_operation = error->operation;
            const mc_entity_nbt_status first_nbt = error->nbt.status;
            const unsigned int first_player_pc = error->nbt.player_bytecode;
            /* Only native retirement is retried. The complete source cause,
             * Info.Player and already reached fields remain the same. */
            app->registry->entered = 1;
            CHECK(!mc_world_player_load_error_retire(error) && error->status == first_status &&
                  error->operation == first_operation && error->nbt.status == first_nbt &&
                  error->nbt.player_bytecode == first_player_pc &&
                  error->cleanup.status == MC_WORLD_STORAGE_BUSY && error->nbt.sleep_work != NULL &&
                  error->nbt.sleep_cleanup != NULL &&
                  error->nbt.sleep_cleanup->child == MC_ENTITY_NBT_SLEEP_CLEANUP_REGISTRY &&
                  m.player->holds == holds && m.world->persistent_bindings == bindings &&
                  app->registry->runtime_borrows == borrows + 1);
            if (scenario == 2)
                CHECK(error->nbt.sleep != NULL && error->nbt.sleep->acquisition == NULL &&
                      error->nbt.sleep->occupancy == NULL && error->nbt.sleep->bed == NULL &&
                      error->nbt.sleep->world_sleep == NULL && error->nbt.sleep->position == NULL);
            else
                CHECK(error->nbt.sleep == NULL && !fields->u && fields->private_a == 0 &&
                      fields->g == 900);
            app->registry->entered = registry_entered;
            CHECK(mc_world_player_load_error_retire(error) &&
                  mc_world_player_load_error_retire(error) && error->status == first_status &&
                  error->operation == first_operation && error->nbt.status == first_nbt &&
                  error->nbt.player_bytecode == first_player_pc && error->nbt.sleep_work == NULL &&
                  error->nbt.sleep == NULL && error->nbt.sleep_cleanup == NULL &&
                  m.player->holds == holds && app->registry->runtime_borrows == borrows &&
                  m.world->persistent_bindings == bindings &&
                  mc_world_info_player_get(m.world->fields.x, &current) == MC_WORLD_INFO_OK &&
                  current == saved && m.world->fields.d->count == 0 &&
                  m.world->fields.b->count == 0);
        }
    }
    ok = true;
cleanup:
    app->registry->entered = registry_entered;
    if (player_world_changed && m.player != NULL)
        m.player->base.world = m.world;
    if (error != NULL && !mc_world_player_load_error_retire(error))
        return false;
    app->active_world = app_world;
    mc_world_info_player_release(saved);
    if (!nbt_metadata_close_world(&m))
        return false;
    if (mc_world_store_free_checked(&m.store, NULL) != MC_WORLD_STORAGE_OK)
        return false;
    mc_world_info_name_release(m.name);
    free(input);
    free(error);
    return ok;
}

static bool nbt_sleep_load_cases(fixture *f, client_application *app) {
    for (unsigned int scenario = 0; scenario < 4; ++scenario)
        if (!nbt_sleep_load_case(f, app, scenario))
            return false;
    puts("Player sleeping NBT World loader: 4 authored scenarios");
    return !ferror(stdout);
}
