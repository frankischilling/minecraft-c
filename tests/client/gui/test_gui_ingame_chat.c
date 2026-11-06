/* Genuine constructed Font/UQ/World owners; declared component text/age inputs. */
#define MC_GUI_INGAME_DRAW_MAIN mc_chat_inherited_hud_main
int mc_chat_inherited_hud_main(int, char **);
#include "client/gui/gui_ingame_chat.h"
#include "client/gui/gui_ingame_chat_internal.h"
#include "test_gui_ingame_draw.c"

static int chat_add(mc_client_source_uq *owner, const uint16_t *units, size_t length, bool null,
                    bool translated) {
    client_application *app = owner->fields.g;
    mc_player_string *text = NULL;
    if (!null)
        CHECK(mc_player_string_create(units, length, &text) == MC_PLAYER_OK);
    mc_client_gui_ingame_chat_call *call = NULL;
    mc_client_gui_ingame_chat_error error;
    const mc_client_gui_ingame_chat_status status =
        translated
            ? mc_client_gui_ingame_chat_translate_begin(owner, app->registry, text, &call, &error)
            : mc_client_gui_ingame_chat_add_begin(owner, text, &call, &error);
    CHECK(status == MC_CLIENT_GUI_INGAME_CHAT_OK && call != NULL && error.source_returned);
    mc_client_gui_ingame_chat_state state;
    CHECK(mc_client_gui_ingame_chat_inspect(call, &state) && state.returned && state.messages > 0 &&
          state.translated == translated);
    mc_player_string_release(text);
    CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK &&
          call == NULL);
    return 0;
}

static int chat_ascii(mc_client_source_uq *owner, const char *ascii, bool translated) {
    const size_t length = strlen(ascii);
    CHECK(length <= 256);
    uint16_t units[256];
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint8_t)ascii[i];
    return chat_add(owner, units, length, false, translated);
}

static int chat_row(mc_client_source_uq *owner, unsigned int profile) {
    mc_client_gui_ingame_list *list = owner->fields.e;
    printf("CHAT_CASE %u %zu %zu %d %d %d\n", profile, list->size, list->capacity, list->mod_count,
           owner->fields.h, owner->fields.j);
    for (size_t i = 0; i < list->size; ++i) {
        mc_gui_chat_message *message = mc_gui_chat_message_find(owner, list->entries[i]);
        CHECK(message != NULL);
        printf("CHAT_LINE %u %zu %d ", profile, i, message->b);
        if (message->a.player == NULL && message->a.registry == NULL)
            puts("null -");
        else {
            mc_player_string_view text;
            CHECK(mc_gui_chat_text_read(message->a, &text));
            printf("text ");
            if (text.length == 0)
                putchar('-');
            for (size_t unit = 0; unit < text.length; ++unit)
                printf("%04x", (unsigned int)text.units[unit]);
            putchar('\n');
        }
    }
    return 0;
}

static int chat_observations(mc_client_source_uq *owner) {
    client_application *app = owner->fields.g;
    mc_client_gui_ingame_error error;
    uint16_t repeated[94];
    for (size_t i = 0; i < sizeof(repeated) / sizeof(repeated[0]); ++i)
        repeated[i] = 'W';
    CHECK(owner->fields.e->size == 0 && owner->fields.e->capacity == 0);
    mc_player_string_view width_text = {repeated, 1};
    int32_t width;
    mc_client_bitmap_font_error font;
    CHECK(mc_client_bitmap_font_measure_view(app->font_renderer, &width_text, &width, &font) ==
              MC_CLIENT_BITMAP_FONT_OK &&
          width == 7);
    for (unsigned int profile = 0; profile < 12; ++profile) {
        if (profile == 0)
            CHECK(chat_add(owner, NULL, 0, true, false) == 0);
        else if (profile == 1)
            CHECK(chat_ascii(owner, "", false) == 0);
        else if (profile == 2)
            CHECK(chat_ascii(owner, "Bed", false) == 0);
        else if (profile >= 3 && profile <= 5)
            CHECK(chat_add(owner, repeated,
                           profile == 3   ? 45
                           : profile == 4 ? 46
                                          : 92,
                           false, false) == 0);
        else if (profile == 6) {
            uint16_t formatted[48] = {0x00a7, 'c'};
            memcpy(formatted + 2, repeated, 46 * sizeof(uint16_t));
            CHECK(chat_add(owner, formatted, 48, false, false) == 0);
        } else if (profile == 7)
            CHECK(chat_ascii(owner, "A\nB", false) == 0);
        else if (profile == 8) {
            const uint16_t special[] = {'A', 0, 'B', 0xd800, 'C'};
            CHECK(chat_add(owner, special, 5, false, false) == 0);
        } else if (profile == 9) {
            CHECK(chat_ascii(owner, "tile.bed.notValid", true) == 0);
            mc_gui_chat_message *message =
                mc_gui_chat_message_find(owner, owner->fields.e->entries[0]);
            CHECK(message != NULL && message->a.registry != NULL && message->registry_held);
        } else if (profile == 10) {
            CHECK(chat_ascii(owner, "chat.missing.key", true) == 0);
            mc_gui_chat_message *message =
                mc_gui_chat_message_find(owner, owner->fields.e->entries[0]);
            CHECK(message != NULL && message->a.player != NULL && message->a.registry == NULL);
        } else {
            for (unsigned int i = 0; i < 51; ++i) {
                char value[8];
                CHECK(snprintf(value, sizeof(value), "v%02u", i) == 3 &&
                      chat_ascii(owner, value, false) == 0);
            }
            CHECK(owner->fields.e->size == 50);
            size_t owners = 0;
            for (mc_gui_chat_message *m = owner->messages; m != NULL; m = m->next)
                ++owners;
            CHECK(owners == 50); /* Removed unreachable SW is reclaimed natively. */
        }
        CHECK(chat_row(owner, profile) == 0);
        CHECK(mc_client_gui_ingame_clear_messages(owner, &error) == MC_CLIENT_GUI_INGAME_OK &&
              owner->messages == NULL);
    }
    CHECK(chat_ascii(owner, "old", false) == 0 && chat_add(owner, NULL, 0, true, false) == 0);
    mc_gui_chat_message *first = mc_gui_chat_message_find(owner, owner->fields.e->entries[0]);
    mc_gui_chat_message *second = mc_gui_chat_message_find(owner, owner->fields.e->entries[1]);
    CHECK(first != NULL && second != NULL);
    first->b = INT32_MAX;
    second->b = 199;
    owner->fields.j = 2;
    CHECK(mc_client_gui_ingame_tick(owner, &error) == MC_CLIENT_GUI_INGAME_OK &&
          first->b == INT32_MIN && second->b == 200 && owner->fields.j == 1);
    CHECK(chat_row(owner, 12) == 0);
    CHECK(mc_client_gui_ingame_clear_messages(owner, &error) == MC_CLIENT_GUI_INGAME_OK);
    CHECK(chat_row(owner, 13) == 0);
    return 0;
}

static int chat_failure_ownership(mc_client_source_uq *owner) {
    mc_client_gui_ingame_error owner_error;
    const size_t allocations = owner->allocations;
    const size_t limit = owner->options.maximum_allocations;
    owner->options.maximum_allocations = allocations + 3;
    uint16_t text[92];
    for (size_t i = 0; i < 92; ++i)
        text[i] = 'W';
    mc_player_string *string = NULL;
    CHECK(mc_player_string_create(text, 92, &string) == MC_PLAYER_OK);
    mc_client_gui_ingame_chat_call *call = NULL;
    mc_client_gui_ingame_chat_error error;
    CHECK(mc_client_gui_ingame_chat_add_begin(owner, string, &call, &error) ==
              MC_CLIENT_GUI_INGAME_CHAT_LIMIT &&
          call != NULL && error.native_policy);
    mc_client_gui_ingame_chat_state state;
    CHECK(mc_client_gui_ingame_chat_inspect(call, &state) && !state.returned &&
          state.messages == 1 && owner->fields.e->size == 1);
    CHECK(mc_client_gui_ingame_tick(owner, &owner_error) == MC_CLIENT_GUI_INGAME_BUSY);
    owner->options.maximum_allocations = limit;
    mc_player_string_release(string);
    CHECK(mc_client_gui_ingame_chat_retire(&call, &error) == MC_CLIENT_GUI_INGAME_CHAT_OK &&
          call == NULL && owner->fields.e->size == 1);
    CHECK(mc_client_gui_ingame_clear_messages(owner, &owner_error) == MC_CLIENT_GUI_INGAME_OK);
    return 0;
}

static int chat_draw(fixture *runtime, client_application *app, mc_entity *player,
                     mc_math_classes *math, const char *screenshot) {
    CHECK(hud_prepare(app, player, 0, 0, false) == 0 && hud_reset(runtime, app) == 0);
    for (unsigned int i = 0; i < 12; ++i)
        CHECK(chat_ascii(app->v, "visible chat", false) == 0);
    const int32_t ages[] = {0, 179, 180, 181, 190, 198, 199, 200, -1, INT32_MIN, 0, 0};
    const int32_t expected[] = {255, 255, 254, 230, 63, 2, 255, 255};
    for (size_t i = 0; i < 12; ++i) {
        mc_gui_chat_message *message =
            mc_gui_chat_message_find(app->v, app->v->fields.e->entries[i]);
        CHECK(message != NULL);
        message->b = ages[i];
    }
    mc_client_gui_ingame_draw_call *call = NULL;
    mc_client_gui_ingame_draw_error error;
    CHECK(mc_client_gui_ingame_draw_begin(app->v, runtime->graphics, math, app->display, .375f,
                                          false, 0, 0, &call,
                                          &error) == MC_CLIENT_GUI_INGAME_DRAW_OK);
    mc_client_gui_ingame_draw_state state;
    CHECK(mc_client_gui_ingame_draw_inspect(call, &state) && state.returned &&
          state.chat_returned && state.chat_entries == 10 && state.chat_draws == 8 &&
          state.chat_backgrounds == 8);
    for (size_t i = 0; i < 8; ++i)
        CHECK(state.chat_alpha[i] == expected[i]);
    mc_client_capture capture = {0};
    CHECK(mc_client_display_capture_rgba(app->display, &capture, NULL));
    SDL_Surface *surface = SDL_CreateSurfaceFrom(
        capture.width, capture.height, SDL_PIXELFORMAT_RGBA32, capture.pixels, (int)capture.stride);
    CHECK(surface != NULL && SDL_SavePNG(surface, screenshot));
    SDL_DestroySurface(surface);
    mc_client_capture_free(&capture);
    CHECK(mc_client_gui_ingame_draw_retire(&call, &error) == MC_CLIENT_GUI_INGAME_DRAW_OK);
    mc_client_gui_ingame_error clear;
    CHECK(mc_client_gui_ingame_clear_messages(app->v, &clear) == MC_CLIENT_GUI_INGAME_OK);
    puts("CHAT_DRAW closed=10 backgrounds=8 text=8 real-font=1");
    return 0;
}

static int chat_device(const char *resources, const char *assets, const char *directory,
                       const char *screenshot) {
    fixture runtime;
    startup_fixture startup;
    CHECK(fixture_open(&runtime, resources) == 0 && startup_services(&startup) == 0 &&
          startup_icons(&startup, assets) == 0);
    client_application *app = NULL;
    CHECK(mc_client_application_constructor_allocate(&runtime.services, NULL, &app,
                                                     runtime.error) == MC_CLIENT_APP_OK);
    const mc_client_application_constructor_arguments arguments = {NULL, NULL, NULL,
                                                                   320,  240,  false};
    CHECK(mc_client_application_constructor_construct(app, &arguments, runtime.error) ==
              MC_CLIENT_APP_OK &&
          statistics_session(app) == 0);
    mc_client_application_startup *attempt = NULL;
    mc_client_startup_options options = mc_client_application_startup_default_options();
    options.hidden = true;
    CHECK(mc_client_application_startup_allocate(app, &startup.services, &options, &attempt,
                                                 startup.error) == MC_CLIENT_STARTUP_OK);
    CHECK(mc_client_application_run_prefix(attempt, startup.error) ==
              MC_CLIENT_STARTUP_UNIMPLEMENTED &&
          startup.error->caller.pc == 32);
    CHECK(statistics_retire_resources(attempt) == 0);
    /* Startup already displayed FU749, whose actual Minecraft.a101 cleared
     * startup UQ.e. The original component starts with new UQ/ArrayList instead.
     * Replay on a distinct genuine receiver, retaining the full startup graph
     * and its unchanged App.v for the later ordinary HUD draw. */
    mc_client_source_uq *startup_owner = app->v, *component = NULL;
    mc_client_gui_ingame_error component_error;
    CHECK(startup_owner != NULL && startup_owner->fields.e->size == 0 &&
          startup_owner->fields.e->mod_count == 1);
    CHECK(mc_client_gui_ingame_allocate(startup_owner->classes, NULL, &component,
                                        &component_error) == MC_CLIENT_GUI_INGAME_OK &&
          mc_client_gui_ingame_construct(component, app, &component_error) ==
              MC_CLIENT_GUI_INGAME_OK);
    CHECK(component->fields.e != startup_owner->fields.e && component->fields.e->mod_count == 0);
    CHECK(chat_observations(component) == 0 && chat_failure_ownership(component) == 0);
    CHECK(app->v == startup_owner && startup_owner->fields.e->size == 0 &&
          startup_owner->fields.e->mod_count == 1);
    CHECK(mc_client_gui_ingame_retire(&component, &component_error) == MC_CLIENT_GUI_INGAME_OK &&
          component == NULL);
    mc_math_classes *math = NULL;
    CHECK(mc_math_classes_create(SIZE_MAX, &math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(math, NULL) == MC_MATH_CLASS_OK);
    hud_world world = {0};
    CHECK(hud_world_open(&world, &runtime, app, math, directory) == 0);
    mc_client_singleplayer_controller *controller = NULL;
    CHECK(mc_client_singleplayer_controller_allocate(runtime.services.entities,
                                                     runtime.services.profile, 0, &controller,
                                                     NULL) == MC_SINGLEPLAYER_CONTROLLER_OK);
    CHECK(mc_client_singleplayer_controller_construct(controller, app, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    app->controller = controller;
    CHECK(chat_draw(&runtime, app, world.player, math, screenshot) == 0);
    app->controller = NULL;
    CHECK(mc_client_singleplayer_controller_free(&controller, NULL) ==
          MC_SINGLEPLAYER_CONTROLLER_OK);
    mc_client_render_texture_error *terminal = calloc(1, sizeof(*terminal));
    CHECK(terminal != NULL);
    mc_client_render_manager_classes *manager = attempt->manager_classes;
    app->i = app->h = NULL;
    app->active_world = NULL;
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(mc_client_render_manager_retire_hands(manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(mc_client_render_manager_classes_retire(&manager, &startup.error->manager) ==
          MC_CLIENT_MANAGER_OK);
    CHECK(hud_world_close(&world, app) == 0 &&
          mc_math_classes_release(&math, NULL) == MC_MATH_CLASS_OK);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_BUSY);
    CHECK(mc_client_render_engine_retire_resources(runtime.graphics, terminal) ==
          MC_CLIENT_RENDER_TEXTURE_OK);
    free(terminal);
    CHECK(mc_client_application_startup_retire(&attempt, startup.error) == MC_CLIENT_STARTUP_OK &&
          attempt == NULL);
    CHECK(statistics_end_app(&runtime, &startup, &app) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 5 && chat_device(argv[1], argv[2], argv[3], argv[4]) == 0);
    printf("GuiIngame chat: %u checks; genuine Font, text/List/age components and closed HUD\n",
           checks);
    return 0;
}
