#ifndef MC_CLIENT_RENDER_ITEM_RENDERER_INTERNAL_H
#define MC_CLIENT_RENDER_ITEM_RENDERER_INTERNAL_H
#include "model_graph_internal.h"

/* CPU owner layout only. Its runtime GUI call is opaque here, so the notice
 * constructor library has no Display, SDL or graphics linkage. */
struct mc_client_item_renderer {
    mc_client_render_classes *classes;
    mc_client_model_phase phase;
    mc_client_item_renderer_fields fields;
    model_allocation *allocations;
    size_t bytes, max_bytes;
    mc_client_model_error failure;
    bool gui_active;
    struct mc_client_item_gui_call *gui_call;
};
#endif
