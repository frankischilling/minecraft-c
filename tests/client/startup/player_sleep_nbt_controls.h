#ifndef MC_TEST_PLAYER_SLEEP_NBT_CONTROLS_H
#define MC_TEST_PLAYER_SLEEP_NBT_CONTROLS_H

/* Authored SDK caller inputs. This header adds no production entry or original
 * observation, and never resumes a failed source Load/Respawn invocation. */
#include "block/registry/registry_internal.h"
#include "client/world/respawn.h"
#include "entity/player/player_internal.h"
#include "mc/entity/player/sleep.h"
#include "mc/storage/nbt/nbt.h"
#include "mc/world/runtime/world_access.h"

static const char *player_sleep_nbt_profile(void) {
    const char *value =
        SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "MC_PLAYER_SLEEP_NBT_PROFILE");
    return value != NULL && (!strcmp(value, "success") || !strcmp(value, "load-failure")) ? value
                                                                                          : NULL;
}

#define SLEEP_NBT_CHECK(condition)                                                                 \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Sleeping NBT SDK check%u at%d: %s\n", checks, __LINE__, #condition);  \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static int player_sleep_nbt_payload(mc_world_info_player **out) {
    int result = 1;
    size_t length = 0, consumed = 0;
    mc_nbt_document *document = NULL;
    void *bytes = NULL;
    const char *path =
        SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "MC_PLAYER_SLEEP_NBT_PAYLOAD");
    SLEEP_NBT_CHECK(out != NULL && *out == NULL && path != NULL);
    bytes = SDL_LoadFile(path, &length);
    SLEEP_NBT_CHECK(bytes != NULL && length != 0 && length <= 1048576);
    mc_nbt_error error;
    SLEEP_NBT_CHECK(mc_nbt_read_compound(bytes, length, NULL, &document, &consumed, &error) ==
                        MC_NBT_OK &&
                    consumed == length && document != NULL);
    SLEEP_NBT_CHECK(mc_world_info_player_copy(mc_nbt_document_root(document), NULL, out, NULL) ==
                        MC_WORLD_INFO_OK &&
                    *out != NULL);
    result = 0;
cleanup:
    mc_nbt_document_free(document);
    SDL_free(bytes);
    if (result != 0 && out != NULL) {
        mc_world_info_player_release(*out);
        *out = NULL;
    }
    return result;
}

static size_t player_sleep_nbt_occurrences(const mc_world_reference_list *list, mc_entity *p) {
    size_t count = 0;
    if (list == NULL || list->count > list->capacity || (list->count != 0 && list->items == NULL))
        return SIZE_MAX;
    for (size_t i = 0; i < list->count; ++i)
        if (list->items[i] == p)
            ++count;
    return count;
}

static int player_sleep_nbt_loaded(fixture *f, size_t epoch) {
    mc_client_screen_transition_state transitions;
    mc_client_world_load_state load;
    CHECK(mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions) &&
          transitions.world_load != NULL &&
          mc_client_world_load_inspect(transitions.world_load, &load) && load.source_returned &&
          load.player_load_returned && load.view_published && load.failure != NULL &&
          load.failure->status == MC_CLIENT_WORLD_LOAD_OK && load.player == f->app->h &&
          f->app->i == f->app->h && load.world == f->app->active_world);
    mc_entity *p = load.player;
    mc_player_fields *fields = &p->local_player->fields.player;
    mc_world_info_player *saved = NULL;
    const bool bed_coordinate = epoch == 1 ? fields->v != NULL && fields->v->x == 3 &&
                                                 fields->v->y == 64 && fields->v->z == 4
                                           : fields->v == NULL;
    CHECK(!fields->u && fields->private_a == 0 && fields->g == 7 && bed_coordinate &&
          fields->private_b != NULL && fields->private_b != fields->v &&
          fields->private_b->x == 3 && fields->private_b->y == 64 && fields->private_b->z == 4 &&
          p->local_player->fields.living.Y == 20 && p->local_player->fields.input == load.input &&
          mc_world_info_player_get(load.world->fields.x, &saved) == MC_WORLD_INFO_OK &&
          saved == NULL && player_sleep_nbt_occurrences(load.world->fields.d, p) == 1 &&
          player_sleep_nbt_occurrences(load.world->fields.b, p) == 1);
    int32_t metadata = -1;
    mc_world_storage_error error;
    CHECK(mc_world_runtime_metadata_get(load.world, 3, 64, 4, &metadata, &error) ==
              MC_WORLD_STORAGE_OK &&
          metadata == 8 && reload_player_observation(load.world, p, epoch) == 0);
    printf("SLEEP_NBT_LOAD epoch=%zu actual-load=1 awake=1 timer=0 reconstructed-v=%d Q=3,64,4 "
           "Q-distinct-v=1 score=7 occupied=0 info-cleared=1 published=1 exact-wake-pose=0\n",
           epoch, epoch == 1 ? 1 : 0);
    return 0;
}

typedef struct player_sleep_nbt_observer {
    client_application *app;
    uintptr_t previous;
    size_t selected, ranges, added, removed;
} player_sleep_nbt_observer;

static mc_world_storage_status player_sleep_nbt_cell(void *context, mc_world_runtime *world,
                                                     int32_t x, int32_t y, int32_t z,
                                                     mc_world_storage_error *error) {
    player_sleep_nbt_observer *observer = context;
    if (x == 3 && y == 64 && z == 4 && observer->app->active_world == world &&
        (uintptr_t)observer->app->h != observer->previous) {
        ++observer->selected;
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "Authored sleeping NBT occupancy refusal", 0};
        return error->status;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status player_sleep_nbt_range(void *context, mc_world_runtime *world,
                                                      int32_t ax, int32_t ay, int32_t az,
                                                      int32_t bx, int32_t by, int32_t bz,
                                                      mc_world_storage_error *error) {
    (void)world;
    (void)ax;
    (void)ay;
    (void)az;
    (void)bx;
    (void)by;
    (void)bz;
    (void)error;
    ++((player_sleep_nbt_observer *)context)->ranges;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status player_sleep_nbt_added(void *context, mc_world_runtime *world,
                                                      mc_entity *p, mc_world_storage_error *error) {
    (void)world;
    (void)p;
    (void)error;
    ++((player_sleep_nbt_observer *)context)->added;
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status player_sleep_nbt_removed(void *context, mc_world_runtime *world,
                                                        mc_entity *p,
                                                        mc_world_storage_error *error) {
    (void)world;
    (void)p;
    (void)error;
    ++((player_sleep_nbt_observer *)context)->removed;
    return MC_WORLD_STORAGE_OK;
}

static int player_sleep_nbt_respawn(fixture *f, mc_client_world_load_call *load) {
    int result = 1;
    bool registered = false;
    mc_world_runtime *world = f->app->active_world;
    mc_registry_classes *registry = f->app->registry;
    const unsigned int entered = registry->entered;
    const bool observed = registry->blocks.t[26];
    mc_world_info_player *payload = NULL;
    mc_world_access *access = NULL;
    mc_client_respawn_call *call = NULL, *other = NULL;
    mc_client_respawn_error error, blocked;
    player_sleep_nbt_observer *observer = calloc(1, sizeof(*observer));
    SLEEP_NBT_CHECK(observer != NULL);
    observer->app = f->app;
    observer->previous = (uintptr_t)f->app->h;
    SLEEP_NBT_CHECK(world != NULL && load != NULL && f->app->h != NULL &&
                    mc_client_menu_loop_retire_completed_draws(f->loop) &&
                    mc_client_menu_loop_inspect(f->loop, &f->state));
    SLEEP_NBT_CHECK(player_sleep_nbt_payload(&payload) == 0 &&
                    mc_world_info_player_set(world->fields.x, payload) == MC_WORLD_INFO_OK);
    bool changed = false;
    mc_world_storage_error world_error;
    SLEEP_NBT_CHECK(mc_world_runtime_metadata_set_quiet(world, 3, 64, 4, 12, &changed,
                                                        &world_error) == MC_WORLD_STORAGE_OK &&
                    changed);
    registry->blocks.t[26] = true; /* Declared mutable source input, not a default flag claim. */
    const mc_world_access_callbacks callbacks = {.cell = player_sleep_nbt_cell,
                                                 .range = player_sleep_nbt_range,
                                                 .entity_added = player_sleep_nbt_added,
                                                 .entity_removed = player_sleep_nbt_removed};
    SLEEP_NBT_CHECK(mc_world_access_create(&callbacks, observer, &access, NULL) ==
                        MC_WORLD_STORAGE_OK &&
                    mc_world_runtime_add_world_access(world, access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    SLEEP_NBT_CHECK(
        mc_client_respawn_begin(f->app, load, false, 0, &call, &error) ==
            MC_CLIENT_RESPAWN_CHILD_FAILED &&
        call != NULL && error.site.pc == 303 && error.player_load != NULL &&
        error.player_load->bytecode == 14 && error.player_load->native_policy &&
        !error.player_load->source_exception && !error.player_load->source_error &&
        !error.player_load->caught_exception && !error.player_load->print_stack_trace_pending &&
        !error.player_load->nbt_returned && !error.player_load->info_cleared &&
        !error.player_load->publication_returned && error.player_load->nbt.player_bytecode == 93 &&
        error.player_load->nbt.sleep_work != NULL && error.player_load->nbt.sleep != NULL &&
        error.player_load->nbt.sleep->native_policy &&
        error.player_load->nbt.sleep->site.pc == 80 &&
        error.player_load->nbt.sleep->occupancy != NULL &&
        error.player_load->nbt.sleep->world.status == MC_WORLD_STORAGE_IO_ERROR &&
        observer->selected == 1 && observer->added == 0);
    mc_client_respawn_state before, current;
    SLEEP_NBT_CHECK(mc_client_respawn_inspect(call, &before) && before.player_stored &&
                    before.view_stored && !before.input_stored && !before.source_returned &&
                    f->app->h->local_player->fields.input == NULL);
    const size_t selected = observer->selected, ranges = observer->ranges, added = observer->added,
                 removed = observer->removed;
    registry->entered = 1; /* Deliberate native checked-retirement refusal only. */
    SLEEP_NBT_CHECK(
        mc_client_respawn_retire(&call, &error) == MC_CLIENT_RESPAWN_CLEANUP_FAILED &&
        call != NULL && error.player_load != NULL &&
        error.player_load->cleanup.status == MC_WORLD_STORAGE_BUSY &&
        error.player_load->nbt.sleep_cleanup != NULL &&
        error.player_load->nbt.sleep_cleanup->child == MC_ENTITY_NBT_SLEEP_CLEANUP_WAKE &&
        error.player_load->nbt.sleep_cleanup->wake != NULL &&
        error.player_load->nbt.sleep_cleanup->wake->child == MC_PLAYER_SLEEP_CLEANUP_REGISTRY &&
        mc_client_respawn_inspect(call, &current) && current.failure == before.failure &&
        current.failure->site.pc == 303 && current.failure->player_load->nbt.sleep->site.pc == 80 &&
        mc_client_world_load_terminal_records(load) == MC_CLIENT_WORLD_LOAD_BUSY &&
        mc_client_respawn_begin(f->app, load, false, 0, &other, &blocked) ==
            MC_CLIENT_RESPAWN_BUSY &&
        other == NULL);
    registry->entered = entered;
    SLEEP_NBT_CHECK(mc_client_respawn_retire(&call, &error) == MC_CLIENT_RESPAWN_OK &&
                    call == NULL && observer->selected == selected && observer->ranges == ranges &&
                    observer->added == added && observer->removed == removed &&
                    f->app->h->local_player->fields.input == NULL);
    puts("SLEEP_NBT_RESPAWN actual-parent=303 fd=14 gs=93 wake=80 native-io=1 retained=1 "
         "cleanup-refused=1 load-lease-busy=1 second-admission-busy=1 repair-retired=1 "
         "callbacks-unreplayed=1 input321-unreached=1");
    result = 0;
cleanup:
    registry->entered = entered;
    if (call != NULL && mc_client_respawn_retire(&call, &error) != MC_CLIENT_RESPAWN_OK)
        result = 1;
    registry->blocks.t[26] = observed;
    if (call == NULL && registered) {
        bool access_removed = false;
        if (mc_world_runtime_remove_world_access(world, access, &access_removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !access_removed)
            result = 1;
        else
            registered = false;
    }
    if (call == NULL && !registered && access != NULL) {
        if (mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
            result = 1;
        else
            access = NULL;
    }
    if (call == NULL && !registered && access == NULL)
        free(observer);
    mc_world_info_player_release(payload);
    /* A still-refused owner is not freed with its graph. The failed SDK child
     * ends immediately; no later source frame or callback is attempted. */
    return result;
}

static int player_sleep_nbt_success(fixture *f, mc_client_source_da *initial,
                                    const char *screenshot) {
    CHECK(!strcmp(player_sleep_nbt_profile(), "success") &&
          repeat_focus_state(f, "sleeping-NBT-first-return") == 0 &&
          player_sleep_nbt_loaded(f, 1) == 0);
    mc_game_settings_state settings;
    mc_game_settings_source_state parsed;
    CHECK(mc_game_settings_read_state(f->app->settings, &settings) &&
          settings.values.render_distance == 3 &&
          mc_game_settings_source_inspect(f->app->settings, &parsed) &&
          parsed.load_report != NULL && parsed.load_report->lines_read == 1 &&
          parsed.load_report->bytes_read == 15 && !parsed.load_report->outer_exception_caught);
    puts("WORLD_SAVE_TITLE_OPTIONS view-distance=3 lines=1 bytes=15 source-loaded=1");
    reload_epoch epochs[2];
    CHECK(reload_capture_epoch(f, &epochs[0]) == 0 && reload_snapshot("before-1") == 0 &&
          save_title_run(f, initial, screenshot, false, 1, 1) == 0 &&
          reload_snapshot("after-1") == 0 &&
          reload_choose(f, f->app->r, "alpha", &epochs[0], &epochs[1], 2) == 0 &&
          player_sleep_nbt_loaded(f, 2) == 0);
    char path[8192];
    const int n = snprintf(path, sizeof(path), "%s-before-respawn.png", screenshot);
    CHECK(n > 0 && (size_t)n < sizeof(path) && capture(f, path) == 0);
    puts("SLEEP_NBT_SAVE_RESELECT source-save-title=1 sdk-alpha-again=1 epochs=2 "
         "second-input-awake=1 info-cleared-twice=1 score7=1 original-restart-claim=0");
    CHECK(player_sleep_nbt_respawn(f, (mc_client_world_load_call *)epochs[1].call) == 0);
    return 0;
}

static int player_sleep_nbt_load_failure(fixture *f, mc_client_source_da **external,
                                         const mc_client_world_load_state *load) {
    int result = 1;
    mc_registry_classes *registry = f->app->registry;
    const unsigned int entered = registry->entered;
    bool entered_changed = false;
    mc_client_screen_transition_state transition;
    mc_client_world_load_state current;
    mc_client_menu_loop_state before;
    SLEEP_NBT_CHECK(load != NULL && load->world != NULL && load->player != NULL &&
                    load->failure != NULL && load->last_change_pc == 324 &&
                    load->failure->player_load != NULL && !load->source_returned &&
                    !load->player_load_returned && !load->view_published &&
                    mc_client_menu_loop_inspect(f->loop, &before) && before.failed &&
                    before.source_pc == 88 && !before.terminal_tail_entered);
    const mc_world_player_load_error *cause = load->failure->player_load;
    SLEEP_NBT_CHECK(cause->bytecode == 14 && cause->catch_bytecode == 88 &&
                    cause->caught_exception && cause->source_exception && !cause->native_policy &&
                    !cause->source_error && cause->exception_class != NULL &&
                    !strcmp(cause->exception_class, "java/lang/ClassCastException") &&
                    cause->print_stack_trace_pending && !cause->nbt_returned &&
                    !cause->info_cleared && !cause->publication_returned &&
                    cause->nbt.status == MC_ENTITY_NBT_WRONG_TAG &&
                    cause->nbt.local_player_bytecode == 9 && cause->nbt.player_bytecode == 152 &&
                    cause->nbt.field != NULL && !strcmp(cause->nbt.field, "Score") &&
                    cause->nbt.sleep_work != NULL && cause->nbt.sleep == NULL);
    mc_entity *p = load->player;
    mc_player_fields *fields = &p->local_player->fields.player;
    mc_world_info_player *saved = NULL;
    SLEEP_NBT_CHECK(!fields->u && fields->private_a == 0 && fields->g == 0 &&
                    fields->private_b != NULL && fields->private_b->x == 3 &&
                    fields->private_b->y == 64 && fields->private_b->z == 4 &&
                    mc_world_info_player_get(load->world->fields.x, &saved) == MC_WORLD_INFO_OK &&
                    saved != NULL && player_sleep_nbt_occurrences(load->world->fields.d, p) == 0 &&
                    mc_client_screen_transitions_inspect(f->app->screen_transitions, &transition) &&
                    transition.world_load != NULL);
    mc_entity_base_fields source;
    memcpy(&source, &p->base, sizeof(source));
    /* Borrow this actual transition-owned Load. terminal_records never frees
     * it; retiring an alias of transition.world_load would be invalid. */
    mc_client_world_load_call *call = (mc_client_world_load_call *)transition.world_load;
    registry->entered = 1;
    entered_changed = true;
    SLEEP_NBT_CHECK(mc_client_world_load_terminal_records(call) == MC_CLIENT_WORLD_LOAD_BUSY &&
                    mc_client_world_load_inspect(call, &current) &&
                    current.failure == load->failure && current.cleanup != NULL &&
                    current.cleanup->player_load == cause && cause->nbt.sleep_work != NULL &&
                    cause->nbt.sleep_cleanup != NULL &&
                    cause->nbt.sleep_cleanup->child == MC_ENTITY_NBT_SLEEP_CLEANUP_REGISTRY &&
                    cause->nbt.sleep_cleanup->registry.status == MC_REGISTRY_BUSY &&
                    cause->nbt.status == MC_ENTITY_NBT_WRONG_TAG &&
                    memcmp(&source, &p->base, sizeof(source)) == 0);
    registry->entered = entered;
    entered_changed = false;
    SLEEP_NBT_CHECK(mc_client_world_load_terminal_records(call) == MC_CLIENT_WORLD_LOAD_OK &&
                    cause->nbt.sleep_work == NULL && cause->nbt.sleep == NULL &&
                    cause->nbt.sleep_cleanup == NULL &&
                    cause->nbt.status == MC_ENTITY_NBT_WRONG_TAG && !cause->info_cleared &&
                    !cause->publication_returned && memcmp(&source, &p->base, sizeof(source)) == 0);
    mc_client_menu_loop *same = f->loop;
    SLEEP_NBT_CHECK(!mc_client_menu_loop_terminal_retire(&f->loop) && f->loop == same &&
                    terminal_source_diagnostic_same(f, &before) == 0 &&
                    !f->state.terminal_tail_entered && f->state.retirement_world != NULL &&
                    f->state.retirement_world->status == MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    /* This explicit legacy fixture cleanup is NOT successful normal terminal
     * coordinator admission of the failed Source graph. */
    SLEEP_NBT_CHECK(external != NULL && *external != NULL &&
                    !mc_client_menu_loop_retire(&f->loop) && f->loop == same &&
                    f->app->active_world == NULL && f->app->h == NULL && f->app->i == NULL &&
                    mc_client_screen_release(external, NULL) == MC_CLIENT_SCREEN_OK &&
                    *external == NULL && mc_client_menu_loop_retire(&f->loop) && f->loop == NULL);
    puts("SLEEP_NBT_LOAD_FAILURE actual-load324=1 fd88=1 gs93-returned=1 score-short=1 "
         "dc9-cast=1 awake=1 spawn-read=1 info-not-cleared=1 publication-unreached=1 "
         "entry-registry-refused=1 same-owner-retried=1 work-consumed=1 "
         "source-unreplayed=1 coordinator-refused=1 explicit-legacy-cleanup=1");
    result = 0;
cleanup:
    if (entered_changed)
        registry->entered = entered;
    return result;
}

#undef SLEEP_NBT_CHECK
#endif
