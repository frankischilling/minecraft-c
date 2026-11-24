/* Authored saved Stone/Dust and Item69; actual SDK placement, both click
 * activation paths, returned terrain/outline and ordinary save/reselection. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/runtime/menu_loop.h"
static bool lever_observed_frame(mc_client_menu_loop *);
#define mc_client_menu_loop_frame lever_observed_frame
#define main mc_lever_included_selection_main
#include "tests/client/startup/test_world_selection_loop.c"
#undef main
#undef mc_client_menu_loop_frame
#include "client/particles/effect_renderer_internal.h"
#include "mc/entity/nbt/errors.h"
#include "mc/inventory/player_current.h"
#include "mc/world/info/info.h"
#include "mc/world/schedule/membership.h"
#include "mc/world/schedule/ordered.h"

typedef struct lever_work {
    fixture *fixture;
    const char *directory, *phase, *kind;
    int32_t id;
    reload_epoch first, reloaded;
    bool right_down, left_down, saving, selected;
    int64_t entry_time;
    size_t trace_count, save_samples, selected_sample, entry_items;
    int32_t entry_health, entry_ground, entry_above, entry_stage, entry_dust;
} lever_work;

static lever_work *lever;

/* Passive Store-owned cells and scalar/collection inventories only. */
static mc_chunk_state *lever_chunk(fixture *f) {
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

static void lever_cell(const mc_chunk_state *chunk, int32_t x, int32_t y, int32_t z,
                       int32_t out[3]) {
    const size_t at = ((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y;
    out[0] = chunk->data->blocks[at];
    out[1] = (chunk->data->metadata[at / 2u] >> ((at & 1u) * 4u)) & 15;
    out[2] = (chunk->data->block_light[at / 2u] >> ((at & 1u) * 4u)) & 15;
}

/* Read actual typed DN memberships without creating a particle or ticking.
 * Zero records finite natural-witness availability, not a source result. */
static int lever_redstone_particles(fixture *f, size_t *out) {
    mc_client_effect_renderer *effects = f->app->effect_renderer;
    mc_effect_renderer_snapshot state;
    CHECK(effects != NULL && mc_client_effect_renderer_read(effects, &state) &&
          state.phase == MC_EFFECT_RENDERER_READY && effects->layers != NULL);
    size_t count = 0;
    for (size_t layer = 0; layer < 4; ++layer) {
        const mc_effect_particle_list *list = effects->layers[layer];
        CHECK(list != NULL && list->size == state.sizes[layer] && list->size <= list->capacity);
        for (size_t i = 0; i < list->size; ++i) {
            const mc_entity *particle = list->elements[i];
            mc_entity_class_literal_view literal;
            CHECK(particle != NULL && particle->phase == MC_ENTITY_READY &&
                  mc_entity_class_literal_inspect(particle->class_literal, &literal) ==
                      MC_ENTITY_OK &&
                  literal.owner == f->app->entities && literal.client_name != NULL);
            if (strcmp(literal.client_name, "im") == 0 && !particle->base.be)
                ++count;
        }
    }
    *out = count;
    return 0;
}

static int lever_trace(fixture *f) {
    mc_chunk_state *chunk = lever_chunk(f);
    if (chunk == NULL)
        return 0;
    mc_world_runtime *world = f->app->active_world;
    int64_t time = 0;
    size_t redstone_particles = 0;
    int32_t target[3], support[3], dust[3], en = -1, fn = -1;
    CHECK(world->fields.r != NULL && world->fields.C != NULL &&
          lever_redstone_particles(f, &redstone_particles) == 0 &&
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
    lever_cell(chunk, 3, 65, 2, target);
    lever_cell(chunk, 3, 65, 3, support);
    lever_cell(chunk, 4, 65, 2, dust);
    uint64_t gaussian;
    memcpy(&gaussian, &world->fields.r->next_next_gaussian, sizeof(gaussian));
    printf("LEVER_GUI_STATE\t{\"index\":%zu,\"phase\":\"%s\",\"world\":\"%" PRIxPTR
           "\",\"time\":%" PRId64 ",\"frames\":%" PRIu64 ",\"ticks\":%" PRIu64
           ",\"world_ticks\":%" PRIu64 ",\"focus\":%d,\"paused\":%d,\"target\":[%d,%d,%d],"
           "\"support\":[%d,%d,%d],\"dust\":[%d,%d,%d],\"light_count\":%zu,\"M\":%" PRId32
           ",\"skylight_subtraction\":%" PRId32 ",\"redstone_particles\":%zu,"
           "\"random\":{\"seed\":\"%012" PRIx64 "\",\"gaussian\":\"%016" PRIx64
           "\",\"have\":%s},\"E\":[",
           lever->trace_count++, lever->phase, (uintptr_t)world, time, f->state.frames,
           f->state.ticks, f->state.world_tick_returns, f->app->N, atomic_load(&f->app->o),
           target[0], target[1], target[2], support[0], support[1], support[2], dust[0], dust[1],
           dust[2], world->fields.C->count, world->fields.M, world->fields.f, redstone_particles,
           world->fields.r->state, gaussian,
           world->fields.r->have_next_next_gaussian ? "true" : "false");
    for (unsigned int list = 0; list < 2; ++list) {
        const mc_scheduled_tick **entries = list == 0 ? ep : fp;
        const size_t count = list == 0 ? ec : fc;
        if (list != 0)
            fputs("],\"F\":[", stdout);
        for (size_t i = 0; i < count; ++i) {
            mc_scheduled_tick_fields value;
            if (mc_scheduled_tick_fields_get(entries[i], &value) != MC_SCHEDULED_TICK_OK) {
                free(ep);
                free(fp);
                return 1;
            }
            printf("%s[\"%" PRIxPTR "\",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId64
                   ",%" PRId64 "]",
                   i == 0 ? "" : ",", (uintptr_t)entries[i], value.x, value.y, value.z, value.block,
                   value.due_time, value.sequence);
        }
    }
    fputs("]}\n", stdout);
    free(ep);
    free(fp);
    return ferror(stdout) ? 1 : 0;
}

static int lever_path(const char *name, char path[8192]) {
    const int n = snprintf(path, 8192, "%s/%s", lever->directory, name);
    CHECK(n > 0 && n < 8192);
    return 0;
}

/* Passive owned-tree serialization, outside any source save or extra frame. */
static int lever_player_nbt(mc_entity *player, const char *name) {
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
        written != bytes || lever_path(name, path) != 0 || !SDL_SaveFile(path, wire, bytes))
        goto cleanup;
    result = 0;
cleanup:
    free(wire);
    mc_nbt_tree_free(tree);
    free(error);
    return result;
}

static int lever_inventory(fixture *f, int32_t count, const mc_item_stack **current) {
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
                  fields.id == lever->id && fields.count == count && fields.damage == 0);
            if (current != NULL)
                *current = stack;
        } else
            CHECK(stack == NULL);
    }
    return 0;
}

static int lever_statistic(fixture *f, int32_t expected) {
    mc_registry_statistic_view view;
    int32_t used = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &view, NULL) == MC_REGISTRY_OK &&
          view.E != NULL && view.E[lever->id] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, view.E[lever->id], &used, NULL) ==
              MC_CLIENT_STATS_OK &&
          used == expected);
    return 0;
}

/* Sample the exact paused entry of the frame that actually selects Save/Quit.
 * Reads and owned NBT serialization balance their native holds. */
static int lever_items(fixture *f, bool save, size_t sample, size_t *count) {
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

static int lever_entry(fixture *f) {
    mc_chunk_state *chunk = lever_chunk(f);
    mc_local_player_snapshot player;
    int32_t target[3], support[3], dust[3];
    CHECK(chunk != NULL && mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          mc_world_info_time_get(f->app->active_world->fields.x, &lever->entry_time) ==
              MC_WORLD_INFO_OK);
    lever_cell(chunk, 3, 65, 2, target);
    lever_cell(chunk, 3, 65, 3, support);
    lever_cell(chunk, 4, 65, 2, dust);
    lever->entry_health = player.living.Y;
    lever->entry_ground = support[0];
    lever->entry_above = target[0];
    lever->entry_stage = target[1];
    lever->entry_dust = dust[1];
    CHECK(target[2] == 0);
    char name[96];
    const int n = snprintf(name, sizeof(name), "lever-player-entry-%04zu.nbt", lever->save_samples);
    CHECK(n > 0 && (size_t)n < sizeof(name) && lever_player_nbt(f->app->h, name) == 0 &&
          lever_items(f, false, 0, &lever->entry_items) == 0);
    ++lever->save_samples;
    return 0;
}

static bool lever_observed_frame(mc_client_menu_loop *loop) {
    fixture *f = lever == NULL ? NULL : lever->fixture;
    const bool sample = lever != NULL && lever->saving && !lever->selected && f != NULL &&
                        f->loop == loop && f->app->active_world == lever->first.world &&
                        f->app->h == lever->first.player && f->app->r != NULL &&
                        f->app->r->kind == MC_CLIENT_SCREEN_PAUSE && !f->app->N &&
                        atomic_load(&f->app->o);
    if (sample && lever_entry(f) != 0)
        return false;
    const bool returned = mc_client_menu_loop_frame(loop);
    if (sample && returned && f->app->active_world == NULL && f->app->h == NULL &&
        f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU) {
        lever->selected = true;
        lever->selected_sample = lever->save_samples - 1u;
    }
    return returned;
}

static int lever_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0);
    const int result = frame(f);
    if (result != 0 || f->state.failed)
        fprintf(stderr,
                "LEVER_GUI_FAILURE phase=%s source=%s.%s pc=%" PRId64 " ticks=%" PRIu64
                " world-ticks=%" PRIu64 " operation=%s\n",
                lever->phase, f->state.source_class == NULL ? "NULL" : f->state.source_class,
                f->state.source_method == NULL ? "NULL" : f->state.source_method,
                f->state.source_pc, f->state.ticks, f->state.world_tick_returns,
                f->state.operation == NULL ? "NULL" : f->state.operation);
    CHECK(result == 0 && !f->state.failed && lever_trace(f) == 0);
    return 0;
}

static int lever_ready(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    for (size_t i = 0; i < 180 && SDL_GetTicks() < deadline; ++i) {
        CHECK(lever_frame(f) == 0);
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

static int lever_choose_first(fixture *f) {
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
          transfer(f) == 0 && lever_ready(f) == 0 && reload_capture_epoch(f, &lever->first) == 0);
    return 0;
}

static int lever_button(fixture *f, bool left, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = left ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    if (left)
        lever->left_down = down;
    else
        lever->right_down = down;
    return 0;
}

static bool lever_plot_matches(fixture *f, bool placed, int32_t metadata, int32_t power) {
    mc_chunk_state *chunk = lever_chunk(f);
    if (chunk == NULL)
        return false;
    int32_t target[3], support[3], dust[3];
    lever_cell(chunk, 3, 65, 2, target);
    lever_cell(chunk, 3, 65, 3, support);
    lever_cell(chunk, 4, 65, 2, dust);
    return support[0] == 1 && target[0] == (placed ? 69 : 0) && target[1] == metadata &&
           target[2] == 0 && dust[0] == 55 && dust[1] == power;
}

static int lever_hit(fixture *f, bool placed) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    CHECK(f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
          mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
          hit.fields.a == kinds.a && hit.fields.b == 3 && hit.fields.c == 65 &&
          hit.fields.d == (placed ? 2 : 3) && hit.fields.e == 2);
    const mc_client_world_frame_state *draw = f->state.world_frame;
    CHECK(draw != NULL && draw->returned && draw->picking_source_returned &&
          draw->picking_block_published && draw->picking_result_identity == (uintptr_t)f->app->y &&
          draw->selection_hit_present && draw->selection_current_returned &&
          draw->selection_calls == 1 && draw->selections[0] != NULL);
    const mc_client_selection_state *selection = draw->selections[0];
    CHECK(selection->outline_returned && selection->outline_block && selection->shape_returned &&
          selection->selected_returned && selection->expanded_returned &&
          selection->shifted_returned && selection->alpha_restored &&
          selection->block_id == (placed ? 69 : 1) && selection->x == 3 && selection->y == 65 &&
          selection->z == (placed ? 2 : 3) && selection->vertices == 18 && selection->draws == 3);
    return 0;
}

static mc_client_source_dk *lever_section(fixture *f) {
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

static int lever_settle(fixture *f, mc_client_source_dk *section) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        if (section->fields.E && !section->fields.u)
            return 0;
        CHECK(lever_frame(f) == 0);
    }
    fprintf(stderr,
            "LEVER_GUI_SETTLE_FAILURE section=%p compiled=%d dirty=%d world=%p "
            "expected-world=%p frame=%p frame-returned=%d\n",
            (const void *)section, section != NULL && section->fields.E,
            section != NULL && section->fields.u,
            section == NULL ? NULL : (const void *)section->fields.a,
            (const void *)f->app->active_world, (const void *)f->state.world_frame,
            f->state.world_frame != NULL && f->state.world_frame->returned);
    return 1;
}

static int lever_light_ready(fixture *f, const char *phase, bool present) {
    (void)phase;
    (void)present;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        mc_chunk_state *chunk = lever_chunk(f);
        int32_t target[3];
        CHECK(chunk != NULL);
        lever_cell(chunk, 3, 65, 2, target);
        if (f->app->active_world->fields.C->count == 0 && f->app->active_world->fields.M == 0 &&
            target[2] == 0)
            return 0;
        CHECK(lever_frame(f) == 0);
    }
    return 1;
}

static int lever_draw(fixture *f, const mc_item_stack *selected) {
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
            held->item_icon_returned && held->item_icon == 96 && held->item_geometry_returned &&
            identity && f->state.hud != NULL && f->state.hud->returned &&
            f->state.hud->slots_returned && f->state.hud->health == 5 &&
            f->state.hud->selected == 0 && f->state.hud->item_slots == 1 &&
            f->state.hud->empty_slots == 8)
            return 0;
        CHECK(lever_frame(f) == 0);
    }
    fprintf(stderr,
            "LEVER_GUI_DRAW_FAILURE kind=%s screen=%p focus=%d paused=%d world-draws=%" PRIu64
            " hud-draws=%" PRIu64 "\n",
            lever->kind, (void *)f->app->r, f->app->N, atomic_load(&f->app->o),
            f->state.world_draws, f->state.hud_draws);
    return 1;
}

static int lever_batch(fixture *f, mc_client_source_dk *section, const char *phase,
                       uintptr_t *identity) {
    mc_client_source_list_state definition;
    mc_client_source_batch_view batch;
    mc_game_settings_state settings;
    CHECK(section != NULL && section->fields.a == f->app->active_world && section->fields.E &&
          !section->fields.u &&
          mc_client_display_list_inspect(f->app->display, section->fields.C, &definition) &&
          definition.defined && !definition.building && definition.draws == 1 &&
          definition.vertices > 0 &&
          mc_client_display_list_batch(f->app->display, section->fields.C, 0, &batch) &&
          batch.mode == 4 && batch.arrays == 7 && batch.vertices > 0 &&
          batch.byte_count == (size_t)batch.vertices * 32u &&
          f->state.world_frame->terrain_count > 0 &&
          mc_game_settings_read_state(f->app->settings, &settings) &&
          settings.values.ambient_occlusion);
    if (identity != NULL)
        *identity = (uintptr_t)batch.bytes;
    char name[96], path[8192];
    int n = snprintf(name, sizeof(name), "lever-geometry-%s.bin", phase);
    CHECK(n > 0 && (size_t)n < sizeof(name) && lever_path(name, path) == 0 &&
          SDL_SaveFile(path, batch.bytes, batch.byte_count));
    mc_chunk_state *chunk = lever_chunk(f);
    CHECK(chunk != NULL);
    const size_t bytes = MC_CHUNK_BLOCKS + 3u * MC_CHUNK_NIBBLE_BYTES + MC_CHUNK_HEIGHTMAP_BYTES;
    uint8_t *cells = malloc(bytes);
    CHECK(cells != NULL);
    size_t cursor = 0;
    memcpy(cells + cursor, chunk->data->blocks, MC_CHUNK_BLOCKS);
    cursor += MC_CHUNK_BLOCKS;
    memcpy(cells + cursor, chunk->data->metadata, MC_CHUNK_NIBBLE_BYTES);
    cursor += MC_CHUNK_NIBBLE_BYTES;
    memcpy(cells + cursor, chunk->data->skylight, MC_CHUNK_NIBBLE_BYTES);
    cursor += MC_CHUNK_NIBBLE_BYTES;
    memcpy(cells + cursor, chunk->data->block_light, MC_CHUNK_NIBBLE_BYTES);
    cursor += MC_CHUNK_NIBBLE_BYTES;
    memcpy(cells + cursor, chunk->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES);
    n = snprintf(name, sizeof(name), "lever-cells-%s.bin", phase);
    bool saved = n > 0 && (size_t)n < sizeof(name) && lever_path(name, path) == 0 &&
                 SDL_SaveFile(path, cells, bytes);
    free(cells);
    CHECK(saved && lever_hit(f, strcmp(phase, "before") != 0) == 0);
    const mc_client_selection_state *selection = f->state.world_frame->selections[0];
    for (unsigned int i = 0; i < 3; ++i) {
        const mc_client_selection_outline_batch *outline = &selection->outline_batches[i];
        CHECK(outline->captured && outline->words > 0 && outline->words <= 64 &&
              outline->vertices > 0 && outline->vertices <= 8);
        n = snprintf(name, sizeof(name), "lever-outline-%s-%u.bin", phase, i);
        CHECK(n > 0 && (size_t)n < sizeof(name) && lever_path(name, path) == 0 &&
              SDL_SaveFile(path, outline->raw_words, outline->words * sizeof(uint32_t)));
        printf("LEVER_GUI_OUTLINE phase=%s batch=%u mode=%" PRId32
               " vertices=%zu words=%zu submitted-bytes=%zu captured=1\n",
               phase, i, outline->mode, outline->vertices, outline->words,
               outline->submitted_bytes);
    }
    uint64_t camera[3];
    memcpy(camera, selection->camera, sizeof(camera));
    printf("LEVER_GUI_PICK phase=%s id=%" PRId32 " xyz=3,65,%d face=2 outline=1 vertices=18 "
           "draws=3 shape=1 selected=1 expanded=1 shifted=1 camera-x=%016" PRIx64
           " camera-y=%016" PRIx64 " camera-z=%016" PRIx64 "\n",
           phase, selection->block_id, strcmp(phase, "before") == 0 ? 3 : 2, camera[0], camera[1],
           camera[2]);
    printf("LEVER_GUI_GEOMETRY phase=%s vertices=%d bytes=%zu arrays=7 mode=4 "
           "terrain-returned=1 type12-inclusion=source-control-flow "
           "skylight-subtraction=%" PRId32 " ao=1 section=0,64,0\n",
           phase, batch.vertices, batch.byte_count, f->app->active_world->fields.f);
    return ferror(stdout) ? 1 : 0;
}

static int lever_tap(fixture *f, const char *action, bool left, bool placement,
                     int32_t before_metadata, int32_t before_dust, int32_t metadata, int32_t dust) {
    const uint64_t tick_before = f->state.ticks, deadline = SDL_GetTicks() + 60000;
    int64_t age = INT64_MAX;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        int64_t now;
        CHECK(lever_frame(f) == 0 &&
              mc_platform_current_time_millis(&now) == MC_PLATFORM_MILLIS_OK);
        age = mc_java_i64_sub(now, f->app->P);
        if (f->state.ticks > tick_before && age <= 100)
            break;
    }
    CHECK(f->state.ticks > tick_before && age <= 100 && f->app->r == NULL && f->app->N &&
          lever_hit(f, !placement) == 0 &&
          lever_plot_matches(f, !placement, before_metadata, before_dust));
    const uint64_t clicks = f->state.gameplay_right_clicks, air = f->state.gameplay_air_uses,
                   blocks = f->state.gameplay_block_uses,
                   admitted = f->state.gameplay_placements_admitted,
                   used = f->state.gameplay_used_statistics,
                   counts = f->state.gameplay_stack_count_stores,
                   placed = f->state.gameplay_placed_blocks,
                   sounds = f->state.gameplay_placement_sounds,
                   clears = f->state.gameplay_selected_slot_clears,
                   equip = f->state.gameplay_equip_resets,
                   left_clicks = f->state.gameplay_left_clicks,
                   swings = f->state.gameplay_swing_calls, starts = f->state.gameplay_mining_starts,
                   progress = f->state.gameplay_mining_progress_calls,
                   mined = f->state.gameplay_mined_blocks;
    CHECK(lever->trace_count > 0);
    const size_t before_index = lever->trace_count - 1u;
    const size_t events = f->state.mouse_events;
    lever->phase = "input";
    CHECK(lever_button(f, left, true) == 0 && lever_button(f, left, false) == 0);
    bool drained = false, direct_suffix = false;
    for (size_t frames = 0; frames < 64 && SDL_GetTicks() < deadline; ++frames) {
        CHECK(lever_frame(f) == 0);
        const mc_client_gameplay_input *input = f->state.gameplay_input;
        if (input != NULL && input->block_uses != 0 && !left) {
            const mc_singleplayer_use_state *state = &input->use_state;
            CHECK(input->returned && !input->failed && input->use_call == NULL &&
                  state->item == NULL && state->returned && state->result);
            if (placement) {
                CHECK(state->item_returned && state->placement_admitted && state->setter_returned &&
                      state->placed && state->sound_returned && state->count_stored &&
                      state->statistic_returned && state->block_id == 69 && state->metadata == 0 &&
                      state->count_before == 2 && state->count_after == 1 && state->x == 3 &&
                      state->y == 65 && state->z == 2 && state->face == 2);
            } else {
                CHECK(state->activation_entered && state->activation_returned && state->activated &&
                      !state->item_entered && !state->item_returned && !state->placement_admitted &&
                      !state->placed && !state->count_stored && !state->statistic_returned &&
                      state->clicked_id == 69 && state->clicked_x == 3 && state->clicked_y == 65 &&
                      state->clicked_z == 2);
            }
            direct_suffix = true;
        }
        if (input != NULL && input->mining_starts != 0 && left) {
            const mc_singleplayer_mining_state *state = &input->mining_state;
            CHECK(input->returned && !input->failed && input->mining_call == NULL &&
                  state->returned && state->initial_hook_returned && state->block_id == 69 &&
                  state->action == MC_SINGLEPLAYER_MINING_START && !state->removal_returned &&
                  !state->progress_stored && state->x == 3 && state->y == 65 && state->z == 2);
            direct_suffix = true;
        }
        mc_client_sdk_input_state input_state;
        mc_client_sdk_bytes_state read, queue, buttons;
        CHECK(
            mc_client_sdk_input_inspect(mc_client_display_inputs(f->app->display), &input_state) &&
            mc_client_sdk_bytes_inspect(input_state.mouse_read, &read) &&
            mc_client_sdk_queue_inspect(input_state.mouse_events, &queue) &&
            mc_client_sdk_bytes_inspect(input_state.buttons, &buttons) && buttons.limit > 1 &&
            buttons.data[0] == 0 && buttons.data[1] == 0);
        drained = f->state.mouse_events >= events + 2 && read.position == read.limit &&
                  queue.position == 0 && input_state.pending_native_event == NULL;
        const bool reached =
            left ? f->state.gameplay_mining_starts > starts : f->state.gameplay_block_uses > blocks;
        if (drained && reached && lever_plot_matches(f, true, metadata, dust))
            break;
    }
    const uint64_t right_delta = left ? 0u : 1u, left_delta = left ? 1u : 0u;
    const uint64_t place_delta = placement ? 1u : 0u;
    CHECK(drained && lever_plot_matches(f, true, metadata, dust) &&
          f->state.gameplay_right_clicks == clicks + right_delta &&
          f->state.gameplay_air_uses == air &&
          f->state.gameplay_block_uses == blocks + right_delta &&
          f->state.gameplay_left_clicks == left_clicks + left_delta &&
          f->state.gameplay_swing_calls == swings + 1u &&
          f->state.gameplay_mining_starts == starts + left_delta &&
          f->state.gameplay_mining_progress_calls == progress &&
          f->state.gameplay_mined_blocks == mined &&
          f->state.gameplay_placements_admitted == admitted + place_delta &&
          f->state.gameplay_used_statistics == used + place_delta &&
          f->state.gameplay_stack_count_stores == counts + place_delta &&
          f->state.gameplay_placed_blocks == placed + place_delta &&
          f->state.gameplay_placement_sounds == sounds + place_delta &&
          f->state.gameplay_selected_slot_clears == clears &&
          f->state.gameplay_equip_resets == equip + place_delta);
    CHECK(lever->trace_count > before_index + 1u);
    printf(
        "LEVER_GUI_INPUT action=%s button=%s before-index=%zu after-index=%zu right-clicks=%" PRIu64
        " left-clicks=%" PRIu64 " block-uses=%" PRIu64 " air-uses=0 admitted=%" PRIu64
        " placed=%" PRIu64 " sounds=%" PRIu64 " count-stores=%" PRIu64 " used=%" PRIu64
        " equip-reset=%" PRIu64 " cleared=0 swing=%" PRIu64 " mining-starts=%" PRIu64
        " mining-progress=0 mined=0 direct-suffix=%d target-metadata=%" PRId32 " dust=%" PRId32
        " drained=1\n",
        action, left ? "left" : "right", before_index, lever->trace_count - 1u, right_delta,
        left_delta, right_delta, place_delta, place_delta, place_delta, place_delta, place_delta,
        place_delta, UINT64_C(1), left_delta, direct_suffix, metadata, dust);
    return ferror(stdout) ? 1 : 0;
}

static int lever_stage(fixture *f, const char *phase, mc_client_source_dk *section,
                       const mc_item_stack *current, int32_t metadata, int32_t dust) {
    lever->phase = phase;
    CHECK(lever_inventory(f, 1, NULL) == 0 && lever_statistic(f, 1) == 0 &&
          lever_plot_matches(f, true, metadata, dust) && lever_light_ready(f, phase, false) == 0 &&
          lever_draw(f, current) == 0 && lever_settle(f, section) == 0 &&
          lever_batch(f, section, phase, NULL) == 0);
    return 0;
}

static int lever_run(fixture *f) {
    lever->phase = "loading";
    CHECK(repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0 &&
          lever_choose_first(f) == 0 && lever_inventory(f, 2, NULL) == 0 &&
          lever_statistic(f, 0) == 0 && lever_plot_matches(f, false, 0, 0));
    const mc_block *block = NULL;
    mc_block_render_description description;
    mc_entity_snapshot pose;
    mc_game_settings_state settings;
    CHECK(mc_registry_block_get(f->app->registry, 69, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && mc_block_render_describe(block, &description) == MC_REGISTRY_OK &&
          description.type == 12 && mc_entity_get_snapshot(f->app->h, &pose) == MC_ENTITY_OK &&
          pose.bounds != NULL && pose.bounds->min_y == 65 && pose.base.x == 3.5 &&
          pose.base.z == .5 && pose.base.y == 65.0 + (double)1.62f && pose.base.yaw == 0 &&
          pose.base.aT == 26 && mc_game_settings_read_state(f->app->settings, &settings) &&
          settings.values.difficulty == 2 && settings.values.ambient_occlusion);
    const mc_item_stack *before = NULL, *current = NULL;
    mc_client_source_dk *section = lever_section(f);
    lever->phase = "before";
    CHECK(lever_inventory(f, 2, &before) == 0 && lever_draw(f, before) == 0 && section != NULL &&
          lever_settle(f, section) == 0 && lever_batch(f, section, "before", NULL) == 0 &&
          lever_player_nbt(f->app->h, "lever-player-before.nbt") == 0 &&
          lever_tap(f, "place", false, true, 0, 0, 4, 0) == 0 &&
          lever_inventory(f, 1, &current) == 0 && current == before &&
          lever_stage(f, "placed", section, current, 4, 0) == 0 &&
          lever_tap(f, "on", false, false, 4, 0, 12, 15) == 0 &&
          lever_stage(f, "on", section, current, 12, 15) == 0 &&
          lever_tap(f, "off", true, false, 12, 15, 4, 0) == 0 &&
          lever_stage(f, "off", section, current, 4, 0) == 0 &&
          lever_tap(f, "powered", false, false, 4, 0, 12, 15) == 0 &&
          lever_stage(f, "powered", section, current, 12, 15) == 0 &&
          lever_player_nbt(f->app->h, "lever-player-on.nbt") == 0);
    const uint64_t draws = f->state.world_draws, hud = f->state.hud_draws;
    lever->phase = "continuing";
    for (unsigned int i = 0; i < 3; ++i)
        CHECK(lever_frame(f) == 0 && lever_draw(f, current) == 0 &&
              lever_plot_matches(f, true, 12, 15) && lever_hit(f, true) == 0);
    CHECK(f->state.world_draws >= draws + 3 && f->state.hud_draws >= hud + 3);
    char path[8192];
    CHECK(lever_path("lever-powered.png", path) == 0 && capture(f, path) == 0);
    printf("LEVER_GUI_RETURN item=69 block=69 metadata=12 dust=15 count-before=2 count-after=1 "
           "damage=0 health=5 same-stack=1 used=1 hud=1 held=1 continuing=3 "
           "right-clicks=3 left-clicks=1\n");
    lever->saving = true;
    lever->phase = "saving";
    CHECK(lever_path("lever-save-title.png", path) == 0);
    const int saved = save_title_run(f, NULL, path, false, 1, 0);
    lever->saving = false;
    CHECK(saved == 0 && lever->selected && lever->save_samples > 0 && lever->entry_health == 5 &&
          lever->entry_ground == 1 && lever->entry_above == 69 && lever->entry_stage == 12 &&
          lever->entry_dust == 15 && lever->entry_items == 0 &&
          reload_snapshot("lever-saved") == 0);
    printf("LEVER_GUI_SAVE sample=%zu candidates=%zu health=5 support=1 block=69 metadata=12 "
           "dust=15 time=%" PRId64 " source-save-title=1 pre-frame-owned-nbt=1 items=0\n",
           lever->selected_sample, lever->save_samples, lever->entry_time);
    lever->phase = "reloading";
    CHECK(reload_choose(f, f->app->r, "alpha", &lever->first, &lever->reloaded, 2) == 0 &&
          lever_ready(f) == 0 && lever_inventory(f, 1, &current) == 0 &&
          lever_light_ready(f, "reloaded", false) == 0 && lever_draw(f, current) == 0 &&
          lever_statistic(f, 1) == 0 && lever_plot_matches(f, true, 12, 15) &&
          lever_player_nbt(f->app->h, "lever-player-reloaded.nbt") == 0);
    section = lever_section(f);
    lever->phase = "reloaded";
    CHECK(section != NULL && lever_settle(f, section) == 0 &&
          lever_batch(f, section, "reloaded", NULL) == 0);
    size_t items = 0;
    CHECK(lever_items(f, false, 0, &items) == 0 && lever_trace(f) == 0);
    printf("LEVER_GUI_RELOAD distinct-world=1 distinct-player=1 health=5 support=1 block=69 "
           "metadata=12 dust=15 item=69 count=1 damage=0 selected=0 hud=1 held=1 used=1 items=0\n");
    return ferror(stdout) ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[2], "wall-circuit") != 0)
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    lever_work *work = calloc(1, sizeof(*work));
    if (f == NULL || work == NULL) {
        free(work);
        free(f);
        return 2;
    }
    lever = work;
    work->fixture = f;
    work->directory = argv[3];
    work->kind = "lever";
    work->phase = "bootstrap";
    work->id = 69;
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
            result = lever_run(f);
        }
    }
    work->saving = false;
    if (f->app != NULL && work->right_down)
        (void)lever_button(f, false, false);
    if (f->app != NULL && work->left_down)
        (void)lever_button(f, true, false);
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
        printf("LEVER_GUI_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    lever = NULL;
    free(work);
    free(f);
    return result;
}
