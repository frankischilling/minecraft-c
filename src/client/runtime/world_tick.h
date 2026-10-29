#ifndef MC_CLIENT_RUNTIME_WORLD_TICK_H
#define MC_CLIENT_RUNTIME_WORLD_TICK_H

#include "mc/client/particles/effect_renderer.h"
#include "mc/client/world/renderer_tick.h"
#include "mc/world/runtime/client_tick_state.h"
#include "mc/world/runtime/entity_tick.h"
#include "mc/world/runtime/world_tick.h"

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
    mc_world_tick_call *scheduler;
    mc_world_tick_state scheduler_state;
    mc_world_tick_error scheduler_error;
    mc_math_class_error ambience_math;
    int32_t ambience_coordinates[3];
    unsigned int ambience_arguments;
    mc_effect_renderer_error effects;
} mc_client_world_tick;

/* Source Minecraft.k1074..1328 on the actual App graph. This segment performs
 * no clock query/store; the caller retains the later P1329/1332 sequence.
 * The record starts zeroed and stays live/disjoint/exclusive through cleanup.
 * Completed fields and children survive terminal failures; no replay/resume.
 * The ordered World scheduler is invoked at1260 and retains its exact reached
 * descendants; ambience fd.q remains a later dependency.
 * All graph owners stay live on the video thread through native cleanup. */
bool mc_client_world_tick_run(client_application *, mc_math_classes *, mc_client_world_tick *);
/* Releases child native occurrences without replaying source work. Top-level
 * value diagnostics and complete entity/field errors survive cleanup retry;
 * renderer/refresh borrowed child views end with their successful release. */
bool mc_client_world_tick_cleanup(mc_client_world_tick *);

#endif
