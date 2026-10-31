#ifndef MC_CLIENT_WORLD_RENDER_GLOBAL_ENTITIES_H
#define MC_CLIENT_WORLD_RENDER_GLOBAL_ENTITIES_H

#include "mc/client/render/manager/draw.h"
#include "mc/client/render/manager/manager.h"
#include "mc/client/render/tile_entity_renderer/tile_entity_renderer.h"
#include "mc/client/world/frustum_culling.h"
#include "mc/core/math/classes.h"
#include "mc/entity/vector/vector.h"
#include "mc/world/runtime/render_entities.h"

typedef struct mc_client_render_global_entities_call mc_client_render_global_entities_call;

/* Actual borrowed native namespaces, independently supplied by the enclosing
 * owner. These are not source N/App operands and are not inspected on delay. */
typedef struct mc_client_render_global_entities_services {
    mc_client_render_classes *models;
    mc_world_runtime_classes *world_classes;
    mc_java_random_profile *profile;
    mc_registry_classes *registry;
    mc_client_render_manager_classes *manager_classes;
    mc_math_classes *math;
    mc_client_application_classes *application_classes;
} mc_client_render_global_entities_services;

typedef struct mc_client_render_global_model_receipt {
    uintptr_t identity;
    const char *class_name; /* Static source-class diagnostic name. */
    unsigned int source_pc, part_calls, part_returns, compiled_parts, faces, vertices;
    bool returned;
} mc_client_render_global_model_receipt;

/* Copied observations from a returned TH call, taken while its real graph is
 * held. Identity integers cannot be dereferenced. Later failed draws preserve
 * these successful receipts; retirement is marked only for their own attempt. */
typedef struct mc_client_render_global_draw_receipt {
    bool valid, returned, retired, post_only, primary_returned, post_draw_returned;
    bool primary_event_returned;
    size_t attempt;
    unsigned int n_invoke_pc, th_source_pc, living_source_pc, post_draw_source_pc;
    uintptr_t actor_identity, actor_class_identity, renderer_identity;
    int32_t actor_kind;
    const char *actor_class_name, *renderer_class_name;
    mc_client_render_global_model_receipt primary_model, secondary_model;
    bool pass_attempted[4], pass_result[4], pass_render_returned[4];
    size_t model_parts_compiled, model_faces, model_vertices;
    size_t shadow_quads, shadow_vertices, shadow_block_queries, shadow_light_queries;
    size_t shadow_normal_queries, shadow_brightness_queries;
} mc_client_render_global_draw_receipt;

typedef struct mc_client_render_global_entities_state {
    bool active, returned, retiring, delayed, ll_returned, th_returned;
    unsigned int source_pc, last_store_pc;
    int32_t initial_delay, remaining_delay;
    int32_t index;
    bool counters_cleared, origins_stored;
    size_t origins_written, weather_visited, entities_visited, distance_checks;
    size_t frustum_checks, camera_skips, presence_checks, tiles_visited;
    float partial;
    mc_client_render_global *owner;
    /* Nullable source arguments are borrowed through the enclosing graph.
     * Their first actual use, not passage into this method, admits a read. */
    const mc_entity_vector *eye;
    const mc_client_source_frustum *frustum;
    mc_client_tile_renderer_dispatcher *ll;
    mc_client_render_manager *th;
    const mc_world_reference_list *entities_list;
    mc_entity *current;
    const mc_client_render_global_error *failure, *cleanup;
    /* Scalar receipts survive successful nested draw retirement. The pending
     * call is borrowed only while this retained N call still owns it. */
    size_t draws_attempted, draws_returned, draws_retired;
    size_t weather_draws_returned, entity_draws_returned;
    size_t model_parts_compiled, model_faces, model_vertices, shadow_quads;
    const mc_client_render_manager_draw_call *draw;
    mc_client_render_global_draw_receipt last_draw, first_entity_draw;
} mc_client_render_global_entities_state;

/* Actual n.a(Lbt;Lyn;F)V on the same constructed N. Native admission allocates
 * one retained call and holds N; the first source operation then reads I1.
 * Positive I rereads9, decrements/stores14 and returns17 without reading any
 * argument, service, dispatcher, World/App operand or FP state. No delay or
 * readiness value is supplied by the caller. An invocation consumes I once.
 *
 * On the reached non-delay path, LL18 and TH47 use their actual namespace,
 * constructor and frame bodies in source order. N owns its distinct source
 * consumer records across calls/ordinary World changes. The call retains each
 * reached singleton receiver; terminal N cleanup ends only its own records.
 * Source effects survive later failure; a failed call is never replayed.
 *
 * The reached traversal uses actual live World b/e Lists and each current
 * Entity's distance, frustum, camera/sleeping and Chunk-presence predicates.
 * One native lease per reached World protects the borrowed containers; at
 * most 16 distinct Worlds can be captured by one call. Entity draws at
 * N329/N516 invoke the real retained TH child. A returned child is checked-
 * retired before traversal continues at N332/N519. Nonempty N.a TileEntity
 * rows at N551 remain an explicit missing dependency. This API does not
 * complete the enclosing World frame.
 *
 * Out starts NULL. Once admitted, return the terminal call even on failure.
 * App and the complete argument/service graph stay live and exclusive on the
 * video thread through checked retirement. Errors/outputs are disjoint from
 * owners and each other; borrowed diagnostic views expire on native cleanup. */
mc_client_render_global_status
mc_client_render_global_entities_begin(mc_client_render_global *, mc_entity_vector *nullable_eye,
                                       mc_client_source_frustum *nullable_frustum, float partial,
                                       const mc_client_render_global_entities_services *,
                                       mc_client_render_global_entities_call **,
                                       mc_client_render_global_error *);
bool mc_client_render_global_entities_inspect(const mc_client_render_global_entities_call *,
                                              mc_client_render_global_entities_state *);
mc_client_render_global_status
mc_client_render_global_entities_retire(mc_client_render_global_entities_call **,
                                        mc_client_render_global_error *);

#endif
