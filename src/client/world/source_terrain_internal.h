#ifndef MC_CLIENT_WORLD_SOURCE_TERRAIN_INTERNAL_H
#define MC_CLIENT_WORLD_SOURCE_TERRAIN_INTERNAL_H

#include "mc/client/settings/game_settings.h"
#include "mc/client/world/render_global_submit.h"
#include "render_global_internal.h"
#include "source_chunk_renderer_internal.h"

/* Synchronous entered source children borrow the public call's one admitted N
 * guard. On failure the complete frame chain remains in this retained call. */
struct mc_client_render_global_submit_call {
    mc_client_render_global_submit_state state;
    mc_client_render_global_error failure, cleanup;
    mc_client_render_global_world_state rebuild;
    mc_client_graphics_site frames[20];
    size_t depth;
    bool camera_held, batch_camera_held, math_held;
    mc_math_class_error math_error, math_cleanup;
    struct mc_client_display_active_query *query;
};

static inline void mc_terrain_pc(mc_client_render_global_submit_call *c, unsigned int pc) {
    c->state.source_pc = pc;
    if (c->depth != 0)
        c->frames[c->depth - 1].pc = (int32_t)pc;
}

static inline void mc_terrain_store(mc_client_render_global_submit_call *c, unsigned int pc) {
    mc_terrain_pc(c, pc);
    c->state.last_store_pc = pc;
}

bool mc_terrain_push(mc_client_render_global_submit_call *, const char *class_name,
                     const char *method, const char *descriptor);
void mc_terrain_pop(mc_client_render_global_submit_call *);
bool mc_terrain_fail(mc_client_render_global_submit_call *, mc_client_render_global_status,
                     const char *, unsigned int, bool native_policy);
bool mc_terrain_fp(mc_client_render_global_submit_call *, unsigned int);
bool mc_terrain_display(mc_client_render_global_submit_call *, bool, unsigned int);
bool mc_terrain_graphics(mc_client_render_global_submit_call *, bool, unsigned int);
bool mc_terrain_child(mc_client_render_global_submit_call *, bool, unsigned int);

static inline bool mc_terrain_array(mc_client_render_global_submit_call *c, bool sorted,
                                    int32_t index, unsigned int pc, mc_client_source_dk **out) {
    mc_client_render_global *n = c->state.owner;
    mc_client_source_dk **array = sorted ? n->fields.n : n->fields.o;
    const size_t length = sorted ? n->n_length : n->o_length;
    if (array == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N ChunkRenderer array", pc, false);
    if (length > INT32_MAX)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT,
                               "N ChunkRenderer native array extent", pc, true);
    if (index < 0 || (size_t)index >= length)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX,
                               "actual N ChunkRenderer array index", pc, false);
    mc_client_source_dk *dk = array[index];
    if (dk != NULL && dk->owner != n)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                               "N ChunkRenderer native occurrence provenance", pc, true);
    *out = dk;
    return true;
}

static inline bool mc_terrain_dk_receiver(mc_client_render_global_submit_call *c,
                                          const mc_client_source_dk *dk, unsigned int pc) {
    return dk != NULL || mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                         "actual ChunkRenderer receiver", pc, false);
}

bool mc_terrain_settings(mc_client_render_global_submit_call *, unsigned int app_pc,
                         unsigned int settings_pc, unsigned int field_pc, mc_game_settings_state *);
bool mc_terrain_math(mc_client_render_global_submit_call *, unsigned int);
bool mc_terrain_distance(mc_client_render_global_submit_call *, mc_client_source_dk *, unsigned int,
                         float *);
bool mc_terrain_subset(mc_client_render_global_submit_call *, int32_t start, int32_t end,
                       unsigned int caller_pc, int32_t *result);
/* Push exactly the actual n.a(ID)V frame. An enclosing subset supplies its
 * own caller484 before entering; the public replay entry has no extra frame. */
bool mc_terrain_replay(mc_client_render_global_submit_call *);
bool mc_terrain_occlusion(mc_client_render_global_submit_call *, int32_t *result);
bool mc_terrain_poll(mc_client_render_global_submit_call *, int32_t start, int32_t end,
                     unsigned int caller_pc);
void mc_terrain_freeze_failure(mc_client_render_global_submit_call *);

#endif
