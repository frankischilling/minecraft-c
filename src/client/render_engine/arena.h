#ifndef MC_CLIENT_RENDER_ENGINE_ARENA_H
#define MC_CLIENT_RENDER_ENGINE_ARENA_H
#include "mc/client/render_engine/render_engine.h"
#include "source.h"

/* Actual pre-existing allocation header, extracted without changing its
 * alignment, payload offset or charge. There is one implementation. */
typedef struct render_allocation {
    struct render_allocation *next;

    union {
        long double align;
        void *pointer;
        uint64_t integer;
        size_t bytes;
    } payload;
} render_allocation;

typedef struct mc_client_render_arena {
    render_allocation *head;
    mc_client_render_engine_options options;
    size_t bytes, allocations;
} mc_client_render_arena;

/* Pure native allocation policy. External source objects use check/commit
 * around their actual distinct owner allocation; their existing native charge
 * does not gain an arena header. No callback allocator or source active use. */
bool mc_client_render_arena_check(const mc_client_render_arena *, size_t bytes,
                                  mc_client_graphics_site, mc_client_graphics_error *);
void mc_client_render_arena_commit_external(mc_client_render_arena *, size_t bytes);
void *mc_client_render_arena_allocate(mc_client_render_arena *, size_t bytes,
                                      mc_client_graphics_site, mc_client_graphics_error *);
void mc_client_render_arena_free(mc_client_render_arena *);
#endif
