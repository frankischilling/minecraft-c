/* Real cold owners and returned CPU prefixes; no Video/GL/READY Font fiction. */
int mc_notice_startup_fixture_main(int, char **);
#define main mc_notice_startup_fixture_main
#include "../startup/test_application_startup.c"
#undef main
#include "client/gui/achievement_notice_draw.h"
#include "client/render/item_gui.h"
#include "mc/entity/nbt/errors.h"

_Static_assert(sizeof(mc_entity_nbt_error) == 544, "existing compact Entity NBT value contract");

static int notice_cpu(const char *resources) {
    fixture f;
    CHECK(fixture_open(&f, resources) == 0 && SDL_WasInit(0) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&f, false, &app) == 0 && app->achievement_notice != NULL &&
          app->display == NULL && app->font_renderer == NULL && app->settings == NULL &&
          app->render_engine == NULL && app->source_namespace_borrowed &&
          app->native_lifetime_phase == MC_CLIENT_APPLICATION_LIFETIME_REGISTERED);
    mc_client_notice_draw_error *error = calloc(1, sizeof(*error));
    mc_client_item_gui_error *item_error = calloc(1, sizeof(*item_error));
    mc_client_bitmap_font_wrap_error *wrap = calloc(1, sizeof(*wrap));
    mc_client_bitmap_font_error *font_error = calloc(1, sizeof(*font_error));
    CHECK(error != NULL && item_error != NULL && wrap != NULL && font_error != NULL);
    mc_client_achievement_notice *notice = app->achievement_notice;
    mc_client_achievement_notice_fields before;
    memcpy(&before, mc_client_achievement_notice_fields_get(notice), sizeof(before));
    const size_t borrows = atomic_load(&app->render_borrows);
    mc_client_notice_draw_call *call = NULL, *second = NULL;
    CHECK(mc_client_notice_draw_begin(notice, NULL, NULL, NULL, &call, error) ==
          MC_CLIENT_NOTICE_DRAW_OK);
    mc_client_notice_draw_state state;
    CHECK(mc_client_notice_draw_inspect(call, &state) && state.returned && state.inactive &&
          state.source_pc == 107 && state.watermark_value == 0 && !state.clock_read &&
          !state.watermark && state.gl_calls == 0 && state.text_calls == 0 &&
          state.background_vertices == 0 && !state.item_returned &&
          atomic_load(&app->render_borrows) == borrows + 1);
    CHECK(mc_client_notice_draw_begin(notice, NULL, NULL, NULL, &second, error) ==
              MC_CLIENT_NOTICE_DRAW_BUSY &&
          second == NULL);
    mc_client_achievement_notice *alias = notice;
    CHECK(mc_client_achievement_notice_free(&alias, NULL) == MC_CLIENT_NOTICE_BUSY &&
          alias == notice);
    CHECK(mc_client_achievement_notice_remind(notice, NULL, NULL) == MC_CLIENT_NOTICE_BUSY &&
          mc_client_achievement_notice_complete(notice, NULL, NULL) == MC_CLIENT_NOTICE_BUSY &&
          mc_client_achievement_notice_draw(notice, NULL) == MC_CLIENT_NOTICE_BUSY &&
          memcmp(&before, mc_client_achievement_notice_fields_get(notice), sizeof(before)) == 0 &&
          state.failure->status == MC_CLIENT_NOTICE_DRAW_OK);
    CHECK(mc_client_notice_draw_retire(&call, error) == MC_CLIENT_NOTICE_DRAW_OK && call == NULL &&
          atomic_load(&app->render_borrows) == borrows &&
          memcmp(&before, mc_client_achievement_notice_fields_get(notice), sizeof(before)) == 0);
    puts("NOTICE_CPU inactive=107 clock=0 gl=0 real-app-borrow=1 retained-mutation-refused=1");

    mc_client_item_renderer *renderer = before.h;
    mc_client_item_gui_call *item = NULL, *item_second = NULL;
    CHECK(renderer != NULL &&
          mc_client_item_gui_begin(renderer, NULL, NULL, NULL, NULL, NULL, NULL, INT32_MIN,
                                   INT32_MAX, &item, item_error) == MC_CLIENT_ITEM_GUI_OK);
    mc_client_item_gui_state item_state;
    CHECK(mc_client_item_gui_inspect(item, &item_state) && item_state.returned &&
          item_state.null_stack && item_state.entry_pc == 4 && item_state.raw_pc == -1 &&
          !item_state.id_read && !item_state.metadata_read && !item_state.raw_entered &&
          !item_state.lookup_returned && item_state.vertices_returned == 0);
    mc_client_item_renderer *renderer_alias = renderer;
    CHECK(mc_client_item_renderer_free(&renderer_alias) == MC_CLIENT_MODEL_BUSY &&
          renderer_alias == renderer &&
          mc_client_item_renderer_bind_manager(renderer, NULL) == MC_CLIENT_MODEL_BUSY &&
          mc_client_item_gui_begin(renderer, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, &item_second,
                                   item_error) == MC_CLIENT_ITEM_GUI_BUSY &&
          item_second == NULL && item_state.failure->status == MC_CLIENT_ITEM_GUI_OK);
    CHECK(mc_client_item_gui_retire(&item, item_error) == MC_CLIENT_ITEM_GUI_OK && item == NULL);
    puts("NOTICE_CPU bb-null-stack=4 no-services=1 retained-bb-refused=1 native-retirement=1");

    mc_client_bitmap_font *font = NULL;
    CHECK(mc_client_bitmap_font_allocate(f.services.gui_classes, NULL, &font) ==
          MC_CLIENT_BITMAP_FONT_OK);
    mc_client_bitmap_font_state raw_before, raw_after;
    CHECK(mc_client_bitmap_font_inspect(font, &raw_before) &&
          raw_before.phase == MC_CLIENT_BITMAP_FONT_RAW);
    int32_t height = -77;
    static const uint16_t a[] = {'A'};
    const mc_player_string_view text = {a, 1};
    CHECK(mc_client_bitmap_font_wrapped_height_view(font, &text, 120, &height, wrap) ==
              MC_CLIENT_BITMAP_FONT_INVALID_PHASE &&
          height == -77 && wrap->font.native_policy && wrap->font.pc == -1 &&
          wrap->recursive_descriptor == NULL && wrap->recursive_pc == -1);
    CHECK(mc_client_bitmap_font_draw_wrapped_view(font, NULL, 0, 0, 120, 0, wrap) ==
              MC_CLIENT_BITMAP_FONT_INVALID_PHASE &&
          wrap->font.native_policy && wrap->font.pc == -1 &&
          mc_client_bitmap_font_inspect(font, &raw_after) && raw_after.phase == raw_before.phase &&
          raw_after.holds == raw_before.holds && raw_after.fields.b == raw_before.fields.b &&
          !raw_after.active && !raw_after.has_native_generation);
    CHECK(mc_client_bitmap_font_wrapped_height_view(NULL, &text, 120, &height, wrap) ==
              MC_CLIENT_BITMAP_FONT_NULL_REFERENCE &&
          wrap->font.native_policy && wrap->font.pc == -1 && height == -77);
    CHECK(mc_client_bitmap_font_release(&font, font_error) == MC_CLIENT_BITMAP_FONT_OK &&
          font == NULL);
    puts("NOTICE_CPU wrap-raw-admission=1 no-ready-font=1 source-split-not-claimed=1");

    mc_client_achievement_notice *unbound = NULL;
    CHECK(mc_client_achievement_notice_allocate(f.services.render_classes, f.services.registry,
                                                f.services.profile, &unbound,
                                                NULL) == MC_CLIENT_NOTICE_OK &&
          mc_client_achievement_notice_construct(unbound, NULL, NULL) == MC_CLIENT_NOTICE_OK &&
          mc_client_notice_draw_begin(unbound, NULL, NULL, NULL, &call, error) ==
              MC_CLIENT_NOTICE_DRAW_DEPENDENCY &&
          error->native_policy && !error->source_exception && error->site.pc == 0 &&
          mc_client_notice_draw_inspect(call, &state) && !state.returned && !state.clock_read &&
          state.gl_calls == 0 &&
          mc_client_notice_draw_retire(&call, error) == MC_CLIENT_NOTICE_DRAW_OK &&
          mc_client_achievement_notice_free(&unbound, NULL) == MC_CLIENT_NOTICE_OK);
    CHECK(SDL_WasInit(0) == 0 && atomic_load(&app->render_borrows) == borrows);
    free(font_error);
    free(wrap);
    free(item_error);
    free(error);
    CHECK(mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_BUSY &&
          mc_client_application_classes_release(&f.services.application_classes, NULL) ==
              MC_CLIENT_APPLICATION_OWNER_OK &&
          mc_client_application_constructor_retire(&app, f.error) == MC_CLIENT_APP_OK &&
          fixture_close(&f) == 0 && SDL_WasInit(0) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 2 && notice_cpu(argv[1]) == 0);
    printf("NOTICE_CPU cleanup=1 checks=%u compact-nbt-error=544 SDL-init=0\n", checks);
    return 0;
}
