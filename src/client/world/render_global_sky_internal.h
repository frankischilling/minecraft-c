#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_SKY_INTERNAL_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_SKY_INTERNAL_H

#include "client/render_engine/texture_lookup.h"
#include "mc/client/world/render_global_sky.h"
#include "mc/world/dimension/sky.h"
#include "mc/world/runtime/sky.h"

struct mc_client_render_global_sky_error {
    mc_client_render_global_status status;
    const char *operation;
    mc_client_graphics_site site, child;
    bool native_policy;
    const mc_client_render_global_error *owner;
    mc_client_display_error display;
    mc_client_graphics_error graphics;
    mc_world_color_error colors, scalar;
    mc_world_provider_sky_error dimension;
    mc_world_storage_error world;
    mc_client_texture_lookup_error texture;
    mc_math_class_error math;
    mc_entity_status entity;
};

#endif
