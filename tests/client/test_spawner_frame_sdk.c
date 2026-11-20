/* Authored saved MobSpawner through the ordinary loaded World frame. */
int mc_spawner_included_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_spawner_included_mining_main
#include "gameplay/test_mining.c"
#undef MC_MINING_FIXTURE_MAIN

static struct {
    unsigned int epoch;
    size_t frames, misses, factory_calls, compiled_parts, faces, vertices;
    uintptr_t preview_identity;
    int failure;
} spawner_observation;

static int spawner_record(fixture *f) {
    const mc_client_world_frame_state *world = f->state.world_frame;
    if (world == NULL || !world->returned || world->world != f->app->active_world ||
        !world->entities_returned || world->entities == NULL ||
        world->entities->tiles_visited == 0 || spawner_observation.frames == 8)
        return 0;
    CHECK(world->source_pc == 1013 && world->entities->returned &&
          world->entities->source_pc == 569 && world->entities->tiles_visited == 1 &&
          f->state.hud != NULL && f->state.hud->returned);
    const mc_client_render_global_tile_receipt *draw = &world->entities->last_tile_draw;
    CHECK(world->entities->tile_draws_attempted == 1 && world->entities->tile_draws_returned == 1 &&
          world->entities->tile_draws_retired == 1 && world->entities->tile_draw == NULL &&
          draw->valid && draw->returned && draw->retired && !draw->distance_culled &&
          draw->renderer_returned && draw->world_rebound && draw->pose_returned &&
          draw->manager_returned && draw->manager_retired && draw->preview_identity != 0 &&
          draw->preview_kind == MC_ENTITY_CLASS_KIND_PIG &&
          draw->preview_world_identity == (uintptr_t)f->app->active_world &&
          draw->cache_entries == 1 && draw->model_parts > 0 && draw->renderer_class != NULL &&
          strcmp(draw->renderer_class, "ag") == 0 &&
          (spawner_observation.preview_identity == 0 ||
           spawner_observation.preview_identity == draw->preview_identity));
    const bool first = spawner_observation.epoch == 0 && spawner_observation.frames == 0;
    CHECK(draw->cache_miss == first && draw->cache_hit == !first &&
          draw->factory_calls == (first ? 1u : 0u));
    const mc_world_reference_list *entities = NULL;
    int32_t count;
    CHECK(mc_world_runtime_render_entities_list(f->app->active_world, &entities, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_render_entity_list_count(f->app->active_world, entities, &count, NULL) ==
              MC_WORLD_STORAGE_OK);
    for (int32_t i = 0; i < count; ++i) {
        mc_entity *entity = NULL;
        CHECK(mc_world_runtime_render_entity_list_get(f->app->active_world, entities, i, &entity,
                                                      NULL) == MC_WORLD_STORAGE_OK &&
              (uintptr_t)entity != draw->preview_identity);
    }
    spawner_observation.preview_identity = draw->preview_identity;
    spawner_observation.misses += draw->cache_miss ? 1u : 0u;
    spawner_observation.factory_calls += draw->factory_calls;
    spawner_observation.compiled_parts += draw->model_compiled_parts;
    spawner_observation.faces += draw->model_faces;
    spawner_observation.vertices += draw->model_vertices;
    printf("SPAWNER_DRAW epoch=%u frame=%zu cache-hit=%d cache-miss=%d factory-calls=%u "
           "models=%u compiled=%u faces=%u vertices=%u preview-pig=1 world-same=1 th-returned=1 "
           "th-retired=1 ll-retired=1 world-list-absent=1\n",
           spawner_observation.epoch, spawner_observation.frames, (int)draw->cache_hit,
           (int)draw->cache_miss, draw->factory_calls, draw->model_parts,
           draw->model_compiled_parts, draw->model_faces, draw->model_vertices);
    CHECK(fflush(stdout) == 0);
    ++spawner_observation.frames;
    return 0;
}

static void spawner_after_frame(fixture *f, bool returned) {
    if (returned && f != NULL && mc_client_menu_loop_inspect(f->loop, &f->state) &&
        f->app->active_world != NULL && spawner_record(f) != 0)
        spawner_observation.failure = 1;
}

static int spawner_ready(fixture *f, mining_work *work, size_t *returned) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    while (SDL_GetTicks() < deadline && spawner_observation.frames < 8) {
        CHECK(save_title_focus(f) == 0 && frame(f) == 0 && !spawner_observation.failure);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL) {
            size_t eligible;
            int64_t time;
            CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &pauses, &eligible,
                                    &time) == 0);
            continue;
        }
    }
    *returned = spawner_observation.frames;
    CHECK(!spawner_observation.failure && *returned == 8 && spawner_observation.misses == 1 &&
          spawner_observation.factory_calls == 1 && spawner_observation.compiled_parts > 0 &&
          spawner_observation.faces > 0 && spawner_observation.vertices > 0 &&
          mining_capture(f, work,
                         spawner_observation.epoch == 0 ? "spawner-frame.png"
                                                        : "spawner-reloaded.png") == 0);
    return 0;
}

static int spawner_run(fixture *f, mining_work *work, uint32_t *frame_id, uint32_t *canvas_id) {
    mining_extra_after_frame = spawner_after_frame;
    CHECK(save_title_host(f, "open", true, frame_id, canvas_id) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && mining_choose(f, work, NULL, &work->epochs[0]) == 0);
    size_t returned = 0;
    CHECK(spawner_ready(f, work, &returned) == 0);
    printf("SPAWNER_FRAME epoch=0 frames=%zu tiles=1 n-pc=569 full-world=1 hud=1 saved-input=1 "
           "source-field-repair=0\n",
           returned);
    char title[8192];
    CHECK(fflush(stdout) == 0 && mining_path(work, "spawner-title.png", title) == 0 &&
          save_title_run(f, NULL, title, false, 1, 0) == 0 &&
          reload_snapshot("spawner-saved") == 0);
    spawner_observation.epoch = 1;
    spawner_observation.frames = 0;
    CHECK(mining_choose(f, work, &work->epochs[0], &work->epochs[1]) == 0 &&
          spawner_ready(f, work, &returned) == 0);
    printf("SPAWNER_FRAME epoch=1 frames=%zu tiles=1 n-pc=569 full-world=1 hud=1 saved-input=1 "
           "source-field-repair=0\n",
           returned);
    puts("SPAWNER_CACHE epochs=2 preview-same=1 world-rebound=1 factory-calls=1 "
         "ordinary-save-reload=1");
    CHECK(fflush(stdout) == 0 &&
          save_title_host(f, "after-spawner", false, frame_id, canvas_id) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3);
    fixture *f = calloc(1, sizeof(*f));
    CHECK(f != NULL);
    mining_fixture = f;
    mining_work work = {.directory = argv[2]};
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    int result = 1;
    uint32_t frame_id = 0, canvas_id = 0;
    if (mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK) {
        f->app = mc_client_bootstrap_app(f->bootstrap);
        result = spawner_run(f, &work, &frame_id, &canvas_id);
    }
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop)) {
        if (mc_client_menu_loop_inspect(f->loop, &f->state))
            fprintf(stderr, "Spawner terminal cleanup refused: %s\n",
                    f->state.retirement_operation);
        result = 1;
    }
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    if (closed != MC_CLIENT_BOOTSTRAP_OK || f->bootstrap != NULL || f->loop != NULL ||
        frame_id == 0 || canvas_id == 0 || SDL_GetWindowFromID(frame_id) != NULL ||
        SDL_GetWindowFromID(canvas_id) != NULL)
        result = 1;
    if (result == 0)
        printf("SPAWNER_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    free(f);
    return result;
}
