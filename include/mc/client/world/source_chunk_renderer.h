#ifndef MC_CLIENT_WORLD_SOURCE_CHUNK_RENDERER_H
#define MC_CLIENT_WORLD_SOURCE_CHUNK_RENDERER_H
#include "mc/client/world/render_global.h"
#include "mc/entity/bounds/bounds.h"

/* Actual DK instance fields; private C/F/E are included under source labels.
 * Lists/boolean array/bounds retain their actual distinct source identities.
 * These views add no occurrence and expire on owner mutation/retirement. */
typedef struct mc_client_source_dk_fields {
    mc_world_runtime *a;
    int32_t C, c, d, e, f, g, h, i, j, k, l, m, n;
    bool o;
    bool *p;
    int32_t q, r, s;
    float t;
    bool u;
    mc_entity_bounds *v;
    int32_t w;
    bool x, y;
    int32_t z;
    bool A, E;
    mc_client_render_list *B, *F;
} mc_client_source_dk_fields;

typedef struct mc_client_source_dk_state {
    mc_client_render_global_phase phase;
    mc_client_source_dk_fields fields;
    const mc_client_render_global *owner;
    unsigned int source_pc, last_store_pc;
    bool constructor_returned, list_open;
    const mc_entity_bounds *pending_pooled_bounds;
    const mc_client_render_global_error *failure;
} mc_client_source_dk_state;

bool mc_client_source_dk_inspect(const mc_client_source_dk *, mc_client_source_dk_state *);
#endif
