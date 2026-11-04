/* SDK setup followed by one declared native integration input and the actual
 * client World tick. The automatic close is never an invented GUI event. */
#include <stdbool.h>
static int auto_statistics_controls(const void *, bool);
#define MC_WORLD_SELECTION_STATISTICS_CONTROLS(f, initial) auto_statistics_controls((f), (initial))
int mc_auto_close_included_workbench_main(int, char **);
#define MC_WORKBENCH_FIXTURE_MAIN mc_auto_close_included_workbench_main
#include "test_workbench.c"
#undef MC_WORKBENCH_FIXTURE_MAIN

#include "client/runtime/world_tick.h"
#include "mc/world/runtime/mutations.h"
#include "mc/world/runtime/world_access.h"

typedef struct auto_close_work {
    const char *mode;
    mc_client_world_tick tick;
    mc_world_access *observer;
    mc_world_runtime *world;
    mc_entity *player;
    const mc_item_stack *expected[2];
    mc_entity *published[2];
    size_t publication_count;
    bool publication_bad, observer_registered;
} auto_close_work;

static auto_close_work *automatic;

static int auto_statistics_controls(const void *context, bool initial) {
    const fixture *f = context;
    if (initial)
        return wb_statistics_controls(context, true);
    /* These declared saved Items are moved, not crafted. They must not earn
     * the crafting achievements required by the separate progression scene. */
    CHECK(wb_achievement(f, 0, 1 + inventory_opened) == 0 && wb_achievement(f, 1, 1) == 0 &&
          wb_achievement(f, 2, 0) == 0 && wb_achievement(f, 3, 0) == 0);
    return 0;
}

static mc_world_storage_status publication(void *context, mc_world_runtime *world,
                                           mc_entity *entity, mc_world_storage_error *error) {
    auto_close_work *w = context;
    if (entity != NULL && mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM))
        for (size_t i = 0; i < 2; ++i)
            if (entity->stack == w->expected[i]) {
                if (w->publication_count != i || w->published[i] != NULL || world != w->world ||
                    w->player->local_player->fields.player.e !=
                        w->player->local_player->fields.player.d)
                    w->publication_bad = true;
                w->published[i] = entity;
                ++w->publication_count;
            }
    if (error != NULL)
        *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

/* This additional authored observer accepts unrelated notifications. The
 * actual game WorldAccess receivers remain registered and execute. */
static mc_world_storage_status ignore_cell(void *c, mc_world_runtime *w, int32_t x, int32_t y,
                                           int32_t z, mc_world_storage_error *e) {
    (void)c;
    (void)w;
    (void)x;
    (void)y;
    (void)z;
    *e = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status ignore_range(void *c, mc_world_runtime *w, int32_t x0, int32_t y0,
                                            int32_t z0, int32_t x1, int32_t y1, int32_t z1,
                                            mc_world_storage_error *e) {
    (void)x1;
    (void)y1;
    (void)z1;
    return ignore_cell(c, w, x0, y0, z0, e);
}

static mc_world_storage_status ignore_sound(void *c, mc_world_runtime *w,
                                            const mc_registry_string *name, double x, double y,
                                            double z, float volume, float pitch,
                                            mc_world_storage_error *e) {
    (void)name;
    (void)x;
    (void)y;
    (void)z;
    (void)volume;
    (void)pitch;
    return ignore_cell(c, w, 0, 0, 0, e);
}

static mc_world_storage_status ignore_particle(void *c, mc_world_runtime *w,
                                               const mc_registry_string *name, double x, double y,
                                               double z, double dx, double dy, double dz,
                                               mc_world_storage_error *e) {
    (void)dx;
    (void)dy;
    (void)dz;
    return ignore_sound(c, w, name, x, y, z, 0, 0, e);
}

static mc_world_storage_status ignore_entity(void *c, mc_world_runtime *w, mc_entity *entity,
                                             mc_world_storage_error *e) {
    (void)entity;
    return ignore_cell(c, w, 0, 0, 0, e);
}

static mc_world_storage_status ignore_tile(void *c, mc_world_runtime *w, int32_t x, int32_t y,
                                           int32_t z, mc_tile_entity *tile,
                                           mc_world_storage_error *e) {
    (void)tile;
    return ignore_cell(c, w, x, y, z, e);
}

static mc_world_storage_status ignore_sky(void *c, mc_world_runtime *w, mc_world_storage_error *e) {
    return ignore_cell(c, w, 0, 0, 0, e);
}

static mc_world_storage_status ignore_auxiliary(void *c, mc_world_runtime *w, mc_entity *p,
                                                int32_t effect, int32_t x, int32_t y, int32_t z,
                                                int32_t data, mc_world_storage_error *e) {
    (void)p;
    (void)effect;
    (void)data;
    return ignore_cell(c, w, x, y, z, e);
}

static int auto_observer_remove(void) {
    if (automatic->observer != NULL) {
        if (automatic->observer_registered) {
            bool removed = false;
            CHECK(mc_world_runtime_remove_world_access(automatic->world, automatic->observer,
                                                       &removed, NULL) == MC_WORLD_STORAGE_OK &&
                  removed);
            automatic->observer_registered = false;
        }
        CHECK(mc_world_access_free(automatic->observer, NULL) == MC_WORLD_STORAGE_OK);
        automatic->observer = NULL;
    }
    return 0;
}

static int auto_resume(fixture *f) {
    mc_client_source_da *recovered[4] = {0};
    size_t focus_pauses = 0, eligible;
    int64_t world_time;
    CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &focus_pauses, &eligible,
                            &world_time) == 0);
    return 0;
}

static int auto_choose(fixture *f, mining_work *work) {
    mc_client_source_rq *rq = NULL;
    CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          open_selection(f, f->app->r, &rq) == 0 && release_mouse(f, rq) == 0);
    int32_t index = -1;
    for (size_t i = 0; i < mc_client_save_list_size(rq->fields.n); ++i)
        if (filename_is(mc_client_save_list_at(rq->fields.n, i), "alpha")) {
            CHECK(index == -1);
            index = (int32_t)i;
        }
    CHECK(index >= 0);
    const mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + index * 36 + 8;
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, true) == 0 && transfer(f) == 0);
    const uint64_t selection_deadline = SDL_GetTicks() + 5000;
    while (rq->fields.m != index && SDL_GetTicks() < selection_deadline)
        CHECK(frame(f) == 0);
    CHECK(rq->fields.m == index && release_mouse(f, rq) == 0 &&
          push_button(f, &rq->base, 1, true) == 0 && push_button(f, &rq->base, 1, false) == 0 &&
          transfer(f) == 0);
    /* The mining observer requires held Dirt. This scene starts with an
     * authored Pickaxe and uses the ordinary source frame/focus path instead. */
    const uint64_t deadline = SDL_GetTicks() + 180000;
    for (size_t frames = 0; frames < 180 && SDL_GetTicks() < deadline; ++frames) {
        CHECK(save_title_focus(f) == 0 && frame(f) == 0);
        ++work->frames;
        if (f->app->active_world == NULL)
            continue;
        CHECK(auto_resume(f) == 0 && reload_capture_epoch(f, &work->epochs[0]) == 0);
        return 0;
    }
    return 1;
}

static int auto_world_ready(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 15000;
    while (SDL_GetTicks() < deadline) {
        CHECK(inventory_frame(f) == 0);
        if (f->app->r != NULL || !f->app->N) {
            CHECK(auto_resume(f) == 0);
            continue;
        }
        const mc_client_render_global_entities_state *entities =
            f->state.world_frame == NULL ? NULL : f->state.world_frame->entities;
        if (entities == NULL || entities->delayed || !entities->th_returned)
            continue;
        mc_client_manager_state manager;
        CHECK(f->app->render_classes != NULL &&
              mc_client_render_manager_inspect(f->app->render_classes->th, &manager));
        const bool ready =
            manager.source_views_available && manager.phase == MC_CLIENT_MANAGER_READY &&
            manager.a != NULL && manager.a == manager.candidate && manager.last_frame_pc >= 0 &&
            manager.fields.e == f->app->render_engine && manager.fields.g == f->app->active_world &&
            manager.fields.h == f->app->i && manager.fields.k == f->app->settings &&
            manager.fields.p == f->app->font_renderer && f->app->r == NULL;
        if (!ready)
            fprintf(stderr,
                    "Automatic-close warm owner views%d phase%d engine%d world%d camera%d "
                    "settings%d font%d screen%d\n",
                    (int)manager.source_views_available, (int)manager.phase,
                    manager.fields.e == f->app->render_engine,
                    manager.fields.g == f->app->active_world, manager.fields.h == f->app->i,
                    manager.fields.k == f->app->settings, manager.fields.p == f->app->font_renderer,
                    f->app->r == NULL ? -1 : (int)f->app->r->kind);
        CHECK(ready);
        puts(
            "WORKBENCH_WORLD_READY source-N-TH-frame=1 engine-camera-settings=1 no-field-repair=1");
        return 0;
    }
    return 1;
}

typedef struct auto_input_observation {
    mc_client_sdk_input_state input;
    mc_client_sdk_bytes_state read, queue;
    bool press_unread, release_unread, press_queued, release_queued;
} auto_input_observation;

static bool auto_input_record(const uint8_t *record, uint64_t nanos, bool pressed) {
    if (nanos == 0 || record[0] != 1 || record[1] != (pressed ? 1 : 0))
        return false;
    uint64_t timestamp = 0;
    for (size_t i = 14; i < 22; ++i)
        timestamp = (timestamp << 8) | record[i];
    return timestamp == nanos;
}

static int auto_input_observe(fixture *f, uint64_t press, uint64_t release,
                              auto_input_observation *out) {
    *out = (auto_input_observation){0};
    CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &out->input) &&
          !out->input.active && mc_client_sdk_bytes_inspect(out->input.mouse_read, &out->read) &&
          mc_client_sdk_queue_inspect(out->input.mouse_events, &out->queue) &&
          out->read.position <= out->read.limit && out->read.limit <= out->read.capacity &&
          out->read.position % 22 == 0 && out->read.limit % 22 == 0 &&
          out->queue.position <= out->queue.limit && out->queue.limit <= out->queue.capacity &&
          out->queue.position % 22 == 0);
    /* Inspect only live ByteBuffer ranges. Never advance a source position,
     * poll/clear either queue or change the timestamp of a retained event. */
    for (size_t i = out->read.position; i < out->read.limit; i += 22) {
        out->press_unread |= auto_input_record(out->read.data + i, press, true);
        out->release_unread |= auto_input_record(out->read.data + i, release, false);
    }
    for (size_t i = 0; i < out->queue.position; i += 22) {
        out->press_queued |= auto_input_record(out->queue.data + i, press, true);
        out->release_queued |= auto_input_record(out->queue.data + i, release, false);
    }
    return 0;
}

static int auto_input_receipt(fixture *f, const char *stage, unsigned int attempt, uint64_t press,
                              uint64_t release) {
    auto_input_observation state;
    CHECK(auto_input_observe(f, press, release, &state) == 0);
    mc_entity_hit_result_view hit = {0};
    mc_entity_hit_kind_class_view kinds = {0};
    const bool represented =
        f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
        mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK;
    const int kind = !represented              ? -1
                     : hit.fields.a == kinds.a ? 0
                     : hit.fields.a == kinds.b ? 1
                                               : 2;
    const mc_entity *player = f->app->h;
    printf("WORKBENCH_AUTO_QUEUE stage=%s attempt=%u read=%zu/%zu/%zu queue=%zu/%zu/%zu "
           "full=%zu current-button=%" PRId32 " current-pressed=%d current-nanos=%" PRId64
           " press=%" PRIu64 " release=%" PRIu64 " press-unread=%d release-unread=%d "
           "press-queued=%d release-queued=%d hit=%d hit-x=%" PRId32 " hit-y=%" PRId32
           " hit-z=%" PRId32 " hit-face=%" PRId32 " player=%d x=%.17g y=%.17g z=%.17g "
           "yaw=%.9g pitch=%.9g\n",
           stage, attempt, state.read.position, state.read.limit, state.read.capacity,
           state.queue.position, state.queue.limit, state.queue.capacity,
           state.input.source_queue_full_results, state.input.current_mouse.button,
           (int)state.input.current_mouse.pressed, state.input.current_mouse.nanos, press, release,
           (int)state.press_unread, (int)state.release_unread, (int)state.press_queued,
           (int)state.release_queued, kind, hit.fields.b, hit.fields.c, hit.fields.d, hit.fields.e,
           player != NULL, player == NULL ? 0.0 : player->base.x,
           player == NULL ? 0.0 : player->base.y, player == NULL ? 0.0 : player->base.z,
           player == NULL ? 0.0 : (double)player->base.yaw,
           player == NULL ? 0.0 : (double)player->base.aT);
    CHECK(fflush(stdout) == 0);
    return 0;
}

static int auto_open(fixture *f, mc_client_source_id **out) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (unsigned int attempt = 0; attempt < 12 && SDL_GetTicks() < deadline; ++attempt) {
        if (f->app->r != NULL || !f->app->N)
            CHECK(auto_resume(f) == 0);
        CHECK(auto_input_receipt(f, "before-drain", attempt + 1, 0, 0) == 0);
        /* Native focus/setup motion may precede the tap in both original SDK
         * buffers. Let actual source frames consume that history before
         * appending one new tap. A batch of motion is not a tap receipt. */
        auto_input_observation empty;
        CHECK(auto_input_observe(f, 0, 0, &empty) == 0);
        while ((empty.read.position != empty.read.limit || empty.queue.position != 0) &&
               SDL_GetTicks() < deadline) {
            CHECK(inventory_frame(f) == 0);
            if (f->app->r != NULL || !f->app->N)
                CHECK(auto_resume(f) == 0);
            CHECK(auto_input_observe(f, 0, 0, &empty) == 0);
        }
        CHECK(auto_input_receipt(f, "before-tap", attempt + 1, 0, 0) == 0);
        CHECK(empty.read.position == empty.read.limit && empty.queue.position == 0);
        CHECK(wb_hit(f, 58, 2));
        uint64_t timestamps[2];
        for (unsigned int i = 0; i < 2; ++i) {
            SDL_Event event = {0};
            event.type = i == 0 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            timestamps[i] = SDL_GetTicksNS();
            event.common.timestamp = timestamps[i];
            event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
            event.button.button = SDL_BUTTON_RIGHT;
            event.button.down = i == 0;
            CHECK(SDL_PushEvent(&event));
        }
        CHECK(transfer(f) == 0);
        CHECK(auto_input_receipt(f, "transferred", attempt + 1, timestamps[0], timestamps[1]) == 0);
        const size_t events = f->state.mouse_events;
        const uint64_t clicks = f->state.gameplay_right_clicks;
        size_t frames = 0;
        bool opened = false, press_consumed = false, release_consumed = false;
        while (SDL_GetTicks() < deadline) {
            auto_input_observation before, after;
            CHECK(auto_input_observe(f, timestamps[0], timestamps[1], &before) == 0);
            const size_t before_events = f->state.mouse_events;
            const size_t unread = (before.read.limit - before.read.position) / 22;
            CHECK(inventory_frame(f) == 0);
            ++frames;
            CHECK(auto_input_observe(f, timestamps[0], timestamps[1], &after) == 0);
            /* A complete gameplay Mouse.next loop drains its prior read range
             * before Display.update refills it. The cumulative source counter
             * proves that actual drain, even if a later motion is current. */
            if (unread != 0 && f->state.mouse_events - before_events >= unread) {
                press_consumed |= before.press_unread;
                release_consumed |= before.release_unread;
            }
            if (after.input.current_mouse.button == 1) {
                press_consumed |= after.input.current_mouse.pressed &&
                                  (uint64_t)after.input.current_mouse.nanos == timestamps[0];
                release_consumed |= !after.input.current_mouse.pressed &&
                                    (uint64_t)after.input.current_mouse.nanos == timestamps[1];
            }
            if (f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_WORKBENCH) {
                opened = true;
                break;
            }
            if (f->app->r != NULL) {
                /* Recover an actual focus-loss Pause through its SDK button.
                 * An unexpected screen remains a failure in auto_resume. */
                CHECK(auto_resume(f) == 0);
                break;
            }
            if (f->state.gameplay_right_clicks > clicks || release_consumed)
                break; /* Motion-only batches never authorize another tap. */
        }
        int64_t now;
        CHECK(mc_platform_current_time_millis(&now) == MC_PLATFORM_MILLIS_OK);
        const mc_client_gameplay_input *input = f->state.gameplay_input;
        printf("WORKBENCH_AUTO_INPUT attempt=%u frames=%zu mouse-events=%zu right-clicks=%" PRIu64
               " stale-last-tick=%zu opened=%d source-focus=%d input-age=%" PRId64
               " press-consumed=%d release-consumed=%d\n",
               attempt + 1, frames, f->state.mouse_events - events,
               f->state.gameplay_right_clicks - clicks,
               input == NULL ? 0 : input->stale_mouse_events, (int)opened, (int)f->app->N,
               mc_java_i64_sub(now, f->app->P), (int)press_consumed, (int)release_consumed);
        CHECK(fflush(stdout) == 0);
        CHECK(auto_input_receipt(f, "after-tap", attempt + 1, timestamps[0], timestamps[1]) == 0);
        if (opened)
            return wb_wait(f, MC_CLIENT_SCREEN_WORKBENCH, out);
        /* Never leave an unobserved tap pending and add another one. A later
         * ordinary frame could otherwise open a Workbench after setup ended. */
        CHECK(f->state.gameplay_right_clicks > clicks || release_consumed);
    }
    fprintf(stderr, "Automatic-close setup did not open actual picked Workbench\n");
    return 1;
}

static double auto_distance(const mc_entity *player) {
    const double x = player->base.x - .5;
    const double y = player->base.y - 64.5;
    const double z = player->base.z - 2.5;
    return x * x + y * y + z * z;
}

static int auto_close(fixture *f, mc_client_source_id *o) {
    mc_entity *player = f->app->h;
    mc_container_state state;
    const mc_item_stack *cursor = NULL, *grid = NULL;
    CHECK(f->app->r == &o->base && !f->app->N && !atomic_load(&f->app->o) &&
          player->local_player->fields.player.e == o->id.j &&
          mc_inventory_player_source_cursor_get(player->local_player->fields.player.c, &cursor,
                                                NULL) == MC_INVENTORY_OK &&
          mc_container_read_state(o->id.j, &state) && state.world == f->app->active_world &&
          state.x == 0 && state.y == 64 && state.z == 2 &&
          mc_inventory_crafting_borrow_slot(state.crafting, 0, &grid, NULL) == MC_INVENTORY_OK &&
          wb_stack(cursor, 5, 7) == 0 && wb_stack(grid, 280, 2) == 0 && cursor != grid &&
          auto_distance(player) <= 64);
    automatic->world = f->app->active_world;
    automatic->player = player;
    automatic->expected[0] = cursor;
    automatic->expected[1] = grid;
    const double before_distance = auto_distance(player);
    const bool removed_mode = strcmp(automatic->mode, "removed") == 0;
    if (removed_mode) {
        mc_world_mutation_error *mutation = calloc(1, sizeof(*mutation));
        CHECK(mutation != NULL);
        bool changed = false;
        const mc_world_storage_status status =
            mc_world_runtime_block_set(automatic->world, 0, 64, 2, 0, &changed, mutation);
        if (status != MC_WORLD_STORAGE_OK)
            fprintf(stderr, "Authored Table removal: %s pc%u\n", mutation->operation,
                    mutation->bytecode);
        free(mutation);
        CHECK(status == MC_WORLD_STORAGE_OK && changed);
    } else {
        /* Declared integration input through the real Entity method. This is
         * not an SDK movement claim; OO suppresses Minecraft's game-input loop. */
        CHECK(mc_entity_set_position(player, 10.5, player->base.y, player->base.z) ==
                  MC_ENTITY_OK &&
              auto_distance(player) > 64);
    }
    int32_t block = -1;
    CHECK(mc_world_runtime_block_get(automatic->world, 0, 64, 2, &block, NULL) ==
              MC_WORLD_STORAGE_OK &&
          block == (removed_mode ? 0 : 58) && f->app->r == &o->base &&
          player->local_player->fields.player.e == o->id.j);
    const mc_world_access_callbacks callbacks = {.cell = ignore_cell,
                                                 .range = ignore_range,
                                                 .sound = ignore_sound,
                                                 .particle = ignore_particle,
                                                 .entity_added = publication,
                                                 .tile_entity_dirty = ignore_tile,
                                                 .entity_removed = ignore_entity,
                                                 .sky_changed = ignore_sky,
                                                 .auxiliary = ignore_auxiliary};
    CHECK(mc_world_access_create(&callbacks, automatic, &automatic->observer, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(automatic->world, automatic->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    automatic->observer_registered = true;
    mc_world_info_fields before, after;
    CHECK(mc_world_info_fields_get(automatic->world->fields.x, &before) == MC_WORLD_INFO_OK);
    /* Exactly this source client World call must close it. No key, frame,
     * display call, Container reset or second tick intervenes in the check. */
    const bool returned =
        mc_client_world_tick_run(f->app, f->app->source_math_classes, &automatic->tick);
    if (!returned)
        fprintf(stderr, "Automatic-close World tick %s.%s:%d: %s\n",
                automatic->tick.site.class_name, automatic->tick.site.method,
                automatic->tick.site.pc, automatic->tick.operation);
    printf("WORKBENCH_AUTO_FOCUS mode=%s after-close-source-focus=%d screen-null=%d\n",
           automatic->mode, (int)f->app->N, f->app->r == NULL);
    CHECK(returned && automatic->tick.returned && automatic->tick.entities_returned &&
          automatic->tick.close_calls == 1 && automatic->tick.close_returns == 1 &&
          f->app->r == NULL && !atomic_load(&f->app->o) && f->app->h == player &&
          player->local_player->fields.player.e == player->local_player->fields.player.d &&
          o->inherited_closed_returned && o->direct_closed_returned && o->closed_parent != NULL &&
          o->closed_direct != NULL && !automatic->publication_bad &&
          automatic->publication_count == 2 &&
          mc_world_info_fields_get(automatic->world->fields.x, &after) == MC_WORLD_INFO_OK &&
          mc_java_i64_sub(after.value.time, before.value.time) == 1);
    mc_player_container_state parent, direct;
    CHECK(mc_player_container_inspect(o->closed_parent->call, &parent) && parent.returned &&
          parent.container == player->local_player->fields.player.d && parent.drop_calls == 1 &&
          mc_player_container_inspect(o->closed_direct->call, &direct) && direct.returned &&
          direct.direct_target && direct.container == o->id.j && direct.drop_calls == 1);
    mc_player_drop_state drops[2];
    CHECK(mc_player_container_drop_inspect(o->closed_parent->call, 0, &drops[0]) &&
          mc_player_container_drop_inspect(o->closed_direct->call, 0, &drops[1]));
    for (size_t i = 0; i < 2; ++i) {
        CHECK(drops[i].returned && drops[i].publication_returned && drops[i].inserted &&
              drops[i].statistic_returned && drops[i].stack == automatic->expected[i] &&
              drops[i].item == automatic->published[i] && drops[i].item != NULL &&
              drops[i].item->stack == automatic->expected[i] &&
              drops[i].item->base.world == automatic->world);
        const mc_world_reference_list *live = automatic->world->fields.b;
        size_t occurrences = 0;
        CHECK(live != NULL && live->count <= live->capacity);
        for (size_t j = 0; j < live->count; ++j)
            if (live->items[j] == drops[i].item)
                ++occurrences;
        CHECK(occurrences == 1);
    }
    mc_item_stack_fields values[2];
    for (size_t i = 0; i < 2; ++i)
        CHECK(mc_item_stack_fields_get(automatic->expected[i], &values[i]) == MC_ITEM_STACK_OK &&
              values[i].animation == 0);
    bool rest_empty = true;
    for (int32_t slot = 2; slot <= 9; ++slot) {
        const mc_item_stack *actual = NULL;
        CHECK(mc_inventory_crafting_borrow_slot(state.crafting, slot - 1, &actual, NULL) ==
              MC_INVENTORY_OK);
        rest_empty = rest_empty && actual == NULL;
        CHECK(wb_slot(o, slot, 0, 0) == 0);
    }
    const mc_item_stack *retained_grid = NULL, *after_cursor = NULL;
    CHECK(mc_inventory_crafting_borrow_slot(state.crafting, 0, &retained_grid, NULL) ==
              MC_INVENTORY_OK &&
          mc_inventory_player_source_cursor_get(player->local_player->fields.player.c,
                                                &after_cursor, NULL) == MC_INVENTORY_OK &&
          retained_grid == grid && after_cursor == NULL && rest_empty && wb_cursor(f, 0, 0) == 0 &&
          wb_slot(o, 1, 280, 2) == 0);
    printf("WORKBENCH_AUTO_CLOSE_STRUCTURE {\"profile\":\"tick-%s\",\"screen-null\":%s,"
           "\"active-default\":%s,\"cursor-null\":%s,\"grid0-retained-same\":%s,"
           "\"grid0\":[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "],"
           "\"grid-rest-empty\":%s,\"drop-input-refs\":[\"%s\",\"%s\"],"
           "\"drop-values\":[[%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 "],[%" PRId32
           ",%" PRId32 ",%" PRId32 ",%" PRId32 "]],\"drop-stack-same-input\":[%s,%s]}\n",
           automatic->mode, f->app->r == NULL ? "true" : "false",
           player->local_player->fields.player.e == player->local_player->fields.player.d ? "true"
                                                                                          : "false",
           after_cursor == NULL ? "true" : "false", retained_grid == grid ? "true" : "false",
           values[1].id, values[1].count, values[1].damage, values[1].animation,
           rest_empty ? "true" : "false", drops[0].stack == cursor ? "cursor" : "other",
           drops[1].stack == grid ? "grid0" : "other", values[0].id, values[0].count,
           values[0].damage, values[0].animation, values[1].id, values[1].count, values[1].damage,
           values[1].animation, drops[0].item->stack == cursor ? "true" : "false",
           drops[1].item->stack == grid ? "true" : "false");
    CHECK(mc_client_world_tick_cleanup(&automatic->tick) && auto_observer_remove() == 0);
    printf("WORKBENCH_AUTO_CLOSE mode=%s trigger=%s client-world-ticks=1 same-call-r-null=1 "
           "default-restored=1 cursor-drop=1 grid-drop=1 publication-order=cursor-grid "
           "same-stacks=1 grid-retained=1 world-time-delta=1 before-distance=%016" PRIx64
           " after-distance=%016" PRIx64 "\n",
           automatic->mode, removed_mode ? "authored-world-set" : "authored-entity-position",
           wb_double_bits(before_distance), wb_double_bits(auto_distance(player)));
    return 0;
}

static int auto_run(fixture *f) {
    mining_work *m = &workbench->mining;
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && auto_choose(f, m) == 0 && auto_world_ready(f) == 0);
    mc_client_source_id *oo = NULL;
    const uint64_t deadline = SDL_GetTicks() + 15000;
    while (!wb_hit(f, 58, 2) && SDL_GetTicks() < deadline)
        CHECK(inventory_frame(f) == 0);
    CHECK(auto_open(f, &oo) == 0 && oo != NULL &&
          mining_capture(f, m, "workbench-auto-open.png") == 0);
    mc_world_info_fields before, after;
    CHECK(mc_world_info_fields_get(f->app->active_world->fields.x, &before) == MC_WORLD_INFO_OK);
    const uint64_t ui_deadline = SDL_GetTicks() + 15000;
    do {
        CHECK(inventory_frame(f) == 0 && wb_ui(f, oo, 0, 0) == 0 &&
              mc_world_info_fields_get(f->app->active_world->fields.x, &after) == MC_WORLD_INFO_OK);
        ++workbench->ui_frames;
    } while (
        (workbench->ui_frames < 3 || mc_java_i64_sub(after.value.time, before.value.time) < 3) &&
        SDL_GetTicks() < ui_deadline);
    CHECK(workbench->ui_frames >= 3 && mc_java_i64_sub(after.value.time, before.value.time) >= 3);
    workbench->ui_ticks = (uint64_t)mc_java_i64_sub(after.value.time, before.value.time);
    CHECK(wb_click(f, oo, 12, false, false, 280, 2) == 0 &&
          wb_click(f, oo, 1, false, false, 0, 0) == 0 && wb_slot(oo, 1, 280, 2) == 0 &&
          wb_click(f, oo, 10, false, false, 5, 7) == 0 &&
          mining_capture(f, m, "workbench-auto-before-close.png") == 0);
    printf("WORKBENCH_AUTO_SETUP mode=%s sdk-open=1 sdk-cursor-grid=1 ui-frames=%zu "
           "world-ticks=%" PRIu64 "\n",
           automatic->mode, workbench->ui_frames, workbench->ui_ticks);
    CHECK(auto_close(f, oo) == 0);
    /* Source focus may return early when the real Display is inactive.
     * The completed close is observed before any later SDK acquisition. */
    if (!f->app->N)
        CHECK(auto_resume(f) == 0);
    size_t held_frames = 0;
    const uint64_t held_deadline = SDL_GetTicks() + 15000;
    while (held_frames < 3 && SDL_GetTicks() < held_deadline) {
        CHECK(inventory_frame(f) == 0);
        const mc_client_world_frame_state *world = f->state.world_frame;
        const mc_client_first_person_hand_state *hand = world == NULL ? NULL : world->hand;
        const mc_item_stack *selected = NULL;
        mc_client_entity_renderer_state renderer;
        CHECK(f->app->r == NULL && f->app->N && !atomic_load(&f->app->o) &&
              mc_inventory_player_current(f->app->h->local_player->fields.player.c, &selected,
                                          NULL) == MC_INVENTORY_OK);
        if (hand != NULL && hand->returned && hand->held_returned && hand->overlays_returned &&
            hand->held_item != NULL && hand->held_item->returned &&
            hand->held_item->equipped >= .99f && selected != NULL &&
            mc_client_entity_renderer_inspect(world->owner, &renderer) &&
            renderer.ra_b == selected) {
            CHECK(wb_stack(selected, 270, 1) == 0 && hand->held_item->source_pc == 1377 &&
                  hand->held_item->flat_item && hand->held_item->item_icon_returned &&
                  hand->held_item->item_geometry_returned && hand->held_item->faces == 66 &&
                  hand->held_item->vertices == 264 && f->state.hud != NULL &&
                  f->state.hud->returned);
            ++held_frames;
        }
    }
    CHECK(held_frames == 3 && mining_capture(f, m, "workbench-auto-held.png") == 0);
    workbench->saving = true;
    mining_extra_before_frame = wb_save_before;
    mining_extra_after_frame = wb_save_after;
    char path[8192];
    CHECK(mining_path(m, "workbench-save-title.png", path) == 0 &&
          save_title_run(f, NULL, path, false, 1, 0) == 0 && workbench->save_selected &&
          workbench->save_samples > 0 && reload_snapshot("workbench-saved") == 0);
    printf("WORKBENCH_AUTO_SDK mode=%s held-pickaxe-frames=%zu save-samples=%zu ordinary-save=1\n",
           automatic->mode, held_frames, workbench->save_samples);
    return 0;
}

#ifndef MC_WORKBENCH_AUTO_CLOSE_FIXTURE_MAIN
#define MC_WORKBENCH_AUTO_CLOSE_FIXTURE_MAIN main
#endif
int MC_WORKBENCH_AUTO_CLOSE_FIXTURE_MAIN(int argc, char **argv) {
    CHECK(argc == 4 && (strcmp(argv[3], "removed") == 0 || strcmp(argv[3], "distance") == 0));
    fixture *f = calloc(1, sizeof(*f));
    workbench = calloc(1, sizeof(*workbench));
    automatic = calloc(1, sizeof(*automatic));
    if (f == NULL || workbench == NULL || automatic == NULL) {
        free(automatic);
        free(workbench);
        free(f);
        return 1;
    }
    automatic->mode = argv[3];
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
            result = auto_run(f);
        }
    }
    mining_extra_before_frame = NULL;
    mining_extra_after_frame = NULL;
    const bool tick_clean = mc_client_world_tick_cleanup(&automatic->tick);
    if (!tick_clean || auto_observer_remove() != 0)
        result = 1;
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
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
        printf("WORKBENCH_AUTO_FINISH mode=%s cleanup=1 windows-absent=1 checks=%u\n",
               automatic->mode, checks);
    /* Failed checked retirement leaves callback/fixture borrows live until
     * process exit. It never becomes a successful cleanup receipt. */
    if (closed == MC_CLIENT_BOOTSTRAP_OK && tick_clean && automatic->observer == NULL) {
        free(automatic);
        free(workbench);
        free(f);
    }
    return result;
}
