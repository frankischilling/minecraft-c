#ifndef MC_CLIENT_WORLD_FIRST_PERSON_H
#define MC_CLIENT_WORLD_FIRST_PERSON_H

#include "mc/client/settings/game_settings.h"
#include "mc/entity/movement/material.h"
#include "mc/entity/player/player.h"

typedef struct client_application client_application;

#define MC_CLIENT_CAMERA_MAX_OPERATIONS 32u

typedef enum mc_client_camera_status {
    MC_CLIENT_CAMERA_OK,
    MC_CLIENT_CAMERA_INVALID_ARGUMENT,
    MC_CLIENT_CAMERA_OWNER_FAILED,
    MC_CLIENT_CAMERA_NULL_REFERENCE,
    MC_CLIENT_CAMERA_UNSUPPORTED_FP,
    MC_CLIENT_CAMERA_REGISTRY_FAILED,
    MC_CLIENT_CAMERA_MATERIAL_FAILED,
    MC_CLIENT_CAMERA_WORLD_FAILED,
    MC_CLIENT_CAMERA_UNIMPLEMENTED_DEPENDENCY
} mc_client_camera_status;

typedef enum mc_client_camera_operation_kind {
    MC_CLIENT_CAMERA_TRANSLATE,
    MC_CLIENT_CAMERA_ROTATE,
    MC_CLIENT_CAMERA_PERSPECTIVE
} mc_client_camera_operation_kind;

typedef struct mc_client_camera_operation {
    mc_client_camera_operation_kind kind;
    bool projection;
    const char *method;
    unsigned int pc;
    /* Translate xyz; rotate angle/axis xyz; perspective fov/aspect/near/far. */
    float arguments[4];
} mc_client_camera_operation;

typedef struct mc_client_camera_frame {
    int32_t width, height;
    float projection[16], modelview[16]; /* Column-major native GL3 matrices. */
    double render_x, render_y, render_z; /* Raw aJ/K/L -> aM/N/O interpolation. */
    double fog_x, fog_y, fog_z;          /* g(F)'s separate bf-1.62 eye offset. */
    float far_distance, vertical_fov, portal, portal_scale;
    mc_client_camera_operation operations[MC_CLIENT_CAMERA_MAX_OPERATIONS];
    size_t operation_count;
    bool complete, cloud_fog;
} mc_client_camera_frame;

typedef struct mc_client_camera_error {
    mc_client_camera_status status;
    const char *operation, *method;
    unsigned int pc;
    mc_entity_status entity;
    mc_player_error player;
    mc_entity_material_error material;
    mc_registry_error registry;
    mc_world_storage_error world;
} mc_client_camera_error;

void mc_client_camera_error_init(mc_client_camera_error *error);

/* Finite component entry for selected real px.a(FI) -> d/e/f/g operations.
 * Supplied settings/World identities and dimensions are explicit fixed inputs;
 * scalar Settings fields are nevertheless reloaded at every reached source
 * read. This component entry does not model App slot replacement. Use the
 * application entry below for normal production forwarding.
 *
 * Actual dc is selected for both h and i. Constructor-normal E=1,F=G=0,
 * z=A=B=C=0 are source-pinned; this is not a completed px/hand/map owner,
 * render tick, fog, scene scheduler or App frontend. Positive portal stops at
 * real px.l a292; third-person stops at real px.u g292. No invented clock.
 *
 * Keep Player, Worlds, Settings, Registry/profile and initialized shared trig
 * live/exclusive. Each actual reached World receiver is held across callbacks;
 * same Entity/Registry/profile namespaces are native admission requirements.
 * Lower World descendants retain their strict-FP/lifetime contracts. Camera
 * rechecks FP before reached arithmetic after effectful calls; lower immersion
 * additionally guards rp.d85 after metadata. No MXCSR reset or numeric clamp.
 * Native FP failure is separate from original Java/GL behavior.
 *
 * Frame receives every completed operation/matrix prefix on reached failure;
 * complete stays false. Native admission preserves frame. Inputs/outputs/errors
 * must be disjoint. Matrices use declared native float composition; exact
 * driver bits/visual equality are not implied. The actual n cloud query is the
 * verified literal false leaf, consumed on a live renderer. Render origin is
 * raw final g790..848 interpolation; fog eye subtracts its stored height offset.
 */
mc_client_camera_status
mc_client_first_person_camera(mc_entity *local_player, const mc_game_settings *settings,
                              mc_world_runtime *application_world,
                              mc_registry_classes *code_registry, const mc_math_trig *trig,
                              int32_t width, int32_t height, float partial_tick, int32_t eye_pass,
                              mc_client_camera_frame *frame, mc_client_camera_error *error);

/* Normal forwarding over the REAL complete application representation. Reload
 * actual z/settings at a8/35/176/208 and g122/282/728; actual f at g135 and
 * g179, independently of Entity.world and any geometry owner. Load real d/e
 * dimensions after acquiring d(F). Replaced/null settings and World references
 * retain source failure order. Application storage remains borrowed and live;
 * selected_local_player is the explicit same h/i identity boundary until their
 * actual outer App publication/dispatch is implemented. No shadow App or
 * generic field callback substitutes for these concrete live getters. */
mc_client_camera_status mc_client_first_person_camera_for_application(
    const client_application *application, mc_entity *selected_local_player,
    mc_registry_classes *code_registry, const mc_math_trig *trig, float partial_tick,
    int32_t eye_pass, mc_client_camera_frame *frame, mc_client_camera_error *error);

#endif
