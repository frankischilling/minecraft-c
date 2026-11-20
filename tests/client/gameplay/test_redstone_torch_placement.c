/* Frozen authored wall/Plate/Dust and Item76; genuine SDK placement, natural schedule,
 * save/reselection. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/runtime/menu_loop.h"
static bool rt_observed_frame(mc_client_menu_loop *);
#define mc_client_menu_loop_frame rt_observed_frame
#define main mc_rt_included_selection_main
#include "tests/client/startup/test_world_selection_loop.c"
#undef main
#undef mc_client_menu_loop_frame
#include "mc/entity/nbt/errors.h"
#include "mc/inventory/player_current.h"
#include "mc/world/info/info.h"
#include "mc/world/schedule/membership.h"
#include "mc/world/schedule/ordered.h"

typedef struct rt_work {
    fixture *fixture;
    const char *directory, *phase, *kind;
    int32_t id;
    reload_epoch first, reloaded;
    bool right_down, saving, selected, started;
    bool lit_seen, scheduled_seen, off_seen;
    int64_t scheduled_time, entry_time;
    size_t trace_count, save_samples, selected_sample, entry_items;
    int32_t entry_health, entry_ground, entry_above, entry_stage, entry_plate, entry_dust;
} rt_work;

static rt_work *rt;

/* Passive Store-owned cells and scalar/collection inventories only. */
static mc_chunk_state *rt_chunk(fixture *f) {
    if (f->app == NULL || f->app->active_world == NULL)
        return NULL;
    mc_world_store *store = f->app->active_world->fields.w;
    mc_chunk_state *found = NULL;
    if (store == NULL)
        return NULL;
    for (size_t i = 0; i < mc_world_store_chunk_count(store); ++i) {
        mc_chunk_state *candidate = NULL;
        if (mc_world_store_chunk_at(store, i, &candidate, NULL) != MC_WORLD_STORAGE_OK)
            return NULL;
        if (candidate != NULL && candidate->x == 0 && candidate->z == 0) {
            if (found != NULL)
                return NULL;
            found = candidate;
        }
    }
    return found != NULL && !found->blank && found->data != NULL ? found : NULL;
}

static void rt_cell(const mc_chunk_state *chunk, int32_t x, int32_t y, int32_t z, int32_t out[3]) {
    const size_t at = ((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y;
    out[0] = chunk->data->blocks[at];
    out[1] = (chunk->data->metadata[at / 2u] >> ((at & 1u) * 4u)) & 15;
    out[2] = (chunk->data->block_light[at / 2u] >> ((at & 1u) * 4u)) & 15;
}

static int rt_trace(fixture *f) {
    mc_chunk_state *chunk = rt_chunk(f);
    if (chunk == NULL)
        return 0; /* A partially loaded World is not an invented zero cell. */
    mc_world_runtime *world = f->app->active_world;
    int64_t time = 0;
    int32_t target[3], support[3], plate[3], dust[3], en = -1, fn = -1;
    CHECK(world->fields.r != NULL && world->fields.C != NULL &&
          mc_world_info_time_get(world->fields.x, &time) == MC_WORLD_INFO_OK &&
          mc_scheduled_ordered_size(world->fields.E, &en) == MC_SCHEDULED_ORDERED_OK &&
          mc_scheduled_membership_size(world->fields.F, &fn) == MC_SCHEDULED_MEMBERSHIP_OK &&
          en >= 0 && fn == en);
    const mc_scheduled_tick **ep = en == 0 ? NULL : malloc((size_t)en * sizeof(*ep));
    const mc_scheduled_tick **fp = fn == 0 ? NULL : malloc((size_t)fn * sizeof(*fp));
    if ((en != 0 && ep == NULL) || (fn != 0 && fp == NULL)) {
        free(ep);
        free(fp);
        return 1;
    }
    size_t ec = 0, fc = 0;
    if (mc_scheduled_ordered_snapshot(world->fields.E, ep, (size_t)en, &ec) !=
            MC_SCHEDULED_ORDERED_OK ||
        mc_scheduled_membership_snapshot(world->fields.F, fp, (size_t)fn, &fc) !=
            MC_SCHEDULED_MEMBERSHIP_OK ||
        ec != (size_t)en || fc != (size_t)fn) {
        free(ep);
        free(fp);
        return 1;
    }
    rt_cell(chunk, 3, 65, 2, target);
    rt_cell(chunk, 3, 65, 3, support);
    rt_cell(chunk, 3, 66, 3, plate);
    rt_cell(chunk, 4, 65, 2, dust);
    uint64_t gaussian;
    memcpy(&gaussian, &world->fields.r->next_next_gaussian, sizeof(gaussian));
    printf("RT_GUI_STATE\t{\"index\":%zu,\"phase\":\"%s\",\"world\":\"%" PRIxPTR
           "\",\"time\":%" PRId64 ",\"frames\":%" PRIu64 ",\"ticks\":%" PRIu64
           ",\"world_ticks\":%" PRIu64 ",\"focus\":%d,\"paused\":%d,\"target\":[%d,%d,%d],"
           "\"support\":[%d,%d,%d],\"plate\":[%d,%d,%d],\"dust\":[%d,%d,%d],"
           "\"light_count\":%zu,\"M\":%" PRId32 ",\"random\":{\"seed\":\"%012" PRIx64
           "\",\"gaussian\":\"%016" PRIx64 "\",\"have\":%s},\"E\":[",
           rt->trace_count++, rt->phase, (uintptr_t)world, time, f->state.frames, f->state.ticks,
           f->state.world_tick_returns, f->app->N, atomic_load(&f->app->o), target[0], target[1],
           target[2], support[0], support[1], support[2], plate[0], plate[1], plate[2], dust[0],
           dust[1], dust[2], world->fields.C->count, world->fields.M, world->fields.r->state,
           gaussian, world->fields.r->have_next_next_gaussian ? "true" : "false");
    bool scheduled = false;
    for (size_t i = 0; i < ec; ++i) {
        mc_scheduled_tick_fields value;
        if (mc_scheduled_tick_fields_get(ep[i], &value) != MC_SCHEDULED_TICK_OK) {
            free(ep);
            free(fp);
            return 1;
        }
        printf("%s[\"%" PRIxPTR "\",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId64
               ",%" PRId64 "]",
               i == 0 ? "" : ",", (uintptr_t)ep[i], value.x, value.y, value.z, value.block,
               value.due_time, value.sequence);
        if (rt->started && target[0] == 76 && value.x == 3 && value.y == 65 && value.z == 2 &&
            value.block == 76 && mc_java_i64_sub(value.due_time, time) > 0 &&
            mc_java_i64_sub(value.due_time, time) <= 2 && !rt->scheduled_seen) {
            rt->scheduled_seen = true;
            rt->scheduled_time = value.due_time;
            scheduled = true;
        }
    }
    fputs("],\"F\":[", stdout);
    for (size_t i = 0; i < fc; ++i) {
        mc_scheduled_tick_fields value;
        if (mc_scheduled_tick_fields_get(fp[i], &value) != MC_SCHEDULED_TICK_OK) {
            free(ep);
            free(fp);
            return 1;
        }
        printf("%s[\"%" PRIxPTR "\",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId64
               ",%" PRId64 "]",
               i == 0 ? "" : ",", (uintptr_t)fp[i], value.x, value.y, value.z, value.block,
               value.due_time, value.sequence);
    }
    printf("],\"new_future_target76\":%s}\n", scheduled ? "true" : "false");
    free(ep);
    free(fp);
    if (rt->started && target[0] == 76 && target[1] == 4 && plate[0] == 70 && plate[1] == 1)
        rt->lit_seen = true;
    if (rt->started && rt->lit_seen && rt->scheduled_seen && target[0] == 75 && target[1] == 4 &&
        plate[0] == 70 && plate[1] == 1 && dust[0] == 55 && dust[1] == 0 &&
        mc_java_i64_sub(time, rt->scheduled_time) >= 0)
        rt->off_seen = true;
    return ferror(stdout) ? 1 : 0;
}

static int rt_path(const char *name, char path[8192]) {
    const int n = snprintf(path, 8192, "%s/%s", rt->directory, name);
    CHECK(n > 0 && n < 8192);
    return 0;
}

/* Passive owned-tree serialization, outside any source save or extra frame. */
static int rt_player_nbt(mc_entity *player, const char *name) {
    int result = 1;
    mc_entity_nbt_error *error = calloc(1, sizeof(*error));
    mc_nbt_tree *tree = NULL;
    uint8_t *wire = NULL;
    char path[8192];
    if (error == NULL || mc_entity_build_nbt(player, NULL, &tree, error) != MC_ENTITY_NBT_OK)
        goto cleanup;
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_output_bytes = 1048576;
    size_t bytes = 0, written = 0;
    mc_nbt_error wire_error;
    if (mc_nbt_measure(mc_nbt_tree_root(tree), &limits, &bytes, &wire_error) != MC_NBT_OK ||
        bytes > 1048576 || (wire = malloc(bytes)) == NULL ||
        mc_nbt_write(mc_nbt_tree_root(tree), &limits, wire, bytes, &written, &wire_error) !=
            MC_NBT_OK ||
        written != bytes || rt_path(name, path) != 0 || !SDL_SaveFile(path, wire, bytes))
        goto cleanup;
    result = 0;
cleanup:
    free(wire);
    mc_nbt_tree_free(tree);
    free(error);
    return result;
}

static int rt_inventory(fixture *f, int32_t count, const mc_item_stack **current) {
    mc_local_player_snapshot player;
    mc_inventory_player_state inventory;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          player.living.Y == 5 && mc_inventory_player_read_state(player.player.c, &inventory) &&
          inventory.selected == 0);
    for (int32_t i = 0; i < 36; ++i) {
        const mc_item_stack *stack = NULL;
        CHECK(mc_inventory_player_borrow(player.player.c, i, &stack, NULL) == MC_INVENTORY_OK);
        if (i == 0) {
            mc_item_stack_fields fields;
            CHECK(stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
                  fields.id == rt->id && fields.count == count && fields.damage == 0);
            if (current != NULL)
                *current = stack;
        } else
            CHECK(stack == NULL);
    }
    return 0;
}

static int rt_statistic(fixture *f, int32_t expected) {
    mc_registry_statistic_view view;
    int32_t used = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &view, NULL) == MC_REGISTRY_OK &&
          view.E != NULL && view.E[rt->id] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, view.E[rt->id], &used, NULL) ==
              MC_CLIENT_STATS_OK &&
          used == expected);
    return 0;
}

/* Sample the exact paused entry of the frame that actually selects Save/Quit.
 * Reads and owned NBT serialization balance their native holds. */
static int rt_items(fixture *f, bool save, size_t sample, size_t *count) {
    (void)save;
    (void)sample;
    const mc_world_reference_list *list = f->app->active_world->fields.b;
    CHECK(list != NULL && list->count <= list->capacity);
    *count = 0;
    for (size_t i = 0; i < list->count; ++i)
        if (list->items[i] != NULL &&
            mc_entity_is_kind(list->items[i], MC_ENTITY_CLASS_KIND_ITEM) &&
            !((mc_entity *)list->items[i])->base.be)
            ++*count;
    CHECK(*count == 0);
    return 0;
}

static int rt_entry(fixture *f) {
    mc_chunk_state *chunk = rt_chunk(f);
    mc_local_player_snapshot player;
    int32_t target[3], support[3], plate[3], dust[3];
    CHECK(chunk != NULL && mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          mc_world_info_time_get(f->app->active_world->fields.x, &rt->entry_time) ==
              MC_WORLD_INFO_OK);
    rt_cell(chunk, 3, 65, 2, target);
    rt_cell(chunk, 3, 65, 3, support);
    rt_cell(chunk, 3, 66, 3, plate);
    rt_cell(chunk, 4, 65, 2, dust);
    rt->entry_health = player.living.Y;
    rt->entry_ground = support[0];
    rt->entry_above = target[0];
    rt->entry_stage = target[1];
    rt->entry_plate = plate[1];
    rt->entry_dust = dust[1];
    CHECK(target[2] == 0);
    char name[96];
    const int n = snprintf(name, sizeof(name), "rt-player-entry-%04zu.nbt", rt->save_samples);
    CHECK(n > 0 && (size_t)n < sizeof(name) && rt_player_nbt(f->app->h, name) == 0 &&
          rt_items(f, false, 0, &rt->entry_items) == 0);
    ++rt->save_samples;
    return 0;
}

static bool rt_observed_frame(mc_client_menu_loop *loop) {
    fixture *f = rt == NULL ? NULL : rt->fixture;
    const bool sample = rt != NULL && rt->saving && !rt->selected && f != NULL && f->loop == loop &&
                        f->app->active_world == rt->first.world && f->app->h == rt->first.player &&
                        f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_PAUSE &&
                        !f->app->N && atomic_load(&f->app->o);
    if (sample && rt_entry(f) != 0)
        return false;
    const bool returned = mc_client_menu_loop_frame(loop);
    if (sample && returned && f->app->active_world == NULL && f->app->h == NULL &&
        f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU) {
        rt->selected = true;
        rt->selected_sample = rt->save_samples - 1u;
    }
    return returned;
}

static int rt_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0 && frame(f) == 0 && !f->state.failed && rt_trace(f) == 0);
    return 0;
}

static int rt_ready(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    for (size_t i = 0; i < 180 && SDL_GetTicks() < deadline; ++i) {
        CHECK(rt_frame(f) == 0);
        if (f->app->active_world == NULL)
            continue;
        if (f->app->r != NULL || !f->app->N) {
            size_t eligible = 0;
            int64_t world_time = 0;
            CHECK(save_title_resume(f, f->app->active_world, NULL, recovered, &pauses, &eligible,
                                    &world_time) == 0);
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

static int rt_choose_first(fixture *f) {
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
          transfer(f) == 0 && rt_ready(f) == 0 && reload_capture_epoch(f, &rt->first) == 0);
    return 0;
}

static int rt_button(fixture *f, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = SDL_BUTTON_RIGHT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    rt->right_down = down;
    return 0;
}

static int rt_plot(fixture *f, int32_t support_id, int32_t target_id, int32_t *stage) {
    mc_chunk_state *chunk = rt_chunk(f);
    int32_t target[3], support[3], plate[3], dust[3];
    CHECK(chunk != NULL);
    rt_cell(chunk, 3, 65, 2, target);
    rt_cell(chunk, 3, 65, 3, support);
    rt_cell(chunk, 3, 66, 3, plate);
    rt_cell(chunk, 4, 65, 2, dust);
    CHECK(support[0] == support_id && target[0] == target_id &&
          target[1] == (target_id == 0 ? 0 : 4) && plate[0] == 70 && plate[1] == 1 &&
          dust[0] == 55);
    if (stage != NULL)
        *stage = target[1];
    return 0;
}

static int rt_hit(fixture *f, int32_t y, int32_t id) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    mc_chunk_state *chunk = rt_chunk(f);
    int32_t support[3];
    CHECK(chunk != NULL);
    rt_cell(chunk, 3, 65, 3, support);
    CHECK(f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
          mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
          hit.fields.a == kinds.a && hit.fields.b == 3 && hit.fields.c == y && hit.fields.d == 3 &&
          hit.fields.e == 2 && support[0] == id);
    return 0;
}

static mc_client_source_dk *rt_section(fixture *f) {
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

static int rt_settle(fixture *f, mc_client_source_dk *section) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        if (section->fields.E && !section->fields.u)
            return 0;
        CHECK(rt_frame(f) == 0);
    }
    fprintf(stderr,
            "RT_GUI_SETTLE_FAILURE section=%p compiled=%d dirty=%d world=%p "
            "expected-world=%p frame=%p frame-returned=%d\n",
            (const void *)section, section != NULL && section->fields.E,
            section != NULL && section->fields.u,
            section == NULL ? NULL : (const void *)section->fields.a,
            (const void *)f->app->active_world, (const void *)f->state.world_frame,
            f->state.world_frame != NULL && f->state.world_frame->returned);
    return 1;
}

static int rt_light_ready(fixture *f, const char *phase, bool present) {
    (void)phase;
    (void)present;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        mc_chunk_state *chunk = rt_chunk(f);
        int32_t target[3];
        CHECK(chunk != NULL);
        rt_cell(chunk, 3, 65, 2, target);
        if (f->app->active_world->fields.C->count == 0 && f->app->active_world->fields.M == 0 &&
            target[2] == 0)
            return 0;
        CHECK(rt_frame(f) == 0);
    }
    return 1;
}

static int rt_draw(fixture *f, const mc_item_stack *selected) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        const mc_client_world_frame_state *draw = f->state.world_frame;
        const mc_client_first_person_hand_state *hand = draw == NULL ? NULL : draw->hand;
        const mc_client_held_item_draw_state *held = hand == NULL ? NULL : hand->held_item;
        mc_client_entity_renderer_state renderer;
        mc_client_held_item_state live;
        const bool identity = mc_client_entity_renderer_inspect(f->app->t, &renderer) &&
                              renderer.ra_occurrence != NULL &&
                              mc_client_held_item_inspect(renderer.ra_occurrence, &live) &&
                              live.ra_app == f->app && live.ra_b == selected && live.ra_g == 0 &&
                              live.ra_c == 1.0f;
        if (draw != NULL && draw->returned && draw->source_pc == 1013 && draw->terrain_returned &&
            draw->updates_returned && draw->terrain_count > 0 && hand != NULL && hand->returned &&
            hand->held_returned && held != NULL && held->returned && !held->empty_hand &&
            held->flat_item && held->item_color_returned && held->color_set &&
            held->item_icon_returned && held->item_icon == 99 && held->item_geometry_returned &&
            identity && f->state.hud != NULL && f->state.hud->returned &&
            f->state.hud->slots_returned && f->state.hud->health == 5 &&
            f->state.hud->selected == 0 && f->state.hud->item_slots == 1 &&
            f->state.hud->empty_slots == 8)
            return 0;
        CHECK(rt_frame(f) == 0);
    }
    fprintf(stderr,
            "RT_GUI_DRAW_FAILURE kind=%s screen=%p focus=%d paused=%d world-draws=%" PRIu64
            " hud-draws=%" PRIu64 "\n",
            rt->kind, (void *)f->app->r, f->app->N, atomic_load(&f->app->o), f->state.world_draws,
            f->state.hud_draws);
    return 1;
}

static int rt_batch(fixture *f, mc_client_source_dk *section, const char *phase,
                    uintptr_t *identity) {
    mc_client_source_list_state definition;
    mc_client_source_batch_view batch;
    CHECK(section != NULL && section->fields.a == f->app->active_world && section->fields.E &&
          !section->fields.u &&
          mc_client_display_list_inspect(f->app->display, section->fields.C, &definition) &&
          definition.defined && !definition.building && definition.draws == 1 &&
          definition.vertices > 0 &&
          mc_client_display_list_batch(f->app->display, section->fields.C, 0, &batch) &&
          batch.mode == 4 && batch.arrays == 7 && batch.vertices > 0 &&
          batch.byte_count == (size_t)batch.vertices * 32u &&
          f->state.world_frame->terrain_count > 0);
    if (identity != NULL)
        *identity = (uintptr_t)batch.bytes;
    char name[96], path[8192];
    const int n = snprintf(name, sizeof(name), "rt-geometry-%s.bin", phase);
    CHECK(n > 0 && (size_t)n < sizeof(name) && rt_path(name, path) == 0 &&
          SDL_SaveFile(path, batch.bytes, batch.byte_count));
    printf("RT_GUI_GEOMETRY phase=%s vertices=%d bytes=%zu arrays=7 mode=4 terrain-returned=1 "
           "type2-inclusion=source-control-flow\n",
           phase, batch.vertices, batch.byte_count);
    return 0;
}

static int rt_tap(fixture *f) {
    const uint64_t tick_before = f->state.ticks, deadline = SDL_GetTicks() + 60000;
    int64_t age = INT64_MAX;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        int64_t now;
        CHECK(rt_frame(f) == 0 && mc_platform_current_time_millis(&now) == MC_PLATFORM_MILLIS_OK);
        age = mc_java_i64_sub(now, f->app->P);
        if (f->state.ticks > tick_before && age <= 100)
            break;
    }
    CHECK(f->state.ticks > tick_before && age <= 100 && f->app->r == NULL && f->app->N &&
          rt_hit(f, 65, 1) == 0 && rt_plot(f, 1, 0, NULL) == 0);
    const uint64_t clicks = f->state.gameplay_right_clicks, air = f->state.gameplay_air_uses,
                   blocks = f->state.gameplay_block_uses,
                   admitted = f->state.gameplay_placements_admitted,
                   used = f->state.gameplay_used_statistics,
                   counts = f->state.gameplay_stack_count_stores,
                   placed = f->state.gameplay_placed_blocks,
                   sounds = f->state.gameplay_placement_sounds,
                   clears = f->state.gameplay_selected_slot_clears,
                   equip = f->state.gameplay_equip_resets;
    const size_t events = f->state.mouse_events;
    rt->started = true;
    rt->phase = "input";
    CHECK(rt_button(f, true) == 0 && rt_button(f, false) == 0);
    bool drained = false, direct_suffix = false;
    for (size_t frames = 0; frames < 64 && SDL_GetTicks() < deadline; ++frames) {
        CHECK(rt_frame(f) == 0);
        const mc_client_gameplay_input *input = f->state.gameplay_input;
        if (input != NULL && input->block_uses != 0) {
            const mc_singleplayer_use_state *s = &input->use_state;
            CHECK(input->returned && !input->failed && input->use_call == NULL && s->item == NULL &&
                  s->returned && s->result && s->item_returned && s->placement_admitted &&
                  s->setter_returned && s->placed && s->sound_returned && s->count_stored &&
                  s->statistic_returned && s->block_id == rt->id && s->metadata == 0 &&
                  s->count_before == 2 && s->count_after == 1 && s->x == 3 && s->y == 65 &&
                  s->z == 2 && s->face == 2);
            direct_suffix = true;
        }
        mc_client_sdk_input_state input_state;
        mc_client_sdk_bytes_state read, queue, buttons;
        CHECK(
            mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input_state) &&
            mc_client_sdk_bytes_inspect(input_state.mouse_read, &read) &&
            mc_client_sdk_queue_inspect(input_state.mouse_events, &queue) &&
            mc_client_sdk_bytes_inspect(input_state.buttons, &buttons) && buttons.limit > 1 &&
            buttons.data[1] == 0);
        drained = f->state.mouse_events >= events + 2 && read.position == read.limit &&
                  queue.position == 0 && input_state.pending_native_event == NULL;
        if (drained && f->state.gameplay_placed_blocks > placed && rt->off_seen)
            break;
    }
    CHECK(rt->lit_seen && rt->scheduled_seen && rt->off_seen && drained &&
          f->state.gameplay_right_clicks == clicks + 1 && f->state.gameplay_air_uses == air &&
          f->state.gameplay_block_uses == blocks + 1 &&
          f->state.gameplay_placements_admitted == admitted + 1 &&
          f->state.gameplay_used_statistics == used + 1 &&
          f->state.gameplay_stack_count_stores == counts + 1 &&
          f->state.gameplay_placed_blocks == placed + 1 &&
          f->state.gameplay_placement_sounds == sounds + 1 &&
          f->state.gameplay_selected_slot_clears == clears &&
          f->state.gameplay_equip_resets == equip + 1);
    /* Sound/count/statistic completion follows both placed hooks in CK. Only
     * value diagnostics survive native child retirement; no hook is replayed. */
    printf("RT_GUI_INPUT kind=%s taps=1 right-clicks=1 block-uses=1 air-uses=0 admitted=1 "
           "placed=1 sounds=1 count-stores=1 used=1 cleared=0 equip-reset=1 drained=1 "
           "placed-hooks=source-control-flow onAdded-before-face=source-control-flow "
           "direct-suffix=%d metadata=0 target=3,65,2 face=2\n",
           rt->kind, direct_suffix);
    return 0;
}

static int rt_run(fixture *f) {
    rt->phase = "loading";
    CHECK(repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0 &&
          rt_choose_first(f) == 0 && rt_inventory(f, 2, NULL) == 0 && rt_statistic(f, 0) == 0 &&
          rt_plot(f, 1, 0, NULL) == 0);
    const mc_block *block = NULL;
    mc_block_render_description description;
    mc_entity_snapshot pose;
    mc_game_settings_state settings;
    CHECK(mc_registry_block_get(f->app->registry, 76, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && mc_block_render_describe(block, &description) == MC_REGISTRY_OK &&
          description.type == 2 && mc_entity_get_snapshot(f->app->h, &pose) == MC_ENTITY_OK &&
          pose.bounds != NULL && pose.bounds->min_y == 65 && pose.base.x == 3.5 &&
          pose.base.z == .5 && pose.base.y == 65.0 + (double)1.62f && pose.base.yaw == 0 &&
          pose.base.aT == 26 && mc_game_settings_read_state(f->app->settings, &settings) &&
          settings.values.difficulty == 2);
    const mc_item_stack *before = NULL, *current = NULL;
    uintptr_t initial_bytes = 0, off_bytes = 0;
    mc_client_source_dk *section = rt_section(f);
    rt->phase = "before";
    CHECK(rt_inventory(f, 2, &before) == 0 && rt_draw(f, before) == 0 && section != NULL &&
          rt_settle(f, section) == 0 && rt_batch(f, section, "before", &initial_bytes) == 0 &&
          rt_player_nbt(f->app->h, "rt-player-before.nbt") == 0 && rt_tap(f) == 0 &&
          rt_inventory(f, 1, &current) == 0 && current == before && rt_statistic(f, 1) == 0 &&
          rt_plot(f, 1, 75, NULL) == 0);
    rt->phase = "off";
    CHECK(rt_light_ready(f, "off", false) == 0 && rt_draw(f, current) == 0 &&
          rt_settle(f, section) == 0 && rt_batch(f, section, "off", &off_bytes) == 0 &&
          initial_bytes != off_bytes && rt_player_nbt(f->app->h, "rt-player-off.nbt") == 0);
    const uint64_t draws = f->state.world_draws, hud = f->state.hud_draws;
    rt->phase = "continuing";
    for (unsigned int i = 0; i < 3; ++i)
        CHECK(rt_frame(f) == 0 && rt_draw(f, current) == 0 && rt_plot(f, 1, 75, NULL) == 0);
    CHECK(f->state.world_draws >= draws + 3 && f->state.hud_draws >= hud + 3);
    char path[8192];
    CHECK(rt_path("rt-off.png", path) == 0 && capture(f, path) == 0);
    printf("RT_GUI_RETURN item=76 block=75 metadata=4 count-before=2 count-after=1 damage=0 "
           "health=5 same-stack=1 used=1 hud=1 held=1 continuing=3 future-due=%" PRId64 "\n",
           rt->scheduled_time);
    rt->saving = true;
    rt->phase = "saving";
    CHECK(rt_path("rt-save-title.png", path) == 0);
    const int saved = save_title_run(f, NULL, path, false, 1, 0);
    rt->saving = false;
    CHECK(saved == 0 && rt->selected && rt->save_samples > 0 && rt->entry_health == 5 &&
          rt->entry_ground == 1 && rt->entry_above == 75 && rt->entry_stage == 4 &&
          rt->entry_plate == 1 && rt->entry_dust == 0 && rt->entry_items == 0 &&
          reload_snapshot("rt-saved") == 0);
    printf("RT_GUI_SAVE sample=%zu candidates=%zu health=5 support=1 block=75 metadata=4 plate=1 "
           "dust=0 time=%" PRId64 " source-save-title=1 pre-frame-owned-nbt=1 items=0\n",
           rt->selected_sample, rt->save_samples, rt->entry_time);
    rt->phase = "reloading";
    CHECK(reload_choose(f, f->app->r, "alpha", &rt->first, &rt->reloaded, 2) == 0 &&
          rt_ready(f) == 0 && rt_inventory(f, 1, &current) == 0 &&
          rt_light_ready(f, "reloaded", false) == 0 && rt_draw(f, current) == 0 &&
          rt_statistic(f, 1) == 0 && rt_plot(f, 1, 75, NULL) == 0 &&
          rt_player_nbt(f->app->h, "rt-player-reloaded.nbt") == 0);
    section = rt_section(f);
    rt->phase = "reloaded";
    CHECK(section != NULL && rt_settle(f, section) == 0 &&
          rt_batch(f, section, "reloaded", NULL) == 0);
    size_t items = 0;
    CHECK(rt_items(f, false, 0, &items) == 0 && rt_trace(f) == 0);
    printf(
        "RT_GUI_RELOAD distinct-world=1 distinct-player=1 health=5 support=1 block=75 metadata=4 "
        "plate=1 dust=0 item=76 count=1 damage=0 selected=0 hud=1 held=1 used=1 items=0\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[2], "powered-placement") != 0)
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    rt_work *work = calloc(1, sizeof(*work));
    if (f == NULL || work == NULL) {
        free(work);
        free(f);
        return 2;
    }
    rt = work;
    work->fixture = f;
    work->directory = argv[3];
    work->kind = "redstone-torch";
    work->phase = "bootstrap";
    work->id = 76;
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
            result = rt_run(f);
        }
    }
    work->saving = false;
    if (f->app != NULL && work->right_down)
        (void)rt_button(f, false);
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
        printf("RT_GUI_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    rt = NULL;
    free(work);
    free(f);
    return result;
}
