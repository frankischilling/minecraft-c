#ifndef MC_CLIENT_DISPLAY_SDK_INPUT_H
#define MC_CLIENT_DISPLAY_SDK_INPUT_H

#include "events.h"
#include "mc/entity/player/player.h"
#include "sdk_platform.h"
#include "sdk_queue.h"

typedef struct mc_client_sdk_input mc_client_sdk_input;
typedef struct mc_client_display mc_client_display;

typedef enum mc_client_sdk_input_status {
    MC_CLIENT_SDK_INPUT_OK,
    MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT,
    MC_CLIENT_SDK_INPUT_INVALID_PHASE,
    MC_CLIENT_SDK_INPUT_BUSY,
    MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED,
    MC_CLIENT_SDK_INPUT_LIMIT,
    MC_CLIENT_SDK_INPUT_PLATFORM_FAILED,
    MC_CLIENT_SDK_INPUT_UNSUPPORTED_PROFILE,
    MC_CLIENT_SDK_INPUT_UNSUPPORTED_EVENT,
    MC_CLIENT_SDK_INPUT_UNSUPPORTED_FP,
    MC_CLIENT_SDK_INPUT_SOURCE_ILLEGAL_STATE,
    MC_CLIENT_SDK_INPUT_SOURCE_INDEX
} mc_client_sdk_input_status;

typedef enum mc_client_sdk_input_phase {
    MC_CLIENT_SDK_INPUT_RAW,
    MC_CLIENT_SDK_INPUT_CONSTRUCTING,
    MC_CLIENT_SDK_INPUT_READY,
    MC_CLIENT_SDK_INPUT_FAILED,
    MC_CLIENT_SDK_INPUT_RETIRING
} mc_client_sdk_input_phase;

typedef struct mc_client_sdk_input_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_client_sdk_input_site;

typedef struct mc_client_sdk_input_error {
    mc_client_sdk_input_status status;
    const char *operation;
    mc_client_sdk_input_site site, child;
    /* Innermost-to-outermost completed caller invokes in this finite closure;
     * never a synthetic JVM stack. Eight covers the derived fixed call chain. */
    mc_client_sdk_input_site callers[8];
    size_t caller_count;
    bool caller_limit;
    mc_client_sdk_input_site pending_caller;
    mc_client_events_error events;
    mc_client_sdk_queue_error queue;
    mc_client_sdk_platform_status platform;
    mc_player_status string;
    uint32_t pending_event_type;
    bool native_policy;
    char detail[256];
} mc_client_sdk_input_error;

typedef enum mc_client_sdk_controller_environment_phase {
    MC_CLIENT_SDK_CONTROLLER_ENV_COLD,
    MC_CLIENT_SDK_CONTROLLER_ENV_ALLOCATED,
    MC_CLIENT_SDK_CONTROLLER_ENV_INITIALIZING,
    MC_CLIENT_SDK_CONTROLLER_ENV_SUBSYSTEM,
    MC_CLIENT_SDK_CONTROLLER_ENV_ENUMERATED,
    MC_CLIENT_SDK_CONTROLLER_ENV_FAILED,
    MC_CLIENT_SDK_CONTROLLER_ENV_RETIRING
} mc_client_sdk_controller_environment_phase;

typedef enum mc_client_sdk_controller_backend {
    MC_CLIENT_SDK_CONTROLLER_LINUX = 1u,
    MC_CLIENT_SDK_CONTROLLER_RAWINPUT = 2u,
    MC_CLIENT_SDK_CONTROLLER_DINPUT = 4u,
    MC_CLIENT_SDK_CONTROLLER_XINPUT = 8u,
    MC_CLIENT_SDK_CONTROLLER_WGI = 16u,
    MC_CLIENT_SDK_CONTROLLER_GAMEINPUT = 32u,
    MC_CLIENT_SDK_CONTROLLER_VIRTUAL = 64u
} mc_client_sdk_controller_backend;

typedef struct mc_client_sdk_controllers_state {
    const void *controllers_identity, *events_identity, *event_identity;
    size_t controllers_count, events_count;
    int32_t controller_count, last_clinit_store_pc, last_create_pc;
    bool created, create_attempt_retained;
    const void *returned_array_identity, *filtered_list_identity, *iterator_identity;
    const void *environment_identity;
    mc_client_sdk_controller_environment_phase environment_phase;
    bool native_active, native_retired, subsystem_reference_owned, initialization_attempted,
        enumeration_returned;
    int native_count;
    const uint32_t *native_ids;
    size_t init_calls, enumeration_calls, quit_calls;
    uint32_t configured_backend_mask;
    const mc_client_sdk_input_error *environment_failure;
} mc_client_sdk_controllers_state;

typedef struct mc_client_sdk_key_event {
    int32_t key;
    uint32_t character;
    int64_t nanos;
    bool pressed, repeat;
} mc_client_sdk_key_event;

typedef struct mc_client_sdk_mouse_event {
    int32_t button, x, y, wheel, dx, dy;
    int64_t nanos;
    bool pressed;
} mc_client_sdk_mouse_event;

typedef struct mc_client_sdk_input_state {
    mc_client_sdk_input_phase phase;
    bool active, mouse_initialized, mouse_created, keyboard_initialized, keyboard_created;
    bool controllers_created, grabbed, clip_coordinates, repeat_enabled;
    mc_client_sdk_platform platform;
    int32_t x, y, dx, dy, wheel, button_count;
    const mc_client_sdk_bytes *buttons, *key_down, *mouse_read, *keyboard_read;
    const int32_t *coordinates;
    const mc_client_sdk_queue *mouse_events, *keyboard_events;
    const mc_player_string *const *button_names, *const *key_names;
    size_t button_names_count, key_names_count;
    size_t source_queue_full_results;
    int32_t last_mouse_store_pc, last_keyboard_store_pc;
    const mc_client_event_record *pending_native_event;
    const mc_client_sdk_input_error *failure;
    mc_client_sdk_key_event current_key;
    mc_client_sdk_mouse_event current_mouse;
} mc_client_sdk_input_state;

/* Explicit real source-window attachment. RAW allocation does not publish any
 * created flag. construct_default performs real native platform/window/input
 * binding, owned maps/Strings/buffers and once-only Mouse->Keyboard creation.
 * Default SDK properties and null explicit cursor are the admitted profile;
 * Animated Cursor/nondefault-property adapters remain explicit. Controllers use
 * the separately bounded genuine discovery entries below.
 * Physical-device presence is observed, not used as a fake created=false. A
 * logical OS/window attachment can exist without a connected physical HID.
 * The actual SDL port/window stays borrowed and live until native retirement. */
mc_client_sdk_input_status mc_client_sdk_input_allocate(mc_client_display *, mc_client_event_port *,
                                                        mc_client_sdk_input **out,
                                                        mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_input_construct_default(mc_client_sdk_input *,
                                                                 mc_client_sdk_input_error *);
/* Actual public create() entries at Minecraft.a362/365 on the same completed
 * default SDK owner. Display.isCreated6 and the actual implementation identity
 * precede the overload's created0/return6. No queue, grab, buffer or input state
 * is reset. Cold recreation/nondefault-property paths remain unsupported. */
mc_client_sdk_input_status mc_client_sdk_keyboard_create(mc_client_sdk_input *,
                                                         mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_create(mc_client_sdk_input *,
                                                      mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_poll(mc_client_sdk_input *,
                                                    mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_update_cursor(mc_client_sdk_input *,
                                                             mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_set_grabbed(mc_client_sdk_input *, bool,
                                                           mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_keyboard_poll(mc_client_sdk_input *,
                                                       mc_client_sdk_input_error *);
/* Actual Controllers.create/poll on this SAME SDK namespace. Discovery owns
 * a real SDL joystick subsystem occurrence and enumeration. Only an observed
 * nonnull count0 array completes this increment. Nonempty hardware remains a
 * retained native mapping dependency at create12, not an empty environment or
 * caught Java exception. created is published only at125 after real temporary
 * array/list/cursor construction. Repeated create returns6 without discovery.
 * poll has no Source created guard and does not pump/remove SDL events.
 * The full physical target/component/event/rumbler branch remains required.
 * Error/view/context storage is live and disjoint; all calls are main-thread.
 * Source Controllers.destroy remains its separate literal no-op; native final
 * retirement is performed only by the existing input owner retirement. */
mc_client_sdk_input_status mc_client_sdk_controllers_create(mc_client_sdk_input *,
                                                            mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_controllers_poll(mc_client_sdk_input *,
                                                          mc_client_sdk_input_error *);
/* Borrowed main-thread snapshot; pointers stay live only under the actual
 * owner lifetime/exclusive domain. Inspection does not initialize or poll. */
bool mc_client_sdk_controllers_inspect(const mc_client_sdk_input *,
                                       mc_client_sdk_controllers_state *);
mc_client_sdk_input_status mc_client_sdk_controllers_is_created(mc_client_sdk_input *, bool *out,
                                                                mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_take_dx(mc_client_sdk_input *, int32_t *out,
                                                       mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_take_dy(mc_client_sdk_input *, int32_t *out,
                                                       mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_take_wheel(mc_client_sdk_input *, int32_t *out,
                                                          mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_x(mc_client_sdk_input *, int32_t *,
                                                 mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_y(mc_client_sdk_input *, int32_t *,
                                                 mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_button_count(mc_client_sdk_input *, int32_t *,
                                                            mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_has_wheel(mc_client_sdk_input *, bool *,
                                                         mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_mouse_button_down(mc_client_sdk_input *, int32_t, bool *,
                                                           mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_keyboard_key_down(mc_client_sdk_input *, int32_t, bool *,
                                                           mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_keyboard_enable_repeat(mc_client_sdk_input *, bool,
                                                                mc_client_sdk_input_error *);
/* next consumes only genuine owned SDK read buffers, never pumps SDL. Unread
 * native events are retained through unsupported translation/failure. Repeat
 * filtering follows actual Keyboard.next. Empty succeeds with result=false;
 * event output is unchanged when no event is returned. */
mc_client_sdk_input_status mc_client_sdk_mouse_next(mc_client_sdk_input *, bool *result,
                                                    mc_client_sdk_mouse_event *,
                                                    mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_keyboard_next(mc_client_sdk_input *, bool *result,
                                                       mc_client_sdk_key_event *,
                                                       mc_client_sdk_input_error *);
bool mc_client_sdk_input_inspect(const mc_client_sdk_input *, mc_client_sdk_input_state *);
/* Native final ownership cleanup, no Source destroy/save/flag completion. It
 * reports unread data as lifetime state until explicit retirement consumes the
 * namespace; no ordinary update silently drops a retained FIFO. BUSY preserves
 * every occurrence. Input remains attached while any source call is active. */
mc_client_sdk_input_status mc_client_sdk_input_retire(mc_client_sdk_input **,
                                                      mc_client_sdk_input_error *);

#endif
