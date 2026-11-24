/* Saved stable Lever/Piston. Every moving Tile/progress comes from real input. */
int mc_piston_included_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_piston_included_mining_main
#include "gameplay/test_mining.c"
#undef MC_MINING_FIXTURE_MAIN

#include "world/runtime/tile_entities/tile_entities_internal.h"

static struct {
    const char *phase;
    size_t extending_draws, retracting_draws, frames;
    bool failed, down;
} piston_sdk;

static mc_chunk_state *piston_chunk(fixture *f) {
    mc_world_runtime *world = f->app->active_world;
    if (world == NULL || world->fields.w == NULL)
        return NULL;
    for (size_t i = 0; i < mc_world_store_chunk_count(world->fields.w); ++i) {
        mc_chunk_state *chunk = NULL;
        if (mc_world_store_chunk_at(world->fields.w, i, &chunk, NULL) != MC_WORLD_STORAGE_OK)
            return NULL;
        if (chunk != NULL && chunk->x == 0 && chunk->z == 0 && !chunk->blank && chunk->data != NULL)
            return chunk;
    }
    return NULL;
}

static bool piston_cell(mc_chunk_state *chunk, int32_t x, int32_t y, int32_t z, int32_t id,
                        int32_t metadata) {
    const size_t at = ((size_t)x << 11u) | ((size_t)z << 7u) | (size_t)y;
    return chunk != NULL && chunk->data->blocks[at] == id &&
           ((chunk->data->metadata[at / 2u] >> ((at & 1u) * 4u)) & 15) == metadata;
}

static int piston_draw_record(fixture *f) {
    const mc_client_world_frame_state *world = f->state.world_frame;
    if (world == NULL || !world->returned || world->world != f->app->active_world ||
        world->entities == NULL || !world->entities_returned || f->app->r != NULL || !f->app->N ||
        atomic_load(&f->app->o))
        return 0;
    const mc_client_render_global_tile_receipt *draw = &world->entities->last_tile_draw;
    if (!draw->valid || draw->renderer_class == NULL || strcmp(draw->renderer_class, "hy") != 0)
        return 0;
    CHECK(world->source_pc == 1013 && world->entities->returned &&
          world->entities->source_pc == 569 && f->state.hud != NULL && f->state.hud->returned &&
          draw->returned && draw->retired && draw->renderer_returned &&
          draw->piston_snapshot_valid && draw->tile_identity != 0 && draw->piston_facing == 3);
    if (draw->piston_skipped)
        return 0;
    CHECK(draw->piston_cv_identity != 0 && draw->tessellator_begun && draw->tessellator_drawn &&
          draw->piston_vertices > 0 && draw->piston_translation[0] == 0 &&
          draw->piston_translation[1] == 0 && draw->piston_translation[2] == 0);
    /* The returned LL snapshot is tied back to the same actual held Tile in
     * N's existing provenance ledger. No acquiring Tile getter is introduced. */
    mc_tile_entity *tile = NULL;
    for (const mc_render_global_tile_occurrence *h = f->app->g->tile_occurrences; h != NULL;
         h = h->next)
        if (h->active && (uintptr_t)h->tile == draw->tile_identity)
            tile = h->tile;
    mc_tile_entity_snapshot state;
    CHECK(tile != NULL && mc_tile_entity_get_snapshot(tile, &state) == MC_TILE_ENTITY_OK &&
          state.kind == MC_TILE_ENTITY_PISTON && state.world == f->app->active_world &&
          !state.invalid && state.x == 4 && state.y == 65 &&
          state.piston_progress_bits == draw->piston_current_progress_bits &&
          state.piston_previous_progress_bits == draw->piston_previous_progress_bits &&
          state.piston_block_id == draw->piston_block_id &&
          state.piston_block_data == draw->piston_metadata &&
          state.piston_extending == draw->piston_extending &&
          state.piston_render_head == draw->piston_render_head);
    if (draw->piston_extending) {
        CHECK(state.z >= 3 && state.z <= 4 && !state.piston_render_head);
        ++piston_sdk.extending_draws;
    } else {
        CHECK(state.z == 2 && state.piston_render_head && state.piston_block_id == 33 &&
              draw->piston_head_returned && draw->piston_base_returned);
        ++piston_sdk.retracting_draws;
    }
    printf("PISTON_DRAW index=%zu phase=%s carried=%" PRId32 " metadata=%" PRId32
           " facing=3 extending=%d render-head=%d progress=%08" PRIx32 " current=%08" PRIx32
           " previous=%08" PRIx32 " vertices=%zu tiles=%zu "
           "head=%d base=%d block=%d world-returned=1 hud-returned=1 ll-retired=1 "
           "translation-zero=1 actual-tile=1\n",
           piston_sdk.frames++, piston_sdk.phase, draw->piston_block_id, draw->piston_metadata,
           (int)draw->piston_extending, (int)draw->piston_render_head, draw->piston_progress_bits,
           draw->piston_current_progress_bits, draw->piston_previous_progress_bits,
           draw->piston_vertices, world->entities->tiles_visited, (int)draw->piston_head_returned,
           (int)draw->piston_base_returned, (int)draw->piston_block_returned);
    CHECK(fflush(stdout) == 0);
    return 0;
}

static void piston_after_frame(fixture *f, bool returned) {
    if (returned && f != NULL && mc_client_menu_loop_inspect(f->loop, &f->state) &&
        f->app->active_world != NULL && piston_draw_record(f) != 0)
        piston_sdk.failed = true;
}

static int piston_button(fixture *f, bool down) {
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.windowID = SDL_GetWindowID(mc_client_display_window(f->app->display));
    event.button.button = SDL_BUTTON_RIGHT;
    event.button.down = down;
    CHECK(SDL_PushEvent(&event) && transfer(f) == 0);
    piston_sdk.down = down;
    return 0;
}

static int piston_hit(fixture *f) {
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    CHECK(f->app->y != NULL && mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
          mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
          hit.fields.a == kinds.a && hit.fields.b == 3 && hit.fields.c == 65 && hit.fields.d == 2 &&
          hit.fields.e == 2);
    return 0;
}

static int piston_inventory(fixture *f) {
    mc_local_player_snapshot player;
    const mc_item_stack *stack = NULL;
    mc_item_stack_fields fields;
    CHECK(mc_local_player_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          player.living.Y == 5 &&
          mc_inventory_player_borrow(player.player.c, 0, &stack, NULL) == MC_INVENTORY_OK &&
          stack != NULL && mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK &&
          fields.id == 3 && fields.count == 1 && fields.damage == 0);
    return 0;
}

static void piston_prefix(fixture *f) {
    mc_world_runtime *world = f->app->active_world;
    if (world == NULL || !mc_live_tile_entities_lists_valid(world))
        return;
    printf("PISTON_PREFIX phase=%s moving-tiles=%zu actual-world-ticks=%" PRIu64 "\n",
           piston_sdk.phase, world->fields.c->count, f->state.world_tick_returns);
    for (size_t i = 0; i < world->fields.c->count; ++i) {
        const mc_tile_entity *tile = world->fields.c->items[i];
        mc_tile_entity_snapshot state;
        if (mc_tile_entity_get_snapshot(tile, &state) == MC_TILE_ENTITY_OK &&
            state.kind == MC_TILE_ENTITY_PISTON)
            printf("PISTON_PREFIX_TILE index=%zu xyz=%" PRId32 ",%" PRId32 ",%" PRId32
                   " carried=%" PRId32 " metadata=%" PRId32 " facing=%" PRId32
                   " extending=%d render-head=%d current=%08" PRIx32 " previous=%08" PRIx32 "\n",
                   i, state.x, state.y, state.z, state.piston_block_id, state.piston_block_data,
                   state.piston_facing, (int)state.piston_extending, (int)state.piston_render_head,
                   state.piston_progress_bits, state.piston_previous_progress_bits);
    }
    (void)fflush(stdout);
}

static int piston_frames(fixture *f, const char *phase, bool extended, bool pushed,
                         size_t minimum_draws) {
    piston_sdk.phase = phase;
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 256 && SDL_GetTicks() < deadline; ++i) {
        CHECK(save_title_focus(f) == 0);
        const bool returned = mining_observed_frame(f->loop);
        CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
        if (!returned) {
            piston_prefix(f);
            fprintf(stderr, "Piston SDK actual frontier %s.%s:%" PRId64 ": %s\n",
                    f->state.source_class == NULL ? "native" : f->state.source_class,
                    f->state.source_method == NULL ? "frame" : f->state.source_method,
                    f->state.source_pc, f->state.operation == NULL ? "(none)" : f->state.operation);
            return 1;
        }
        CHECK(!piston_sdk.failed);
        mc_chunk_state *chunk = piston_chunk(f);
        if (chunk == NULL || f->app->r != NULL || !f->app->N || atomic_load(&f->app->o))
            continue;
        const size_t draws = extended ? piston_sdk.extending_draws : piston_sdk.retracting_draws;
        const bool settled = piston_cell(chunk, 4, 65, 2, 33, extended ? 11 : 3) &&
                             piston_cell(chunk, 4, 65, 3, extended ? 34 : 0, extended ? 3 : 0) &&
                             piston_cell(chunk, 4, 65, 4, pushed ? 1 : 0, 0) &&
                             f->app->active_world->fields.c->count == 0;
        if (settled && draws >= minimum_draws) {
            CHECK(piston_hit(f) == 0 && piston_inventory(f) == 0);
            printf("PISTON_SETTLED phase=%s base=33 metadata=%d head=%d pushed-stone=%d "
                   "moving-tiles=0 transient-draws=%zu actual-world-ticks=%" PRIu64 "\n",
                   phase, extended ? 11 : 3, extended ? 34 : 0, (int)pushed, draws,
                   f->state.world_tick_returns);
            return 0;
        }
    }
    fprintf(stderr, "Piston SDK actual Timer opportunity did not retain a %s transient frame\n",
            phase);
    return 1;
}

static int piston_tap(fixture *f, const char *phase) {
    CHECK(piston_hit(f) == 0 && piston_inventory(f) == 0);
    const uint64_t clicks = f->state.gameplay_right_clicks, uses = f->state.gameplay_block_uses,
                   placements = f->state.gameplay_placed_blocks,
                   counts = f->state.gameplay_stack_count_stores, events = f->state.mouse_events;
    piston_sdk.phase = phase;
    printf("PISTON_TAP phase=%s sdk-down-up=1\n", phase);
    CHECK(fflush(stdout) == 0);
    CHECK(piston_button(f, true) == 0 && piston_button(f, false) == 0);
    const uint64_t deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 64 && SDL_GetTicks() < deadline; ++i) {
        const bool returned = mining_observed_frame(f->loop);
        CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
        if (!returned) {
            piston_prefix(f);
            fprintf(stderr, "Piston SDK actual input frontier %s.%s:%" PRId64 ": %s\n",
                    f->state.source_class == NULL ? "native" : f->state.source_class,
                    f->state.source_method == NULL ? "frame" : f->state.source_method,
                    f->state.source_pc, f->state.operation == NULL ? "(none)" : f->state.operation);
            return 1;
        }
        CHECK(!piston_sdk.failed);
        if (f->state.gameplay_block_uses > uses)
            break;
    }
    CHECK(f->state.gameplay_right_clicks == clicks + 1 &&
          f->state.gameplay_block_uses == uses + 1 &&
          f->state.gameplay_placed_blocks == placements &&
          f->state.gameplay_stack_count_stores == counts && f->state.mouse_events == events + 2);
    printf("PISTON_INPUT phase=%s sdk-right-click=1 block-use=1 placed=0 count-store=0 "
           "mouse-events=2\n",
           phase);
    return 0;
}

static int piston_stop(fixture *f, uint32_t frame_id) {
    SDL_Event event = {0};
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    event.window.windowID = frame_id;
    CHECK(SDL_PushEvent(&event));
    mc_client_sdk_update_error error = {0};
    for (size_t i = 0; i < 100 && atomic_load(&f->app->J); ++i) {
        CHECK(
            mc_client_sdk_display_process_messages(mc_client_display_update_owner(f->app->display),
                                                   &error) == MC_CLIENT_SDK_UPDATE_OK);
        SDL_Delay(2);
    }
    CHECK(!atomic_load(&f->app->J));
    puts("PISTON_STOP actual-frame-close=1 source-frame-retry=0");
    return 0;
}

static int piston_run(fixture *f, mining_work *work, bool pushed, uint32_t *frame_id,
                      uint32_t *canvas_id) {
    piston_sdk.phase = "loading";
    mining_extra_after_frame = piston_after_frame;
    CHECK(save_title_host(f, "open", true, frame_id, canvas_id) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && mining_choose(f, work, NULL, &work->epochs[0]) == 0);
    mc_chunk_state *chunk = piston_chunk(f);
    CHECK(chunk != NULL && piston_cell(chunk, 3, 65, 2, 69, 4) &&
          piston_cell(chunk, 4, 65, 2, 33, 3) && piston_cell(chunk, 4, 65, 3, pushed ? 1 : 0, 0) &&
          piston_cell(chunk, 4, 65, 4, 0, 0) && piston_hit(f) == 0 &&
          mining_capture(f, work, "piston-before.png") == 0 && piston_tap(f, "extend") == 0 &&
          piston_frames(f, "extend", true, pushed, 1) == 0 &&
          mining_capture(f, work, "piston-extended.png") == 0 && piston_tap(f, "retract") == 0 &&
          piston_frames(f, "retract", false, pushed, 1) == 0 &&
          mining_capture(f, work, "piston-retracted.png") == 0);
    char title[8192];
    CHECK(mining_path(work, "piston-title.png", title) == 0 &&
          save_title_run(f, NULL, title, false, 1, 0) == 0 &&
          reload_snapshot("piston-saved") == 0 &&
          mining_choose(f, work, &work->epochs[0], &work->epochs[1]) == 0);
    chunk = piston_chunk(f);
    CHECK(chunk != NULL && piston_cell(chunk, 3, 65, 2, 69, 4) &&
          piston_cell(chunk, 4, 65, 2, 33, 3) && piston_cell(chunk, 4, 65, 3, 0, 0) &&
          piston_cell(chunk, 4, 65, 4, pushed ? 1 : 0, 0) && piston_inventory(f) == 0 &&
          mining_capture(f, work, "piston-reloaded.png") == 0);
    puts("PISTON_RELOAD ordinary-save-reselection=1 distinct-world-player=1 stable-piston=1 "
         "lever-off=1 actual-inventory=1");
    CHECK(save_title_host(f, "after-piston", false, frame_id, canvas_id) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4 &&
          (strcmp(argv[3], "pushed-stone") == 0 || strcmp(argv[3], "empty-front") == 0));
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
        result = piston_run(f, &work, strcmp(argv[3], "pushed-stone") == 0, &frame_id, &canvas_id);
    }
    /* Also stop an actual failed frame, without replaying the Source frame. */
    if (f->app != NULL && frame_id != 0 && piston_stop(f, frame_id) != 0)
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
        printf(
            "PISTON_FINISH profile=%s cleanup=1 windows-absent=1 checks=%u source-field-repair=0\n",
            argv[3], checks);
    free(f);
    return result;
}
