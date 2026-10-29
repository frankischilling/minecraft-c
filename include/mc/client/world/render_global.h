#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_H

#include "mc/client/render/model.h"
#include "mc/client/render_engine/render_engine.h"
#include "mc/core/java/java.h"

typedef struct client_application client_application;
typedef struct mc_world_runtime mc_world_runtime;
typedef struct mc_client_render_global mc_client_render_global;
typedef struct mc_client_render_batch mc_client_render_batch; /* Actual tg, graph-owned. */
typedef struct mc_client_source_dk mc_client_source_dk;
typedef struct mc_client_render_global_error mc_client_render_global_error;

typedef enum mc_client_render_global_phase {
    MC_CLIENT_RENDER_GLOBAL_RAW,
    MC_CLIENT_RENDER_GLOBAL_CONSTRUCTING,
    MC_CLIENT_RENDER_GLOBAL_READY,
    MC_CLIENT_RENDER_GLOBAL_FAILED,
    MC_CLIENT_RENDER_GLOBAL_RETIRING
} mc_client_render_global_phase;

typedef enum mc_client_render_global_status {
    MC_CLIENT_RENDER_GLOBAL_OK,
    MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
    MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
    MC_CLIENT_RENDER_GLOBAL_BUSY,
    MC_CLIENT_RENDER_GLOBAL_LIMIT,
    MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED,
    MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
    MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
    MC_CLIENT_RENDER_GLOBAL_CX_FAILED,
    MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
} mc_client_render_global_status;

/* These are the actual n fields, not diagnostic world-renderer storage.
 * The graph owns Lists/arrays/tg instances. Engine/App/World remain exact
 * nullable source references. Unreached dk/cv/World fields retain defaults. */
typedef struct mc_client_render_global_fields {
    mc_client_render_list *a;
    mc_world_runtime *k;
    mc_client_render_engine *l;
    mc_client_render_list *m;
    mc_client_source_dk **n, **o;
    int32_t p, q, r, s;
    client_application *t;
    mc_client_render_blocks *u;
    mc_client_direct_int_buffer *v;
    bool w;
    int32_t x, y, z, A, B, C, D, E, F, G, H, I, J, K, L;
    int32_t *b;
    mc_client_direct_int_buffer *c;
    int32_t M, N, O, P, Q, R;
    mc_client_render_list *S;
    mc_client_render_batch **T;
    int32_t d, e;
    double f, g, h;
    float i;
    int32_t j;
} mc_client_render_global_fields;

typedef struct mc_client_render_batch_state {
    mc_client_render_global_phase phase;
    int32_t a, b, c;
    float d, e, f;
    const mc_client_direct_int_buffer *g;
    bool h, i;
    unsigned int last_store_pc;
} mc_client_render_batch_state;

typedef struct mc_client_render_global_state {
    mc_client_render_global_phase phase;
    mc_client_render_global_fields fields;
    const mc_client_render_engine_classes *classes; /* Borrowed actual native namespace. */
    size_t holds, arena_bytes, arena_allocations;
    unsigned int last_store_pc, last_constructor_pc, star_pc;
    mc_client_render_batch *const *pending_T;
    const mc_client_render_batch *pending_batch;
    unsigned int constructed_batches, published_batches;
    bool source_returned, queries_issued, list_open, matrix_pushed;
    bool star_random_created;
    mc_java_random star_random;
    unsigned int star_attempts, accepted_stars, star_vertices, top_quads, bottom_quads;
    const mc_client_render_global_error *failure, *retirement;
} mc_client_render_global_state;

/* Cold native allocation retains the SAME ge/nw classes but does not activate
 * ge/nw/Math or a Display. All source defaults are real zero/null fields.
 * Default native arena budget8MiB; zero quota fields have their literal value.
 * App/Engine arguments are borrowed only when construct reaches t163/l168;
 * callers keep them and the supplied Display stable through every retained
 * source/failure/cleanup prefix. No App or Engine is inferred from classes. */
mc_client_render_global_status
mc_client_render_global_allocate(mc_client_render_engine_classes *,
                                 const mc_client_render_engine_options *,
                                 mc_client_render_global **, mc_client_render_global_error *);
mc_client_render_global_status
mc_client_render_global_construct(mc_client_render_global *, client_application *nullable_app,
                                  mc_client_render_engine *nullable_engine, mc_client_display *,
                                  mc_client_render_global_error *);
mc_client_render_global_status mc_client_render_global_retain(mc_client_render_global *,
                                                              mc_client_render_global_error *);
/* Successful nonfinal release consumes its occurrence. Final BUSY/failure
 * retains the same owner/arena, native query rights and complete cleanup cause.
 * No source method replay, Java list clear, source field reset or GL EndList
 * success is invented by retirement. */
mc_client_render_global_status mc_client_render_global_release(mc_client_render_global **,
                                                               mc_client_render_global_error *);
/* Strict native enclosing-owner retirement. Extra occurrences or an entered
 * operation preserve this slot and the complete source/storage lifetime.
 * Once admitted it delegates the same canonical retryable release body. */
mc_client_render_global_status mc_client_render_global_retire(mc_client_render_global **,
                                                              mc_client_render_global_error *);
bool mc_client_render_global_inspect(const mc_client_render_global *,
                                     mc_client_render_global_state *);
bool mc_client_render_batch_inspect(const mc_client_render_batch *, mc_client_render_batch_state *);

/* Owners/classes and returned views require exclusive serialized native use.
 * Device calls run on the Display thread. Cold CPU calls need no Display.
 * Inspected arena/source references expire on subsequent owner mutation;
 * graph-owned tg views have no independent source lifetime/retain API here. */

#endif
