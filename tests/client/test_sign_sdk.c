/* Saved Sign input only; all World/Timer/Font/model/LL work is the real frame. */
int mc_sign_included_mining_main(int, char **);
#define MC_MINING_FIXTURE_MAIN mc_sign_included_mining_main
#include "gameplay/test_mining.c"
#undef MC_MINING_FIXTURE_MAIN

#include "world/runtime/tile_entities/tile_entities_internal.h"

static struct {
    const char *phase, *profile;
    int32_t metadata;
    bool standing, failed;
    size_t frames, phase_frames;
} sign_sdk;

static bool sign_profile(const char *profile) {
    static const char *const names[] = {"standing-0", "standing-4", "standing-8", "standing-12",
                                        "wall-2",     "wall-3",     "wall-4",     "wall-5"};
    static const int32_t values[] = {0, 4, 8, 12, 2, 3, 4, 5};
    for (size_t i = 0; i < sizeof(values) / sizeof(*values); ++i)
        if (strcmp(profile, names[i]) == 0) {
            sign_sdk.profile = profile;
            sign_sdk.standing = i < 4;
            sign_sdk.metadata = values[i];
            return true;
        }
    return false;
}

static mc_chunk_state *sign_chunk(fixture *f) {
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

static int sign_owner(fixture *f, uintptr_t identity) {
    mc_world_runtime *world = f->app->active_world;
    CHECK(world != NULL && mc_live_tile_entities_lists_valid(world) && world->fields.c->count == 1);
    mc_tile_entity *tile = world->fields.c->items[0];
    mc_tile_entity_snapshot state;
    CHECK(tile != NULL && (uintptr_t)tile == identity &&
          mc_tile_entity_get_snapshot(tile, &state) == MC_TILE_ENTITY_OK &&
          state.kind == MC_TILE_ENTITY_SIGN && state.phase == MC_TILE_ENTITY_READY &&
          state.world == world && !state.invalid && state.x == 3 && state.y == 64 && state.z == 3 &&
          state.sign_line_count == 4 && state.sign_edit_row == -1 && !state.sign_read_flag);
    static const char *const expected[] = {"Native Sign", "Beta 1.7.3", "0123456789", ""};
    for (int32_t row = 0; row < 4; ++row) {
        mc_tile_entity_name *line = NULL;
        mc_tile_entity_name_view view;
        CHECK(mc_tile_entity_sign_get_line(tile, row, &line) == MC_TILE_ENTITY_OK);
        const mc_tile_entity_status status = mc_tile_entity_name_get_view(line, &view);
        bool equal = status == MC_TILE_ENTITY_OK && !view.is_null &&
                     view.text.length == strlen(expected[row]);
        for (size_t i = 0; equal && i < view.text.length; ++i)
            equal = view.text.units[i] == (uint16_t)(unsigned char)expected[row][i];
        mc_tile_entity_name_release(line);
        CHECK(equal);
    }
    mc_chunk_state *chunk = sign_chunk(f);
    const size_t at = (3u << 11u) | (3u << 7u) | 64u;
    CHECK(chunk != NULL && chunk->data->blocks[at] == (sign_sdk.standing ? 63 : 68) &&
          ((chunk->data->metadata[at / 2u] >> ((at & 1u) * 4u)) & 15) == sign_sdk.metadata);
    return 0;
}

static int sign_draw_record(fixture *f) {
    const mc_client_world_frame_state *world = f->state.world_frame;
    if (world == NULL || !world->returned || world->world != f->app->active_world ||
        world->entities == NULL || !world->entities_returned || f->app->r != NULL || !f->app->N ||
        atomic_load(&f->app->o) || world->entities->sign_draws_returned == 0)
        return 0;
    const mc_client_render_global_entities_state *entities = world->entities;
    const mc_client_render_global_tile_receipt *draw = &entities->last_sign_draw;
    CHECK(world->source_pc == 1013 && entities->returned && entities->source_pc == 569 &&
          f->state.hud != NULL && f->state.hud->returned && f->state.hud->source_pc == 2101 &&
          f->state.clocks.frame_returned && f->state.clocks.timer_ticks >= 0 &&
          f->state.clocks.timer_ticks <= 10 &&
          f->state.clocks.tick_returns == f->state.clocks.timer_ticks &&
          entities->tiles_visited == 1 && entities->tile_draws_attempted == 1 &&
          entities->tile_draws_returned == 1 && entities->tile_draws_retired == 1 &&
          entities->sign_draws_returned == 1 && entities->sign_draws_retired == 1 &&
          entities->sign_rows_measured == 4 && entities->sign_rows_drawn == 4 &&
          entities->sign_model_parts == 2 && draw->valid && draw->returned && draw->retired &&
          draw->renderer_returned && !draw->distance_culled && draw->renderer_class != NULL &&
          strcmp(draw->renderer_class, "po") == 0 && draw->tile_identity != 0 &&
          draw->sign_block_returned && draw->sign_metadata_returned &&
          draw->sign_standing == sign_sdk.standing && draw->sign_metadata == sign_sdk.metadata &&
          draw->sign_texture_returned && draw->sign_texture_bound && draw->sign_model_returned &&
          draw->sign_model_popped && draw->sign_font_returned && draw->sign_depth_disabled &&
          draw->sign_depth_restored && draw->sign_color_restored && draw->sign_matrix_popped &&
          draw->model_parts == 2 && draw->model_compiled_parts <= (sign_sdk.standing ? 2u : 1u) &&
          draw->sign_rows_measured == 4 && draw->sign_rows_drawn == 4 &&
          draw->sign_text_units == 31 && draw->sign_null_rows == 0 &&
          draw->sign_text_digest == UINT64_C(0x3e05f47480edd3fa) && draw->sign_last_row == 3 &&
          draw->sign_last_width == 0 && draw->sign_last_x == 0 && draw->sign_last_y == 10 &&
          draw->sign_text_scale_bits == UINT32_C(0x3c360b62) &&
          sign_owner(f, draw->tile_identity) == 0);
    const float angle = sign_sdk.standing        ? (float)(sign_sdk.metadata * 360) / 16.f
                        : sign_sdk.metadata == 2 ? 180.f
                        : sign_sdk.metadata == 4 ? 90.f
                        : sign_sdk.metadata == 5 ? -90.f
                                                 : 0.f;
    uint32_t angle_bits;
    memcpy(&angle_bits, &angle, sizeof(angle_bits));
    CHECK(draw->sign_angle_bits == angle_bits);
    size_t occurrences = 0;
    for (const mc_render_global_tile_occurrence *h = f->app->g->tile_occurrences; h != NULL;
         h = h->next)
        if (h->active && (uintptr_t)h->tile == draw->tile_identity)
            ++occurrences;
    CHECK(occurrences == 1);
    printf("SIGN_DRAW index=%zu phase=%s profile=%s standing=%d metadata=%" PRId32
           " angle=%08" PRIx32 " rows-measured=4 rows-drawn=4 units=31 null-rows=0"
           " text-digest=%016" PRIx64 " model-parts=2 compiled=%u faces=%u vertices=%u"
           " tiles=1 sign-returned=1 sign-retired=1 row=3 width=0 x=0 y=10"
           " text-scale=3c360b62 texture-bound=1 depth-restored=1 color-restored=1"
           " matrix-popped=1 actual-tile=1 occurrence=1 world-returned=1 hud-returned=1"
           " timer-ticks=%" PRId32 " world-ticks=%" PRIu64 "\n",
           sign_sdk.frames++, sign_sdk.phase, sign_sdk.profile, (int)sign_sdk.standing,
           sign_sdk.metadata, angle_bits, draw->sign_text_digest, draw->model_compiled_parts,
           draw->model_faces, draw->model_vertices, f->state.clocks.timer_ticks,
           f->state.world_tick_returns);
    ++sign_sdk.phase_frames;
    CHECK(fflush(stdout) == 0);
    return 0;
}

static void sign_after_frame(fixture *f, bool returned) {
    if (returned && f != NULL && mc_client_menu_loop_inspect(f->loop, &f->state) &&
        f->app->active_world != NULL && sign_draw_record(f) != 0)
        sign_sdk.failed = true;
}

static int sign_frames(fixture *f) {
    const uint64_t before = f->state.world_tick_returns, deadline = SDL_GetTicks() + 60000;
    for (size_t i = 0; i < 256 && SDL_GetTicks() < deadline; ++i) {
        CHECK(save_title_focus(f) == 0);
        const bool returned = mining_observed_frame(f->loop);
        CHECK(mc_client_menu_loop_inspect(f->loop, &f->state));
        if (!returned) {
            fprintf(stderr, "Sign SDK actual frontier %s.%s:%" PRId64 ": %s\n",
                    f->state.source_class == NULL ? "native" : f->state.source_class,
                    f->state.source_method == NULL ? "frame" : f->state.source_method,
                    f->state.source_pc, f->state.operation == NULL ? "(none)" : f->state.operation);
            return 1;
        }
        CHECK(!sign_sdk.failed);
        if (sign_sdk.phase_frames >= 3 && f->state.world_tick_returns >= before + 2) {
            printf("SIGN_PHASE phase=%s profile=%s draws=%zu world-ticks=%" PRIu64
                   " actual-world-tick-delta=%" PRIu64 " current-text=1\n",
                   sign_sdk.phase, sign_sdk.profile, sign_sdk.phase_frames,
                   f->state.world_tick_returns, f->state.world_tick_returns - before);
            return 0;
        }
    }
    fprintf(stderr, "Sign SDK did not observe three genuine draws and two actual World ticks\n");
    return 1;
}

static int sign_stop(fixture *f, uint32_t frame_id) {
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
    puts("SIGN_STOP actual-frame-close=1 source-frame-retry=0");
    return 0;
}

static int sign_run(fixture *f, mining_work *work, uint32_t *frame_id, uint32_t *canvas_id) {
    sign_sdk.phase = "loaded";
    mining_extra_after_frame = sign_after_frame;
    CHECK(save_title_host(f, "open", true, frame_id, canvas_id) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0 &&
          ticks(f, 2) == 0 && mining_choose(f, work, NULL, &work->epochs[0]) == 0 &&
          sign_frames(f) == 0 && mining_capture(f, work, "sign-loaded.png") == 0);
    /* Disable only the test observer while the real Escape/save/title runs;
     * it does not change a Source field, Timer operand or Sign owner. */
    mining_extra_after_frame = NULL;
    char title[8192];
    CHECK(mining_path(work, "sign-title.png", title) == 0 &&
          save_title_run(f, NULL, title, false, 1, 0) == 0 && reload_snapshot("sign-saved") == 0);
    sign_sdk.phase = "reloaded";
    sign_sdk.phase_frames = 0;
    mining_extra_after_frame = sign_after_frame;
    CHECK(mining_choose(f, work, &work->epochs[0], &work->epochs[1]) == 0 && sign_frames(f) == 0 &&
          mining_capture(f, work, "sign-reloaded.png") == 0);
    mining_extra_after_frame = NULL;
    puts("SIGN_RELOAD ordinary-save-reselection=1 distinct-world-player=1 current-text=1 rows=4");
    CHECK(save_title_host(f, "after-sign", false, frame_id, canvas_id) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4 && sign_profile(argv[3]));
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
        result = sign_run(f, &work, &frame_id, &canvas_id);
    }
    if (f->app != NULL && frame_id != 0 && sign_stop(f, frame_id) != 0)
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
            "SIGN_FINISH profile=%s cleanup=1 windows-absent=1 checks=%u source-field-repair=0\n",
            argv[3], checks);
    free(f);
    return result;
}
