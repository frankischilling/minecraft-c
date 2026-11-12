#ifndef MC_CLIENT_RENDER_BIPED_INTERNAL_H
#define MC_CLIENT_RENDER_BIPED_INTERNAL_H
#include "model_graph_internal.h"

struct mc_client_biped_owner {
    mc_client_render_classes *classes;
    mc_client_biped_phase phase;
    mc_client_biped_model fields;
    model_allocation *allocations;
    size_t bytes, max_bytes, holds;
    bool active;
    mc_client_model_error model_failure;
    mc_client_biped_state progress;
};
#endif
