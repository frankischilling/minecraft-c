/* Reuse genuine bootstrap/SDK/World/terminal helpers; existing modes are unchanged. */
int mc_notice_world_fixture_main(int, char **);
#define main mc_notice_world_fixture_main
#include "../startup/test_world_selection_loop.c"
#undef main
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/gui/achievement_notice_draw.h"
#include "client/gui/bitmap_font.h"
#include "client/render/item_gui.h"
#include "client/render_engine/internal.h"
#include "mc/entity/nbt/errors.h"

_Static_assert(sizeof(mc_entity_nbt_error) == 544, "existing compact Entity NBT value contract");

typedef struct notice_test {
    mc_client_source_da *external;
    mc_client_notice_draw_call *call;
    mc_client_notice_draw_state state;
    mc_client_notice_draw_error error, cleanup;
    mc_client_bitmap_font_wrap_error wrap;
    mc_client_bitmap_font_error font_error;
    mc_client_display_error display;
    mc_client_notice_error notification;
    mc_client_source_draw_state draw_before, draw_after;
    mc_client_source_fog_state fog_before, fog_after;
    mc_entity_vector_snapshot vectors;
    float projection[16], modelview[16], projection_after[16], modelview_after[16];
} notice_test;

static int notice_inventory(fixture *f, const mc_statistic **out, bool absent) {
    mc_registry_achievement_view achievements;
    CHECK(mc_registry_classes_inspect_achievements(f->app->registry, &achievements, NULL) ==
              MC_REGISTRY_OK &&
          achievements.named != NULL && achievements.named[0] != NULL);
    const mc_statistic *inventory = achievements.named[0];
    CHECK(mc_statistic_fields_get(inventory)->e == 5242880 &&
          mc_statistic_kind_get(inventory) == MC_STATISTIC_ACHIEVEMENT);
    if (absent) {
        bool present = true;
        CHECK(mc_client_stats_writer_contains(f->app->stats_writer, inventory, &present, NULL) ==
                  MC_CLIENT_STATS_OK &&
              !present);
    }
    *out = inventory;
    return 0;
}

static int notice_file(const char *directory, const char *name, const void *data, size_t bytes) {
    char path[8192];
    const int length = snprintf(path, sizeof(path), "%s/%s", directory, name);
    CHECK(length > 0 && (size_t)length < sizeof(path) && bytes <= 4u * 1024u * 1024u);
    FILE *stream = fopen(path, "wb");
    CHECK(stream != NULL);
    const bool written = fwrite(data, 1, bytes, stream) == bytes;
    const bool closed = fclose(stream) == 0;
    CHECK(written && closed);
    return 0;
}

static int notice_pixels(fixture *f, const char *directory, const char *label) {
    mc_client_capture pixels = {0};
    mc_client_display_error error;
    CHECK(mc_client_display_capture_rgba(f->app->display, &pixels, &error) &&
          pixels.pixels != NULL && pixels.width > 0 && pixels.height > 0 && pixels.width <= 1024 &&
          pixels.height <= 1024 && pixels.stride == (size_t)pixels.width * 4u);
    char name[80];
    const int length = snprintf(name, sizeof(name), "%s.rgba", label);
    CHECK(length > 0 && (size_t)length < sizeof(name));
    const int result =
        notice_file(directory, name, pixels.pixels, pixels.stride * (size_t)pixels.height);
    printf("NOTICE_PIXELS label=%s width=%d height=%d stride=%zu bytes=%zu raw=1\n", label,
           pixels.width, pixels.height, pixels.stride, pixels.stride * (size_t)pixels.height);
    mc_client_capture_free(&pixels);
    CHECK(result == 0);
    return 0;
}

static uint64_t notice_double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static int notice_receipt(const char *label, const mc_client_notice_draw_state *s) {
    CHECK(s != NULL && s->failure != NULL);
    printf("NOTICE_DRAW label=%s pc=%" PRId32 " returned=%d inactive=%d expired=%d tutorial=%d "
           "clock=%d now=%" PRId64 " g=%" PRId64 " elapsed=%016" PRIx64 " easing=%016" PRIx64
           " x=%" PRId32 " y=%" PRId32 " width=%" PRId32 " height=%" PRId32
           " physical-width=%" PRId32 " physical-height=%" PRId32 " projections=%zu "
           "background=%d vertices=%zu texts=%zu item=%d id=%" PRId32 " icon=%" PRId32
           " item-vertices=%zu bg-hit=%d item-hit=%d\n",
           label, s->source_pc, s->returned, s->inactive, s->expired, s->tutorial, s->clock_read,
           s->now, s->g, notice_double_bits(s->elapsed), notice_double_bits(s->easing), s->x, s->y,
           s->resolution.width, s->resolution.height, s->physical_width, s->physical_height,
           s->projection_returns, s->background_drawn, s->background_vertices, s->text_calls,
           s->item_returned, s->item.id, s->item.icon, s->item.vertices_returned,
           s->texture_cache_hit, s->item.lookup_cache_hit);
    if (s->item_returned) {
        CHECK(s->returned && s->source_pc == 477 && s->background_drawn &&
              s->background_vertices == 4 && s->item.returned && s->item.entry_pc == 27 &&
              s->item.raw_pc == 367 && s->item.quad_pc == 166 && s->item.id == 340 &&
              s->item.metadata == 0 && s->item.icon == 59 && s->item.texture_x == 176 &&
              s->item.texture_y == 48 && s->item.color == 16777215 && s->item.color_set &&
              s->item.quad_drawn && s->item.vertices_returned == 4 && s->item.lighting_disabled &&
              s->item.lighting_enabled && s->item.culling_enabled &&
              !s->item.pending_lookup_owned && !s->pending_lookup_owned);
        for (size_t i = 0; i < 4; ++i) {
            printf("NOTICE_VERTEX label=%s row=%zu", label, i);
            for (size_t j = 0; j < 5; ++j)
                printf(" %016" PRIx64, notice_double_bits(s->vertices[i][j]));
            for (size_t j = 0; j < 5; ++j)
                printf(" %016" PRIx64, notice_double_bits(s->item.vertices[i][j]));
            putchar('\n');
        }
    }
    return 0;
}

static int notice_begin(fixture *f, notice_test *t, const char *label) {
    CHECK(t->call == NULL &&
          mc_client_notice_draw_begin(f->app->achievement_notice,
                                      mc_client_bootstrap_graphics(f->bootstrap),
                                      mc_client_bootstrap_math(f->bootstrap), f->app->display,
                                      &t->call, &t->error) == MC_CLIENT_NOTICE_DRAW_OK &&
          mc_client_notice_draw_inspect(t->call, &t->state) &&
          notice_receipt(label, &t->state) == 0);
    return 0;
}

static int notice_retire_unchanged(fixture *f, notice_test *t) {
    const int64_t g = mc_client_achievement_notice_fields_get(f->app->achievement_notice)->g;
    CHECK(mc_client_display_source_draw_inspect(f->app->display, &t->draw_before) &&
          mc_client_display_source_fog_inspect(f->app->display, &t->fog_before) &&
          mc_client_display_camera_matrices_inspect(f->app->display, t->projection, t->modelview));
    CHECK(mc_client_notice_draw_retire(&t->call, &t->cleanup) == MC_CLIENT_NOTICE_DRAW_OK &&
          t->call == NULL &&
          mc_client_achievement_notice_fields_get(f->app->achievement_notice)->g == g &&
          mc_client_display_source_draw_inspect(f->app->display, &t->draw_after) &&
          mc_client_display_source_fog_inspect(f->app->display, &t->fog_after) &&
          mc_client_display_camera_matrices_inspect(f->app->display, t->projection_after,
                                                    t->modelview_after));
    CHECK(memcmp(t->projection, t->projection_after, sizeof(t->projection)) == 0 &&
          memcmp(t->modelview, t->modelview_after, sizeof(t->modelview)) == 0 &&
          t->draw_before.texture_enabled == t->draw_after.texture_enabled &&
          t->draw_before.lighting_enabled == t->draw_after.lighting_enabled &&
          t->draw_before.shade_flat == t->draw_after.shade_flat &&
          memcmp(t->draw_before.color, t->draw_after.color, sizeof(t->draw_before.color)) == 0 &&
          t->fog_before.rescale_normal_enabled == t->fog_after.rescale_normal_enabled &&
          t->fog_before.color_material == t->fog_after.color_material &&
          memcmp(t->fog_before.light_parameters, t->fog_after.light_parameters,
                 sizeof(t->fog_before.light_parameters)) == 0);
    return 0;
}

static int notice_ownership(fixture *f, notice_test *t, const mc_statistic *inventory,
                            size_t app_before, size_t font_before) {
    mc_client_achievement_notice *notice = f->app->achievement_notice, *alias = notice;
    const mc_client_achievement_notice_fields *fields =
        mc_client_achievement_notice_fields_get(notice);
    const int64_t g = fields->g;
    const mc_registry_string *description = fields->e;
    CHECK(atomic_load(&f->app->render_borrows) == app_before + 1 &&
          mc_client_achievement_notice_free(&alias, &t->notification) == MC_CLIENT_NOTICE_BUSY &&
          alias == notice &&
          mc_client_achievement_notice_remind(notice, inventory, &t->notification) ==
              MC_CLIENT_NOTICE_BUSY &&
          mc_client_achievement_notice_complete(notice, inventory, &t->notification) ==
              MC_CLIENT_NOTICE_BUSY &&
          fields->g == g && fields->e == description);
    mc_client_notice_draw_call *second = NULL;
    CHECK(mc_client_notice_draw_begin(notice, mc_client_bootstrap_graphics(f->bootstrap),
                                      mc_client_bootstrap_math(f->bootstrap), f->app->display,
                                      &second, &t->error) == MC_CLIENT_NOTICE_DRAW_BUSY &&
          second == NULL && t->state.failure->status == MC_CLIENT_NOTICE_DRAW_OK);
    mc_client_item_renderer *renderer = fields->h;
    CHECK(mc_client_item_renderer_free(&renderer) == MC_CLIENT_MODEL_BUSY &&
          renderer == fields->h &&
          mc_client_item_renderer_bind_manager(renderer, NULL) == MC_CLIENT_MODEL_BUSY);
    mc_client_bitmap_font_state font;
    mc_client_bitmap_font *font_alias = f->app->font_renderer;
    CHECK(mc_client_bitmap_font_inspect(font_alias, &font) && font.holds == font_before + 1 &&
          mc_client_bitmap_font_retire(&font_alias, &t->font_error) == MC_CLIENT_BITMAP_FONT_BUSY &&
          font_alias == f->app->font_renderer);
    CHECK(notice_retire_unchanged(f, t) == 0 &&
          atomic_load(&f->app->render_borrows) == app_before &&
          mc_client_bitmap_font_inspect(f->app->font_renderer, &font) && font.holds == font_before);
    puts("NOTICE_OWNERS pending-notice=1 pending-bb=1 real-font-hold=1 real-app-borrow=1 "
         "reentry-refused=1 native-retire-no-gl-restore=1 source-g-unchanged=1");
    return 0;
}

static int notice_wrap(fixture *f, notice_test *t) {
    static const uint16_t empty[] = {0}, spaces[] = {' ', ' ', ' '}, plain[] = {'A'},
                          two[] = {'A', '\n', 'A'}, middle[] = {'A', '\n', '\n', 'A'},
                          trailing[] = {'A', '\n'}, newlines[] = {'\n', '\n'},
                          greedy[] = {'A', ' ', 'A'},
                          word[] = {'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A'},
                          formatted[] = {0xa7, 'c', 'A', 'A', 'A'},
                          utf16[] = {'A', 0, 0x3a9, 0xd83d, 0xde00};

    static const struct wrap_case {
        const char *name;
        const uint16_t *units;
        size_t length;
        int32_t width, expected;
    } cases[] = {{"empty", empty, 0, 120, 8},       {"spaces", spaces, 3, 120, 8},
                 {"plain", plain, 1, 120, 8},       {"two", two, 3, 120, 16},
                 {"middle", middle, 4, 120, 24},    {"trailing", trailing, 2, 120, 8},
                 {"newlines", newlines, 2, 120, 8}, {"greedy-exact", greedy, 3, 16, 16},
                 {"greedy-over", greedy, 3, 17, 8}, {"long-word", word, 10, 12, 40},
                 {"format", formatted, 5, 120, 8},  {"utf16", utf16, 5, 120, 8}};

    mc_client_bitmap_font *font = f->app->font_renderer;
    int32_t a_width = -1;
    const mc_player_string_view a = {plain, 1};
    CHECK(mc_client_bitmap_font_measure_view(font, &a, &a_width, &t->font_error) ==
              MC_CLIENT_BITMAP_FONT_OK &&
          a_width == 6);
    for (size_t i = 0; i < sizeof(cases) / sizeof(*cases); ++i) {
        const struct wrap_case *c = &cases[i];
        const mc_player_string_view text = {c->units, c->length};
        int32_t height = -77;
        CHECK(mc_client_bitmap_font_wrapped_height_view(font, &text, c->width, &height, &t->wrap) ==
                  MC_CLIENT_BITMAP_FONT_OK &&
              height == c->expected &&
              mc_client_bitmap_font_draw_wrapped_view(font, &text, 2 + (int32_t)i * 30, 60,
                                                      c->width, UINT32_MAX,
                                                      &t->wrap) == MC_CLIENT_BITMAP_FONT_OK);
        printf("NOTICE_WRAP case=%s width=%" PRId32 " height=%" PRId32 " units=", c->name, c->width,
               height);
        for (size_t j = 0; j < c->length; ++j)
            printf("%04x", (unsigned)c->units[j]);
        puts(" actual-ready-font=1 draw-returned=1");
    }
    int32_t untouched = -77;
    CHECK(mc_client_bitmap_font_wrapped_height_view(font, NULL, 120, &untouched, &t->wrap) ==
              MC_CLIENT_BITMAP_FONT_NULL_REFERENCE &&
          untouched == -77 && !t->wrap.font.native_policy && t->wrap.font.pc == 3 &&
          mc_client_bitmap_font_draw_wrapped_view(font, NULL, 0, 0, 120, 0, &t->wrap) ==
              MC_CLIENT_BITMAP_FONT_NULL_REFERENCE &&
          !t->wrap.font.native_policy && t->wrap.font.pc == 3);
    CHECK(mc_client_bitmap_font_wrapped_height_view(font, &a, 0, &untouched, &t->wrap) ==
              MC_CLIENT_BITMAP_FONT_LIMIT &&
          t->wrap.font.native_policy && t->wrap.font.pc == 235 && untouched == -77 &&
          mc_client_bitmap_font_draw_wrapped_view(font, &a, 0, 0, 0, 0, &t->wrap) ==
              MC_CLIENT_BITMAP_FONT_LIMIT &&
          t->wrap.font.native_policy && t->wrap.font.pc == 264);
    puts("NOTICE_WRAP controls=null-source-split3,height-nonprogress235,draw-nonprogress264 "
         "output-preserved=1 native-policy-distinct=1");
    return 0;
}

static int notice_components(fixture *f, notice_test *t, const char *directory) {
    const mc_statistic *inventory = NULL;
    CHECK(notice_inventory(f, &inventory, false) == 0 && notice_begin(f, t, "inactive") == 0 &&
          t->state.inactive && !t->state.clock_read && t->state.gl_calls == 0 &&
          notice_retire_unchanged(f, t) == 0);
    CHECK(mc_client_achievement_notice_complete(f->app->achievement_notice, inventory,
                                                &t->notification) == MC_CLIENT_NOTICE_OK);
    const mc_client_achievement_notice_fields *fields =
        mc_client_achievement_notice_fields_get(f->app->achievement_notice);
    const int64_t completed = fields->g;
    CHECK(completed != 0 && fields->f == inventory && !fields->i);
    SDL_Delay(600); /* Real declared wait; the production call chooses its own clock. */
    CHECK(mc_client_display_begin_scene(f->app->display, .0625f, .125f, .1875f, &t->display));
    mc_client_bitmap_font_state font;
    CHECK(mc_client_bitmap_font_inspect(f->app->font_renderer, &font) &&
          mc_entity_vector_get_snapshot(f->app->entities, &t->vectors) == MC_ENTITY_OK);
    const size_t app_before = atomic_load(&f->app->render_borrows), font_before = font.holds;
    const int32_t cursor = t->vectors.cursor;
    CHECK(cursor >= 0 && cursor <= INT32_MAX - 4);
    CHECK(notice_begin(f, t, "completion") == 0 && t->state.clock_read && !t->state.tutorial &&
          t->state.g == completed && t->state.source_pc == 477 && t->state.y == 0 &&
          t->state.elapsed >= .125 && t->state.elapsed <= .875 && t->state.text_calls == 2 &&
          notice_pixels(f, directory, "completion") == 0 &&
          mc_entity_vector_get_snapshot(f->app->entities, &t->vectors) == MC_ENTITY_OK &&
          t->vectors.cursor == cursor + 4 &&
          notice_ownership(f, t, inventory, app_before, font_before) == 0);
    CHECK(notice_begin(f, t, "cache") == 0 && t->state.source_pc == 477 &&
          t->state.texture_cache_hit && t->state.item.lookup_cache_hit &&
          notice_retire_unchanged(f, t) == 0);
    int64_t now = 0;
    const uint64_t deadline = SDL_GetTicks() + 10000;
    do {
        CHECK(mc_platform_current_time_millis(&now) == MC_PLATFORM_MILLIS_OK);
        if (mc_java_i64_sub(now, completed) > 3050)
            break;
        SDL_Delay(10);
    } while (SDL_GetTicks() < deadline);
    const mc_registry_string *title = fields->d, *description = fields->e;
    CHECK(mc_java_i64_sub(now, completed) > 3050 && notice_begin(f, t, "expired") == 0 &&
          t->state.expired && t->state.source_pc == 156 && t->state.clock_read &&
          t->state.gl_calls == 0 && fields->g == 0 && fields->f == inventory && !fields->i &&
          fields->d == title && fields->e == description && notice_retire_unchanged(f, t) == 0 &&
          notice_begin(f, t, "inactive-after-expiry") == 0 && t->state.source_pc == 107 &&
          !t->state.clock_read && t->state.gl_calls == 0 && notice_retire_unchanged(f, t) == 0);
    CHECK(mc_client_achievement_notice_remind(f->app->achievement_notice, inventory,
                                              &t->notification) == MC_CLIENT_NOTICE_OK &&
          fields->i && fields->f == inventory &&
          mc_client_display_begin_scene(f->app->display, .0625f, .125f, .1875f, &t->display) &&
          notice_begin(f, t, "tutorial-direct") == 0 && t->state.tutorial &&
          t->state.text_calls == 1 && t->state.source_pc == 477 && t->state.y == 0 &&
          notice_pixels(f, directory, "tutorial-direct") == 0 &&
          notice_retire_unchanged(f, t) == 0);
    CHECK(notice_wrap(f, t) == 0 && notice_pixels(f, directory, "wrapped") == 0);
    puts("NOTICE_COMPONENTS completion=1 cache=1 actual-expiry=1 tutorial=1 wrapped=12 "
         "clock-injected=0 source-fields-repaired=0");
    return 0;
}

static int notice_tutorial(fixture *f, notice_test *t, const char *directory) {
    const mc_statistic *inventory = NULL;
    CHECK(notice_inventory(f, &inventory, true) == 0 &&
          mc_client_menu_loop_create(f->bootstrap, &f->loop) &&
          mc_client_menu_loop_inspect(f->loop, &f->state));
    mc_client_source_da *initial = f->app->r;
    CHECK(initial != NULL && initial->kind == MC_CLIENT_SCREEN_MAIN_MENU &&
          save_title_focus(f) == 0);
    mc_client_source_rq *selection = NULL;
    CHECK(ticks(f, 2) == 0 && open_selection(f, initial, &selection) == 0 &&
          selection->fields.n != NULL && mc_client_save_list_size(selection->fields.n) == 3 &&
          filename_is(mc_client_save_list_at(selection->fields.n, 0), "alpha"));
    mc_client_source_da *external = &selection->base;
    CHECK(mc_client_screen_retain(external, NULL) == MC_CLIENT_SCREEN_OK);
    t->external = external;
    CHECK(release_mouse(f, selection) == 0 && row_press(f, selection, true) == 0 &&
          release_mouse(f, selection) == 0 && push_button(f, external, 1, true) == 0 &&
          push_button(f, external, 1, false) == 0 && transfer(f) == 0);
    const uint64_t started = SDL_GetTicks();
    size_t frames = 0, active = 0, refreshes = 0, focus_pauses = 0;
    int64_t previous_g = 0;
    bool captured = false;
    mc_client_source_da *recovered[4] = {0};
    do {
        CHECK(save_title_focus(f) == 0 && frame(f) == 0 &&
              notice_inventory(f, &inventory, true) == 0);
        ++frames;
        if (f->app->active_world == NULL)
            continue;
        mc_client_screen_transition_state transitions;
        mc_client_world_load_state load;
        mc_world_runtime_construction world;
        CHECK(mc_client_screen_transitions_inspect(f->app->screen_transitions, &transitions) &&
              transitions.world_load != NULL &&
              mc_client_world_load_inspect(transitions.world_load, &load) && load.source_returned &&
              load.world == f->app->active_world && load.player == f->app->h &&
              f->app->i == f->app->h && load.input != NULL && load.terrain_samples == 289 &&
              mc_world_runtime_construction_get(load.world, &world) == MC_WORLD_STORAGE_OK &&
              world.phase == MC_WORLD_RUNTIME_READY && world.mode == MC_WORLD_RUNTIME_GAME &&
              world.entry == MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE);
        if (f->app->r != NULL) {
            mc_chunk_provider_fields provider;
            size_t eligible;
            int64_t time;
            CHECK(mc_chunk_provider_bind_fields(load.world->fields.v, &provider, NULL) ==
                      MC_WORLD_STORAGE_OK &&
                  save_title_resume(f, load.world, &provider, recovered, &focus_pauses, &eligible,
                                    &time) == 0);
            continue;
        }
        const mc_client_notice_draw_state *s = f->state.notice;
        if (s == NULL || !s->item_returned)
            continue;
        CHECK(s->returned && !s->inactive && !s->expired && s->tutorial && s->clock_read &&
              s->source_pc == 477 && s->owner == f->app->achievement_notice && s->text_calls == 1 &&
              f->state.world_draws > 0 && f->state.hud_draws == f->state.world_draws &&
              f->state.render_returned && !f->state.failed && f->app->N &&
              !atomic_load(&f->app->o) && f->state.world_tick_returns > 0 &&
              f->state.notice_draws > 0 && s->failure->status == MC_CLIENT_NOTICE_DRAW_OK &&
              mc_client_achievement_notice_fields_get(f->app->achievement_notice)->f == inventory);
        char label[48];
        const int length = snprintf(label, sizeof(label), "world-%zu", active);
        CHECK(length > 0 && (size_t)length < sizeof(label) && notice_receipt(label, s) == 0);
        if (previous_g != 0 && s->g != previous_g)
            ++refreshes;
        previous_g = s->g;
        ++active;
        if (!captured && s->y == 0) {
            CHECK(notice_pixels(f, directory, "world-tutorial") == 0);
            captured = true;
            printf("NOTICE_WORLD_CAPTURE label=%s raw=world-tutorial.rgba\n", label);
        }
    } while ((active < 3 || refreshes < 2 || !captured) && frames < 64 &&
             SDL_GetTicks() - started < 60000);
    CHECK(active >= 3 && refreshes >= 2 && captured && f->app->r == NULL && f->app->N &&
          notice_inventory(f, &inventory, true) == 0 && !f->state.failed);
    printf("NOTICE_WORLD frames=%zu active=%zu refreshes=%zu focus-pauses=%zu SDK-selection=1 "
           "actual-world-constructor=1 absent-openInventory=1 book=340 icon=59 "
           "returned-world-hud=1 timer-written=0 notice-written=0 native-actor-published=0\n",
           frames, active, refreshes, focus_pauses);
    CHECK(normal_terminal_retirement_controls(f, &t->external, true, false) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4 && (!strcmp(argv[2], "components") || !strcmp(argv[2], "rounded") ||
                        !strcmp(argv[2], "tutorial")));
    const bool tutorial = !strcmp(argv[2], "tutorial"), rounded = !strcmp(argv[2], "rounded");
    fixture f = {0};
    notice_test *t = calloc(1, sizeof(*t));
    CHECK(t != NULL);
    const mc_client_application_options options = {.assets = argv[1],
                                                   .username = "Notice control",
                                                   .width = rounded ? 641 : 854,
                                                   .height = rounded ? 481 : 480,
                                                   .hidden = !tutorial};
    CHECK(mc_client_bootstrap_open_standalone(&options, &f.bootstrap) == MC_CLIENT_BOOTSTRAP_OK);
    f.app = mc_client_bootstrap_app(f.bootstrap);
    CHECK(f.app != NULL && f.app->r != NULL && f.app->r->kind == MC_CLIENT_SCREEN_MAIN_MENU);
    mc_client_source_frame *host = mc_client_bootstrap_frame(f.bootstrap);
    mc_client_frame_state frame_host;
    mc_game_settings_state settings;
    CHECK(host != NULL && mc_client_source_frame_inspect(host, &frame_host) &&
          frame_host.app == f.app && frame_host.canvas == f.app->m &&
          frame_host.canvas_window != NULL && frame_host.frame_window != NULL &&
          frame_host.canvas_window != frame_host.frame_window &&
          mc_client_display_window(f.app->display) == frame_host.canvas_window &&
          mc_game_settings_read_state(f.app->settings, &settings) && !settings.values.anaglyph);
    const uint32_t frame_id = SDL_GetWindowID(frame_host.frame_window),
                   canvas_id = SDL_GetWindowID(frame_host.canvas_window);
    CHECK(frame_id != 0 && canvas_id != 0);
    int result = tutorial ? notice_tutorial(&f, t, argv[3]) : notice_components(&f, t, argv[3]);
    if (t->call != NULL &&
        mc_client_notice_draw_retire(&t->call, &t->cleanup) != MC_CLIENT_NOTICE_DRAW_OK)
        result = 1;
    if (f.loop != NULL && !mc_client_menu_loop_terminal_prepare(f.loop))
        result = 1;
    if (t->external != NULL && mc_client_screen_release(&t->external, NULL) != MC_CLIENT_SCREEN_OK)
        result = 1;
    if (f.loop != NULL && !mc_client_menu_loop_terminal_retire(&f.loop))
        result = 1;
    mc_client_bootstrap_status closed;
    const uint64_t deadline = SDL_GetTicks() + 35000;
    do {
        closed = mc_client_bootstrap_close(&f.bootstrap);
        if (closed == MC_CLIENT_BOOTSTRAP_BUSY)
            SDL_Delay(1);
    } while (closed == MC_CLIENT_BOOTSTRAP_BUSY && SDL_GetTicks() < deadline);
    CHECK(closed == MC_CLIENT_BOOTSTRAP_OK && f.bootstrap == NULL && t->call == NULL &&
          f.loop == NULL && SDL_GetWindowFromID(frame_id) == NULL &&
          SDL_GetWindowFromID(canvas_id) == NULL);
    free(t);
    if (result == 0)
        printf("NOTICE_FINISH mode=%s cleanup=1 checks=%u compact-nbt-error=544 windows-absent=1\n",
               argv[2], checks);
    return result;
}
