#ifndef MC_CLIENT_RENDER_MANAGER_DRAW_INTERNAL_H
#define MC_CLIENT_RENDER_MANAGER_DRAW_INTERNAL_H
#include "../living_draw_internal.h"
#include "client/display/source_surface.h"
#include "internal.h"
#include "mc/client/render/manager/draw.h"
#include "mc/world/storage/source_chunk_loader.h"

typedef enum manager_draw_owner_kind {
    MANAGER_DRAW_WORLD,
    MANAGER_DRAW_ENGINE,
    MANAGER_DRAW_STRING,
    MANAGER_DRAW_SETTINGS,
    MANAGER_DRAW_APPLICATION
} manager_draw_owner_kind;

typedef struct manager_draw_owner {
    manager_draw_owner_kind kind;

    union {
        mc_world_runtime *world;
        mc_client_render_engine *engine;
        const mc_player_string *string;
        mc_game_settings *settings;
        client_application *application;
    } value;

    /* Exact private lookup-attempt identity, never a foreign pending attempt. */
    const void *pending_lookup;
} manager_draw_owner;

struct mc_client_render_manager_draw_call {
    mc_client_manager_draw_state state;
    mc_client_manager_draw_error error, cleanup;
    mc_client_manager_error owner_work;
    mc_client_manager_draw_limits limits;
    mc_client_manager_draw_services services;
    mc_client_draw_scope scope;
    mc_entity_render_error brightness;
    mc_client_living_draw_error living, post;
    mc_client_render_manager *manager_hold;
    mc_client_managed_renderer *renderer_hold;
    mc_entity *actor_hold;
    mc_math_classes *math_hold;
    mc_client_render_engine_classes *graphics_hold;
    mc_client_application_classes *application_classes_hold;
    mc_client_manager_draw_query *queries;
    size_t query_count, query_capacity;
    manager_draw_owner *owners;
    size_t owner_count, owner_capacity, bytes, peak_bytes;
    bool executing, retiring;
    /* Only this call's actual unfinished PS definition can be aborted. */
    bool pending_list;
    int32_t owned_list;
    mc_client_display *list_display;
    const void *list_identity;
    mc_client_texture_lookup_error lookup_cleanup, first_lookup_cleanup;
    mc_client_display_error list_cleanup, first_list_cleanup;
    mc_client_application_owner_error application_error, application_cleanup,
        first_application_cleanup;
    /* Fixed charged diagnostic receipts. Distinct live failures in this
     * synchronous source closure fit these explicit finite extents. */
    mc_world_access_failure receipts[16];
    size_t receipt_count;
    mc_world_source_chunk_error source_chunks[4];
    const mc_world_source_chunk_error *source_chunk_identities[4];
    size_t source_chunk_count;
};

void manager_draw_error_init(mc_client_manager_draw_error *);
/* Pre-source native allocation plus actual receiver occurrence only. Source
 * services and nullable actor are not initialized or preflighted here. */
mc_client_manager_status manager_draw_allocate(mc_client_render_manager *,
                                               const mc_client_manager_draw_services *,
                                               const mc_client_manager_draw_limits *,
                                               mc_client_render_manager_draw_call **,
                                               mc_client_manager_draw_error *);
void *manager_draw_allocate_bytes(mc_client_render_manager_draw_call *, size_t,
                                  mc_client_manager_error *);
void manager_draw_free_bytes(mc_client_render_manager_draw_call *, void *, size_t);
bool manager_draw_capture_actor(mc_client_render_manager_draw_call *, mc_entity *,
                                mc_client_manager_error *);
/* Entered capture accepts this call's active manager guard; it proves actual
 * renderer membership and takes the same native hold without a public reentry. */
bool manager_draw_capture_renderer(mc_client_render_manager_draw_call *,
                                   mc_client_managed_renderer *, mc_client_manager_error *);
bool manager_draw_capture_math(mc_client_render_manager_draw_call *, mc_client_manager_error *);
bool manager_draw_capture_graphics(mc_client_render_manager_draw_call *, mc_client_manager_error *);
/* Copy loader views and retain exact selected WorldAccess receipts before any
 * native child retirement. Native failure preserves every earlier capture. */
bool manager_draw_capture_children(mc_client_render_manager_draw_call *, mc_client_manager_error *);
bool manager_draw_append_entity_queries(mc_client_render_manager_draw_call *,
                                        mc_entity_render_error *, mc_client_manager_error *);

#endif
