#include "arena.h"
#include <stdlib.h>

bool mc_client_render_arena_check(const mc_client_render_arena *a, size_t bytes,
                                  mc_client_graphics_site site, mc_client_graphics_error *e) {
    if (bytes > a->options.max_constructor_bytes - a->bytes ||
        a->allocations >= a->options.max_constructor_allocations) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_LIMIT, "graphics native constructor limit",
                                site, true);
        return false;
    }
    return true;
}

void mc_client_render_arena_commit_external(mc_client_render_arena *a, size_t bytes) {
    a->bytes += bytes;
    ++a->allocations;
}

void *mc_client_render_arena_allocate(mc_client_render_arena *o, size_t bytes,
                                      mc_client_graphics_site site, mc_client_graphics_error *e) {
    if (bytes > SIZE_MAX - sizeof(render_allocation)) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_LIMIT, "graphics native constructor limit",
                                site, true);
        return NULL;
    }
    if (!mc_client_render_arena_check(o, sizeof(render_allocation) + bytes, site, e))
        return NULL;
    render_allocation *a = calloc(1, sizeof(*a) + bytes);
    if (a == NULL) {
        mc_client_graphics_fail(e, MC_CLIENT_GRAPHICS_ALLOCATION_FAILED,
                                "graphics constructor allocation", site, true);
        return NULL;
    }
    a->next = o->head;
    o->head = a;
    o->bytes += sizeof(*a) + bytes;
    ++o->allocations;
    return a + 1;
}

void mc_client_render_arena_free(mc_client_render_arena *o) {
    render_allocation *a = o->head;
    while (a != NULL) {
        render_allocation *next = a->next;
        free(a);
        a = next;
    }
    o->head = NULL;
}
