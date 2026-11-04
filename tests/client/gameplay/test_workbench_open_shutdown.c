/* A genuine SDK-open OO survives Source window quit into native preparation.
 * Native collection neither supplies Minecraft.d nor invents Source close. */
int mc_open_shutdown_included_auto_main(int, char **);
#define MC_WORKBENCH_AUTO_CLOSE_FIXTURE_MAIN mc_open_shutdown_included_auto_main
#include "test_workbench_auto_close.c"
#undef MC_WORKBENCH_AUTO_CLOSE_FIXTURE_MAIN

#include "client/display/sdk_update.h"
#include "core/java/random_profile_internal.h"
#include "entity/player/terminal_inventory.h"
#include "inventory/player/terminal_internal.h"

typedef struct shutdown_stack {
    const mc_item_stack *identity;
    mc_item_stack_fields fields;
} shutdown_stack;

typedef struct shutdown_statistic {
    const mc_statistic *key;
    int32_t value;
} shutdown_statistic;

typedef struct shutdown_random {
    const mc_java_random *identity;
    mc_java_random value;
} shutdown_random;

typedef struct shutdown_snapshot {
    mc_client_source_da *screen;
    mc_entity *player;
    mc_inventory_player *inventory;
    mc_container *default_container, *active_container, *screen_container;
    int32_t selected;
    bool changed, inherited_closed, direct_closed;
    size_t source_calls, stack_count, entity_count, random_count;
    shutdown_stack stacks[128];
    const mc_entity *entities[512];
    shutdown_random randoms[520];
    uint64_t uniquifier, math_state;
    unsigned int math_phase;
    int64_t world_time;
    mc_client_stats_state statistics;
    shutdown_statistic values[2][128];
    uint64_t ticks, world_ticks, drops, publications;
} shutdown_snapshot;

static int shutdown_stack_read(shutdown_snapshot *out, const mc_item_stack *stack) {
    CHECK(out->stack_count < sizeof(out->stacks) / sizeof(out->stacks[0]));
    shutdown_stack *entry = &out->stacks[out->stack_count++];
    entry->identity = stack;
    if (stack != NULL)
        CHECK(mc_item_stack_fields_get(stack, &entry->fields) == MC_ITEM_STACK_OK);
    return 0;
}

static int shutdown_random_read(shutdown_snapshot *out, const mc_java_random *random) {
    if (random != NULL) {
        CHECK(out->random_count < sizeof(out->randoms) / sizeof(out->randoms[0]));
        shutdown_random *entry = &out->randoms[out->random_count++];
        entry->identity = random;
        entry->value = *random;
    }
    return 0;
}

static int shutdown_read(fixture *f, mc_client_source_id *o, shutdown_snapshot *out) {
    memset(out, 0, sizeof(*out));
    mc_entity *player = f->app->h;
    mc_inventory_player_state inventory;
    mc_container *it = o->id.j;
    mc_container_state container;
    mc_world_info_fields info;
    CHECK(player != NULL && player->local_player != NULL &&
          mc_inventory_player_read_state(player->local_player->fields.player.c, &inventory) &&
          mc_container_read_state(it, &container) &&
          mc_world_info_fields_get(f->app->active_world->fields.x, &info) == MC_WORLD_INFO_OK &&
          mc_client_menu_loop_inspect(f->loop, &f->state));
    out->screen = f->app->r;
    out->player = player;
    out->inventory = player->local_player->fields.player.c;
    out->default_container = player->local_player->fields.player.d;
    out->active_container = player->local_player->fields.player.e;
    out->screen_container = o->id.j;
    out->selected = inventory.selected;
    out->changed = inventory.changed;
    out->inherited_closed = o->inherited_closed_returned;
    out->direct_closed = o->direct_closed_returned;
    out->source_calls = o->source_calls;
    for (size_t i = 0; i < 40; ++i)
        CHECK(shutdown_stack_read(out, i < 36 ? inventory.main_slots[i]
                                              : inventory.armor_slots[i - 36]) == 0);
    CHECK(shutdown_stack_read(out, inventory.cursor) == 0);
    for (int32_t i = 0; i < 9; ++i) {
        const mc_item_stack *stack = NULL;
        CHECK(mc_inventory_crafting_borrow_slot(container.crafting, i, &stack, NULL) ==
                  MC_INVENTORY_OK &&
              shutdown_stack_read(out, stack) == 0);
    }
    const mc_item_stack *result = NULL;
    CHECK(mc_inventory_result_borrow(container.result, 0, &result, NULL) == MC_INVENTORY_OK &&
          shutdown_stack_read(out, result) == 0 && it->snapshots != NULL &&
          it->snapshots->size <= 46);
    for (size_t i = 0; i < it->snapshots->size; ++i)
        CHECK(shutdown_stack_read(out, it->snapshots->values[i]) == 0);
    const mc_world_reference_list *entities = f->app->active_world->fields.b;
    CHECK(entities != NULL && entities->count <= entities->capacity && entities->count <= 512);
    out->entity_count = entities->count;
    for (size_t i = 0; i < entities->count; ++i) {
        const mc_entity *entity = entities->items[i];
        out->entities[i] = entity;
        if (entity != NULL)
            CHECK(shutdown_random_read(out, entity->random) == 0);
    }
    const mc_client_item_renderer_fields *item =
        mc_client_item_renderer_fields_get(o->base.classes->inventory_renderer);
    CHECK(item != NULL && shutdown_random_read(out, player->random) == 0 &&
          shutdown_random_read(out, f->app->active_world->fields.r) == 0 &&
          shutdown_random_read(out, o->base.classes->a) == 0 &&
          shutdown_random_read(out, item->f) == 0 &&
          mc_client_stats_writer_read_state(f->app->stats_writer, &out->statistics));
    const mc_client_stats_map *maps[] = {out->statistics.total_identity,
                                         out->statistics.unsent_identity};
    for (size_t map = 0; map < 2; ++map) {
        const size_t count = mc_client_stats_map_size(maps[map]);
        CHECK(count <= 128 &&
              count == (map == 0 ? out->statistics.total_count : out->statistics.unsent_count));
        for (size_t i = 0; i < count; ++i)
            CHECK(mc_client_stats_map_entry(maps[map], i, &out->values[map][i].key,
                                            &out->values[map][i].value,
                                            NULL) == MC_CLIENT_STATS_OK);
    }
    out->uniquifier = atomic_load(&f->app->profile->uniquifier);
    out->math_phase = atomic_load(&f->app->profile->holder_phase);
    if (f->app->profile->math != NULL)
        out->math_state = atomic_load(&f->app->profile->math->state);
    out->world_time = info.value.time;
    out->ticks = f->state.ticks;
    out->world_ticks = f->state.world_tick_returns;
    out->drops = f->state.gameplay_drop_calls;
    out->publications = f->state.gameplay_drop_publications;
    return 0;
}

static int shutdown_getters_revoked(fixture *f) {
    CHECK(mc_client_bootstrap_app(f->bootstrap) == NULL &&
          mc_client_bootstrap_frame(f->bootstrap) == NULL &&
          mc_client_bootstrap_resources(f->bootstrap) == NULL &&
          mc_client_bootstrap_class_files(f->bootstrap) == NULL &&
          mc_client_bootstrap_trig(f->bootstrap) == NULL &&
          mc_client_bootstrap_math(f->bootstrap) == NULL &&
          mc_client_bootstrap_graphics(f->bootstrap) == NULL);
    return 0;
}

static int shutdown_owner(fixture *f, mc_client_source_id *o, const shutdown_snapshot *before,
                          bool sealed) {
    mc_inventory_player_terminal record = {0};
    mc_local_player_snapshot player;
    mc_container_state container;
    CHECK(mc_player_terminal_inventory_inspect(before->player, &record) &&
          mc_local_player_get_snapshot(before->player, &player) == MC_ENTITY_OK &&
          player.player.c == before->inventory && player.player.d == before->default_container &&
          player.player.e == before->active_container && f->app->r == before->screen &&
          o->id.j == before->screen_container &&
          mc_container_read_state(before->screen_container, &container) && container.constructed &&
          container.kind == MC_CONTAINER_WORKBENCH &&
          container.player_inventory == before->inventory &&
          container.world == f->app->active_world);
    if (sealed) {
        CHECK(record.sealed && record.player == before->player &&
              record.inventory == before->inventory &&
              record.default_container == before->default_container &&
              record.active_container == before->screen_container &&
              o->captured_container == NULL && !o->container_owned &&
              o->base.phase == MC_CLIENT_SCREEN_RETIRING &&
              mc_local_player_terminal_sealed(before->player));
        mc_client_workbench_screen_state screen;
        CHECK(mc_client_screen_workbench_inspect((mc_client_source_oo *)(void *)o, &screen, NULL) ==
                  MC_CLIENT_SCREEN_OK &&
              screen.id.j == NULL && screen.constructor_container == NULL);
    } else
        CHECK(!record.sealed && record.active_container == NULL &&
              o->captured_container == before->screen_container && o->container_owned &&
              !mc_local_player_terminal_sealed(before->player));
    return 0;
}

static int shutdown_denials(fixture *f, const shutdown_snapshot *before, const char *profile) {
    mc_inventory_error inventory = {0};
    mc_client_screen_error screen = {0};
    CHECK(shutdown_getters_revoked(f) == 0 && mc_entity_retain(before->player) == MC_ENTITY_BUSY &&
          mc_container_retain(before->screen_container, &inventory) == MC_INVENTORY_BUSY &&
          mc_inventory_player_set(before->inventory, 0,
                                  (mc_item_stack *)(void *)before->stacks[0].identity,
                                  &inventory) == MC_INVENTORY_BUSY &&
          mc_inventory_player_source_set(before->inventory, 0,
                                         (mc_item_stack *)(void *)before->stacks[0].identity,
                                         &inventory) == MC_INVENTORY_BUSY &&
          mc_client_screen_transitions_inventory(f->app->screen_transitions, &screen) !=
              MC_CLIENT_SCREEN_OK &&
          !screen.source_exception && !screen.source_error && !mc_client_menu_loop_frame(f->loop));
    printf("WORKBENCH_OPEN_DENIALS profile=%s getters-revoked=1 player-retain=busy "
           "container-retain=busy ix-set=busy source-set=busy router=refused frame=refused\n",
           profile);
    return 0;
}

static void shutdown_prepare_failure(fixture *f, mc_client_source_id *o) {
    mc_client_screen_error diagnostic = {0};
    const mc_client_screen_status retried =
        mc_client_screen_transitions_terminal_prepare(f->app->screen_transitions, &diagnostic);
    fprintf(stderr,
            "WORKBENCH_OPEN_PREPARE_FAILURE screen=%s.%s%s:%u status=%d operation=%s "
            "r=%p e=%p j=%p owner=%p container-owned=%d phase=%d holds=%zu retry-status=%d\n",
            diagnostic.site.class_name == NULL ? "native" : diagnostic.site.class_name,
            diagnostic.site.method == NULL ? "none" : diagnostic.site.method,
            diagnostic.site.descriptor == NULL ? "" : diagnostic.site.descriptor,
            diagnostic.site.pc, (int)diagnostic.status,
            diagnostic.operation == NULL ? "unknown" : diagnostic.operation, (void *)f->app->r,
            (void *)f->app->h->local_player->fields.player.e, (void *)o->id.j,
            (void *)o->captured_container, (int)o->container_owned, (int)o->base.phase,
            o->base.holds, (int)retried);
}

static bool shutdown_retire(fixture *f) {
    const bool returned = mc_client_menu_loop_terminal_retire(&f->loop);
    if (!returned && f->loop != NULL && mc_client_menu_loop_inspect(f->loop, &f->state))
        fprintf(stderr,
                "WORKBENCH_OPEN_RETIRE refused bootstrap=%d world-status=%d tail=%d "
                "operation=%s\n",
                (int)f->state.retirement_bootstrap,
                f->state.retirement_world == NULL ? -1 : (int)f->state.retirement_world->status,
                (int)f->state.terminal_tail_entered,
                f->state.retirement_operation == NULL ? "unknown" : f->state.retirement_operation);
    return returned;
}

static bool shutdown_join(fixture *f, const char *profile) {
    if (f->bootstrap == NULL)
        return false;
    const uintptr_t owner = (uintptr_t)f->bootstrap;
    mc_client_bootstrap_status status = MC_CLIENT_BOOTSTRAP_BUSY;
    unsigned int attempts = 0, busy = 0;
    /* Match the ordinary client's native stop/join protocol. BUSY retains the
     * same graph; no expired App/World view or Source entry is used here. */
    while (status == MC_CLIENT_BOOTSTRAP_BUSY && attempts < 35000) {
        ++attempts;
        status = mc_client_bootstrap_close(&f->bootstrap);
        if (status == MC_CLIENT_BOOTSTRAP_BUSY) {
            ++busy;
            if (f->bootstrap == NULL || (uintptr_t)f->bootstrap != owner)
                break;
            SDL_Delay(1);
        }
    }
    printf(
        "WORKBENCH_OPEN_JOIN profile=%s attempts=%u busy=%u limit=35000 status=%d slot-null=%d\n",
        profile, attempts, busy, (int)status, (int)(f->bootstrap == NULL));
    return status == MC_CLIENT_BOOTSTRAP_OK && f->bootstrap == NULL;
}

static int shutdown_setup(fixture *f, mc_client_source_id **out) {
    mining_work *m = &workbench->mining;
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && auto_choose(f, m) == 0 && auto_world_ready(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 15000;
    while (!wb_hit(f, 58, 2) && SDL_GetTicks() < deadline)
        CHECK(inventory_frame(f) == 0);
    CHECK(auto_open(f, out) == 0 && *out != NULL &&
          wb_click(f, *out, 10, false, false, 17, 2) == 0 &&
          wb_click(f, *out, 1, false, false, 0, 0) == 0 && wb_slot(*out, 1, 17, 2) == 0 &&
          wb_slot(*out, 0, 5, 4) == 0 && wb_click(f, *out, 11, false, false, 5, 7) == 0 &&
          mining_capture(f, m, "workbench-open-shutdown.png") == 0);
    mc_container_state state;
    mc_inventory_slot *slot = NULL;
    const mc_item_stack *grid = NULL, *result = NULL, *slot_value = NULL;
    CHECK(mc_container_read_state((*out)->id.j, &state) &&
          mc_inventory_crafting_borrow_slot(state.crafting, 0, &grid, NULL) == MC_INVENTORY_OK &&
          mc_container_source_slot((*out)->id.j, 1, &slot, NULL) == MC_INVENTORY_OK &&
          mc_inventory_slot_source_get(slot, &slot_value, NULL) == MC_INVENTORY_OK &&
          grid == slot_value && grid != NULL &&
          mc_inventory_result_borrow(state.result, 0, &result, NULL) == MC_INVENTORY_OK &&
          mc_container_source_slot((*out)->id.j, 0, &slot, NULL) == MC_INVENTORY_OK &&
          mc_inventory_slot_source_get(slot, &slot_value, NULL) == MC_INVENTORY_OK &&
          result == slot_value && result != NULL && result != grid);
    puts("WORKBENCH_OPEN_SETUP sdk-open=1 sdk-grid=1 sdk-cursor=1 result-preview=1 "
         "actual-aliases=1");
    return 0;
}

static int shutdown_run(fixture *f, uint32_t frame_id, const char *profile) {
    mc_client_source_id *o = NULL;
    CHECK(shutdown_setup(f, &o) == 0);
    SDL_Event event = {0};
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    event.window.windowID = frame_id;
    CHECK(SDL_PushEvent(&event));
    /* Window callbacks use the real Display process-messages owner. A close
     * request is not a Minecraft keyboard/mouse record. */
    mc_client_sdk_update_error update = {0};
    for (unsigned int i = 0; i < 100 && atomic_load(&f->app->J); ++i) {
        CHECK(
            mc_client_sdk_display_process_messages(mc_client_display_update_owner(f->app->display),
                                                   &update) == MC_CLIENT_SDK_UPDATE_OK);
        SDL_Delay(2);
    }
    CHECK(!atomic_load(&f->app->J) && mc_client_menu_loop_frame(f->loop));
    mc_client_frame_state frame_state;
    CHECK(!atomic_load(&f->app->J) && mc_client_menu_loop_inspect(f->loop, &f->state) &&
          !f->state.running && !f->state.failed && f->app->r == &o->base &&
          mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &frame_state) &&
          frame_state.close_dispatched && !o->inherited_closed_returned &&
          !o->direct_closed_returned && reload_snapshot("open-stopped") == 0);
    shutdown_snapshot *before = calloc(1, sizeof(*before)), *after = calloc(1, sizeof(*after));
    CHECK(before != NULL && after != NULL && shutdown_read(f, o, before) == 0);
    const bool container_hold = strcmp(profile, "container") == 0;
    const bool player_hold = strcmp(profile, "player") == 0;
    const bool world_hold = strcmp(profile, "world") == 0;
    mc_world_runtime *world = f->app->active_world;
    if (container_hold)
        CHECK(mc_container_retain(before->screen_container, NULL) == MC_INVENTORY_OK);
    if (player_hold)
        CHECK(mc_entity_retain(before->player) == MC_ENTITY_OK);
    if (world_hold)
        CHECK(mc_world_runtime_binding_retain(world));
    bool prepared = mc_client_menu_loop_terminal_prepare(f->loop);
    CHECK(shutdown_read(f, o, after) == 0);
    CHECK(memcmp(before, after, sizeof(*before)) == 0 && shutdown_getters_revoked(f) == 0);
    if (container_hold) {
        CHECK(!prepared && shutdown_owner(f, o, before, false) == 0 &&
              before->screen_container->interaction_holds == 1 &&
              reload_snapshot("open-container-busy") == 0);
        printf("WORKBENCH_OPEN_BUSY profile=container phase=pre-transfer source-unchanged=1 "
               "native-owner-unchanged=1\n");
        CHECK(mc_container_release(before->screen_container, NULL) == MC_INVENTORY_OK);
        prepared = mc_client_menu_loop_terminal_prepare(f->loop);
    }
    if (!prepared)
        shutdown_prepare_failure(f, o);
    CHECK(prepared && shutdown_owner(f, o, before, true) == 0 &&
          shutdown_denials(f, before, profile) == 0 && shutdown_read(f, o, after) == 0 &&
          memcmp(before, after, sizeof(*before)) == 0 &&
          reload_snapshot("open-after-prepare") == 0);
    printf("WORKBENCH_OPEN_PREPARE profile=%s returned=1 source-unchanged=1 world-ticks=%" PRIu64
           " stacks=%zu entities=%zu randoms=%zu r=%p e=%p j=%p\n",
           profile, after->world_ticks - before->world_ticks, after->stack_count,
           after->entity_count, after->random_count, (void *)after->screen,
           (void *)after->active_container, (void *)after->screen_container);
    CHECK(mc_client_menu_loop_terminal_prepare(f->loop) &&
          shutdown_owner(f, o, before, true) == 0 && shutdown_read(f, o, after) == 0 &&
          memcmp(before, after, sizeof(*before)) == 0);
    printf("WORKBENCH_OPEN_TRANSFER profile=%s same-it=1 source-aliases=1 sealed=1 owner-moved=1 "
           "idempotent=1\n",
           profile);
    if (player_hold || world_hold) {
        CHECK(!shutdown_retire(f) && f->loop != NULL &&
              mc_client_menu_loop_inspect(f->loop, &f->state) && !f->state.terminal_tail_entered &&
              f->state.retirement_bootstrap == MC_CLIENT_BOOTSTRAP_OK &&
              f->state.retirement_world != NULL &&
              f->state.retirement_world->status == MC_WORLD_STORAGE_BUSY &&
              shutdown_owner(f, o, before, true) == 0 && shutdown_read(f, o, after) == 0 &&
              memcmp(before, after, sizeof(*before)) == 0 &&
              reload_snapshot("open-audit-busy") == 0);
        printf("WORKBENCH_OPEN_BUSY profile=%s phase=world-audit source-unchanged=1 same-it=1 "
               "status=busy\n",
               profile);
        if (player_hold)
            mc_entity_release(before->player);
        if (world_hold)
            mc_world_runtime_binding_release(world);
    }
    CHECK(shutdown_owner(f, o, before, true) == 0 && shutdown_read(f, o, after) == 0 &&
          memcmp(before, after, sizeof(*before)) == 0 &&
          reload_snapshot("open-after-release") == 0);
    printf("WORKBENCH_OPEN_RELEASE profile=%s existing-hold-only=1 released=%s same-it=1\n",
           profile, strcmp(profile, "clean") == 0 ? "none" : profile);
    free(after);
    free(before);
    printf("WORKBENCH_OPEN_TERMINAL profile=%s native-prepare=1 source-close=0 source-save=0 "
           "unchanged=1\n",
           profile);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4 && (strcmp(argv[3], "clean") == 0 || strcmp(argv[3], "container") == 0 ||
                        strcmp(argv[3], "player") == 0 || strcmp(argv[3], "world") == 0));
    fixture *f = calloc(1, sizeof(*f));
    workbench = calloc(1, sizeof(*workbench));
    CHECK(f != NULL && workbench != NULL);
    mining_fixture = f;
    workbench->mining.directory = argv[2];
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = 854,
                                                   .height = 480,
                                                   .hidden = false};
    int result = 1;
    uint32_t frame_id = 0, canvas_id = 0;
    if (mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK) {
        f->app = mc_client_bootstrap_app(f->bootstrap);
        mc_client_frame_state host;
        if (f->app != NULL &&
            mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
            host.frame_window != NULL && host.canvas_window != NULL) {
            frame_id = SDL_GetWindowID(host.frame_window);
            canvas_id = SDL_GetWindowID(host.canvas_window);
            result = shutdown_run(f, frame_id, argv[3]);
        }
    }
    if (result != 0) {
        /* The actual failing root remains allocated for diagnostics until
         * process exit. No Escape, source close or fallback free is supplied. */
        if (f->app != NULL && f->app->display != NULL) {
            native_focus_receipt(f, "open-shutdown-failure");
            save_title_resume_diagnostics(f, "open-shutdown-failure");
        }
        fflush(stdout);
        fflush(stderr);
        return result;
    }
    CHECK(shutdown_retire(f) && f->loop == NULL && shutdown_join(f, argv[3]) &&
          f->bootstrap == NULL && SDL_GetWindowFromID(frame_id) == NULL &&
          SDL_GetWindowFromID(canvas_id) == NULL &&
          reload_snapshot("open-after-native-close") == 0);
    printf("WORKBENCH_OPEN_FINISH profile=%s native-cleanup=1 windows-absent=1 source-close=0 "
           "source-save=0 checks=%u\n",
           argv[3], checks);
    free(workbench);
    free(f);
    return 0;
}
