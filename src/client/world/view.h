#ifndef MC_CLIENT_WORLD_VIEW_H
#define MC_CLIENT_WORLD_VIEW_H

#include "assets.h"
#include "mesher.h"
#include "renderer.h"

#include "mc/world/runtime/propagation.h"

typedef struct mc_client_world_view mc_client_world_view;

typedef struct mc_client_world_view_error {
    const char *operation;
    mc_client_assets_error assets;
    mc_client_display_error display;
    mc_client_world_mesher_error mesher;
    mc_chunk_acquire_status acquisition;
    mc_world_storage_error world;
    mc_world_light_operation_error light;
} mc_client_world_view_error;

/* Finite native World inspection: actual generated/populated sections, textures
 * and GL buffers. This is not a Player, world tick or RenderGlobal scheduler.
 * Build a fixed three-by-three column view around the actual World spawn.
 * Acquire its padded ChunkCache footprint and drain actual World lighting
 * before building any section, so later population cannot stale earlier meshes.
 * World/profile/Registry/Assets remain exclusive during creation. The finished
 * owner retains only Display and copied GPU geometry; no World view survives. */
bool mc_client_world_view_create(mc_world_runtime *world, mc_client_display *display,
                                 mc_client_assets *assets, const mc_math_trig *trig, bool anaglyph,
                                 bool ambient_occlusion, bool fancy_leaves,
                                 mc_client_world_view **out, mc_client_world_view_error *error);
bool mc_client_world_view_draw(mc_client_world_view *view, mc_client_display_error *error);
bool mc_client_world_view_retire(mc_client_world_view **view, mc_client_display_error *error);

#endif
