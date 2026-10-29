#ifndef MC_CLIENT_DISPLAY_OCCLUSION_H
#define MC_CLIENT_DISPLAY_OCCLUSION_H

#include "display.h"

typedef struct mc_client_display_query_names mc_client_display_query_names;
typedef struct mc_client_display_active_query mc_client_display_active_query;

typedef struct mc_client_display_query_state {
    size_t count;
    size_t query_holds;
    uint64_t generation;
    bool issued, deleted;
} mc_client_display_query_state;

/* Current-context capability corresponding to LWJGL2.9 ContextCapabilities.
 * Requires THIS already-current live Display; never makes another context
 * current. Cache belongs to this owner's exact context generation. Membership
 * is the actual GL_ARB_occlusion_query token plus all eight exact optional ARB
 * entry points. Version promotion and SDL hint/environment overrides are not
 * substituted. Failed introspection is a typed native failure, not false.
 * No glGetError, swap, event pump or query-name allocation occurs. Optional
 * function absence can produce supported=false and never rejects Display
 * creation. Output remains unchanged on failure. Ordinary Display main-thread,
 * exclusive-use/lifetime rules apply; this does not acquire a lasting hold.
 * A future query resource must retain this same Display/context generation;
 * it can reuse these optional function pointers without a second namespace. */
bool mc_client_display_source_arb_occlusion_query(mc_client_display *, bool *supported,
                                                  mc_client_display_error *);

/* Actual glGenQueriesARB on the same cached context. storage is the real
 * writable IntBuffer position..limit span, retained by its concrete arena.
 * No cursor mutation, capability re-query, ID replacement or GetError occurs.
 * A native owner/context hold is reserved and published BEFORE the void call;
 * failure can therefore leave a nonnull pending owner. Keep the input arena
 * alive until query_names_retire consumes that owner, including failed calls.
 * Generation and terminal deletion rights belong to that exact occurrence. */
bool mc_client_display_source_gen_queries(mc_client_display *, void *storage, size_t count,
                                          mc_client_display_query_names **,
                                          mc_client_display_error *);
bool mc_client_display_query_names_retire(mc_client_display_query_names **,
                                          mc_client_display_error *);
bool mc_client_display_query_names_inspect(const mc_client_display_query_names *,
                                           mc_client_display_query_state *);

typedef struct mc_client_display_active_query_state {
    int32_t id;
    uint64_t generation;
    bool begin_issued, source_ended, native_aborted;
} mc_client_display_active_query_state;

/* Actual ARB calls on the SAME already-current Display, cached capability,
 * generation and generated-name owner. The ID must belong to that live owner;
 * this is native provenance admission, not a synthetic Java/GL exception.
 * Begin's fixed source target is GL_SAMPLES_PASSED_ARB (35092). out starts
 * NULL. Once allocated, its name/context right and out are published before
 * the void Begin boundary; an entered failure may retain a pending handle.
 * Source End performs one real End call and leaves its ended handle for
 * explicit retirement. No GetError, core alias, swap, pump or assumed result.
 * Keep the Names buffer/arena and both owners live/exclusive through retire. */
bool mc_client_display_source_begin_query(mc_client_display *, mc_client_display_query_names *,
                                          int32_t id, mc_client_display_active_query **out,
                                          mc_client_display_error *);
bool mc_client_display_source_end_query(mc_client_display_active_query *,
                                        mc_client_display_error *);
/* pname is RESULT_AVAILABLE (34919) or unsigned RESULT (34918). storage is
 * the actual writable Ge IntBuffer position..limit span; remaining counts
 * int32 elements and must be at least1, matching LWJGL2.9 checkBuffer(...,1).
 * Only the first word is passed to the real GetQueryObjectuivARB. No cursor
 * changes or writes happen before that call; source callers retain the Ge
 * owner and use their actual absolute read afterwards. A RESULT call may
 * block as the real ARB function specifies; AVAILABLE is never substituted. */
bool mc_client_display_source_query_result(mc_client_display *, mc_client_display_query_names *,
                                           int32_t id, uint32_t pname, void *storage,
                                           size_t remaining, mc_client_display_error *);
bool mc_client_display_active_query_inspect(const mc_client_display_active_query *,
                                            mc_client_display_active_query_state *);
/* Checked terminal native cleanup, distinct from a completed source End.
 * A possibly issued, unended Begin is ended on its original context exactly
 * once as a native abort. Failure preserves the same handle and Names/context
 * right for retry. No list operation, visibility store or source End success
 * is invented. Names and Display/context retirement stay blocked meanwhile. */
bool mc_client_display_active_query_retire(mc_client_display_active_query **,
                                           mc_client_display_error *);

#endif
