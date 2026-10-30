#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_INTERNAL_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_INTERNAL_H

#include "../display/occlusion.h"
#include "../render/cx.h"
#include "../render_engine/ge.h"
#include "../render_engine/nw.h"
#include "mc/client/render/manager/manager.h"
#include "mc/client/sound/sound.h"
#include "mc/client/world/render_global.h"
#include "mc/core/java/java.h"
#include "mc/core/math/classes.h"
#include "mc/entity/bounds/bounds.h"

typedef struct mc_render_global_access mc_render_global_access;
typedef struct mc_render_global_cv mc_render_global_cv;
typedef struct mc_client_image_download_error mc_client_image_download_error;
typedef struct mc_client_image_buffer_error mc_client_image_buffer_error;

struct mc_client_render_global_error {
    mc_client_render_global_status status;
    const char *operation, *source_class, *source_method, *source_descriptor;
    unsigned int source_pc, enclosing_pc;
    bool native_policy, source_returned;
    mc_client_render_engine_status classes;
    mc_client_graphics_error graphics, cleanup_graphics;
    mc_client_cx_error cx;
    mc_client_display_error display, cleanup_display;
    mc_client_graphics_site ancestor, parent, child;
    mc_world_storage_error world;
    mc_client_manager_error manager;
    mc_client_model_error model;
    mc_entity_bounds_error bounds;
    mc_registry_error registry;
    mc_sound_error sound;
    mc_entity_error entity;
    mc_player_error player;
    mc_math_class_error math;
    const mc_client_image_download_error *image_download;
    const mc_client_image_buffer_error *image_buffer;
};

struct mc_client_render_batch {
    mc_client_render_batch_state state;
};

struct mc_client_render_global {
    mc_client_render_global_phase phase;
    size_t holds;
    bool active, scene_guard;
    mc_client_render_engine_classes *classes;
    mc_client_render_arena arena;
    mc_client_render_global_fields fields;
    mc_client_render_batch **pending_T;
    mc_client_render_batch *pending_batch;
    unsigned int constructed_batches, published_batches;
    mc_client_display *display; /* Borrowed through retained class/query context leases. */
    mc_client_display_query_names *queries;
    mc_client_tessellator *tessellator; /* Borrowed actual singleton. */
    mc_java_random *star_random;
    unsigned int last_store_pc, last_constructor_pc, star_pc;
    unsigned int star_attempts, accepted_stars, star_vertices, top_quads, bottom_quads;
    int32_t open_list;
    bool matrix_pushed, source_returned, queries_issued;
    mc_client_render_global_error failure, retirement;
    mc_render_global_access *access; /* Native stable interface for this SAME n. */
    mc_client_render_global_world_call *world_call;
    struct mc_client_render_global_sky_call *sky_call;
    struct mc_client_culling_call *cull_call;
    struct mc_client_render_global_update_call *update_call;
    struct mc_client_render_global_submit_call *submit_call;
    mc_world_runtime *world_occurrence;         /* Independent occurrence backing actual k. */
    mc_world_runtime *manager_world_occurrence; /* Independent World hold for legacy TH cleanup. */
    mc_client_render_manager *world_manager;
    mc_render_global_cv *cv_owners;
    mc_client_source_dk *chunk_owners;
    size_t o_length, n_length, t_length;
    bool world_retiring;
    bool terminal_preserve_manager_world; /* Latched across every terminal retry. */
};

mc_client_render_global_status
mc_render_global_source_fail(mc_client_render_global_error *, mc_client_render_global_status,
                             const char *operation, const char *class_name, const char *method,
                             const char *descriptor, unsigned int pc, bool native_policy);
mc_client_render_global_status mc_render_global_mark_cell(mc_client_render_global *, int32_t,
                                                          int32_t, int32_t,
                                                          mc_client_render_global_error *);
mc_client_render_global_status mc_render_global_mark_range(mc_client_render_global *, int32_t,
                                                           int32_t, int32_t, int32_t, int32_t,
                                                           int32_t,
                                                           mc_client_render_global_error *);

mc_client_render_global_status mc_render_global_fail(mc_client_render_global_error *,
                                                     mc_client_render_global_status, const char *,
                                                     const char *, unsigned int,
                                                     bool native_policy);
bool mc_render_global_graphics(mc_client_render_global *, unsigned int enclosing_pc);
bool mc_render_global_display(mc_client_render_global *, unsigned int source_pc);
bool mc_render_global_fp(mc_client_render_global *, const char *method, unsigned int pc);
bool mc_render_global_stars(mc_client_render_global *);
bool mc_render_global_sky(mc_client_render_global *);
bool mc_render_batch_construct(mc_client_render_global *, mc_client_render_batch *,
                               unsigned int enclosing_pc);

#endif
