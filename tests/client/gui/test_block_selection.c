/* Actual source bootstrap/SDK helpers; no existing fixture body is replaced. */
int mc_selection_world_fixture_main(int, char **);
#define main mc_selection_world_fixture_main
#include "../startup/test_world_selection_loop.c"
#undef main
#include "client/display/fog_surface.h"
#include "client/world/entity_renderer/selection_internal.h"
#include "mc/entity/nbt/errors.h"

_Static_assert(sizeof(mc_entity_nbt_error) == 544, "compact Entity diagnostic contract");

typedef struct aim_work {
    mc_client_source_da *external;
    mc_entity_bounds *held;
    mc_entity_classes *held_classes;
    uint64_t held_frame;
    bool table_written;
    size_t targeted, focus_pauses;
    mc_client_source_da *recovered[4];
} aim_work;

static uint64_t aim_double(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t aim_float(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int aim_file(const char *directory, const char *name, const void *data, size_t size) {
    char path[8192];
    const int n = snprintf(path, sizeof(path), "%s/%s", directory, name);
    CHECK(n > 0 && (size_t)n < sizeof(path) && size <= 1048576);
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    const bool written = fwrite(data, 1, size, file) == size;
    const bool closed = fclose(file) == 0;
    CHECK(written && closed);
    return 0;
}

static void aim_box(const char *kind, size_t sample, const mc_entity_bounds *box) {
    printf("SELECTION_BOX sample=%zu kind=%s identity=%" PRIxPTR " values=%016" PRIx64
           ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 "\n",
           sample, kind, (uintptr_t)box, aim_double(box->min_x), aim_double(box->min_y),
           aim_double(box->min_z), aim_double(box->max_x), aim_double(box->max_y),
           aim_double(box->max_z));
}

static int aim_batches(size_t sample, const mc_client_selection_state *state) {
    for (size_t i = 0; i < 4; ++i)
        CHECK(state->bounds_cursor_captured[i] &&
              (i == 0 ||
               (int64_t)state->bounds_cursor[i] == (int64_t)state->bounds_cursor[i - 1] + 1));
    printf("SELECTION_POOL sample=%zu cursors=%" PRId32 ",%" PRId32 ",%" PRId32 ",%" PRId32
           " ordinary-child-reset=0\n",
           sample, state->bounds_cursor[0], state->bounds_cursor[1], state->bounds_cursor[2],
           state->bounds_cursor[3]);
    const void *nw = state->outline_batches[0].nw;
    CHECK(nw != NULL);
    for (size_t batch = 0; batch < 3; ++batch) {
        const mc_client_selection_outline_batch *b = &state->outline_batches[batch];
        const size_t count = batch == 2 ? 8 : 5;
        printf(
            "SELECTION_BATCH sample=%zu batch=%zu nw=%" PRIxPTR " mode=%" PRId32
            " vertices=%zu bytes=%zu byte-position=%zu line=%08" PRIx32 " color=%08" PRIx32
            ",%08" PRIx32 ",%08" PRIx32 ",%08" PRIx32
            " texture=%d alpha=%d depth-mask=%d blend=%d blend-source=%" PRId32
            " blend-destination=%" PRId32 " translation=%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64
            " uv-array=%d color-array=%d normal-array=%d color-disabled=%d fog=%d lighting=%d\n",
            sample, batch, (uintptr_t)b->nw, b->mode, b->vertices, b->submitted_bytes,
            b->byte_position, aim_float(b->line_width), aim_float(b->color[0]),
            aim_float(b->color[1]), aim_float(b->color[2]), aim_float(b->color[3]),
            b->texture_enabled, b->alpha_test, b->requested_depth_mask, b->requested_blend_enabled,
            b->requested_blend_source, b->requested_blend_destination,
            aim_double(b->translation[0]), aim_double(b->translation[1]),
            aim_double(b->translation[2]), b->texture_coordinates, b->vertex_colors, b->normals,
            b->color_disabled, b->fog_enabled, b->lighting_enabled);
        printf("SELECTION_BATCH_CAPTURE sample=%zu batch=%zu captured=%d draw=%d fog=%d "
               "words=%zu color-known=%d depth-known=%d blend-known=%d blend-function-known=%d\n",
               sample, batch, b->captured, b->draw_state_captured, b->fog_captured, b->words,
               b->color_known, b->depth_mask_known, b->blend_known, b->blend_function_known);
        CHECK(b->captured && b->draw_state_captured && b->fog_captured && b->nw == nw &&
              b->vertices == count && b->words == count * 8 && b->submitted_bytes == count * 32 &&
              b->byte_position == 0 && b->byte_limit == b->byte_capacity &&
              b->submitted_bytes <= b->byte_limit && b->mode == (batch == 2 ? 1 : 3) &&
              b->color_known && b->line_width == 2.0f && b->depth_mask_known &&
              !b->requested_depth_mask && b->blend_known && b->requested_blend_enabled &&
              b->blend_function_known && b->requested_blend_source == 770 &&
              b->requested_blend_destination == 771 && !b->texture_enabled && !b->alpha_test &&
              !b->texture_coordinates && !b->vertex_colors && !b->normals && !b->color_disabled &&
              !b->lighting_enabled && b->fog_enabled);
        for (size_t vertex = 0; vertex < count; ++vertex) {
            printf("SELECTION_VERTEX sample=%zu batch=%zu index=%zu words=", sample, batch, vertex);
            for (size_t word = 0; word < 8; ++word)
                printf("%s%08" PRIx32, word == 0 ? "" : ",", b->raw_words[vertex * 8 + word]);
            putchar('\n');
        }
    }
    return 0;
}

static int aim_receipt(fixture *f, aim_work *work, const char *directory, int32_t expected_id) {
    const mc_client_world_frame_state *world = f->state.world_frame;
    CHECK(world != NULL && world->returned && world->source_pc == 1013 && !world->anaglyph &&
          world->picking_returned && world->picking == NULL && world->picking_source_returned &&
          world->picking_block_published &&
          world->picking_result_identity == (uintptr_t)f->app->y &&
          world->picking_block_identity == (uintptr_t)f->app->y && world->selection_calls == 1 &&
          !world->selection_water && world->selection_hit_present &&
          world->selection_current_returned);
    const mc_client_selection_state *s = world->selections[0];
    CHECK(s != NULL && s->failure != NULL && s->failure->operation == NULL && !s->underwater &&
          s->overlay_returned && s->overlay_pc == 506 && s->outline_returned &&
          s->outline_pc == 235 && s->alpha_restored && s->clock_sampled && s->progress == 0.0f &&
          !s->texture_bound && !s->matrix_pushed && !s->crack_cv_returned && s->outline_block &&
          s->shape_returned && s->selected_returned && s->expanded_returned &&
          s->shifted_returned && s->vertices == 18 && s->draws == 3 &&
          s->overlay_hit == f->app->y && s->outline_hit == f->app->y && s->overlay_stack == NULL &&
          s->outline_stack == NULL && s->x == 0 && s->y == 64 && s->z == 2 &&
          s->block_id == expected_id && s->selected != NULL && s->expanded != NULL &&
          s->shifted != NULL && s->selected != s->expanded && s->expanded != s->shifted &&
          s->selected != s->shifted);
    mc_entity_hit_result_view hit;
    mc_entity_hit_kind_class_view kinds;
    mc_entity_snapshot player;
    mc_local_player_snapshot local;
    const mc_item_stack *stack = (const mc_item_stack *)(uintptr_t)1;
    mc_inventory_current_error inventory;
    CHECK(mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK &&
          mc_entity_hit_kind_class_inspect(f->app->entities, &kinds) == MC_ENTITY_HIT_OK &&
          hit.fields.a == kinds.a && hit.fields.b == 0 && hit.fields.c == 64 && hit.fields.d == 2 &&
          hit.classes == f->app->entities &&
          mc_entity_get_snapshot(f->app->h, &player) == MC_ENTITY_OK &&
          mc_local_player_get_snapshot(f->app->h, &local) == MC_ENTITY_OK &&
          mc_inventory_player_current(local.player.c, &stack, &inventory) == MC_INVENTORY_OK &&
          stack == NULL && world->camera == f->app->h && f->app->i == f->app->h);
    const mc_block *block = NULL;
    CHECK(mc_registry_block_get(f->app->registry, expected_id, &block, NULL) == MC_REGISTRY_OK &&
          block != NULL);
    const mc_block_fields *b = mc_block_fields_get(block);
    CHECK(b != NULL && b->bn == expected_id);
    const size_t sample = work->targeted;
    printf(
        "BLOCK_SELECTION sample=%zu frame=%" PRIu64 " world-pc=%u overlay=%u outline=%u"
        " alpha-restored=%d progress=%08" PRIx32 " now=%" PRId64 " pulse=%08" PRIx32
        " partial=%08" PRIx32 " hit=%" PRIxPTR " overlay-hit=%" PRIxPTR " outline-hit=%" PRIxPTR
        " block=%" PRId32 " xyz=%" PRId32 ",%" PRId32 ",%" PRId32
        " draws=%u vertices=%u empty-hand=1 sdk-selection=1 hit-injected=0 progress-assigned=0\n",
        sample, f->state.frames, world->source_pc, s->overlay_pc, s->outline_pc, s->alpha_restored,
        aim_float(s->progress), s->wall_clock, aim_float(s->pulse_alpha), aim_float(world->partial),
        (uintptr_t)f->app->y, (uintptr_t)s->overlay_hit, (uintptr_t)s->outline_hit, s->block_id,
        s->x, s->y, s->z, s->draws, s->vertices);
    printf("SELECTION_OPERANDS sample=%zu previous=%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64
           " current=%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 " camera=%016" PRIx64 ",%016" PRIx64
           ",%016" PRIx64 " block-bounds=%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64 ",%016" PRIx64
           ",%016" PRIx64 ",%016" PRIx64 "\n",
           sample, aim_double(player.base.bl), aim_double(player.base.bm),
           aim_double(player.base.bn), aim_double(player.base.x), aim_double(player.base.y),
           aim_double(player.base.z), aim_double(s->camera[0]), aim_double(s->camera[1]),
           aim_double(s->camera[2]), aim_double(b->bs), aim_double(b->bt), aim_double(b->bu),
           aim_double(b->bv), aim_double(b->bw), aim_double(b->bx));
    aim_box("selected", sample, s->selected);
    aim_box("expanded", sample, s->expanded);
    aim_box("shifted", sample, s->shifted);
    CHECK(aim_batches(sample, s) == 0);
    if (!work->table_written) {
        const mc_math_trig *table = mc_client_bootstrap_trig(f->bootstrap);
        mc_math_class_state math;
        CHECK(table != NULL &&
              mc_math_classes_state(mc_client_bootstrap_math(f->bootstrap), &math, NULL) ==
                  MC_MATH_CLASS_OK &&
              math.phase == MC_MATH_CLASS_READY && math.source_a == table &&
              aim_file(directory, "math-table.bin", table->values, sizeof(table->values)) == 0);
        work->table_written = true;
    }
    if (work->held == NULL && sample == 0) {
        work->held = (mc_entity_bounds *)s->shifted;
        work->held_classes = f->app->entities;
        work->held_frame = f->state.frames;
        CHECK(mc_entity_bounds_retain(work->held_classes, work->held) == MC_ENTITY_OK);
    } else if (work->held != NULL && f->state.frames > work->held_frame) {
        aim_box("held-after-real-frame", sample, work->held);
        CHECK(mc_entity_bounds_release(work->held_classes, work->held) == MC_ENTITY_OK);
        work->held = NULL;
        printf("SELECTION_HELD sample=%zu ordinary-next-frame=1 storage-live=1 balanced-release=1 "
               "coordinate-change-asserted=0\n",
               sample);
    }
    ++work->targeted;
    return 0;
}

static int aim_frame(fixture *f) {
    const int result = frame(f);
    if (result != 0 && f->state.world_frame != NULL && f->state.world_frame->failure != NULL &&
        f->state.world_frame->failure->selection != NULL) {
        const mc_client_selection_error *error = f->state.world_frame->failure->selection;
        fprintf(stderr,
                "Selection retained %s.%s:%" PRId32 " child=%s.%s:%" PRId32
                " native=%d operation=%s\n",
                error->site.class_name == NULL ? "native" : error->site.class_name,
                error->site.method == NULL ? "selection" : error->site.method, error->site.pc,
                error->child.class_name == NULL ? "native" : error->child.class_name,
                error->child.method == NULL ? "child" : error->child.method, error->child.pc,
                error->native_policy, error->operation == NULL ? "unknown" : error->operation);
    }
    return result;
}

static int aim_case(fixture *f, aim_work *work, const char *directory, int32_t target) {
    CHECK(mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state) &&
          repeat_statistics_controls(f, true) == 0 && save_title_focus(f) == 0);
    mc_client_source_da *initial = f->app->r;
    mc_client_source_rq *selection = NULL;
    CHECK(initial != NULL && initial->kind == MC_CLIENT_SCREEN_MAIN_MENU && ticks(f, 2) == 0 &&
          open_selection(f, initial, &selection) == 0 && selection->fields.n != NULL &&
          mc_client_save_list_size(selection->fields.n) == 3 &&
          filename_is(mc_client_save_list_at(selection->fields.n, 0), "alpha"));
    work->external = &selection->base;
    CHECK(mc_client_screen_retain(work->external, NULL) == MC_CLIENT_SCREEN_OK &&
          release_mouse(f, selection) == 0 && row_press(f, selection, true) == 0 &&
          release_mouse(f, selection) == 0 && push_button(f, work->external, 1, true) == 0 &&
          push_button(f, work->external, 1, false) == 0 && transfer(f) == 0);
    const uint64_t deadline = SDL_GetTicks() + 120000;
    size_t frames = 0;
    while (work->targeted < 3 && frames < 96 && SDL_GetTicks() < deadline) {
        CHECK(save_title_focus(f) == 0 && aim_frame(f) == 0);
        ++frames;
        if (f->app->active_world == NULL)
            continue;
        mc_client_screen_transition_state transitions;
        mc_client_world_load_state loaded;
        CHECK(mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions) &&
              transitions.world_load != NULL &&
              mc_client_world_load_inspect(transitions.world_load, &loaded) &&
              loaded.source_returned && loaded.world == f->app->active_world &&
              loaded.player == f->app->h && loaded.input != NULL && loaded.terrain_samples == 289);
        if (f->app->r != NULL) {
            mc_chunk_provider_fields provider;
            size_t eligible;
            int64_t time;
            CHECK(mc_chunk_provider_bind_fields(f->app->active_world->fields.v, &provider, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  save_title_resume(f, f->app->active_world, &provider, work->recovered,
                                    &work->focus_pauses, &eligible, &time) == 0);
            continue;
        }
        const mc_client_world_frame_state *world = f->state.world_frame;
        if (world == NULL || world->selection_calls == 0)
            continue;
        mc_entity_hit_result_view hit;
        CHECK(f->app->y != NULL &&
              mc_entity_hit_result_inspect(f->app->y, &hit) == MC_ENTITY_HIT_OK);
        if (hit.fields.b != 0 || hit.fields.c != 64 || hit.fields.d != 2)
            continue;
        CHECK(f->app->N && !atomic_load(&f->app->o) && !f->state.failed &&
              f->state.render_returned && f->state.hud_draws == f->state.world_draws &&
              f->state.world_tick_returns > 0 && repeat_statistics_controls(f, false) == 0 &&
              aim_receipt(f, work, directory, target) == 0);
    }
    CHECK(work->targeted == 3 && work->held == NULL);
    printf("SELECTION_WORLD targeted=%zu outer-frames=%zu focus-recoveries=%zu returned-world-hud=1"
           " authored-rotation=1 actor-injected=0 source-fields-assigned=0\n",
           work->targeted, frames, work->focus_pauses);
    CHECK(normal_terminal_retirement_controls(f, &work->external, true, false, NULL) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4);
    const bool rounded = strstr(argv[2], "-rounded") != NULL;
    const bool snow = strncmp(argv[2], "snow", 4) == 0;
    CHECK(strcmp(argv[2], "dirt") == 0 || strcmp(argv[2], "snow") == 0 ||
          strcmp(argv[2], "dirt-rounded") == 0 || strcmp(argv[2], "snow-rounded") == 0);
    fixture *f = calloc(1, sizeof(*f));
    aim_work *work = calloc(1, sizeof(*work));
    if (f == NULL || work == NULL) {
        free(work);
        free(f);
        return 1;
    }
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Selection control",
                                                   .width = rounded ? 641 : 854,
                                                   .height = rounded ? 481 : 480,
                                                   .hidden = false};
    int result = 1;
    uint32_t frame_id = 0, canvas_id = 0;
    if (mc_client_bootstrap_open_standalone(&options, &f->bootstrap) == MC_CLIENT_BOOTSTRAP_OK) {
        f->app = mc_client_bootstrap_app(f->bootstrap);
        mc_client_frame_state host;
        int width = 0, height = 0;
        mc_client_display_error display;
        if (f->app != NULL &&
            mc_client_source_frame_inspect(mc_client_bootstrap_frame(f->bootstrap), &host) &&
            host.app == f->app && host.canvas == f->app->m && host.frame_window != NULL &&
            host.canvas_window == mc_client_display_window(f->app->display) &&
            mc_client_display_pixel_size(f->app->display, &width, &height, &display) &&
            width == options.width && height == options.height) {
            frame_id = SDL_GetWindowID(host.frame_window);
            canvas_id = SDL_GetWindowID(host.canvas_window);
            printf("SELECTION_HOST profile=%s width=%d height=%d frame-id=%" PRIu32
                   " canvas-id=%" PRIu32 " ordinary-frame-canvas=1\n",
                   argv[2], width, height, frame_id, canvas_id);
            result = aim_case(f, work, argv[3], snow ? 78 : 3);
        }
    }
    if (work->held != NULL) {
        if (mc_entity_bounds_release(work->held_classes, work->held) != MC_ENTITY_OK)
            result = 1;
        else
            work->held = NULL;
    }
    if (f->loop != NULL && !mc_client_menu_loop_terminal_prepare(f->loop))
        result = 1;
    if (work->external != NULL &&
        mc_client_screen_release(&work->external, NULL) != MC_CLIENT_SCREEN_OK)
        result = 1;
    if (f->loop != NULL && !mc_client_menu_loop_terminal_retire(&f->loop))
        result = 1;
    mc_client_bootstrap_status closed;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f->bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    const bool clean = closed == MC_CLIENT_BOOTSTRAP_OK && f->bootstrap == NULL &&
                       f->loop == NULL && work->held == NULL && work->external == NULL &&
                       frame_id != 0 && canvas_id != 0 && SDL_GetWindowFromID(frame_id) == NULL &&
                       SDL_GetWindowFromID(canvas_id) == NULL;
    if (!clean)
        result = 1;
    if (result == 0)
        printf("BLOCK_SELECTION_FINISH profile=%s cleanup=1 windows-absent=1 checks=%u "
               "compact-nbt-error=544\n",
               argv[2], checks);
    free(work);
    free(f);
    return result;
}
