#ifndef MC_CLIENT_RUNTIME_WORLD_TICK_H
#define MC_CLIENT_RUNTIME_WORLD_TICK_H

#include "../gui/source_screen_transition_receipts_internal.h"
#include "mc/client/particles/effect_renderer.h"
#include "mc/client/world/render_global.h"
#include "mc/client/world/renderer_tick.h"
#include "mc/world/runtime/client_tick_state.h"
#include "mc/world/runtime/entity_tick.h"
#include "mc/world/runtime/random_display.h"
#include "mc/world/runtime/world_tick.h"

typedef enum mc_client_world_tick_close_child {
    MC_CLIENT_WORLD_TICK_CLOSE_CONTAINER,
    MC_CLIENT_WORLD_TICK_CLOSE_CLEANUP_CONTAINER,
    MC_CLIENT_WORLD_TICK_CLOSE_INHERITED,
    MC_CLIENT_WORLD_TICK_CLOSE_DIRECT,
    MC_CLIENT_WORLD_TICK_CLOSE_CHILDREN
} mc_client_world_tick_close_child;

/* Value projection of the actual publication WorldAccess failure. Its producer
 * remains borrowed separately in close_publications until successful cleanup;
 * these sites/classification contain no borrowed producer objects. */
typedef struct mc_client_world_tick_close_access {
    mc_client_render_global_status status;
    const char *operation, *exception_class;
    mc_client_graphics_site site, ancestor, parent, child;
    bool present, native_policy, source_exception, source_error, source_returned;
} mc_client_world_tick_close_access;

typedef struct mc_client_world_tick {
    bool active, failed, returned, counter_stored, counter_reset, difficulty_stored;
    bool renderer_returned, render_global_returned, entities_returned, spawn_flags_stored;
    const char *operation;
    mc_client_graphics_site site;
    mc_world_refresh_entity_call *refresh;
    mc_world_refresh_entity_state refresh_state;
    mc_world_refresh_entity_error refresh_error;
    mc_client_entity_renderer_tick_call *renderer;
    mc_client_renderer_tick_state renderer_state;
    mc_client_renderer_tick_error renderer_error;
    mc_world_client_tick_state_error fields;
    mc_world_entity_tick_error entities;
    /* Actual synchronous GS143 -> DC.r child, using the App's native router
     * lease. All diagnostic copies belong to this record; they own no Screen,
     * Container or WorldAccess rights and never replay source close work. */
    size_t close_calls, close_returns;
    mc_client_screen_error close_screen;
    mc_screen_transition_receipts close_receipts;
    mc_world_entity_error close_publications[MC_CLIENT_WORLD_TICK_CLOSE_CHILDREN];
    mc_player_statistic_error close_statistics[MC_CLIENT_WORLD_TICK_CLOSE_CHILDREN];
    mc_client_world_tick_close_access close_access[MC_CLIENT_WORLD_TICK_CLOSE_CHILDREN];
    mc_world_tick_call *scheduler;
    mc_world_tick_state scheduler_state;
    mc_world_tick_error scheduler_error;
    mc_math_class_error ambience_math;
    int32_t ambience_coordinates[3];
    unsigned int ambience_arguments;
    mc_world_random_display_call *ambience;
    mc_world_random_display_state ambience_state;
    mc_world_random_display_error ambience_error;
    mc_effect_renderer_error effects;
    /* Complete selected movement value copied from the exact pending PB call
     * before its native record retires. Nested producer views remain borrowed. */
    mc_entity_move_error effects_movement;
    /* Exact DN receiver captured at the reached source invocation. Native
     * cleanup keeps its pending tick/WA rights alive independently of App.j. */
    mc_client_effect_renderer *effects_owner;
    mc_effect_renderer_error effects_cleanup;
} mc_client_world_tick;

/* Source Minecraft.k1074..1328 on the actual App graph. This segment performs
 * no clock query/store; the caller retains the later P1329/1332 sequence.
 * The record starts zeroed and stays live/disjoint/exclusive through cleanup.
 * Completed fields and children survive terminal failures; no replay/resume.
 * Failed Player/Mob/FallingSand children transfer owned work into entities;
 * this record must not be copied, cleared or reused before cleanup succeeds.
 * The ordered World scheduler is invoked at1260 and retains its exact reached
 * descendants, followed by the actual fd.q random display call at1311.
 * The actual App/menu loop keeps its native transition lease and all failed
 * Screen/Container producers live through this record's successful cleanup.
 * All graph owners stay live on the video thread through native cleanup. */
bool mc_client_world_tick_run(client_application *, mc_math_classes *, mc_client_world_tick *);
/* Releases child native occurrences without replaying source work. Top-level
 * value diagnostics and still-owned entity/field errors survive cleanup retry;
 * successful owned-Entity retirement ends that child projection before any
 * later cleanup refusal. The outer failure operation/site remain intact.
 * renderer/refresh borrowed child views end with their successful release.
 * effects copies its selected complete movement value before releasing the PB
 * record; its nested producer-borrowed World/loader/WorldAccess views must be
 * consumed before successful cleanup releases their native protection. They
 * never become owning error fields. Close copies retain their typed values,
 * publication/statistic copies and stable WorldAccess sites/classification;
 * successful cleanup ends producer provenance and nested WorldAccess borrows.
 * Refused cleanup preserves the still-live close views. No transition/Screen
 * occurrence is consumed here; its actual router retires afterward. */
bool mc_client_world_tick_cleanup(mc_client_world_tick *);

#endif
