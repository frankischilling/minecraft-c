#ifndef MC_CLIENT_RENDER_MODEL_GRAPH_INTERNAL_H
#define MC_CLIENT_RENDER_MODEL_GRAPH_INTERNAL_H
#include "mc/client/render/biped.h"

struct mc_client_render_classes {
    mc_entity_classes *entities;
    mc_java_random_profile *profile;
    size_t owners;
    bool cv_ready, cv_a;
    bool px_ready, px_a; /* Actual px clinit cells, not an instance mirror. */
    int32_t px_b;
    bool th_retired; /* Explicit terminal association cannot be rebound in this namespace. */
    struct mc_client_render_manager_classes *th; /* Same actual native class namespace. */
    struct mc_client_frustum_namespace *frustum; /* Actual w.e in this class namespace. */
    bool frustum_retired;
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
mc_client_render_blocks *mc_client_model_graph_blocks(mc_client_model_graph *, const char *,
                                                      int32_t);
bool mc_client_render_blocks_active_use(mc_client_render_classes *, mc_client_model_error *);
void mc_client_render_blocks_construct_fields(mc_client_render_blocks *, const void *nullable_xp,
                                              bool with_xp_argument);
mc_client_model_part *mc_client_model_graph_part_allocate(mc_client_model_graph *, const char *,
                                                          int32_t);
void mc_client_model_graph_part_construct(mc_client_model_part *, int32_t, int32_t);
bool mc_client_model_graph_box(mc_client_model_graph *, mc_client_model_part *, float, float, float,
                               int32_t, int32_t, int32_t, float);
void mc_client_model_graph_pivot(mc_client_model_part *, float, float, float);

/* Borrowed slots of the real BW subobject; no copied source BW. */
typedef struct mc_client_bw_slots {
    mc_client_biped_model **model;
    mc_client_render_blocks **blocks;
    float *shadow_size, *shadow_opacity;
} mc_client_bw_slots;

void mc_client_model_graph_bw_set_manager(const void **actual_slot, const void *manager);
bool mc_client_model_graph_bw_construct(mc_client_model_graph *, const mc_client_bw_slots *);
bool mc_client_model_graph_fill_biped(mc_client_model_graph *, mc_client_biped_model *, float,
                                      float);
bool mc_client_model_graph_fill_zero(mc_client_model_graph *, mc_client_biped_model *);
mc_client_model_status mc_client_model_graph_retire(mc_client_model_graph *,
                                                    mc_client_model_error *);
#endif
