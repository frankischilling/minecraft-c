#ifndef MC_ENTITY_ARROW_TICK_INTERNAL_H
#define MC_ENTITY_ARROW_TICK_INTERNAL_H

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../item/render/internal.h"
#include "../../world/runtime/collision/blocks_execution_internal.h"
#include "../../world/runtime/collision/blocks_internal.h"
#include "../../world/runtime/construction_internal.h"
#include "../../world/runtime/execution_children_internal.h"
#include "../../world/runtime/mutation/mutation_internal.h"
#include "../../world/runtime/runtime_internal.h"
#include "../../world/runtime/world_access_internal.h"
#include "../player/tick_math_internal.h"
#include "../position_internal.h"
#include "../tick/arrow_base_execution_internal.h"
#include "../tick/execution_internal.h"
#include "internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/entity/arrow/tick.h"
#include "mc/world/runtime/presentation.h"
#include "mc/world/storage/source_chunk_loader.h"
#include "references_internal.h"
#include "tick_execution_dependencies.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Private spelling bridge for the EXACT borrowed historical Source body.
 * The actual declared/tested implementation is mc_java_i32_add; this adds no
 * signed-add implementation, duplicated evaluation, stub or Source work. */
#define mc_java_iadd mc_java_i32_add

typedef union mc_arrow_tick_work {
    mc_entity_tick_error base;
    mc_world_collision_error block;
    mc_world_nearby_error nearby;
    mc_entity_effect_error effect;
    mc_entity_environment_error environment;
    mc_world_movement_error sound;
    mc_world_mutation_error particle;
} mc_arrow_tick_work;

struct mc_arrow_tick_call {
    mc_arrow_tick_state state;
    mc_arrow_tick_error error, cleanup;
    mc_arrow_tick_limits limits;
    mc_world_runtime_classes *code_classes;
    mc_registry_classes *registry;
    bool code_held, receiver_held, failed;
    bool effect_cleanup_owned;
    mc_arrow_tick_child work_tag;
    mc_arrow_tick_work work;
    mc_math_execution math;
    mc_math_class_error ray_math_error; /* Actual Ray failure VALUE; no child borrow. */
    mc_entity_tick_execution execution;
    /* Frame-only VALUE state. Never part of Entity or the reused work union. */
    mc_world_runtime_execution_result query_result;
    mc_world_runtime_execution_code query_code;
    mc_tick_arrow_base_execution base_execution;
    mc_entity_tick_execution_world_token direct_world;
    mc_entity_tick_execution_site direct_world_site; /* Actual evaluated load VALUE. */
    mc_entity_tick_execution_world_token captured_worlds[MC_ENTITY_TICK_EXECUTION_WORLDS];
    mc_world_storage_status query_status;
    mc_arrow_tick_native_unwind_phase query_unwind;
    bool direct_world_open, query_live, query_invoked;
    /* The fixed parent ceiling includes these exact separately allocated bytes.
     * Only native cleanup snapshots this protected first diagnostic. */
    char *exception_storage;
    size_t exception_storage_bytes;
    mc_entity_error entity_error, retire_entity_error;
    mc_registry_error registry_error;
    mc_entity_vector_error vector_error;
    mc_entity_bounds_error bounds_error;
    mc_entity_geometry_error geometry_error;
    mc_entity_hit_error hit_error, hit_cleanup, retire_hit_error;
    mc_entity_source_position_error position_error;
    mc_world_raytrace_call *ray;
    mc_entity_bounds_intercept_call *intercept;
    mc_world_entity_list_view nearby;
    mc_entity_vector *start, *end, *point, *replaced_vector;
    mc_entity_bounds *block_box, *swept, *query_bounds, *candidate_bounds, *replaced_bounds;
    mc_entity_classes *block_box_classes;
    mc_entity_hit_result *hit, *pending_hit, *old_hit, *intercept_hit, *replaced_hit;
    mc_entity *candidate, *selected, *hurt_source;
    mc_entity *hurt_target;
};

void mc_arrow_tick_at(mc_arrow_tick_call *, unsigned int);
void mc_arrow_tick_select(mc_arrow_tick_call *, mc_arrow_tick_child, unsigned int);
bool mc_arrow_tick_fail(mc_arrow_tick_call *, mc_arrow_tick_status, const char *, bool,
                        const char *);
bool mc_arrow_tick_fail_classified(mc_arrow_tick_call *, mc_arrow_tick_status, const char *,
                                   mc_arrow_tick_failure_kind, const char *);
bool mc_arrow_tick_null(mc_arrow_tick_call *, const char *);
bool mc_arrow_tick_fp(mc_arrow_tick_call *, unsigned int);
bool mc_arrow_tick_math_root(mc_arrow_tick_call *, double, unsigned int, float *);
bool mc_arrow_tick_random(mc_arrow_tick_call *, unsigned int, float *);
bool mc_arrow_tick_world_begin(mc_arrow_tick_call *, mc_world_runtime *, unsigned int);
bool mc_arrow_tick_literal(mc_arrow_tick_call *, const char *, unsigned int,
                           const mc_registry_string **);
bool mc_arrow_tick_block_value(mc_arrow_tick_call *, mc_world_runtime *, int32_t, int32_t, int32_t,
                               bool, unsigned int, int32_t *);
bool mc_arrow_tick_block_get(mc_arrow_tick_call *, int32_t, unsigned int, const mc_block **);
bool mc_arrow_tick_block_shape(mc_arrow_tick_call *, mc_world_runtime *, const mc_block *, int32_t,
                               int32_t, int32_t, bool, unsigned int);
bool mc_arrow_tick_vector_new(mc_arrow_tick_call *, double, double, double, unsigned int,
                              mc_entity_vector **);
bool mc_arrow_tick_vector_drop(mc_arrow_tick_call *, mc_entity_vector **);
bool mc_arrow_tick_bounds_drop(mc_arrow_tick_call *, mc_entity_bounds **);
bool mc_arrow_tick_entity_drop(mc_entity **);
bool mc_arrow_tick_hit_drop(mc_arrow_tick_call *, mc_entity_hit_result **);
bool mc_arrow_tick_ray(mc_arrow_tick_call *, mc_world_runtime *);
bool mc_arrow_tick_nearby(mc_arrow_tick_call *, mc_world_runtime *);
bool mc_arrow_tick_intercept(mc_arrow_tick_call *);
bool mc_arrow_tick_base_source_failure(mc_arrow_tick_call *, const char **, const char **);
bool mc_arrow_tick_hurt(mc_arrow_tick_call *, mc_entity *, mc_entity *, bool *);
bool mc_arrow_tick_sound(mc_arrow_tick_call *, mc_world_runtime *, const mc_registry_string *,
                         float, unsigned int);
bool mc_arrow_tick_particle(mc_arrow_tick_call *, mc_world_runtime *, const mc_registry_string *,
                            double, double, double, double, double, double);
bool mc_arrow_tick_dead(mc_arrow_tick_call *, unsigned int);
bool mc_arrow_tick_body(mc_arrow_tick_call *);

/* Called ONLY after this parent's genuine non-OK pop. Classify only
 * its own created token's consumed native VALUE poststate, not an ambient
 * owner/error. No World/producer dereference, replay or new permission. */
static inline void mc_arrow_tick_own_pop_returned(mc_arrow_tick_call *c,
                                                  mc_world_storage_status status) {
    const mc_entity_tick_execution_world_token *token = &c->direct_world;
    if (status != MC_WORLD_STORAGE_OK && c->execution.identity == &c->execution &&
        token->owner == &c->execution && token->slot < MC_ENTITY_TICK_EXECUTION_WORLDS &&
        token->generation != 0 &&
        c->execution.world_slots[token->slot].generation == token->generation &&
        (c->execution.world_slots[token->slot].phase == MC_ENTITY_TICK_EXECUTION_WORLD_RETAINED ||
         c->execution.world_slots[token->slot].phase == MC_ENTITY_TICK_EXECUTION_WORLD_FREE))
        c->direct_world_open = false;
}

/* Native work ceiling: called before the pending source iteration. */
static inline bool mc_arrow_tick_angle_work(mc_arrow_tick_call *c, unsigned int pc) {
    mc_arrow_tick_at(c, pc);
    if (c->state.angle_iterations == c->limits.max_angle_iterations)
        return mc_arrow_tick_fail(c, MC_ARROW_TICK_LIMIT,
                                  "Arrow pending angle-normalization iteration ceiling", true,
                                  NULL);
    ++c->state.angle_iterations;
    return true;
}

#endif
