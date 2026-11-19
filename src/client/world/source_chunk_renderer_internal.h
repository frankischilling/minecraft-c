#ifndef MC_CLIENT_WORLD_SOURCE_CHUNK_RENDERER_INTERNAL_H
#define MC_CLIENT_WORLD_SOURCE_CHUNK_RENDERER_INTERNAL_H
#include "mc/client/world/source_chunk_renderer.h"
#include "render_global_internal.h"

typedef struct mc_source_dk_tile_hold {
    struct mc_source_dk_tile_hold *next;
    mc_client_render_list *list;
    mc_tile_entity *tile;
    size_t index;
} mc_source_dk_tile_hold;

struct mc_client_source_dk {
    mc_client_render_global *owner;
    mc_client_source_dk *next;
    mc_client_source_dk_fields fields;
    mc_client_render_global_phase phase;
    unsigned int source_pc, last_store_pc;
    bool world_held, constructor_returned, list_open;
    mc_entity_bounds *pending_pooled_bounds;
    mc_entity_bounds *displaced_bounds;
    const mc_client_render_global_error *failure; /* Same owner's retained current failure. */
    mc_source_dk_tile_hold *tile_holds;      /* Actual B List owning occurrences, native ledger. */
    mc_client_render_list *tile_list_origin; /* Actual constructor-created B List. */
};

mc_client_source_dk *mc_source_dk_allocate(mc_client_render_global *,
                                           mc_client_render_global_error *);
bool mc_source_dk_construct(mc_client_source_dk *, mc_world_runtime *, mc_client_render_list *,
                            int32_t, int32_t, int32_t, int32_t, int32_t,
                            mc_client_render_global_error *);
/* The move path is entered only after comparing the current coordinates. */
bool mc_source_dk_move(mc_client_source_dk *, int32_t, int32_t, int32_t,
                       mc_client_render_global_error *);

static inline bool mc_source_dk_position(mc_client_source_dk *section, int32_t x, int32_t y,
                                         int32_t z, mc_client_render_global_error *error) {
    const mc_client_source_dk_fields *fields = &section->fields;
    if (x == fields->c && y == fields->d && z == fields->e) {
        section->source_pc = 24;
        return true;
    }
    return mc_source_dk_move(section, x, y, z, error);
}

bool mc_source_dk_clear(mc_client_source_dk *, mc_client_render_global_error *);
bool mc_source_dk_close(mc_client_source_dk *, mc_client_render_global_error *);
/* Whole entered dk.a(I)I, e()Z and d()V leaves. No class activation or
 * compilation is implied. Caller retains the real N/DK/Display graph. */
bool mc_source_dk_render_list(mc_client_source_dk *, int32_t pass, int32_t *out,
                              mc_client_render_global_error *);
bool mc_source_dk_empty(mc_client_source_dk *, bool *out, mc_client_render_global_error *);
bool mc_source_dk_call_bounds(mc_client_source_dk *, mc_client_render_global_error *);
void mc_source_dk_dirty(mc_client_source_dk *);
mc_client_render_global_status mc_source_dk_native_retire(mc_client_source_dk *,
                                                          mc_client_render_global_error *);
/* End only native Tile rights for this cleared List, or all rights at native
 * DK retirement (list NULL). Source clear/close is never synthesized here. */
void mc_source_dk_tile_occurrences_release(mc_client_source_dk *, mc_client_render_list *list);

bool mc_render_global_pointer_append(mc_client_render_global *, mc_client_render_list *, void *,
                                     mc_client_graphics_site, mc_client_render_global_error *);
void *mc_render_global_allocate(mc_client_render_global *, size_t, mc_client_graphics_site,
                                mc_client_render_global_error *);
#endif
