#ifndef MC_CLIENT_DISPLAY_OCCLUSION_H
#define MC_CLIENT_DISPLAY_OCCLUSION_H

#include "display.h"

typedef struct mc_client_display_query_names mc_client_display_query_names;

typedef struct mc_client_display_query_state {
    size_t count;
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

#endif
