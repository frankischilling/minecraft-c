#ifndef MC_ENTITY_PLAYER_PICKUP_H
#define MC_ENTITY_PLAYER_PICKUP_H

#include "mc/client/particles/effect_renderer.h"

typedef struct mc_player_pickup_error {
    mc_entity_status status;
    const char *operation;
    unsigned int notification_pc;
    mc_entity_error entity;
    mc_particle_error particle;
    mc_effect_renderer_error renderer;
} mc_player_pickup_error;

/* Actual canonical dc.b(sn,int). Amount is genuinely unused. Capture the live
 * App.j before new em; allocate before reloading App.f; construct em with the
 * actual picked Entity/Player/-0.5f, then invoke that captured manager. A null
 * manager is checked only at dn invocation25, after all constructor effects.
 * A null App at4 precedes allocation and all constructor effects. Native
 * capture holds preserve the exact receiver across argument evaluation.
 *
 * The Player and picked actor are live, held ordinary READY same-owner actors.
 * Native admission preserves source state. The real App and all dependencies
 * outlive this exclusive call. The source constructor retains actors only at
 * its reached stores. Temporary candidates retire after failure without
 * rewinding shared IDs, Random constructors/draws, World or callback effects.
 * This CPU notification does not draw particles or complete outer Item pickup,
 * World ticking, Minecraft startup or frame scheduling. */
mc_entity_status mc_local_player_pickup_notification(mc_entity *, mc_entity *, int32_t,
                                                     mc_player_pickup_error *);

#endif
