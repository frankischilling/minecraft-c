#ifndef MC_CLIENT_RENDER_ENGINE_NW_INTERNAL_H
#define MC_CLIENT_RENDER_ENGINE_NW_INTERNAL_H
#include "nw.h"

typedef struct mc_client_nw_namespace {
    mc_client_render_engine_phase phase;
    bool b, c;
    mc_client_tessellator *a; /* Actual mutable Source static field. */
    /* Immutable native successful allocation occurrence, independent of a.
     * The class namespace owns its lifetime, without a counted owner cycle. */
    mc_client_tessellator *published;
    mc_client_tessellator *candidate; /* Only a failed/in-progress constructor. */
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
