#ifndef MC_CLIENT_RENDER_ENGINE_NW_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_NW_INTERNAL_H
#include "nw.h"

typedef struct mc_client_nw_namespace {
    mc_client_render_engine_phase phase;
    bool b, c;
    mc_client_tessellator *a, *candidate;
    mc_client_render_engine_options options;
    mc_client_graphics_error failure;
} mc_client_nw_namespace;

struct mc_client_tessellator {
    mc_client_render_engine_classes *classes; /* borrowed from sole class owner */
    mc_client_render_arena arena;
    mc_client_nw_fields fields;
    int32_t last_store_pc;
    bool active;
};
#endif
