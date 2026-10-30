#ifndef MC_CLIENT_ENTITY_RENDERER_INTERNAL_H
#define MC_CLIENT_ENTITY_RENDERER_INTERNAL_H
#include "../../render/model_graph_internal.h"
#include "../../render_engine/font_lease.h"
#include "../../render_engine/ge.h"
#include "mc/client/world/entity_renderer.h"
#include "mc/client/world/picking.h"
#include "mc/entity/hit_result.h"
#include <stdatomic.h>

struct mc_client_source_iy {
    mc_client_entity_renderer_phase phase;
    int32_t last_store;
    mc_client_color_array *a, *array_candidate;
    int32_t b;
    mc_game_settings *c;
    mc_client_bitmap_font *d, *retained_font;
    mc_client_render_engine *retained_engine;
    mc_client_argb_image *image;
    mc_client_font_lease *lease; /* Native image-generation lease, not a sj. */
    size_t zero_stores;
};

struct mc_client_source_ra {
    mc_client_entity_renderer_phase phase;
    int32_t last_store;
    client_application *a;
    mc_item_stack *b;            /* Actual nullable iz; constructor leaves NULL. */
    mc_item_stack *b_occurrence; /* Independent native hold for published b. */
    float c, d;
    mc_client_render_blocks *e;
    mc_client_source_iy *f, *pending_iy;
    int32_t g;
    model_allocation *allocations;
    size_t bytes;
};

struct mc_client_source_px {
    mc_client_entity_renderer_services services;
    mc_client_entity_renderer_options options;
    mc_client_entity_renderer_phase phase;
    size_t holds;
    bool active, models_held, graphics_held;
    mc_client_render_arena arena;
    mc_client_entity_renderer_fields fields;
    _Atomic(int32_t) d, e; /* Actual volatile source d/e, no scalar mirrors. */
    int32_t last_store;
    mc_java_random *pending_random;
    mc_client_held_item *pending_ra;
    mc_client_entity_renderer_error failure, retirement;
    mc_client_entity_renderer_pick_call *pick_call;
    struct mc_client_entity_renderer_tick_call *tick_call;
    struct mc_client_entity_renderer_focus_call *focus_call;
    struct mc_client_world_frame_call *world_frame_call;
    mc_entity *pick_m_occurrence;
    bool pick_m_published;
    struct mc_entity_hit_result *pick_y_occurrence;
    client_application *pick_y_app;
    mc_client_entity_renderer_pick_error pick_retirement;
    mc_entity_hit_error pick_hit_cleanup;
};

mc_client_graphics_site entity_renderer_site(const char *, int32_t);
mc_client_entity_renderer_status entity_renderer_fail(mc_client_entity_renderer_error *,
                                                      mc_client_entity_renderer_status,
                                                      const char *, const char *, int32_t, bool);

/* Synchronous borrowed view of real services/options/arena/error storage.
 * No source PX object or copied source fields are needed by RA/IY. */
typedef struct entity_renderer_context {
    const mc_client_entity_renderer_services *services;
    const mc_client_entity_renderer_options *options;
    mc_client_render_arena *arena;
    mc_client_entity_renderer_error *failure;
} entity_renderer_context;

struct mc_client_held_item {
    mc_client_source_ra *source;
    entity_renderer_context context;
    size_t holds;
    bool active, independent, models_held, graphics_held;
    mc_client_entity_renderer_services services;
    mc_client_entity_renderer_options options;
    mc_client_render_arena arena;
    mc_client_entity_renderer_error failure, retirement;
    struct mc_client_held_item_draw_call *draw_call;
};

void *entity_renderer_context_allocate(entity_renderer_context *, size_t, const char *, int32_t);
void *entity_renderer_allocate_child(mc_client_source_px *, size_t, const char *, int32_t);
bool entity_renderer_ra_construct(entity_renderer_context *, mc_client_source_ra *,
                                  client_application *);
bool entity_renderer_ra_retire(entity_renderer_context *, mc_client_source_ra *,
                               mc_client_entity_renderer_error *);
bool entity_renderer_iy_construct(entity_renderer_context *, mc_client_source_iy *,
                                  mc_game_settings *);
bool entity_renderer_iy_retire(mc_client_source_iy *, mc_client_entity_renderer_error *);
mc_client_held_item *entity_renderer_held_borrowed(entity_renderer_context *, const char *,
                                                   int32_t);
#endif
