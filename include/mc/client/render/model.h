#ifndef MC_CLIENT_RENDER_MODEL_H
#define MC_CLIENT_RENDER_MODEL_H
#include "mc/entity/vector/vector.h"

typedef struct mc_client_render_classes mc_client_render_classes;
typedef struct mc_client_item_renderer mc_client_item_renderer;

typedef enum mc_client_model_status {
    MC_CLIENT_MODEL_OK,
    MC_CLIENT_MODEL_INVALID_ARGUMENT,
    MC_CLIENT_MODEL_INVALID_PHASE,
    MC_CLIENT_MODEL_BUSY,
    MC_CLIENT_MODEL_LIMIT,
    MC_CLIENT_MODEL_ALLOCATION_FAILED,
    MC_CLIENT_MODEL_VECTOR_FAILED,
    MC_CLIENT_MODEL_RANDOM_FAILED,
    MC_CLIENT_MODEL_UNIMPLEMENTED_DEPENDENCY
} mc_client_model_status;

typedef struct mc_client_model_error {
    mc_client_model_status status;
    const char *source_class;
    int32_t source_pc;
    mc_entity_vector_error vector;
    mc_java_random_profile_status random;
} mc_client_model_error;

typedef struct mc_client_model_vertex {
    mc_entity_vector *a;
    float b, c;
} mc_client_model_vertex;

typedef struct mc_client_model_quad {
    mc_client_model_vertex **a;
    int32_t b;
    bool c;
} mc_client_model_quad;

typedef struct mc_client_model_part {
    mc_client_model_vertex **j;
    mc_client_model_quad **k;
    int32_t l, m;
    float a, b, c, d, e, f;
    bool n;
    int32_t o;
    bool g, h, i;
} mc_client_model_part;

typedef struct mc_client_biped_model {
    float m;
    bool n;
    mc_client_model_part *a, *b, *c, *d, *e, *f, *g, *h, *i;
    bool j, k, l;
} mc_client_biped_model;

/* Actual cv fields. Arrays follow source n..N, P..aa, ab..am declaration order. */
typedef struct mc_client_render_blocks {
    const void *c;
    int32_t d;
    bool e, f, b;
    int32_t g, h, i, j, k, l;
    bool m;
    float n_to_N[27];
    int32_t O;
    float P_to_aa[12];
    bool ab_to_am[12];
} mc_client_render_blocks;

typedef enum mc_client_model_phase {
    MC_CLIENT_MODEL_RAW,
    MC_CLIENT_MODEL_CONSTRUCTING,
    MC_CLIENT_MODEL_READY,
    MC_CLIENT_MODEL_FAILED
} mc_client_model_phase;

typedef struct mc_client_item_renderer_fields {
    const void *render_manager;
    mc_client_biped_model *base_model;    /* bw.a */
    mc_client_render_blocks *base_blocks; /* bw.e */
    float shadow_size, shadow_opacity;
    mc_client_render_blocks *e; /* bb.e, distinct */
    mc_java_random *f;
    bool a;
} mc_client_item_renderer_fields;

/* One shared native class owner retains the exact client Entity namespace and
 * borrows its profile. No initializer or Random constructor runs in create. */
mc_client_model_status mc_client_render_classes_create(mc_entity_classes *,
                                                       mc_java_random_profile *,
                                                       mc_client_render_classes **);
mc_client_model_status mc_client_render_classes_free(mc_client_render_classes *);
mc_client_model_status mc_client_render_classes_retain(mc_client_render_classes *,
                                                       mc_java_random_profile *);
mc_client_model_status mc_client_render_classes_release(mc_client_render_classes *);
bool mc_client_render_blocks_static_get(const mc_client_render_classes *, bool *ready, bool *a);
/* RAW allocation is separate from source construction. max_bytes==0 is 1MiB,
 * charging owner/allocation metadata; Vec3 has its separate real Entity ledger.
 * Failed prefixes remain inspectable and cannot be retried. Every original
 * position is a directly allocated Vec3; UV copies share it. No pool reset. */
mc_client_model_status mc_client_item_renderer_allocate(mc_client_render_classes *,
                                                        size_t max_bytes,
                                                        mc_client_item_renderer **);
mc_client_model_status mc_client_item_renderer_construct(mc_client_item_renderer *,
                                                         mc_client_model_error *);
mc_client_model_phase mc_client_item_renderer_phase(const mc_client_item_renderer *);
const mc_client_item_renderer_fields *
mc_client_item_renderer_fields_get(const mc_client_item_renderer *);
size_t mc_client_item_renderer_allocation_bytes(const mc_client_item_renderer *);
mc_client_model_status mc_client_item_renderer_free(mc_client_item_renderer **);
/* Same inherited BW setter, used only by the actual TH constructor loop. */
mc_client_model_status mc_client_item_renderer_bind_manager(mc_client_item_renderer *,
                                                            const void *);
/* GL icon/model rendering remains a precise failed dependency. */
mc_client_model_status mc_client_item_renderer_draw(mc_client_item_renderer *,
                                                    mc_client_model_error *);
#endif
