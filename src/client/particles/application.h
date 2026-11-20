#ifndef MC_CLIENT_PARTICLES_APPLICATION_H
#define MC_CLIENT_PARTICLES_APPLICATION_H

#include "mc/client/gui/achievement_notice.h"
#include "mc/client/particles/effect_renderer.h"

/* Actual Minecraft.a startup new dn645, f650/p654 argument loads,
 * constructor657 and j660 publication on the complete native App owner.
 * This selected call belongs after real ji/n construction. It is not inserted
 * into menu startup ahead of those still-unimplemented resource descendants.
 * Failed candidates remain retained until explicit native retirement. */
mc_effect_renderer_status mc_client_application_construct_effect_renderer(
    client_application *, const mc_effect_renderer_options *, mc_effect_renderer_error *);
mc_effect_renderer_status mc_client_application_retire_effect_renderer(client_application *);

/* Counted native adoption of the actual f slot for the enclosing World entry.
 * No Player/session-derived World, source unload/save or dn.clear is implied.
 * Caller supplies the exact same-profile World and later executes the actual
 * surrounding World-change methods in their own source order. */
mc_effect_renderer_status mc_client_application_publish_world(client_application *,
                                                              mc_world_runtime *);

#endif
