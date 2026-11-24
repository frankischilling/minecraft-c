/* Reuse genuine World/Store/Chunk/App construction; its observation main is
 * retained under a distinct name and is never executed by this control. */
#define MC_WORLD_ENTITY_TICK_MAIN mc_respawn_entity_tick_fixture_main
#include "test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN

#include "../../../src/world/info/tick_source_internal.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/respawn.h"

static bool respawn_provider_cases(void) {
    bool ok = false;
    const mc_world_provider_kind kinds[] = {MC_WORLD_PROVIDER_OVERWORLD, MC_WORLD_PROVIDER_NETHER,
                                            MC_WORLD_PROVIDER_SKY};
    mc_world_provider *provider = NULL;
    for (size_t i = 0; i < COUNT(kinds); ++i) {
        bool allowed = kinds[i] == MC_WORLD_PROVIDER_NETHER;
        CHECK(mc_world_provider_allocate(kinds[i], NULL, &provider, NULL) == MC_WORLD_PROVIDER_OK);
        CHECK(mc_world_provider_can_respawn(provider, &allowed, NULL) == MC_WORLD_PROVIDER_OK &&
              allowed == (kinds[i] != MC_WORLD_PROVIDER_NETHER));
        CHECK(mc_world_provider_construct(provider, NULL) == MC_WORLD_PROVIDER_OK);
        CHECK(mc_world_provider_can_respawn(provider, &allowed, NULL) == MC_WORLD_PROVIDER_OK &&
              allowed == (kinds[i] != MC_WORLD_PROVIDER_NETHER));
        mc_world_provider_release(provider);
        provider = NULL;
    }
    bool allowed = true;
    CHECK(mc_world_provider_can_respawn(NULL, &allowed, NULL) ==
              MC_WORLD_PROVIDER_INVALID_ARGUMENT &&
          allowed);
    ok = true;
cleanup:
    mc_world_provider_release(provider);
    return ok;
}

static bool respawn_spawn_cases(fixture *f, mc_world_respawn_error *e) {
    bool ok = false, changed = false;
    mc_world_runtime *w = f->worlds[0];
    mc_world_info *info = w->fields.x;
    mc_java_random *random = w->fields.r;
    mc_world_respawn_position *a = NULL, *b = NULL;
    mc_world_mutation_error *mutation = malloc(sizeof(*mutation));
    CHECK(mutation != NULL);
    CHECK(mc_world_info_source_spawn_axis_set(info, 0, 8) == MC_WORLD_INFO_OK &&
          mc_world_info_source_spawn_axis_set(info, 1, 0) == MC_WORLD_INFO_OK &&
          mc_world_info_source_spawn_axis_set(info, 2, 8) == MC_WORLD_INFO_OK);
    const mc_java_random before = *random;
    CHECK(mc_world_runtime_respawn_repair_spawn(w, 0, e) == MC_WORLD_STORAGE_LIMIT &&
          e->bytecode == 38 && e->queries == 0 && e->random_draws == 0 &&
          random->state == before.state &&
          random->have_next_next_gaussian == before.have_next_next_gaussian);
    int32_t y;
    CHECK(mc_world_info_source_spawn_get(info, 1, &y) == MC_WORLD_INFO_OK && y == 64);
    CHECK(mc_world_runtime_block_set_quiet(w, 8, 63, 8, 1, &changed, mutation) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_respawn_repair_spawn(w, 1, e) == MC_WORLD_STORAGE_OK &&
          e->bytecode == 107 && e->queries == 1 && e->random_draws == 0 &&
          random->state == before.state);
    CHECK(mc_world_runtime_respawn_spawn_new(w, &a, e) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_respawn_spawn_new(w, &b, e) == MC_WORLD_STORAGE_OK && a != b &&
          a->x == 8 && a->y == 64 && a->z == 8 && b->x == 8 && b->y == 64 && b->z == 8);
    mc_world_respawn_position *unchanged = a;
    w->fields.x = NULL; /* Authored nullable field input, restored before retirement. */
    const mc_world_storage_status no_info = mc_world_runtime_respawn_spawn_new(w, &unchanged, e);
    w->fields.x = info;
    CHECK(no_info == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e->bytecode == 8 && e->source_null &&
          unchanged == a);
    CHECK(mc_world_runtime_block_set_quiet(w, 8, 63, 8, 0, &changed, mutation) ==
          MC_WORLD_STORAGE_OK);
    mc_java_random_reseed(random, 123);
    mc_java_random expected = *random;
    int32_t draw[4];
    for (size_t i = 0; i < COUNT(draw); ++i)
        CHECK(mc_java_random_next_bounded(&expected, 8, &draw[i]));
    CHECK(mc_world_runtime_respawn_repair_spawn(w, 1, e) == MC_WORLD_STORAGE_LIMIT &&
          e->bytecode == 38 && e->queries == 1 && e->random_draws == 4 &&
          e->x == mc_java_i32_add(8, mc_java_i32_sub(draw[0], draw[1])) &&
          e->z == mc_java_i32_add(8, mc_java_i32_sub(draw[2], draw[3])) &&
          random->state == expected.state);
    int32_t x, z;
    CHECK(mc_world_info_source_spawn_get(info, 0, &x) == MC_WORLD_INFO_OK && x == 8 &&
          mc_world_info_source_spawn_get(info, 2, &z) == MC_WORLD_INFO_OK && z == 8);
    w->fields.r = NULL;
    const mc_world_storage_status no_random = mc_world_runtime_respawn_repair_spawn(w, 1, e);
    w->fields.r = random;
    CHECK(no_random == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e->source_null && e->bytecode == 51 &&
          e->queries == 1 && e->random_draws == 0);
    CHECK(w->active_calls == 0);
    ok = true;
cleanup:
    w->fields.x = info;
    w->fields.r = random;
    mc_world_respawn_position_free(a);
    mc_world_respawn_position_free(b);
    free(mutation);
    return ok;
}

static bool respawn_cleanup_cases(fixture *f, mc_entity *p, mc_entity *q,
                                  mc_world_entity_tick_error *e) {
    bool ok = false, added = false;
    mc_world_runtime *w = f->worlds[0];
    mc_world_access *access = NULL;
    const mc_world_access_callbacks callbacks = {.entity_added = added_callback,
                                                 .entity_removed = removed_callback};
    CHECK(mc_world_access_create(&callbacks, NULL, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(w, access, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_insert_entity(w, p, &added, NULL) == MC_WORLD_STORAGE_OK && added);
    CHECK(mc_world_runtime_insert_entity(w, q, &added, NULL) == MC_WORLD_STORAGE_OK && added);
    removed_events = 0;
    p->base.be = true;
    q->base.bt = 123;
    const mc_java_random random = *q->random;
    CHECK(mc_world_runtime_respawn_cleanup(w, &tick_context, NULL, e) == MC_WORLD_STORAGE_OK &&
          removed_events == 1 && w->fields.b->count == 1 && w->fields.b->items[0] == q &&
          q->base.bt == 123 && q->random->state == random.state && w->fields.D->count == 0);
    CHECK(tick_defer(w, q) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_respawn_cleanup(w, &tick_context, NULL, e) == MC_WORLD_STORAGE_OK &&
          removed_events == 2 && w->fields.b->count == 0 && w->fields.D->count == 0 &&
          q->base.bt == 123 && q->random->state == random.state);
    p->base.be = false;
    CHECK(mc_world_runtime_insert_entity(w, p, &added, NULL) == MC_WORLD_STORAGE_OK && added);
    p->base.be = true;
    fail_removed = true;
    CHECK(mc_world_runtime_respawn_cleanup(w, &tick_context, NULL, e) ==
              MC_WORLD_STORAGE_IO_ERROR &&
          e->bytecode == 267 && removed_events == 3 && w->fields.b->count == 0 &&
          w->fields.D->count == 0);
    fail_removed = false;
    CHECK(mc_world_runtime_respawn_cleanup(w, &tick_context, NULL, e) == MC_WORLD_STORAGE_OK &&
          removed_events == 3);
    CHECK(mc_world_runtime_insert_entity(w, q, &added, NULL) == MC_WORLD_STORAGE_OK && added &&
          tick_defer(w, q) == MC_WORLD_STORAGE_OK);
    fail_removed = true;
    CHECK(mc_world_runtime_respawn_cleanup(w, &tick_context, NULL, e) ==
              MC_WORLD_STORAGE_IO_ERROR &&
          e->bytecode == 117 && removed_events == 4 && w->fields.b->count == 0 &&
          w->fields.D->count == 1);
    fail_removed = false;
    CHECK(mc_world_runtime_respawn_cleanup(w, &tick_context, NULL, e) == MC_WORLD_STORAGE_OK &&
          removed_events == 5 && w->fields.D->count == 0);
    /* The source deliberately skips a dead rider while its live mount still
     * points back. Cleanup does not replace the branch with unconditional dead removal. */
    p->base.be = false;
    CHECK(mc_entity_mount(p, q, NULL, 0, NULL) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_insert_entity(w, p, &added, NULL) == MC_WORLD_STORAGE_OK && added);
    p->base.be = true;
    CHECK(mc_world_runtime_respawn_cleanup(w, &tick_context, NULL, e) == MC_WORLD_STORAGE_OK &&
          w->fields.b->count == 1 && p->base.aH == q && q->base.aG == p && removed_events == 5);
    q->base.be = true;
    CHECK(mc_world_runtime_respawn_cleanup(w, &tick_context, NULL, e) == MC_WORLD_STORAGE_OK &&
          w->fields.b->count == 0 && p->base.aH == NULL && q->base.aG == NULL &&
          removed_events == 6);
    mc_world_reference_list *deferred = w->fields.D;
    w->fields.D = NULL;
    const mc_world_storage_status null_list =
        mc_world_runtime_respawn_cleanup(w, &tick_context, NULL, e);
    w->fields.D = deferred;
    CHECK(null_list == MC_WORLD_STORAGE_UNSUPPORTED_STATE && e->bytecode == 8 &&
          w->active_calls == 0);
    ok = true;
cleanup:
    fail_removed = false;
    if (access != NULL) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(w, access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed || mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
            ok = false;
    }
    return ok;
}

static bool respawn_remove_cases(fixture *f, client_application *app, const char *root,
                                 mc_entity *p, mc_entity *q, mc_world_respawn_error *e) {
    bool ok = false, added = false;
    mc_entity *player = NULL;
    mc_world_runtime *w = f->worlds[0];
    p->base.be = q->base.be = false;
    CHECK(mc_entity_set_position(p, 8.5, 8.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(q, 8.5, 8.5, 8.5) == MC_ENTITY_OK);
    CHECK(mc_entity_mount(p, q, NULL, 0, NULL) == MC_ENTITY_OK &&
          mc_world_runtime_insert_entity(w, p, &added, NULL) == MC_WORLD_STORAGE_OK && added);
    CHECK(mc_world_runtime_respawn_remove(w, p, f->registry[0], NULL, NULL, 0, e) ==
              MC_WORLD_STORAGE_OK &&
          e->bytecode == 56 && p->base.be && p->base.aH == NULL && q->base.aG == NULL &&
          w->fields.b->count == 1 && w->fields.d->count == 0);
    CHECK(mc_world_runtime_respawn_cleanup(w, &tick_context, NULL, tick_error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(app_setup(f, app, root) &&
          mc_local_player_allocate(app->entities, &player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(player, w, app, app->session, 0, NULL, NULL) == MC_PLAYER_OK &&
          mc_entity_set_position(player, 8.5, 8.5, 8.5) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_insert_entity(w, player, &added, NULL) == MC_WORLD_STORAGE_OK && added &&
          w->fields.d->count == 1 && !w->fields.J);
    CHECK(mc_world_runtime_respawn_remove(w, player, f->registry[0], NULL, NULL, 0, e) ==
              MC_WORLD_STORAGE_OK &&
          player->base.be && w->fields.d->count == 0 && !w->fields.J && w->fields.b->count == 1 &&
          w->fields.b->items[0] == player);
    CHECK(mc_world_runtime_respawn_cleanup(w, &tick_context, NULL, tick_error) ==
              MC_WORLD_STORAGE_OK &&
          w->fields.b->count == 0 && w->active_calls == 0);
    ok = true;
cleanup:
    mc_entity_release(player);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_entity *p = NULL, *q = NULL;
    client_application *app = NULL;
    mc_world_respawn_error *error = NULL;
    char root[8192], resource_root[8192];
    if (argc != 4 || strcmp(argv[1], "world-respawn") != 0 ||
        !decode_path(argv[2], root, sizeof root) ||
        !decode_path(argv[3], resource_root, sizeof resource_root) || !mc_math_trig_init(&trig))
        return 2;
    f = calloc(1, sizeof(*f));
    app = calloc(1, sizeof(*app));
    error = malloc(sizeof(*error));
    tick_error = malloc(sizeof(*tick_error));
    CHECK(f != NULL && app != NULL && error != NULL && tick_error != NULL);
    CHECK(mc_resource_files_create(resource_root, &app->files) == MC_REGISTRY_OK);
    tick_resources = mc_resource_files_source(app->files);
    CHECK(respawn_provider_cases() && tick_setup(f, root));
    CHECK(respawn_spawn_cases(f, error));
    CHECK(make_entity(f, 0, f->worlds[0], 8.5, 8.5, 8.5, &p) &&
          make_entity(f, 0, f->worlds[0], 8.5, 8.5, 8.5, &q));
    CHECK(respawn_cleanup_cases(f, p, q, tick_error));
    CHECK(respawn_remove_cases(f, app, root, p, q, error));
    result = 0;
cleanup:
    /* make_entity publishes its one constructor hold into fixture.entities;
     * tick_cleanup releases those roots after World/Chunk occurrences. */
    free(error);
    free(tick_error);
    if (app != NULL) {
        if (mc_client_application_retire_statistics(app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
            mc_client_application_retire_water_effect(app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
            mc_client_achievement_notice_free(&app->achievement_notice, NULL) !=
                MC_CLIENT_NOTICE_OK ||
            (app->render_classes != NULL &&
             mc_client_render_classes_free(app->render_classes) != MC_CLIENT_MODEL_OK))
            return 1;
    }
    if (!tick_cleanup(f))
        return 1;
    if (app != NULL) {
        if (mc_game_settings_retire(&app->settings, NULL) != MC_GAME_SETTINGS_OK)
            return 1;
        mc_client_game_directory_free(app->diagnostic_game_directory);
        mc_resource_files_free(app->files);
        free(app);
    }
    if (result == 0 && printf("World respawn: %u checks\n", checks) < 0)
        return 1;
    return result;
}
