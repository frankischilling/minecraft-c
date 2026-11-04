/* Genuine saved-world input, till/plant clicks and ordinary save/reselection. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/runtime/menu_loop.h"
static bool farming_observed_frame(mc_client_menu_loop *);
#define mc_client_menu_loop_frame farming_observed_frame
#define main mc_farming_included_selection_main
#include "../startup/test_world_selection_loop.c"
#undef main
#undef mc_client_menu_loop_frame
#include "mc/entity/nbt/errors.h"
#include "mc/inventory/player_current.h"

typedef struct farming_work {
    fixture *fixture;
    const char *directory;
    reload_epoch first, reloaded;
    bool right_down, saving, sampled, selected;
    size_t save_samples, selected_sample;
    int32_t entry_ground, entry_crop, entry_moisture, entry_stage;
} farming_work;

static farming_work *farming;

static int farming_path(const char *name, char path[8192]) {
    const int n = snprintf(path, 8192, "%s/%s", farming->directory, name);
    CHECK(n > 0 && n < 8192);
    return 0;
}

static int farming_player_nbt(mc_entity *player, const char *name) {
    mc_entity_nbt_error *error = calloc(1, sizeof(*error));
    mc_nbt_tree *tree = NULL;
    CHECK(error != NULL && mc_entity_build_nbt(player, NULL, &tree, error) == MC_ENTITY_NBT_OK);
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_output_bytes = 1048576;
    size_t bytes = 0, written = 0;
    mc_nbt_error wire_error;
    CHECK(mc_nbt_measure(mc_nbt_tree_root(tree), &limits, &bytes, &wire_error) == MC_NBT_OK &&
          bytes <= 1048576);
    uint8_t *wire = malloc(bytes);
    char path[8192];
    CHECK(wire != NULL &&
          mc_nbt_write(mc_nbt_tree_root(tree), &limits, wire, bytes, &written, &wire_error) ==
              MC_NBT_OK &&
          written == bytes && farming_path(name, path) == 0 && SDL_SaveFile(path, wire, bytes));
    free(wire);
    mc_nbt_tree_free(tree);
    free(error);
    return 0;
}

/* Read native Store arrays and serialize a separate owned Player NBT tree to
 * evidence files. The NBT API balances its temporary native owner holds. No
 * World/Player/Inventory/statistic/Random/Timer field is assigned, and no source
 * save/flush, tick, input poll or additional frame is requested here. */
static int farming_entry(fixture *f) {
    mc_chunk_state *chunk = NULL;
    const size_t count = mc_world_store_chunk_count(farming->first.handler);
    for (size_t i = 0; i < count; ++i) {
        mc_chunk_state *candidate = NULL;
        CHECK(mc_world_store_chunk_at(farming->first.handler, i, &candidate, NULL) ==
              MC_WORLD_STORAGE_OK);
        if (candidate != NULL && candidate->x == 0 && candidate->z == 0) {
            CHECK(chunk == NULL);
            chunk = candidate;
        }
    }
    const size_t index = (3u << 11u) | (3u << 7u) | 64u;
    CHECK(chunk != NULL && !chunk->blank && chunk->data != NULL && chunk->data->blocks != NULL &&
          chunk->data->metadata != NULL);
    farming->entry_ground = chunk->data->blocks[index];
    farming->entry_crop = chunk->data->blocks[index + 1u];
    farming->entry_moisture = chunk->data->metadata[index / 2u] & 15;
    farming->entry_stage = chunk->data->metadata[index / 2u] >> 4u;
    char name[96];
    const int n =
        snprintf(name, sizeof(name), "farming-player-entry-%04zu.nbt", farming->save_samples);
    CHECK(n > 0 && (size_t)n < sizeof(name) && farming_player_nbt(f->app->h, name) == 0);
    farming->sampled = true;
    ++farming->save_samples;
    return 0;
}

static bool farming_observed_frame(mc_client_menu_loop *loop) {
    fixture *f = farming == NULL ? NULL : farming->fixture;
    const bool sample = farming != NULL && farming->saving && !farming->selected && f != NULL &&
                        f->loop == loop && f->app->active_world == farming->first.world &&
                        f->app->h == farming->first.player && f->app->r != NULL &&
                        f->app->r->kind == MC_CLIENT_SCREEN_PAUSE && !f->app->N &&
                        atomic_load(&f->app->o);
    if (farming != NULL)
        farming->sampled = false;
    if (sample && farming_entry(f) != 0)
        return false;
    const bool returned = mc_client_menu_loop_frame(loop);
    if (sample && farming->sampled && returned && f->app->active_world == NULL &&
        f->app->h == NULL && f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU) {
        farming->selected = true;
        farming->selected_sample = farming->save_samples - 1u;
    }
    return returned;
}

static int farming_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0 && frame(f) == 0 && !f->state.failed);
    return 0;
}

static int farming_ready(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    for (size_t i = 0; i < 180 && SDL_GetTicks() < deadline; ++i) {
        CHECK(farming_frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL || !f->app->N) {
            mc_chunk_provider_fields provider;
            size_t eligible = 0;
            int64_t world_time = 0;
            CHECK(mc_chunk_provider_bind_fields(f->app->active_world->fields.v, &provider, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  save_title_resume(f, f->app->active_world, &provider, recovered, &pauses,
                                    &eligible, &world_time) == 0);
            continue;
        }
        const mc_client_world_frame_state *draw = f->state.world_frame;
        if (f->app->h != NULL && f->app->i == f->app->h && draw != NULL && draw->returned &&
            draw->terrain_returned && draw->updates_returned && draw->source_pc == 1013 &&
            f->state.hud != NULL && f->state.hud->returned)
            return 0;
    }
    return 1;
}

static int farming_choose_first(fixture *f) {
    mc_client_source_rq *rq = NULL;
    CHECK(f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          open_selection(f, f->app->r, &rq) == 0 && release_mouse(f, rq) == 0);
    int32_t selected = -1;
    for (size_t i = 0; i < mc_client_save_list_size(rq->fields.n); ++i)
        if (filename_is(mc_client_save_list_at(rq->fields.n, i), "alpha")) {
            CHECK(selected == -1);
            selected = (int32_t)i;
        }
    CHECK(selected >= 0);
    const mc_client_world_list_fields *list = &rq->fields.o->fields;
    const int32_t y = list->h + 4 - mc_java_float_to_i32(list->m) + selected * 36 + 8;
    CHECK(push_mouse(f, &rq->base, rq->base.fields.c / 2, y, true) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 5000;
    while (rq->fields.m != selected && SDL_GetTicks() < deadline)
        CHECK(frame(f) == 0);
    CHECK(rq->fields.m == selected && release_mouse(f, rq) == 0 &&
          push_button(f, &rq->base, 1, true) == 0 && push_button(f, &rq->base, 1, false) == 0 &&
          transfer(f) == 0 && farming_ready(f) == 0 &&
          reload_capture_epoch(f, &farming->first) == 0);
    return 0;
}

static int farming_inventory(fixture *f, int32_t selected, int32_t wear, int32_t seeds) {
    mc_local_player_snapshot player;
    mc_inventory_player_state inventory;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          mc_inventory_player_read_state(player.player.c, &inventory) &&
          inventory.selected == selected);
    for (int32_t i = 0; i < 36; ++i) {
        const mc_item_stack *stack = NULL;
        CHECK(mc_inventory_player_borrow(player.player.c, i, &stack, NULL) == MC_INVENTORY_OK);
        if (i < 2) {
            mc_item_stack_fields fields;
            CHECK(stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
                  fields.id == (i == 0 ? 290 : 295) && fields.count == (i == 0 ? 1 : seeds) &&
                  fields.damage == (i == 0 ? wear : 0));
        } else
            CHECK(stack == NULL);
    }
    return 0;
}

static int farming_plot(fixture *f, int32_t ground, int32_t crop, int32_t *stage) {
    int32_t id = -1, above = -1, metadata = -1;
    CHECK(mc_world_runtime_block_get(f->app->active_world, 3, 64, 3, &id, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_block_get(f->app->active_world, 3, 65, 3, &above, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(f->app->active_world, 3, 65, 3, &metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          id == ground && above == crop && metadata >= 0 && metadata <= 7);
    if (stage != NULL)
        *stage = metadata;
    return 0;
}

static int farming_hit(fixture *f, int32_t y, int32_t id) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    int32_t actual = -1;
    CHECK(f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
          mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
          hit.fields.a == kinds.a && hit.fields.b == 3 && hit.fields.c == y && hit.fields.d == 3 &&
          hit.fields.e == 1 &&
          mc_world_runtime_block_get(f->app->active_world, 3, y, 3, &actual, NULL) ==
              MC_WORLD_STORAGE_OK &&
          actual == id);
    return 0;
}

static int farming_statistic(fixture *f, int32_t id, int32_t expected) {
    mc_registry_statistic_view view;
    int32_t used = -1, broken = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &view, NULL) == MC_REGISTRY_OK &&
          view.E != NULL && view.E[id] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, view.E[id], &used, NULL) ==
              MC_CLIENT_STATS_OK &&
          used == expected);
    if (id == 290)
        CHECK(view.F != NULL && view.F[id] != NULL &&
              mc_client_stats_writer_value(f->app->stats_writer, view.F[id], &broken, NULL) ==
                  MC_CLIENT_STATS_OK &&
              broken == 0);
    return 0;
}

static mc_client_source_dk *farming_section(fixture *f) {
    mc_client_source_dk *found = NULL;
    for (mc_client_source_dk *p = f->app->g->chunk_owners; p != NULL; p = p->next)
        if (p->fields.a == f->app->active_world && p->fields.c == 0 && p->fields.d == 64 &&
            p->fields.e == 0) {
            if (found != NULL)
                return NULL;
            found = p;
        }
    return found;
}

static int farming_batch(fixture *f, mc_client_source_dk *section,
                         mc_client_source_batch_view *batch) {
    CHECK(section != NULL && section->fields.E && !section->fields.u &&
          mc_client_display_list_batch(f->app->display, section->fields.C, 0, batch) &&
          batch->mode == 4 && batch->arrays == 7 && batch->vertices > 0 &&
          batch->byte_count == (size_t)batch->vertices * 32u);
    return 0;
}

static int farming_settle(fixture *f, mc_client_source_dk *section) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        if (section->fields.E && !section->fields.u)
            return 0;
        CHECK(farming_frame(f) == 0);
    }
    return 1;
}

static int farming_button(fixture *f, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = SDL_BUTTON_RIGHT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    return 0;
}

static int farming_tap(fixture *f, bool plant) {
    const uint64_t clicks = f->state.gameplay_right_clicks;
    const uint64_t placed = f->state.gameplay_placed_blocks;
    const uint64_t sounds = f->state.gameplay_placement_sounds;
    const uint64_t counts = f->state.gameplay_stack_count_stores;
    const uint64_t statistics = f->state.gameplay_used_statistics;
    const uint64_t admitted = f->state.gameplay_placements_admitted;
    const uint64_t clears = f->state.gameplay_selected_slot_clears;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t attempt = 0; attempt < 12 && SDL_GetTicks() < deadline; ++attempt) {
        CHECK(farming_hit(f, 64, plant ? 60 : 3) == 0);
        const size_t events = f->state.mouse_events;
        CHECK(farming_button(f, true) == 0);
        farming->right_down = true;
        CHECK(farming_button(f, false) == 0);
        farming->right_down = false;
        bool drained = false;
        for (size_t frames = 0; frames < 64 && SDL_GetTicks() < deadline; ++frames) {
            CHECK(farming_frame(f) == 0);
            mc_client_sdk_input_state input;
            mc_client_sdk_bytes_state read, queue, buttons;
            CHECK(mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input) &&
                  mc_client_sdk_bytes_inspect(input.mouse_read, &read) &&
                  mc_client_sdk_queue_inspect(input.mouse_events, &queue) &&
                  mc_client_sdk_bytes_inspect(input.buttons, &buttons) && buttons.limit > 1 &&
                  buttons.data[1] == 0);
            drained = f->state.mouse_events > events && read.position == read.limit &&
                      queue.position == 0 && input.pending_native_event == NULL;
            if (drained)
                break;
        }
        CHECK(drained);
        if (f->state.gameplay_placed_blocks > placed) {
            CHECK(f->state.gameplay_right_clicks > clicks &&
                  f->state.gameplay_placed_blocks == placed + 1 &&
                  f->state.gameplay_placement_sounds == sounds + (plant ? 0u : 1u) &&
                  f->state.gameplay_stack_count_stores == counts + (plant ? 1u : 0u) &&
                  f->state.gameplay_used_statistics == statistics + 1 &&
                  f->state.gameplay_placements_admitted == admitted &&
                  f->state.gameplay_selected_slot_clears == clears);
            printf("FARMING_INPUT action=%s attempts=%zu right-clicks=%" PRIu64
                   " placed=1 sounds=%u count-stores=%u used=1 ck-predicate=0 drained=1\n",
                   plant ? "plant" : "till", attempt + 1, f->state.gameplay_right_clicks - clicks,
                   plant ? 0u : 1u, plant ? 1u : 0u);
            return 0;
        }
    }
    return 1;
}

static int farming_run(fixture *f) {
    CHECK(repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0 &&
          farming_choose_first(f) == 0 && farming_inventory(f, 0, 0, 2) == 0 &&
          farming_plot(f, 3, 0, NULL) == 0 && farming_hit(f, 64, 3) == 0 &&
          farming_statistic(f, 290, 0) == 0 && farming_statistic(f, 295, 0) == 0);
    mc_entity_snapshot pose;
    CHECK(mc_entity_get_snapshot(f->app->h, &pose) == MC_ENTITY_OK && pose.bounds != NULL &&
          pose.bounds->min_y == 65 && pose.base.x == 3.5 && pose.base.z == .5 &&
          pose.base.y == 65.0 + (double)1.62f && pose.base.yaw == 0 && pose.base.aT == 26);
    mc_client_source_dk *section = farming_section(f);
    mc_client_source_batch_view before, tilled;
    CHECK(section != NULL && farming_settle(f, section) == 0 &&
          farming_batch(f, section, &before) == 0);
    const uintptr_t before_bytes = (uintptr_t)before.bytes;
    CHECK(farming_tap(f, false) == 0 && farming_inventory(f, 0, 1, 2) == 0 &&
          farming_plot(f, 60, 0, NULL) == 0 && farming_statistic(f, 290, 1) == 0 &&
          farming_settle(f, section) == 0 && farming_batch(f, section, &tilled) == 0 &&
          (uintptr_t)tilled.bytes != before_bytes);
    const uintptr_t tilled_bytes = (uintptr_t)tilled.bytes;
    const size_t keys = f->state.keyboard_events;
    CHECK(gameplay_key(f, SDL_SCANCODE_2, true) == 0 &&
          gameplay_key(f, SDL_SCANCODE_2, false) == 0 && transfer(f) == 0);
    const uint64_t key_deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0;
         f->state.keyboard_events < keys + 2 && i < 64 && SDL_GetTicks() < key_deadline; ++i)
        CHECK(farming_frame(f) == 0);
    CHECK(f->state.keyboard_events >= keys + 2 && farming_inventory(f, 1, 1, 2) == 0 &&
          farming_hit(f, 64, 60) == 0 && farming_tap(f, true) == 0 &&
          farming_inventory(f, 1, 1, 1) == 0 && farming_statistic(f, 290, 1) == 0 &&
          farming_statistic(f, 295, 1) == 0);
    int32_t stage = -1;
    CHECK(farming_plot(f, 60, 59, &stage) == 0 && stage == 0 && farming_settle(f, section) == 0);
    loaded_block_reference reference;
    CHECK(block_loaded_reference(&reference, 59, 0, "MC_FARMING_CROPS_REFERENCE",
                                 "FARMING_CROPS_V1") == 0);
    static const standing_block_profile profile = {"farming", "FARMING", "crops", "", "", 59,  0,
                                                   6,         48,        64,      3,  60, 108, 26};
    mc_client_source_batch_view planted;
    size_t colors = 0;
    CHECK(standing_loaded_list(f, section, &reference, &profile, &planted, &colors) == 0 &&
          (uintptr_t)planted.bytes != tilled_bytes);
    char path[8192];
    CHECK(farming_path("farming-planted.png", path) == 0 && capture(f, path) == 0);
    printf("FARMING_RETURN ground=60 crop=59 stage=0 hoe-count=1 hoe-damage=1 seeds-count=1 "
           "hotbar=1 target-lists-replaced=2 exact-original-position-uv=48 "
           "source-color-matches=%zu source-colors=48\n",
           colors);
    farming->saving = true;
    CHECK(farming_path("farming-save-title.png", path) == 0);
    const int saved = save_title_run(f, NULL, path, false, 1, 0);
    farming->saving = false;
    CHECK(saved == 0 && farming->selected && farming->save_samples > 0 &&
          farming->entry_ground == 60 && farming->entry_crop == 59 &&
          reload_snapshot("farming-saved") == 0);
    printf("FARMING_SAVE sample=%zu candidates=%zu ground=%" PRId32 " crop=%" PRId32
           " moisture=%" PRId32 " stage=%" PRId32 " source-save-title=1 pre-frame-owned-nbt=1\n",
           farming->selected_sample, farming->save_samples, farming->entry_ground,
           farming->entry_crop, farming->entry_moisture, farming->entry_stage);
    CHECK(reload_choose(f, f->app->r, "alpha", &farming->first, &farming->reloaded, 2) == 0 &&
          farming_ready(f) == 0 && farming_inventory(f, 0, 1, 1) == 0 &&
          farming_plot(f, 60, 59, &stage) == 0 &&
          farming_player_nbt(f->app->h, "farming-player-reloaded.nbt") == 0);
    printf("FARMING_RELOAD distinct-world=1 distinct-player=1 ground=60 crop=59 stage=%" PRId32
           " hoe-count=1 hoe-damage=1 seeds-count=1 selected=0 source-selected-slot-not-saved=1\n",
           stage);
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3)
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    farming_work *work = calloc(1, sizeof(*work));
    if (f == NULL || work == NULL)
        return 2;
    farming = work;
    work->fixture = f;
    work->directory = argv[2];
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
            result = farming_run(f);
        }
    }
    work->saving = false;
    if (f->app != NULL && work->right_down)
        (void)farming_button(f, false);
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
        printf("FARMING_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    farming = NULL;
    free(work);
    free(f);
    return result;
}
