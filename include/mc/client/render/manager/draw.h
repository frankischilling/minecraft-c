#ifndef MC_CLIENT_RENDER_MANAGER_DRAW_H
#define MC_CLIENT_RENDER_MANAGER_DRAW_H
#include "client/render_engine/source.h"
#include "mc/client/render/manager/manager.h"

typedef struct mc_client_render_manager_draw_call mc_client_render_manager_draw_call;
typedef struct mc_client_application_classes mc_client_application_classes;
typedef struct mc_client_living_draw_error mc_client_living_draw_error;
typedef struct mc_client_living_draw_state mc_client_living_draw_state;
typedef struct mc_entity_render_error mc_entity_render_error;

typedef struct mc_client_manager_draw_services {
    mc_math_classes *math;
    mc_client_render_engine_classes *graphics;
    mc_client_display *display;
    mc_client_application_classes *application_classes;
} mc_client_manager_draw_services;

typedef enum mc_client_manager_draw_query_kind {
    MC_CLIENT_MANAGER_DRAW_QUERY_INTEGER,
    MC_CLIENT_MANAGER_DRAW_QUERY_BOOLEAN,
    MC_CLIENT_MANAGER_DRAW_QUERY_FLOAT_BITS
} mc_client_manager_draw_query_kind;

/* One actual direct call attempt. No nested implementation calls are invented.
 * Receiver is scalar identity only; owner captures independently hold reached
 * Worlds/Registry objects. False invoked means the target body was not entered. */
typedef struct mc_client_manager_draw_query {
    uintptr_t receiver;
    mc_client_graphics_site method, caller;
    int32_t args[6];
    size_t nargs;
    mc_client_manager_draw_query_kind kind;

    union {
        int32_t integer;
        bool boolean;
        uint32_t float_bits;
    } value;

    bool invoked, returned, failed, native_policy, source_exception;
    const char *exception_class;
} mc_client_manager_draw_query;

typedef struct mc_client_manager_draw_limits {
    size_t max_bytes, max_owners, max_work, max_shadow_cells, max_shadow_quads, max_queries;
} mc_client_manager_draw_limits;

typedef struct mc_client_manager_draw_error {
    mc_client_manager_status status;
    const char *operation, *exception_class;
    mc_client_graphics_site site, parent, child;
    bool native_policy, source_exception, unclassified_throwable;
    mc_client_manager_error owner;
    mc_client_graphics_error graphics;
    /* Borrow the retained call, including its independently held producers. */
    const mc_entity_render_error *entity;
    const mc_client_living_draw_error *living;
} mc_client_manager_draw_error;

typedef struct mc_client_manager_draw_state {
    bool active, returned, retiring;
    bool post_only;
    bool brightness_returned, color_returned, lookup_returned;
    bool primary_returned, post_draw_returned;
    unsigned int source_pc;
    float partial, yaw, brightness;
    double x, y, z;
    size_t map_queries, memoized_keys, owners, allocation_bytes;
    mc_client_render_manager *receiver;
    mc_entity *actor;
    const mc_entity_class_literal *actor_class;
    const mc_client_managed_renderer *renderer;
    const void *renderer_identity;
    const mc_client_living_draw_state *living, *post_draw;
    const mc_client_manager_draw_error *failure, *cleanup;
} mc_client_manager_draw_state;

mc_client_manager_draw_limits mc_client_render_manager_draw_default_limits(void);
/* Enter actual TH.a(SN,F), including its coordinate overload, on the canonical
 * singleton and existing Map/renderer/model graph. Native admission allocates
 * a retained call; later source/native failure returns that same call. Source
 * effects survive failure and checked retirement never resumes the draw.
 *
 * Namespace services are native dependencies, admitted when reached. The
 * caller keeps the complete service graph live and exclusive on the video
 * thread until checked retirement. Reached owners have independent native
 * occurrences. Explicit zero limits remain zero; NULL selects defaults.
 * Output starts NULL and all owners, outputs and errors are disjoint. */
mc_client_manager_status mc_client_render_manager_draw_begin(
    mc_client_render_manager *, mc_entity *nullable_actor, float partial,
    const mc_client_manager_draw_services *, const mc_client_manager_draw_limits *,
    mc_client_render_manager_draw_call **, mc_client_manager_draw_error *);
/* Enter only the existing renderer's actual BW.b(SN,DDDFF) body. Coordinates
 * and angles are source call operands; TH interpolation/lookup/primary draw
 * are not entered. The same retained scope and checked retirement apply. */
mc_client_manager_status mc_client_render_manager_post_draw_begin(
    mc_client_render_manager *, mc_client_managed_renderer *, mc_entity *nullable_actor, double x,
    double y, double z, float yaw, float partial, const mc_client_manager_draw_services *,
    const mc_client_manager_draw_limits *, mc_client_render_manager_draw_call **,
    mc_client_manager_draw_error *);
bool mc_client_render_manager_draw_inspect(const mc_client_render_manager_draw_call *,
                                           mc_client_manager_draw_state *);
/* Passive ordered observations; indexed read writes out only on success.
 * Completed records remain immutable until checked call retirement succeeds. */
size_t mc_client_render_manager_draw_query_count(const mc_client_render_manager_draw_call *);
bool mc_client_render_manager_draw_query_at(const mc_client_render_manager_draw_call *, size_t,
                                            mc_client_manager_draw_query *out);
mc_client_manager_status mc_client_render_manager_draw_retire(mc_client_render_manager_draw_call **,
                                                              mc_client_manager_draw_error *);
#endif
