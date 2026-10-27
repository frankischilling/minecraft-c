#ifndef MC_CLIENT_RENDER_MODEL_GRAPH_INTERNAL_H
#define MC_CLIENT_RENDER_MODEL_GRAPH_INTERNAL_H
#include "mc/client/render/biped.h"

struct mc_client_render_classes {
    mc_entity_classes *entities;
    mc_java_random_profile *profile;
    size_t owners;
    bool cv_ready, cv_a;
};

typedef struct model_allocation {
    struct model_allocation *next;
    mc_entity_vector *vector;
    long double alignment;
} model_allocation;

/* Borrowed synchronous view of the REAL owner's native ledger fields. No
 * copied source object/layout, callback allocator or second allocation arena. */
typedef struct mc_client_model_graph {
    mc_client_render_classes *classes;
    model_allocation **allocations;
    size_t *bytes;
    size_t max_bytes;
    mc_client_model_error *failure;
    mc_client_biped_error *detail;
    mc_client_biped_state *progress;
} mc_client_model_graph;

void *mc_client_model_graph_allocate(mc_client_model_graph *, size_t, const char *, int32_t);
bool mc_client_model_graph_fill_zero(mc_client_model_graph *, mc_client_biped_model *);
mc_client_model_status mc_client_model_graph_retire(mc_client_model_graph *,
                                                    mc_client_model_error *);
#endif
