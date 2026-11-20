#ifndef MC_CLIENT_RENDER_BIPED_H
#define MC_CLIENT_RENDER_BIPED_H
#include "mc/client/render/model.h"

typedef struct mc_client_biped_owner mc_client_biped_owner;

typedef enum mc_client_biped_phase {
    MC_CLIENT_BIPED_RAW,
    MC_CLIENT_BIPED_CONSTRUCTING,
    MC_CLIENT_BIPED_READY,
    MC_CLIENT_BIPED_FAILED,
    MC_CLIENT_BIPED_RETIRING
} mc_client_biped_phase;

typedef struct mc_client_model_site {
    const char *source_class, *method, *descriptor;
    int32_t pc;
} mc_client_model_site;

typedef struct mc_client_biped_error {
    mc_client_model_status status;
    const char *operation;
    mc_client_model_error model, retirement;
    mc_client_model_site frames[5]; /* deepest first; no fabricated App frame */
    size_t frame_count;
    char part;
    int32_t face, vertex, uv_copy;
    bool native_policy;
} mc_client_biped_error;

typedef struct mc_client_biped_state {
    mc_client_biped_phase phase;
    const mc_client_biped_model *fields;
    size_t holds, allocation_bytes, allocations, next_allocation_bytes;
    mc_client_model_site last_root_store;
    unsigned parts_completed, vertices_completed, faces_completed, uv_copies_completed;
    mc_client_biped_error failure;
} mc_client_biped_state;

/* A distinct actual fh instance on the existing client render/Entity classes.
 * RAW allocation initializes only native ownership/default storage: no Vec3,
 * class initializer, Random, cv, resources, ge or Display operation. max_bytes0
 * selects1MiB, including this owner and each concrete graph allocation header;
 * Entity Vec3 storage retains its existing separate native budget. */
mc_client_model_status mc_client_biped_allocate(mc_client_render_classes *, size_t max_bytes,
                                                mc_client_biped_owner **out,
                                                mc_client_biped_error *);
/* Actual normal fh(F literal+0) -> fh(FF +0,+0) body. One attempt; no arbitrary
 * float admission or source App.x publication. Failed source children and
 * defaults/publications remain inspectable until retirement. Existing delayed
 * native FP admission is retained; it is not an early FP-validation claim. */
mc_client_model_status mc_client_biped_construct_zero(mc_client_biped_owner *,
                                                      mc_client_biped_error *);
mc_client_model_status mc_client_biped_retain(mc_client_biped_owner *);
/* Every OK consumes/nulls exactly this occurrence, even if other holds remain.
 * BUSY preserves it. Final cleanup is allocation-free/retryable. Once native
 * retirement starts borrowed graph access ends; inspect returns fields=NULL.
 * Inputs/classes/Entity dependencies are exclusive; callbacks must not mutate
 * or retire them. No pool reset/clear or source draw/delete is performed. */
mc_client_model_status mc_client_biped_release(mc_client_biped_owner **, mc_client_biped_error *);
bool mc_client_biped_inspect(const mc_client_biped_owner *, mc_client_biped_state *);
#endif
