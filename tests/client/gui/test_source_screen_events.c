/* Prepared source receivers and genuine SDK ByteBuffer consumers. These do
 * not credit a native Display/SDK constructor or complete gameplay startup. */
#include "client/application_state.h"
#include "client/display/sdk_input_internal.h"
#include "client/gui/source_screen_events.h"
#include "client/gui/source_screen_internal.h"
#include "client/startup/application_owner.h"
#include "mc/client/sound/backend.h"
#include "mc/resources/files/files.h"

#include <SDL3/SDL_init.h>
#include <fenv.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "screen event check%u line%d: %s\n", checks, __LINE__, #condition);    \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static void put32(uint8_t *p, uint32_t value) {
    for (unsigned int index = 0; index < 4; ++index)
        p[index] = (uint8_t)(value >> (24u - index * 8u));
}

int main(void) {
    mc_java_random_profile *profile = NULL;
    mc_resource_files *resources = NULL;
    mc_client_screen_classes *classes = NULL;
    mc_client_source_fu *menu = NULL;
    mc_client_source_ke *button = NULL;
    mc_client_source_du *particles = NULL;
    mc_sound_backend *backend = NULL;
    mc_sound_manager_shared *shared = NULL;
    client_application *app = calloc(1, sizeof(*app));
    mc_client_screen_event_error *e = calloc(1, sizeof(*e));
    mc_client_screen_event_report report;
    CHECK(app != NULL && e != NULL && SDL_Init(0));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_resource_files_create("screen-event-absent-resources", &resources) == MC_REGISTRY_OK);
    CHECK(mc_client_screen_classes_create(profile, mc_resource_files_source(resources), NULL,
                                          &classes, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_menu_allocate(classes, NULL, &menu, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_menu_construct(menu, NULL) == MC_CLIENT_SCREEN_OK);
    mc_client_source_da *screen = mc_client_screen_menu_base(menu);
    CHECK(mc_client_application_lifetime_register(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    app->constructor_phase = MC_CLIENT_APP_READY; /* Explicit prepared App receiver. */
    app->d = 800;
    app->e = 480;
    screen->fields.b = app;
    screen->fields.c = 400;
    screen->fields.d = 240;
    CHECK(mc_client_screen_button_allocate(classes, &button, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_button_construct(button, 1, 20, 30, 80, 20, (mc_client_screen_text){0},
                                            NULL) == MC_CLIENT_SCREEN_OK);
    mc_client_screen_list *list = (mc_client_screen_list *)screen->fields.e;
    list->entries = mc_screen_allocate(&screen->memory, sizeof(*list->entries));
    CHECK(list->entries != NULL);
    CHECK(mc_client_screen_button_retain(button, NULL) == MC_CLIENT_SCREEN_OK);
    list->entries[0] = button; /* Prepared actual List owns this independent occurrence. */
    list->capacity = list->size = 1;
    bool hit = false;
    CHECK(mc_client_screen_button_hit(button, 20, 30, &hit, NULL) == MC_CLIENT_SCREEN_OK && hit);
    CHECK(mc_client_screen_button_hit(button, 100, 30, &hit, NULL) == MC_CLIENT_SCREEN_OK && !hit);
    CHECK(mc_client_screen_button_hit(button, 20, 50, &hit, NULL) == MC_CLIENT_SCREEN_OK && !hit);
    button->fields.h = false;
    CHECK(mc_client_screen_button_hit(button, 20, 30, &hit, NULL) == MC_CLIENT_SCREEN_OK && hit);
    button->fields.g = false;
    CHECK(mc_client_screen_button_hit(button, 20, 30, &hit, NULL) == MC_CLIENT_SCREEN_OK && !hit);
    button->fields.g = true;
    const mc_client_sdk_mouse_event down = {.button = 0, .x = 40, .y = 418, .pressed = true};
    CHECK(mc_client_screen_mouse_event(screen, &down, &report, e) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          e->screen.site.pc == 68 && !e->screen.native_policy && screen->fields.a == button &&
          report.mouse_x == 20 && report.mouse_y == 30 && report.click_calls == 0 &&
          report.actions == 0);
    mc_client_sdk_mouse_event up = down;
    up.pressed = false;
    up.button = 1;
    CHECK(mc_client_screen_mouse_event(screen, &up, &report, e) == MC_CLIENT_SCREEN_OK &&
          screen->fields.a == button && report.releases == 0);
    up.button = 0;
    CHECK(mc_client_screen_mouse_event(screen, &up, &report, e) == MC_CLIENT_SCREEN_OK &&
          screen->fields.a == NULL && report.releases == 1);
    CHECK(mc_sound_backend_create(profile, NULL, &backend, NULL) == MC_SOUND_BACKEND_OK);
    mc_sound_manager_backend calls;
    CHECK(mc_sound_backend_manager_interface(backend, &calls));
    CHECK(mc_sound_manager_shared_create(&calls, &shared, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_manager_create(shared, profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES, &app->B,
                                  NULL) == MC_SOUND_OK);
    const unsigned int sites[] = {11, 41, 64, 87};
    const mc_client_screen_action actions[] = {
        MC_CLIENT_SCREEN_ACTION_OPTIONS, MC_CLIENT_SCREEN_ACTION_SINGLEPLAYER,
        MC_CLIENT_SCREEN_ACTION_MULTIPLAYER, MC_CLIENT_SCREEN_ACTION_TEXTURE_PACKS};
    for (int32_t id = 0; id < 4; ++id) {
        button->fields.f = id;
        CHECK(mc_client_screen_mouse_event(screen, &down, &report, e) ==
                  MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY &&
              report.action == actions[id] && report.pending.pc == sites[id] &&
              report.button == button && report.click_calls == 1 && report.actions == 1 &&
              screen->fields.a == button && e->screen.parent_site.pc == 74);
    }
    button->fields.f = 4;
    atomic_store(&app->J, true);
    CHECK(mc_client_screen_mouse_event(screen, &down, &report, e) == MC_CLIENT_SCREEN_OK &&
          report.action == MC_CLIENT_SCREEN_ACTION_QUIT && report.quit_returned &&
          report.click_calls == 1 && !atomic_load(&app->J));
    CHECK(mc_client_screen_mouse_event(screen, &up, &report, e) == MC_CLIENT_SCREEN_OK &&
          screen->fields.a == NULL);
    mc_client_sdk_key_event escape = {.key = 1, .character = 27, .pressed = true};
    CHECK(mc_client_screen_key_event(screen, &escape, &report, e) == MC_CLIENT_SCREEN_OK &&
          report.action == MC_CLIENT_SCREEN_ACTION_NONE && report.source_returned);
    for (unsigned int tick = 0; tick < 2048; ++tick)
        CHECK(mc_client_screen_tick(screen, e) == MC_CLIENT_SCREEN_OK);
    CHECK(menu->fields.i == 2048.0f);
    menu->fields.i = 16777216.0f;
    CHECK(mc_client_screen_tick(screen, e) == MC_CLIENT_SCREEN_OK && menu->fields.i == 16777216.0f);
    CHECK(fesetround(FE_UPWARD) == 0);
    CHECK(mc_client_screen_tick(screen, e) == MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY &&
          menu->fields.i == 16777216.0f);
    CHECK(fesetround(FE_TONEAREST) == 0);
    CHECK(mc_client_screen_particles_allocate(classes, NULL, &particles, NULL) ==
          MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_particles_construct(particles, app, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_particles_tick(particles, e) == MC_CLIENT_SCREEN_OK &&
          e->screen.source_returned);
    CHECK(mc_client_screen_particles_draw(particles, 0.5f, e) ==
              MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE &&
          e->screen.site.pc == 16 && !e->screen.native_policy);

    /* Real Mouse.next/Keyboard.next bodies consume canonical owned ByteBuffers.
     * This selected prepared SDK receiver has no claimed Display construction. */
    mc_client_sdk_input input = {.phase = MC_CLIENT_SDK_INPUT_READY, .parent_dispatch = true};
    mc_client_sdk_key_event current_key = {0};
    input.current_key = &current_key;
    CHECK(mc_client_sdk_bytes_create(44, &input.mouse_read) == MC_CLIENT_SDK_QUEUE_OK);
    CHECK(mc_client_sdk_bytes_create(36, &input.keyboard_read) == MC_CLIENT_SDK_QUEUE_OK);
    uint8_t mouse_bytes[44] = {0}, key_bytes[36] = {0};
    mouse_bytes[0] = mouse_bytes[22] = 255; /* Motion events, no left release. */
    put32(key_bytes, 87);
    key_bytes[4] = 1;
    put32(key_bytes + 18, 1);
    key_bytes[22] = 1;
    CHECK(mc_client_sdk_bytes_write(input.mouse_read, mouse_bytes, sizeof(mouse_bytes)));
    CHECK(mc_client_sdk_bytes_write(input.keyboard_read, key_bytes, sizeof(key_bytes)));
    mc_client_sdk_bytes_flip(input.mouse_read);
    mc_client_sdk_bytes_flip(input.keyboard_read);
    CHECK(mc_client_screen_handle_input(screen, &input, &report, e) ==
              MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY &&
          report.mouse_events == 2 && report.keyboard_events == 1 &&
          report.action == MC_CLIENT_SCREEN_ACTION_FULLSCREEN && report.pending.pc == 18 &&
          mc_client_sdk_bytes_remaining(input.mouse_read) == 0 &&
          mc_client_sdk_bytes_remaining(input.keyboard_read) == 18);
    mc_client_sdk_bytes_clear(input.mouse_read);
    mouse_bytes[0] = 0;
    mouse_bytes[1] = 1;
    put32(mouse_bytes + 2, 40);
    put32(mouse_bytes + 6, 418);
    CHECK(mc_client_sdk_bytes_write(input.mouse_read, mouse_bytes, sizeof(mouse_bytes)));
    mc_client_sdk_bytes_flip(input.mouse_read);
    button->fields.f = 1;
    CHECK(mc_client_screen_handle_input(screen, &input, &report, e) ==
              MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY &&
          report.action == MC_CLIENT_SCREEN_ACTION_SINGLEPLAYER && report.mouse_events == 1 &&
          report.keyboard_events == 0 && mc_client_sdk_bytes_remaining(input.mouse_read) == 22 &&
          mc_client_sdk_bytes_remaining(input.keyboard_read) == 18);
    mc_client_sdk_bytes_free(&input.mouse_read);
    mc_client_sdk_bytes_free(&input.keyboard_read);
    app->d = 0;
    CHECK(mc_client_screen_mouse_event(screen, &down, &report, e) ==
              MC_CLIENT_SCREEN_SOURCE_ARITHMETIC &&
          e->screen.site.pc == 21 && !e->screen.native_policy);
    app->d = 800;
    /* Selected a owns a real extra occurrence; final Screen cleanup must drop
     * it before retiring List/button occurrences, even without a release event. */
    CHECK(screen->fields.a == button);
    CHECK(mc_client_screen_particles_retire(&particles, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_menu_retire(&menu, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_button_retire(&button, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_client_screen_classes_release(&classes, NULL) == MC_CLIENT_SCREEN_OK);
    CHECK(mc_sound_manager_retire(&app->B, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_manager_shared_retire(&shared, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_backend_retire(&backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(mc_client_application_lifetime_retire(app, NULL) == MC_CLIENT_APPLICATION_OWNER_OK);
    mc_java_random_profile_free(profile);
    CHECK(mc_resource_files_free(resources) == MC_REGISTRY_OK);
    free(e);
    free(app);
    SDL_Quit();
    printf("Source screen events: %u checks\n", checks);
    return 0;
}
