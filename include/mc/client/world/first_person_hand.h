#ifndef MC_CLIENT_WORLD_FIRST_PERSON_HAND_H
#define MC_CLIENT_WORLD_FIRST_PERSON_HAND_H
#include "mc/client/world/renderer_camera.h"

typedef struct mc_client_first_person_hand_call mc_client_first_person_hand_call;
typedef struct mc_client_held_item_draw_error mc_client_held_item_draw_error;
typedef struct mc_client_held_item_draw_state mc_client_held_item_draw_state;

typedef enum mc_client_first_person_hand_status {
    MC_CLIENT_FIRST_PERSON_HAND_OK,
    MC_CLIENT_FIRST_PERSON_HAND_ARGUMENT,
    MC_CLIENT_FIRST_PERSON_HAND_BUSY,
    MC_CLIENT_FIRST_PERSON_HAND_ALLOCATION,
    MC_CLIENT_FIRST_PERSON_HAND_SOURCE_NULL,
    MC_CLIENT_FIRST_PERSON_HAND_NATIVE_FAILED,
    MC_CLIENT_FIRST_PERSON_HAND_CHILD_FAILED,
    MC_CLIENT_FIRST_PERSON_HAND_UNSUPPORTED_FP,
    MC_CLIENT_FIRST_PERSON_HAND_DEPENDENCY
} mc_client_first_person_hand_status;

typedef struct mc_client_first_person_hand_error {
    mc_client_first_person_hand_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    mc_client_display_error display;
    mc_entity_status entity;
    mc_player_error player;
    const mc_client_renderer_camera_error *camera;
    const mc_client_held_item_draw_error *held_item;
} mc_client_first_person_hand_error;

typedef struct mc_client_first_person_hand_state {
    bool active, retiring, returned, modelview_reset, anaglyph_read, anaglyph;
    bool matrix_pushed, matrix_popped, first_sleep_read, first_sleeping;
    bool second_sleep_read, second_sleeping, held_returned, overlays_returned;
    unsigned int source_pc, transforms, hurt_calls, bob_calls;
    float partial;
    int32_t eye;
    mc_client_source_px *owner;
    const mc_client_renderer_camera_state *effects[4];
    const mc_client_held_item_draw_state *held_item, *overlays;
    const mc_client_first_person_hand_error *failure;
} mc_client_first_person_hand_state;

bool mc_client_first_person_hand_inspect(const mc_client_first_person_hand_call *,
                                         mc_client_first_person_hand_state *);
/* Native cleanup leaves all reached source matrix/GL effects intact. The
 * parent PX and its complete App graph remain live/exclusive until retirement. */
mc_client_first_person_hand_status
mc_client_first_person_hand_retire(mc_client_first_person_hand_call **,
                                   mc_client_first_person_hand_error *);
#endif
