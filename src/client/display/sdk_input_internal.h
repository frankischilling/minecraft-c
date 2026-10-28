#ifndef MC_CLIENT_DISPLAY_SDK_INPUT_INTERNAL_H
#define MC_CLIENT_DISPLAY_SDK_INPUT_INTERNAL_H

#include "display.h"
#include "sdk_input.h"

typedef struct mc_client_sdk_name_map {
    mc_player_string **names;
    size_t capacity, count;
} mc_client_sdk_name_map;

typedef struct mc_client_sdk_empty_list {
    size_t count, capacity;
    void **entries;
} mc_client_sdk_empty_list;

struct mc_client_sdk_controller_environment;
struct mc_client_sdk_controller_attempt;

struct mc_client_sdk_input {
    mc_client_display *display;
    SDL_Window *window;
    mc_client_event_port *events;
    mc_client_sdk_input_phase phase;
    bool active, parent_dispatch, mouse_initialized, mouse_created, keyboard_initialized,
        keyboard_created;
    bool controllers_created, grabbed, clip_coordinates, repeat_enabled, text_input_started,
        has_wheel;
    mc_client_sdk_platform platform;
    mc_client_sdk_name_map button_map, key_map;
    mc_client_sdk_empty_list *controllers, *controller_events;
    int32_t controller_count; /* Actual Source default0; never SDL device ID. */
    void *controller_event;   /* Actual nullable Controllers.event. */
    int32_t controllers_last_clinit_store, controllers_last_create_pc;
    bool controllers_native_retired;
    struct mc_client_sdk_controller_environment *controller_environment;
    struct mc_client_sdk_controller_attempt *controller_attempt;
    mc_client_sdk_bytes *buttons, *key_down, *mouse_read, *keyboard_read;
    int32_t *coordinates;
    void *current_cursor;
    mc_client_sdk_bytes *mouse_temp, *keyboard_temp;
    mc_client_sdk_queue *mouse_queue, *keyboard_queue;
    mc_client_event_record *pending;
    mc_client_sdk_input_error failure;
    int32_t x, y, absolute_x, absolute_y, dx, dy, wheel, button_count;
    int32_t native_wheel, native_x, native_y, native_dx, native_dy;
    int32_t grab_x, grab_y, event_raw_x, event_raw_y;
    bool backend_grabbed, emulate_cursor_animation;
    int32_t width, height;
    int32_t last_mouse_store, last_keyboard_store;
    mc_client_sdk_key_event *current_key, *temporary_key;
    mc_client_sdk_key_event retained_key;
    uint8_t backend_keys[256];
    uint8_t *backend_buttons;
    bool key_pending;
    mc_client_sdk_mouse_event current_mouse;
    size_t queue_full_results;
};

mc_client_sdk_input_status mc_client_sdk_input_fail(mc_client_sdk_input_error *,
                                                    mc_client_sdk_input_status, const char *,
                                                    const char *, const char *, const char *,
                                                    int32_t, bool);
void mc_client_sdk_input_caller(mc_client_sdk_input_error *, const char *, const char *,
                                const char *, int32_t);
bool mc_client_sdk_input_enter(mc_client_sdk_input *, const char *class_name, const char *method,
                               const char *descriptor, mc_client_sdk_input_error *);
mc_client_sdk_input_status mc_client_sdk_input_leave(mc_client_sdk_input *,
                                                     mc_client_sdk_input_error *,
                                                     mc_client_sdk_input_error *);
bool mc_client_sdk_input_collect_mouse(mc_client_sdk_input *, mc_client_sdk_input_error *);
bool mc_client_sdk_input_collect_keyboard(mc_client_sdk_input *, mc_client_sdk_input_error *);
bool mc_client_sdk_input_read_queue(mc_client_sdk_input *, bool mouse, mc_client_sdk_input_error *);
bool mc_client_sdk_input_flush_key(mc_client_sdk_input *, mc_client_sdk_input_error *);
bool mc_client_sdk_mouse_poll_body(mc_client_sdk_input *, bool collect,
                                   mc_client_sdk_input_error *);
int32_t mc_client_sdk_input_key_code(int32_t sdl_scancode);
/* Implemented by the canonical Display owner. Source-update activity protects
 * its input child even while a real SDL pump callback runs before device poll. */
bool mc_client_display_sdk_operation_busy(const mc_client_display *);

#endif
