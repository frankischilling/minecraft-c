/* Genuine Source Display/input + actual SDL discovery. Optional virtual
 * fixture is a real SDL device, not original JInput/physical-device parity.
 * Only these exact controller TUs are included; archive copies must not be
 * extracted. Allocation hook forwards real calloc before testing reentry. */
#include "client/display/sdk_controllers_internal.h"
#include "client/display/sdk_controllers_native.h"
#include "client/display/sdk_display_internal.h"
#include "client/display/staged.h"
#include "mc/resources/files/files.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mc_client_sdk_input *watched_input;
static bool reentry_done, reentry_ok, enumeration_failure;
static size_t enumeration_calls, retirement_retention_calls, actual_quit_calls;
static bool inject_retirement_failure, quit_reentry_ok;

static mc_client_events_status forwarded_retention(mc_client_event_port *port,
                                                   mc_client_events_error *error) {
    const mc_client_events_status status = mc_client_events_retain_pending(port, error);
    ++retirement_retention_calls;
    if (inject_retirement_failure && retirement_retention_calls == 2 &&
        status == MC_CLIENT_EVENTS_OK) {
        if (error != NULL) {
            error->status = MC_CLIENT_EVENTS_ALLOCATION_FAILED;
            error->operation = "authored failure after real post-drop retention";
        }
        return MC_CLIENT_EVENTS_ALLOCATION_FAILED;
    }
    return status;
}

static void forwarded_quit(SDL_InitFlags flags) {
    ++actual_quit_calls;
    if (watched_input != NULL) {
        mc_client_sdk_input *same = watched_input;
        mc_client_sdk_input_error error = {0};
        quit_reentry_ok =
            mc_client_sdk_controllers_create(watched_input, &error) == MC_CLIENT_SDK_INPUT_BUSY &&
            mc_client_sdk_controllers_poll(watched_input, &error) == MC_CLIENT_SDK_INPUT_BUSY &&
            mc_client_sdk_input_retire(&same, &error) == MC_CLIENT_SDK_INPUT_BUSY &&
            same == watched_input;
    }
    SDL_QuitSubSystem(flags);
}

static void *forwarded_calloc(size_t count, size_t size) {
    void *memory = calloc(count, size);
    if (watched_input != NULL && !reentry_done) {
        reentry_done = true;
        mc_client_sdk_input *same = watched_input;
        mc_client_sdk_input_error error = {0};
        reentry_ok =
            mc_client_sdk_controllers_create(watched_input, &error) == MC_CLIENT_SDK_INPUT_BUSY &&
            mc_client_sdk_controllers_poll(watched_input, &error) == MC_CLIENT_SDK_INPUT_BUSY &&
            mc_client_sdk_input_retire(&same, &error) == MC_CLIENT_SDK_INPUT_BUSY &&
            same == watched_input;
    }
    return memory;
}

static SDL_JoystickID *forwarded_ids(int *count) {
    ++enumeration_calls;
    SDL_JoystickID *ids = SDL_GetJoysticks(count);
    if (enumeration_failure && ids != NULL) {
        SDL_free(ids);
        if (count != NULL)
            *count = 0;
        SDL_SetError("authored enumeration failure after actual native query");
        return NULL;
    }
    return ids;
}

#define calloc forwarded_calloc
#define SDL_GetJoysticks forwarded_ids
#define SDL_QuitSubSystem forwarded_quit
#define mc_client_events_retain_pending forwarded_retention
#include "../../../src/client/display/sdk_controllers.c"
#include "../../../src/client/display/sdk_controllers_native.c"
#undef mc_client_events_retain_pending
#undef SDL_QuitSubSystem
#undef SDL_GetJoysticks
#undef calloc

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    int result = 1;
    mc_resource_files *files = NULL;
    mc_client_sdk_icons *icons = NULL;
    mc_client_display *display = NULL;
    mc_client_display_mode *mode = NULL;
    mc_client_sdk_icons_error icon_error = {0};
    mc_client_display_stage_error display_error = {0};
    mc_client_sdk_input_error error = {0};
    mc_client_events_error event_error = {0};
    mc_client_sdk_update_error update_error = {0};
    SDL_JoystickID virtual_id = 0;
    bool fixture_reference = false;
    const char *observed = NULL;
    CHECK(argc == 2 || (argc == 3 && (strcmp(argv[2], "--virtual") == 0 ||
                                      strcmp(argv[2], "--enumeration-failure") == 0 ||
                                      strcmp(argv[2], "--retire-failure") == 0)));
    const bool retire_profile = argc == 3 && strcmp(argv[2], "--retire-failure") == 0;
    const bool virtual_profile = argc == 3 && strcmp(argv[2], "--virtual") == 0;
    enumeration_failure = argc == 3 && strcmp(argv[2], "--enumeration-failure") == 0;
    CHECK(mc_resource_files_create(argv[1], &files) == MC_REGISTRY_OK);
    CHECK(mc_client_sdk_icons_allocate(mc_resource_files_source(files), &icons, &icon_error) ==
          MC_CLIENT_SDK_ICONS_OK);
    CHECK(mc_client_sdk_icons_construct(icons, &icon_error) == MC_CLIENT_SDK_ICONS_OK);
    CHECK(mc_client_display_stage_allocate(&display, &display_error));
    CHECK(mc_client_display_stage_bind_icons(display, icons, &display_error));
    CHECK(mc_client_display_mode_allocate(&mode, &display_error));
    CHECK(mc_client_display_mode_construct(mode, 160, 120, &display_error));
    CHECK(mc_client_display_stage_set_mode(display, mode, &display_error));
    CHECK(mc_client_display_stage_set_title(display, NULL, &display_error));
    CHECK(mc_client_display_create_staged(display, true, &display_error));
    mc_client_sdk_input *input = mc_client_display_inputs(display);
    mc_client_event_port *port = mc_client_display_event_port(display);
    CHECK(input != NULL && port != NULL);
    bool created = true;
    CHECK(mc_client_sdk_controllers_is_created(input, &created, &error) == MC_CLIENT_SDK_INPUT_OK &&
          !created);
    mc_client_sdk_controllers_state before = {0}, after = {0};
    CHECK(mc_client_sdk_controllers_inspect(input, &before));
    CHECK(before.controllers_identity != NULL && before.events_identity != NULL &&
          before.controllers_identity != before.events_identity && !before.created &&
          before.controllers_count == 0 && before.events_count == 0 &&
          before.controller_count == 0 && before.environment_identity == NULL &&
          before.last_clinit_store_pc == 17);
    CHECK((before.configured_backend_mask & ~(uint32_t)MC_CLIENT_SDK_CONTROLLER_VIRTUAL) != 0);
    mc_client_events_state events_before = {0}, events_after = {0};
    CHECK(mc_client_events_inspect(port, &events_before));
    CHECK(mc_client_sdk_controllers_poll(input, &error) == MC_CLIENT_SDK_INPUT_OK);
    CHECK(mc_client_events_inspect(port, &events_after) &&
          events_before.pump_generation == events_after.pump_generation);
    if (virtual_profile) {
        CHECK(SDL_InitSubSystem(SDL_INIT_JOYSTICK));
        fixture_reference = true;
        SDL_VirtualJoystickDesc description;
        SDL_INIT_INTERFACE(&description);
        description.type = SDL_JOYSTICK_TYPE_UNKNOWN;
        description.naxes = 1;
        description.nbuttons = 1;
        description.nhats = 1;
        description.name = "authored real SDL virtual discovery fixture";
        virtual_id = SDL_AttachVirtualJoystick(&description);
        CHECK(virtual_id != 0);
    }
    watched_input = input;
    const mc_client_sdk_input_status status = mc_client_sdk_controllers_create(input, &error);
    watched_input = NULL;
    CHECK(reentry_done && reentry_ok);
    CHECK(mc_client_sdk_controllers_inspect(input, &after));
    CHECK(after.controllers_identity == before.controllers_identity &&
          after.events_identity == before.events_identity && after.controllers_count == 0 &&
          after.events_count == 0 && after.controller_count == 0 &&
          after.subsystem_reference_owned && after.initialization_attempted &&
          after.init_calls == 1 && after.enumeration_calls == 1 && enumeration_calls == 1);
    CHECK(mc_client_events_inspect(port, &events_after) &&
          events_before.pump_generation == events_after.pump_generation);
    if (enumeration_failure) {
        CHECK(status == MC_CLIENT_SDK_INPUT_PLATFORM_FAILED && error.native_policy &&
              error.site.pc == 12);
        CHECK(!after.created && !after.enumeration_returned && after.native_ids == NULL &&
              after.environment_phase == MC_CLIENT_SDK_CONTROLLER_ENV_FAILED &&
              after.create_attempt_retained);
        CHECK(strcmp(error.detail, "authored enumeration failure after actual native query") == 0);
        observed = "native-enumeration-failure";
    } else if (status == MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE) {
        CHECK(error.native_policy && error.site.pc == 12 && after.enumeration_returned &&
              after.native_count > 0 && after.native_ids != NULL &&
              after.native_ids[after.native_count] == 0);
        CHECK(!after.created && after.create_attempt_retained &&
              after.returned_array_identity == NULL && after.filtered_list_identity == NULL &&
              after.last_create_pc == 7);
        observed = "nonempty-mapping-frontier";
    } else {
        CHECK(!virtual_profile && status == MC_CLIENT_SDK_INPUT_OK);
        CHECK(after.created && after.enumeration_returned && after.native_count == 0 &&
              after.native_ids != NULL && after.native_ids[0] == 0 && after.last_create_pc == 143 &&
              !after.create_attempt_retained);
        const void *environment = after.environment_identity;
        CHECK(mc_client_sdk_controllers_create(input, &error) == MC_CLIENT_SDK_INPUT_OK);
        CHECK(mc_client_sdk_controllers_inspect(input, &after) && after.last_create_pc == 6 &&
              after.environment_identity == environment && after.init_calls == 1 &&
              after.enumeration_calls == 1 && enumeration_calls == 1);
        CHECK(mc_client_sdk_controllers_poll(input, &error) == MC_CLIENT_SDK_INPUT_OK);
        CHECK(mc_client_events_inspect(port, &events_after) &&
              events_before.pump_generation == events_after.pump_generation);
        CHECK(mc_client_sdk_display_update(mc_client_display_update_owner(display),
                                           &update_error) == MC_CLIENT_SDK_UPDATE_OK);
        CHECK(mc_client_sdk_controllers_inspect(input, &after) && after.created &&
              after.controller_count == 0);
        observed = "observed-empty-source-return";
    }
    if (retire_profile) {
        const void *environment = after.environment_identity;
        const uint32_t *ids = after.native_ids;
        mc_client_display *same_display = display;
        retirement_retention_calls = 0;
        inject_retirement_failure = true;
        watched_input = input;
        CHECK(!mc_client_display_stage_retire(&display, &display_error));
        watched_input = NULL;
        CHECK(display == same_display && display_error.has_input &&
              display_error.input.status == MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED &&
              display_error.input.events.status == MC_CLIENT_EVENTS_ALLOCATION_FAILED &&
              display_error.input.site.class_name == NULL && quit_reentry_ok);
        CHECK(mc_client_sdk_controllers_inspect(input, &after) &&
              after.environment_identity == environment && after.native_ids == ids &&
              after.environment_phase == MC_CLIENT_SDK_CONTROLLER_ENV_RETIRING &&
              !after.native_active && !after.subsystem_reference_owned && after.quit_calls == 1 &&
              actual_quit_calls == 1 && retirement_retention_calls == 2);
        CHECK(input->controller_environment->before_drop_retained &&
              !input->controller_environment->after_drop_retained &&
              input->phase == MC_CLIENT_SDK_INPUT_RETIRING && !input->active);
        inject_retirement_failure = false;
        CHECK(mc_client_display_stage_retire(&display, &display_error) && display == NULL);
        CHECK(actual_quit_calls == 1 && retirement_retention_calls == 3);
    }
    result = 0;
cleanup:
    watched_input = NULL;
    inject_retirement_failure = false;
    if (virtual_id != 0 && !SDL_DetachVirtualJoystick(virtual_id))
        result = 1;
    if (fixture_reference) {
        if (display != NULL &&
            mc_client_events_retain_pending(mc_client_display_event_port(display), &event_error) !=
                MC_CLIENT_EVENTS_OK)
            result = 1;
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
        if (display != NULL &&
            mc_client_events_retain_pending(mc_client_display_event_port(display), &event_error) !=
                MC_CLIENT_EVENTS_OK)
            result = 1;
    }
    if (display != NULL && !mc_client_display_stage_retire(&display, &display_error))
        result = 1;
    mc_client_display_mode_release(&mode);
    if (icons != NULL && mc_client_sdk_icons_release(&icons, &icon_error) != MC_CLIENT_SDK_ICONS_OK)
        result = 1;
    if (files != NULL && mc_resource_files_free(files) != MC_REGISTRY_OK)
        result = 1;
    if (result == 0 && printf("SDK Controllers discovery: %u checks; %s\n", checks, observed) < 0)
        result = 1;
    return result;
}
