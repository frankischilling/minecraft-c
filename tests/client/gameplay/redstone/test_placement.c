/* Saved Dirt/Air and Item331; one SDL SDK placement and source save/reselection. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include "client/runtime/menu_loop.h"
static bool redstone_observed_frame(mc_client_menu_loop *);
#define mc_client_menu_loop_frame redstone_observed_frame
#define main mc_redstone_included_selection_main
#include "tests/client/startup/test_world_selection_loop.c"
#undef main
#undef mc_client_menu_loop_frame
#include "block/registry/registry_internal.h"
#include "mc/entity/nbt/errors.h"
#include "mc/inventory/player_current.h"
#include "mc/world/dimension/world_provider.h"

typedef struct redstone_operands {
    uintptr_t identities[8];
    int32_t texture, emission, sky, block_light, subtraction, light;
    int32_t voxels[27][2];
    uint32_t table[16], brightness;
    uint64_t random_state, gaussian;
    bool gaussian_ready;
} redstone_operands;

typedef struct redstone_work {
    fixture *fixture;
    const char *directory;
    int32_t id;
    const char *kind;
    reload_epoch first, reloaded;
    bool right_down, saving, selected;
    size_t save_samples, selected_sample;
    int32_t entry_health, entry_ground, entry_above, entry_stage;
    int32_t entry_lights[9];
    size_t entry_items;
    uintptr_t observed_world, observed_bytes, compiled_bytes;
    redstone_operands compiled_before, compiled_after;
    bool compiled_seen, compiled_paired, compiled_stable;
    uint8_t placed_suffix[12 * 32];
    bool placed_suffix_captured;
} redstone_work;

static redstone_work *redstone;
static mc_client_source_dk *redstone_section(fixture *);

/* Pure field projection. In particular, do not call EW/World brightness:
 * those Source getters may write LM.a even on an already loaded Chunk. */
static bool redstone_operands_read(fixture *f, redstone_operands *out) {
    if (f == NULL || f->app == NULL || f->app->active_world == NULL || f->app->registry == NULL)
        return false;
    mc_world_runtime *world = f->app->active_world;
    mc_world_store *store = world->fields.w;
    if (store == NULL || world->fields.t == NULL || world->fields.r == NULL)
        return false;
    mc_chunk_state *chunk = NULL;
    for (size_t i = 0; i < mc_world_store_chunk_count(store); ++i) {
        mc_chunk_state *candidate = NULL;
        if (mc_world_store_chunk_at(store, i, &candidate, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        if (candidate != NULL && candidate->x == 0 && candidate->z == 0) {
            if (chunk != NULL)
                return false;
            chunk = candidate;
        }
    }
    mc_world_provider_fields dimension;
    mc_registry_classes *registry = f->app->registry;
    if (chunk == NULL || chunk->blank || chunk->data == NULL || chunk->data->blocks == NULL ||
        registry->blocks.m == NULL || registry->blocks.m_length <= 55 ||
        registry->blocks.m[55] == NULL || registry->blocks.s == NULL ||
        mc_world_provider_fields_get(world->fields.t, &dimension) != MC_WORLD_PROVIDER_OK ||
        dimension.f == NULL)
        return false;
    const mc_block *block = registry->blocks.m[55];
    const mc_block_fields *fields = mc_block_fields_get(block);
    if (fields == NULL || fields->bn != 55)
        return false;
    redstone_operands result = {0};
    const uintptr_t identities[8] = {(uintptr_t)world,       (uintptr_t)store,
                                     (uintptr_t)chunk,       (uintptr_t)world->fields.t,
                                     (uintptr_t)dimension.f, (uintptr_t)registry,
                                     (uintptr_t)block,       (uintptr_t)registry->blocks.s};
    memcpy(result.identities, identities, sizeof(identities));
    result.texture = fields->bm;
    result.emission = registry->blocks.s[55];
    const size_t target = (3u << 11u) | (3u << 7u) | 65u;
    result.sky = (chunk->data->skylight[target / 2u] >> 4u) & 15;
    result.block_light = (chunk->data->block_light[target / 2u] >> 4u) & 15;
    result.subtraction = world->fields.f;
    result.light = mc_java_i32_sub(result.sky, result.subtraction);
    if (result.light < result.block_light)
        result.light = result.block_light;
    if (result.light < result.emission)
        result.light = result.emission;
    if (result.light < 0 || result.light >= 16)
        return false;
    for (size_t i = 0; i < 16; ++i)
        memcpy(&result.table[i], dimension.f + i, sizeof(result.table[i]));
    result.brightness = result.table[result.light];
    size_t at = 0;
    for (size_t x = 2; x <= 4; ++x)
        for (size_t y = 64; y <= 66; ++y)
            for (size_t z = 2; z <= 4; ++z) {
                const size_t cell = (x << 11u) | (z << 7u) | y;
                result.voxels[at][0] = chunk->data->blocks[cell];
                result.voxels[at++][1] =
                    (chunk->data->metadata[cell / 2u] >> ((cell & 1u) * 4u)) & 15;
            }
    result.random_state = world->fields.r->state;
    memcpy(&result.gaussian, &world->fields.r->next_next_gaussian, sizeof(result.gaussian));
    result.gaussian_ready = world->fields.r->have_next_next_gaussian;
    *out = result;
    return true;
}

static bool redstone_operands_same(const redstone_operands *a, const redstone_operands *b) {
    if (memcmp(a->identities, b->identities, sizeof(a->identities)) != 0 ||
        a->texture != b->texture || a->emission != b->emission || a->sky != b->sky ||
        a->block_light != b->block_light || a->subtraction != b->subtraction ||
        a->light != b->light || a->brightness != b->brightness ||
        memcmp(a->table, b->table, sizeof(a->table)) != 0)
        return false;
    for (size_t i = 0; i < 27; ++i) {
        if (a->voxels[i][1] != b->voxels[i][1])
            return false;
        if (i == 13) {
            if (!((a->voxels[i][0] == 0 || a->voxels[i][0] == 55) &&
                  (b->voxels[i][0] == 0 || b->voxels[i][0] == 55)) ||
                (a->voxels[i][0] == 55 && b->voxels[i][0] == 0))
                return false;
        } else if (a->voxels[i][0] != b->voxels[i][0])
            return false;
    }
    return true;
}

static void redstone_operands_json(FILE *stream, const redstone_operands *value) {
    fputs("{\"identities\":[", stream);
    for (size_t i = 0; i < 8; ++i)
        fprintf(stream, "%s%" PRIuPTR, i == 0 ? "" : ",", value->identities[i]);
    fprintf(stream,
            "],\"texture\":%" PRId32 ",\"emission\":%" PRId32 ",\"sky\":%" PRId32
            ",\"block_light\":%" PRId32 ",\"sky_subtraction\":%" PRId32 ",\"light_level\":%" PRId32
            ",\"brightness_bits\":\"%08" PRIx32 "\",\"table_bits\":[",
            value->texture, value->emission, value->sky, value->block_light, value->subtraction,
            value->light, value->brightness);
    for (size_t i = 0; i < 16; ++i)
        fprintf(stream, "%s\"%08" PRIx32 "\"", i == 0 ? "" : ",", value->table[i]);
    fputs("],\"voxels\":[", stream);
    for (size_t i = 0; i < 27; ++i)
        fprintf(stream, "%s[%" PRId32 ",%" PRId32 "]", i == 0 ? "" : ",", value->voxels[i][0],
                value->voxels[i][1]);
    fprintf(stream,
            "],\"world_random\":{\"state\":\"%012" PRIx64 "\",\"gaussian\":\"%016" PRIx64
            "\",\"ready\":%s}}",
            value->random_state, value->gaussian, value->gaussian_ready ? "true" : "false");
}

static const int32_t redstone_light_cells[9][3] = {{3, 65, 3}, {4, 65, 3}, {2, 65, 3},
                                                   {3, 65, 4}, {3, 65, 2}, {3, 66, 3},
                                                   {3, 64, 3}, {3, 67, 3}, {4, 65, 4}};
static const int32_t redstone_light_expected[9] = {0};

/* Passive Store-owned nibble reads; no acquiring query, queue mutation or drain. */
static int redstone_light_read(fixture *f, int32_t values[9]) {
    mc_world_store *store = f->app->active_world->fields.w;
    mc_chunk_state *chunk = NULL;
    CHECK(store != NULL);
    for (size_t i = 0; i < mc_world_store_chunk_count(store); ++i) {
        mc_chunk_state *candidate = NULL;
        CHECK(mc_world_store_chunk_at(store, i, &candidate, NULL) == MC_WORLD_STORAGE_OK);
        if (candidate != NULL && candidate->x == 0 && candidate->z == 0) {
            CHECK(chunk == NULL);
            chunk = candidate;
        }
    }
    CHECK(chunk != NULL && !chunk->blank && chunk->data != NULL);
    for (size_t i = 0; i < 9; ++i) {
        const size_t index = ((size_t)redstone_light_cells[i][0] << 11u) |
                             ((size_t)redstone_light_cells[i][2] << 7u) |
                             (size_t)redstone_light_cells[i][1];
        values[i] = (chunk->data->block_light[index / 2u] >> ((index & 1u) * 4u)) & 15;
    }
    return 0;
}

static int redstone_path(const char *name, char path[8192]) {
    const int n = snprintf(path, 8192, "%s/%s", redstone->directory, name);
    CHECK(n > 0 && n < 8192);
    return 0;
}

/* Passive owned-tree serialization, outside any source save or extra frame. */
static int redstone_player_nbt(mc_entity *player, const char *name) {
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
        written != bytes || redstone_path(name, path) != 0 || !SDL_SaveFile(path, wire, bytes))
        goto cleanup;
    result = 0;
cleanup:
    free(wire);
    mc_nbt_tree_free(tree);
    free(error);
    return result;
}

static int redstone_inventory(fixture *f, int32_t count, const mc_item_stack **current) {
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
                  fields.id == redstone->id && fields.count == count && fields.damage == 0);
            if (current != NULL)
                *current = stack;
        } else
            CHECK(stack == NULL);
    }
    return 0;
}

static int redstone_statistic(fixture *f, int32_t expected) {
    mc_registry_statistic_view view;
    int32_t used = -1;
    CHECK(mc_registry_classes_inspect_statistics(f->app->registry, &view, NULL) == MC_REGISTRY_OK &&
          view.E != NULL && view.E[redstone->id] != NULL &&
          mc_client_stats_writer_value(f->app->stats_writer, view.E[redstone->id], &used, NULL) ==
              MC_CLIENT_STATS_OK &&
          used == expected);
    return 0;
}

/* Sample the exact paused entry of the frame that actually selects Save/Quit.
 * Reads and owned NBT serialization balance their native holds. */
/* Count every live Item owner; this isolated unpowered scene authors no drop. */
static int redstone_items(fixture *f, size_t *count) {
    const mc_world_reference_list *list = f->app->active_world->fields.b;
    CHECK(list != NULL && list->count <= list->capacity && list->count <= 4096);
    *count = 0;
    for (size_t i = 0; i < list->count; ++i) {
        const mc_entity *entity = list->items[i];
        if (entity != NULL && mc_entity_is_kind(entity, MC_ENTITY_CLASS_KIND_ITEM) &&
            !entity->base.be)
            ++*count;
    }
    CHECK(*count == 0);
    return 0;
}

static int redstone_entry(fixture *f) {
    mc_chunk_state *chunk = NULL;
    const size_t count = mc_world_store_chunk_count(redstone->first.handler);
    for (size_t i = 0; i < count; ++i) {
        mc_chunk_state *candidate = NULL;
        CHECK(mc_world_store_chunk_at(redstone->first.handler, i, &candidate, NULL) ==
              MC_WORLD_STORAGE_OK);
        if (candidate != NULL && candidate->x == 0 && candidate->z == 0) {
            CHECK(chunk == NULL);
            chunk = candidate;
        }
    }
    mc_local_player_snapshot player;
    const size_t index = (3u << 11u) | (3u << 7u) | 64u;
    CHECK(chunk != NULL && !chunk->blank && chunk->data != NULL && chunk->data->blocks != NULL &&
          chunk->data->metadata != NULL &&
          mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK);
    redstone->entry_health = player.living.Y;
    redstone->entry_ground = chunk->data->blocks[index];
    redstone->entry_above = chunk->data->blocks[index + 1u];
    redstone->entry_stage = chunk->data->metadata[index / 2u] >> 4u;
    CHECK(redstone_light_read(f, redstone->entry_lights) == 0);
    char name[96];
    const int n =
        snprintf(name, sizeof(name), "redstone-player-entry-%04zu.nbt", redstone->save_samples);
    CHECK(n > 0 && (size_t)n < sizeof(name) && redstone_player_nbt(f->app->h, name) == 0);
    CHECK(redstone_items(f, &redstone->entry_items) == 0 && redstone->entry_items == 0);
    ++redstone->save_samples;
    return 0;
}

static bool redstone_observed_frame(mc_client_menu_loop *loop) {
    fixture *f = redstone == NULL ? NULL : redstone->fixture;
    redstone_operands before = {0};
    const bool have_before = redstone_operands_read(f, &before);
    mc_client_source_dk *prior_section = have_before ? redstone_section(f) : NULL;
    const bool dirty_before = prior_section != NULL && prior_section->fields.u;
    const bool sample = redstone != NULL && redstone->saving && !redstone->selected && f != NULL &&
                        f->loop == loop && f->app->active_world == redstone->first.world &&
                        f->app->h == redstone->first.player && f->app->r != NULL &&
                        f->app->r->kind == MC_CLIENT_SCREEN_PAUSE && !f->app->N &&
                        atomic_load(&f->app->o);
    if (sample && redstone_entry(f) != 0)
        return false;
    const bool returned = mc_client_menu_loop_frame(loop);
    redstone_operands after;
    if (returned && redstone_operands_read(f, &after)) {
        mc_client_source_dk *section = redstone_section(f);
        mc_client_source_list_state list;
        mc_client_source_batch_view batch;
        if (section != NULL && section->fields.E && !section->fields.u &&
            mc_client_display_list_inspect(f->app->display, section->fields.C, &list) &&
            list.defined && !list.building && list.draws == 1 &&
            mc_client_display_list_batch(f->app->display, section->fields.C, 0, &batch)) {
            const uintptr_t bytes = (uintptr_t)batch.bytes;
            if (batch.vertices == 72 && after.voxels[13][0] == 55 &&
                (redstone->observed_world != after.identities[0] ||
                 redstone->observed_bytes != bytes || dirty_before)) {
                redstone->compiled_seen = true;
                redstone->compiled_bytes = bytes;
                redstone->compiled_before = before;
                redstone->compiled_after = after;
                redstone->compiled_paired =
                    have_before && before.identities[0] == after.identities[0];
                redstone->compiled_stable =
                    redstone->compiled_paired && redstone_operands_same(&before, &after);
                if (redstone->compiled_paired && !redstone->compiled_stable) {
                    fputs("REDSTONE_ITEM_OPERAND_FAILURE actual-frame-compile=1 stable=0\n",
                          stderr);
                    return false;
                }
            }
            redstone->observed_world = after.identities[0];
            redstone->observed_bytes = bytes;
        }
    }
    if (sample && returned && f->app->active_world == NULL && f->app->h == NULL &&
        f->app->r != NULL && f->app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU) {
        redstone->selected = true;
        redstone->selected_sample = redstone->save_samples - 1u;
    }
    return returned;
}

static int redstone_frame(fixture *f) {
    CHECK(save_title_focus(f) == 0 && frame(f) == 0 && !f->state.failed);
    return 0;
}

static int redstone_ready(fixture *f) {
    const uint64_t deadline = SDL_GetTicks() + 180000;
    mc_client_source_da *recovered[4] = {0};
    size_t pauses = 0;
    for (size_t i = 0; i < 180 && SDL_GetTicks() < deadline; ++i) {
        CHECK(redstone_frame(f) == 0);
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

static int redstone_choose_first(fixture *f) {
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
          transfer(f) == 0 && redstone_ready(f) == 0 &&
          reload_capture_epoch(f, &redstone->first) == 0);
    return 0;
}

static int redstone_button(fixture *f, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = SDL_BUTTON_RIGHT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    redstone->right_down = down;
    return 0;
}

static int redstone_plot(fixture *f, int32_t ground, int32_t crop, int32_t *stage) {
    int32_t id = -1, above = -1, metadata = -1;
    CHECK(mc_world_runtime_block_get(f->app->active_world, 3, 64, 3, &id, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_block_get(f->app->active_world, 3, 65, 3, &above, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_metadata_get(f->app->active_world, 3, 65, 3, &metadata, NULL) ==
              MC_WORLD_STORAGE_OK &&
          id == ground && above == crop && metadata == 0);
    if (stage != NULL)
        *stage = metadata;
    return 0;
}

static int redstone_hit(fixture *f, int32_t y, int32_t id) {
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

static mc_client_source_dk *redstone_section(fixture *f) {
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

static int redstone_settle(fixture *f, mc_client_source_dk *section) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        if (section->fields.E && !section->fields.u)
            return 0;
        CHECK(redstone_frame(f) == 0);
    }
    fprintf(stderr,
            "REDSTONE_ITEM_SETTLE_FAILURE section=%p compiled=%d dirty=%d world=%p "
            "expected-world=%p frame=%p frame-returned=%d\n",
            (const void *)section, section != NULL && section->fields.E,
            section != NULL && section->fields.u,
            section == NULL ? NULL : (const void *)section->fields.a,
            (const void *)f->app->active_world, (const void *)f->state.world_frame,
            f->state.world_frame != NULL && f->state.world_frame->returned);
    return 1;
}

static int redstone_light_ready(fixture *f, const char *phase) {
    const uint64_t deadline = SDL_GetTicks() + 60000;
    int32_t values[9] = {0};
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        CHECK(redstone_light_read(f, values) == 0 && f->app->active_world->fields.C != NULL);
        if (f->app->active_world->fields.C->count == 0 && f->app->active_world->fields.M == 0 &&
            memcmp(values, redstone_light_expected, sizeof(values)) == 0) {
            printf("REDSTONE_ITEM_LIGHT phase=%s queue=0 target=0 east=0 west=0 south=0 north=0 "
                   "up=0 down=0 up-two=0 diagonal=0 normal-frames=1\n",
                   phase);
            return 0;
        }
        CHECK(redstone_frame(f) == 0);
    }
    fprintf(stderr, "REDSTONE_ITEM_LIGHT_FAILURE queue=%zu M=%" PRId32 "\n",
            f->app->active_world->fields.C == NULL ? 0 : f->app->active_world->fields.C->count,
            f->app->active_world->fields.M);
    return 1;
}

static int redstone_draw(fixture *f, const mc_item_stack *selected) {
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
            held->stack == NULL && held->flat_item && held->item_color_returned &&
            held->color_set && held->item_icon_returned && held->item_icon == 56 &&
            held->item_geometry_returned && identity && f->state.hud != NULL &&
            f->state.hud->returned && f->state.hud->slots_returned && f->state.hud->health == 5 &&
            f->state.hud->selected == 0 && f->state.hud->item_slots == 1 &&
            f->state.hud->empty_slots == 8)
            return 0;
        CHECK(redstone_frame(f) == 0);
    }
    fprintf(stderr,
            "REDSTONE_ITEM_DRAW_FAILURE kind=%s screen=%p focus=%d paused=%d world-draws=%" PRIu64
            " hud-draws=%" PRIu64 "\n",
            redstone->kind, (void *)f->app->r, f->app->N, atomic_load(&f->app->o),
            f->state.world_draws, f->state.hud_draws);
    return 1;
}

static int redstone_batch(fixture *f, mc_client_source_dk *section, const char *phase, bool present,
                          uintptr_t *identity) {
    redstone_operands before, after;
    CHECK(redstone_operands_read(f, &before));
    mc_client_source_list_state definition;
    mc_client_source_batch_view batch;
    const size_t vertices = present ? 72u : 60u;
    CHECK(section != NULL && section->fields.a == f->app->active_world && section->fields.E &&
          section->fields.o && !section->fields.u && section->fields.p != NULL &&
          !section->fields.p[0] && section->fields.p[1] &&
          mc_client_display_list_inspect(f->app->display, section->fields.C, &definition) &&
          definition.defined && !definition.building && definition.draws == 1 &&
          definition.vertices == vertices &&
          mc_client_display_list_batch(f->app->display, section->fields.C, 0, &batch) &&
          batch.mode == 4 && batch.arrays == 7 && batch.vertices == (int32_t)vertices &&
          batch.byte_count == vertices * 32u && f->state.world_frame->terrain_count > 0);
    if (identity != NULL)
        *identity = (uintptr_t)batch.bytes;
    char name[96], path[8192];
    const int n = snprintf(name, sizeof(name), "redstone-geometry-%s.bin", phase);
    CHECK(n > 0 && (size_t)n < sizeof(name) && redstone_path(name, path) == 0 &&
          SDL_SaveFile(path, batch.bytes, batch.byte_count));
    CHECK(redstone_operands_read(f, &after) && redstone_operands_same(&before, &after) &&
          before.voxels[13][0] == after.voxels[13][0] &&
          after.voxels[13][0] == (present ? 55 : 0) && after.voxels[13][1] == 0 &&
          after.texture == 164 && after.emission == 0 && after.block_light == 0);
    const bool placed = strcmp(phase, "placed") == 0;
    const bool paired =
        present && redstone->compiled_seen && redstone->compiled_bytes == (uintptr_t)batch.bytes &&
        redstone->compiled_after.identities[0] == after.identities[0] && redstone->compiled_paired;
    if (placed)
        CHECK(paired && redstone->compiled_stable &&
              redstone_operands_same(&redstone->compiled_after, &after));
    if (present) {
        for (size_t i = 0; i < 12; ++i) {
            const uint8_t *actual = batch.bytes + (60u + i) * 32u;
            if (placed)
                memcpy(redstone->placed_suffix + i * 32u, actual, 32u);
            else
                CHECK(redstone->placed_suffix_captured &&
                      memcmp(redstone->placed_suffix + i * 32u, actual, 24u) == 0);
        }
        if (placed)
            redstone->placed_suffix_captured = true;
    }
    const int opn = snprintf(name, sizeof(name), "redstone-operands-%s.json", phase);
    CHECK(opn > 0 && (size_t)opn < sizeof(name) && redstone_path(name, path) == 0);
    FILE *operands = fopen(path, "wb");
    CHECK(operands != NULL);
    fprintf(operands,
            "{\"phase\":\"%s\",\"batch_identity\":%" PRIuPTR
            ",\"initial_compile_paired\":%s,\"current_before\":",
            phase, (uintptr_t)batch.bytes, paired ? "true" : "false");
    redstone_operands_json(operands, &before);
    fputs(",\"current_after\":", operands);
    redstone_operands_json(operands, &after);
    fputs(",\"compile_before\":", operands);
    if (present && paired)
        redstone_operands_json(operands, &redstone->compiled_before);
    else
        fputs("null", operands);
    fputs(",\"compile_after\":", operands);
    if (present && redstone->compiled_seen && redstone->compiled_bytes == (uintptr_t)batch.bytes &&
        redstone->compiled_after.identities[0] == after.identities[0])
        redstone_operands_json(operands, &redstone->compiled_after);
    else
        fputs("null", operands);
    fputs("}\n", operands);
    const bool wrote = !ferror(operands);
    CHECK(fclose(operands) == 0 && wrote);
    printf("REDSTONE_ITEM_GEOMETRY phase=%s vertices=%zu redstone-vertices=%u prefix=%u arrays=7 "
           "mode=4 source-list-returned=1 terrain-returned=1 initial-compile-paired=%d\n",
           phase, vertices, present ? 12u : 0u, 60u, paired);
    return 0;
}

static int redstone_tap(fixture *f) {
    const uint64_t tick_before = f->state.ticks, deadline = SDL_GetTicks() + 60000;
    int64_t age = INT64_MAX;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        int64_t now;
        CHECK(redstone_frame(f) == 0 &&
              mc_platform_current_time_millis(&now) == MC_PLATFORM_MILLIS_OK);
        age = mc_java_i64_sub(now, f->app->P);
        if (f->state.ticks > tick_before && age <= 100)
            break;
    }
    CHECK(f->state.ticks > tick_before && age <= 100 && f->app->r == NULL && f->app->N &&
          redstone_hit(f, 64, 3) == 0 && redstone_plot(f, 3, 0, NULL) == 0);
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
    CHECK(redstone_button(f, true) == 0 && redstone_button(f, false) == 0);
    bool drained = false, direct_suffix = false;
    for (size_t frames = 0; frames < 64 && SDL_GetTicks() < deadline; ++frames) {
        CHECK(redstone_frame(f) == 0);
        const mc_client_gameplay_input *input = f->state.gameplay_input;
        if (input != NULL && input->block_uses != 0) {
            const mc_singleplayer_use_state *s = &input->use_state;
            CHECK(input->returned && !input->failed && input->use_call == NULL && s->item == NULL &&
                  s->returned && s->result && s->item_returned && s->placement_admitted &&
                  s->setter_returned && s->placed && !s->sound_returned && s->count_read &&
                  s->count_stored && s->statistic_returned && s->block_id == 55 &&
                  s->metadata == 0 && s->count_before == 2 && s->count_after == 1 && s->x == 3 &&
                  s->y == 65 && s->z == 3 && s->face == 1);
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
        if (drained && f->state.gameplay_placed_blocks > placed)
            break;
    }
    CHECK(drained && f->state.gameplay_right_clicks == clicks + 1 &&
          f->state.gameplay_air_uses == air && f->state.gameplay_block_uses == blocks + 1 &&
          f->state.gameplay_placements_admitted == admitted + 1 &&
          f->state.gameplay_used_statistics == used + 1 &&
          f->state.gameplay_stack_count_stores == counts + 1 &&
          f->state.gameplay_placed_blocks == placed + 1 &&
          f->state.gameplay_placement_sounds == sounds &&
          f->state.gameplay_selected_slot_clears == clears &&
          f->state.gameplay_equip_resets == equip + 1);
    /* Completed copied IE suffix: count precedes notifying setter; no sound
     * or placed-face/by hook is authored or invoked by this observer. */
    printf("REDSTONE_ITEM_INPUT kind=%s taps=1 right-clicks=1 block-uses=1 air-uses=0 admitted=1 "
           "placed=1 sounds=0 count-stores=1 used=1 cleared=0 equip-reset=1 drained=1 "
           "item=331 block=55 sdk-events=2 "
           "direct-suffix=%d metadata=0 target=3,65,3 face=1\n",
           redstone->kind, direct_suffix);
    return 0;
}

static int redstone_run(fixture *f) {
    CHECK(repeat_statistics_controls(f, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) && ticks(f, 2) == 0 &&
          redstone_choose_first(f) == 0 && redstone_inventory(f, 2, NULL) == 0 &&
          redstone_statistic(f, 0) == 0 && redstone_plot(f, 3, 0, NULL) == 0);
    const mc_block *block = NULL;
    mc_block_render_description description;
    int32_t initial_light[9];
    CHECK(mc_registry_block_get(f->app->registry, 55, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL && mc_block_render_describe(block, &description) == MC_REGISTRY_OK &&
          description.type == 5 && redstone_light_read(f, initial_light) == 0);
    for (size_t i = 0; i < 9; ++i)
        CHECK(initial_light[i] == 0);
    mc_entity_snapshot pose;
    mc_game_settings_state settings;
    CHECK(mc_entity_get_snapshot(f->app->h, &pose) == MC_ENTITY_OK && pose.bounds != NULL &&
          pose.bounds->min_y == 65 && pose.base.x == 3.5 && pose.base.z == .5 &&
          pose.base.y == 65.0 + (double)1.62f && pose.base.yaw == 0 && pose.base.aT == 26 &&
          mc_game_settings_read_state(f->app->settings, &settings) &&
          settings.values.difficulty == 2 && f->app->active_world->fields.q == 2);
    const mc_item_stack *before_stack = NULL, *current = NULL;
    mc_client_source_dk *section = redstone_section(f);
    uintptr_t before_bytes, placed_bytes;
    CHECK(redstone_inventory(f, 2, &before_stack) == 0 && redstone_draw(f, before_stack) == 0 &&
          section != NULL && redstone_settle(f, section) == 0 &&
          redstone_batch(f, section, "before", false, &before_bytes) == 0 &&
          redstone_player_nbt(f->app->h, "redstone-player-before.nbt") == 0 &&
          redstone_tap(f) == 0 && redstone_inventory(f, 1, &current) == 0 &&
          current == before_stack && redstone_statistic(f, 1) == 0);
    int32_t stage;
    CHECK(redstone_plot(f, 3, 55, &stage) == 0 && stage == 0 &&
          redstone_light_ready(f, "placed") == 0 && redstone_draw(f, current) == 0 &&
          redstone_settle(f, section) == 0 &&
          redstone_batch(f, section, "placed", true, &placed_bytes) == 0 &&
          placed_bytes != before_bytes &&
          redstone_player_nbt(f->app->h, "redstone-player-placed.nbt") == 0);
    const uint64_t draws = f->state.world_draws, hud = f->state.hud_draws;
    for (unsigned int i = 0; i < 3; ++i)
        CHECK(redstone_frame(f) == 0 && redstone_draw(f, current) == 0 &&
              redstone_plot(f, 3, 55, &stage) == 0);
    CHECK(f->state.world_draws >= draws + 3 && f->state.hud_draws >= hud + 3);
    char path[8192];
    CHECK(redstone_path("redstone-placed.png", path) == 0 && capture(f, path) == 0);
    printf("REDSTONE_ITEM_RETURN kind=redstone ground=3 item=331 block=55 stage=0 "
           "count-before=2 count-after=1 damage=0 health=5 same-stack=1 selected=0 "
           "hud=1 held=1 continuing=3 used=1 list-replaced=1\n");
    redstone->saving = true;
    CHECK(redstone_path("redstone-save-title.png", path) == 0);
    const int saved = save_title_run(f, NULL, path, false, 1, 0);
    redstone->saving = false;
    CHECK(saved == 0 && redstone->selected && redstone->save_samples > 0 &&
          redstone->entry_health == 5 && redstone->entry_ground == 3 &&
          redstone->entry_above == 55 && redstone->entry_stage == 0 &&
          memcmp(redstone->entry_lights, redstone_light_expected, sizeof(redstone->entry_lights)) ==
              0 &&
          reload_snapshot("redstone-saved") == 0);
    printf("REDSTONE_ITEM_SAVE sample=%zu candidates=%zu health=5 ground=3 block=55 stage=0 "
           "source-save-title=1 pre-frame-owned-nbt=1 blocklight=0 items=%zu\n",
           redstone->selected_sample, redstone->save_samples, redstone->entry_items);
    CHECK(reload_choose(f, f->app->r, "alpha", &redstone->first, &redstone->reloaded, 2) == 0 &&
          redstone_ready(f) == 0 && redstone_inventory(f, 1, &current) == 0 &&
          redstone_light_ready(f, "reloaded") == 0 && redstone_draw(f, current) == 0 &&
          redstone_statistic(f, 1) == 0 && redstone_plot(f, 3, 55, &stage) == 0 && stage == 0 &&
          redstone_player_nbt(f->app->h, "redstone-player-reloaded.nbt") == 0);
    section = redstone_section(f);
    CHECK(section != NULL && redstone_settle(f, section) == 0 &&
          redstone_batch(f, section, "reloaded", true, NULL) == 0);
    size_t items;
    CHECK(redstone_items(f, &items) == 0 && items == 0);
    printf("REDSTONE_ITEM_RELOAD distinct-world=1 distinct-player=1 health=5 ground=3 "
           "block=55 item=331 count=1 damage=0 selected=0 hud=1 held=1 used=1 stage=0 items=0\n");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4 || strcmp(argv[2], "placement") != 0)
        return 2;
    fixture *f = calloc(1, sizeof(*f));
    redstone_work *work = calloc(1, sizeof(*work));
    if (f == NULL || work == NULL) {
        free(work);
        free(f);
        return 2;
    }
    redstone = work;
    work->fixture = f;
    work->directory = argv[3];
    work->kind = "redstone";
    work->id = 331;
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
            result = redstone_run(f);
        }
    }
    work->saving = false;
    if (result != 0 && work->compiled_seen) {
        char failure_path[8192];
        if (redstone_path("redstone-operands-failure.json", failure_path) == 0) {
            FILE *failure = fopen(failure_path, "wb");
            if (failure != NULL) {
                fprintf(failure, "{\"paired\":%s,\"stable\":%s,\"before\":",
                        work->compiled_paired ? "true" : "false",
                        work->compiled_stable ? "true" : "false");
                redstone_operands_json(failure, &work->compiled_before);
                fputs(",\"after\":", failure);
                redstone_operands_json(failure, &work->compiled_after);
                fputs("}\n", failure);
                (void)fclose(failure);
            }
        }
    }
    if (f->app != NULL && work->right_down)
        (void)redstone_button(f, false);
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
    mc_client_bootstrap_status closed = MC_CLIENT_BOOTSTRAP_BUSY;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    const bool owners_null = f->bootstrap == NULL && f->loop == NULL;
    const bool windows_absent = frame_id != 0 && canvas_id != 0 &&
                                SDL_GetWindowFromID(frame_id) == NULL &&
                                SDL_GetWindowFromID(canvas_id) == NULL;
    printf("REDSTONE_ITEM_CLEANUP status=%d owners-null=%d windows-absent=%d\n", (int)closed,
           owners_null, windows_absent);
    if (closed != MC_CLIENT_BOOTSTRAP_OK || !owners_null || !windows_absent)
        result = 1;
    if (result == 0)
        printf("REDSTONE_ITEM_FINISH cleanup=1 windows-absent=1 checks=%u\n", checks);
    redstone = NULL;
    free(work);
    free(f);
    return result;
}
