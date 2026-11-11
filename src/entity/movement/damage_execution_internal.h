#ifndef MC_ENTITY_MOVEMENT_DAMAGE_EXECUTION_INTERNAL_H
#define MC_ENTITY_MOVEMENT_DAMAGE_EXECUTION_INTERNAL_H

#include "../mob/damage_execution_internal.h"
#include "../player/ally_alert_execution_internal.h"
#include "../player/sleep_execution_internal.h"

typedef struct mc_entity_drop_call mc_entity_drop_call;
typedef struct mc_player_drop_call mc_player_drop_call;

typedef enum mc_entity_damage_child {
    MC_ENTITY_DAMAGE_CHILD_NONE,
    MC_ENTITY_DAMAGE_CHILD_KNOCKBACK,
    MC_ENTITY_DAMAGE_CHILD_SOUND,
    MC_ENTITY_DAMAGE_CHILD_ALERT,
    MC_ENTITY_DAMAGE_CHILD_WOLF_QUERY,
    MC_ENTITY_DAMAGE_CHILD_MOB_DEATH,
    MC_ENTITY_DAMAGE_CHILD_PLAYER_DEATH,
    MC_ENTITY_DAMAGE_CHILD_WAKE
} mc_entity_damage_child;

typedef enum mc_entity_damage_work_kind {
    MC_ENTITY_DAMAGE_WORK_NONE,
    MC_ENTITY_DAMAGE_WORK_WOLF,
    MC_ENTITY_DAMAGE_WORK_ALERT,
    MC_ENTITY_DAMAGE_WORK_WAKE,
    MC_ENTITY_DAMAGE_WORK_MOB_DEATH,
    MC_ENTITY_DAMAGE_WORK_PLAYER_DEATH
} mc_entity_damage_work_kind;

/* One actual complete entered hurt dispatcher, with the SAME virtual bodies
 * used by the legacy effect entry. This frame and original error/status/out
 * are fresh, stable, disjoint and exclusive through checked native cleanup.
 * Keep it OUTSIDE any reused caller diagnostic union: retained actor children
 * borrow it and its canonical results even when raw Entity status is OK.
 *
 * Caller holds the evaluated canonical victim and nullable source plus actual
 * defining client runtime/Registry/Entity/profile/IN graph, application and all
 * external callback/provider/Store/import/generation dependencies. No code or
 * Math namespace is inferred from mutable victim.World/Chunk.d. execution->math
 * is the ONE real shared Math context; its declared IN may remain NULL until a
 * reached active use. No future source/World/Random/health argument is admitted
 * early. The top does not create an ambient victim-World scope.
 *
 * bool is Source AND native continuation; raw Entity status and gameplay bool
 * are independent. A false child (including raw OK / native or Math refusal)
 * stops before later stores, wrapper result or callbacks. No Source retry,
 * victim-kind/health whitelist or owned-to-legacy fallback is supplied. The
 * reached GS35 wake retains its own mutation and exact World occurrences.
 *
 * owned is a native-first SUMMARY ONLY: it contains no close authority.
 * inspect exposes the actual child result. Forwarded death result VALUE is
 * not the retained drop's canonical pending/close storage. Parent services
 * only that canonical result, then native_close, then cleanup; expires typed
 * borrows before its final producer/Math/execution retirement. This frame does
 * not retire those shared owners. Checked refusal consumes nothing. */
typedef struct mc_entity_damage_execution {
    mc_entity_tick_execution *execution;
    mc_world_runtime_classes *code_classes;
    mc_world_runtime_execution_code code;
    mc_registry_classes *registry;
    mc_world_runtime_execution_result owned;
    mc_world_runtime_execution_result sound_result, knockback_result, alert_result, death_result;
    mc_living_damage_execution sound, knockback;
    mc_entity *victim;
    mc_entity_effect_error *error;
    const void *acquired_work;
    const void *death_work; /* Only a death created by this actual reached LS316 call. */
    mc_entity_damage_work_kind work_kind;
    mc_entity_damage_child child;
    mc_entity_status child_status;
    mc_world_storage_status native_close_status;
    mc_math_execution_failure math_failure; /* Actual selected failed child return VALUE. */
    const mc_math_execution *math_owner;
    mc_math_class_status math_status;
    bool entered, active, body_invoked, completed, cleaned, child_returned, unresolved,
        prior_pending;
    bool math_failed;
} mc_entity_damage_execution;

typedef struct mc_entity_damage_execution_view {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_result *result;
    mc_entity_damage_child child;
    const void *acquired_work;
    mc_living_damage_execution *sound;
    mc_player_ally_alert_call *alert;
    mc_player_sleep_call *wake;
    mc_entity_drop_call *mob_drop;
    mc_player_drop_call *player_drop;
    const mc_world_nearby_error *nearby;
    mc_entity_tick_execution_world_token world;
    mc_world_storage_status query_status;
    bool world_created, world_open, query_returned, completed, unresolved;
} mc_entity_damage_execution_view;

typedef enum mc_entity_damage_failure_kind {
    MC_ENTITY_DAMAGE_FAILURE_NATIVE,
    MC_ENTITY_DAMAGE_FAILURE_SOURCE_EXCEPTION,
    MC_ENTITY_DAMAGE_FAILURE_SOURCE_ERROR,
    MC_ENTITY_DAMAGE_FAILURE_UNRESOLVED
} mc_entity_damage_failure_kind;

typedef struct mc_entity_damage_failure {
    mc_entity_damage_failure_kind kind;
    const char *operation, *exception_class;
    mc_entity_tick_execution_site site;
    const mc_math_execution_failure *math;
    const mc_math_execution *math_owner;
    mc_math_class_status math_status; /* Actual require return, separate from cause.status. */
    const mc_registry_error *registry;
    bool source_proven, unresolved;
} mc_entity_damage_failure;

void mc_entity_damage_execution_init(mc_entity_damage_execution *, mc_entity_tick_execution *,
                                     mc_world_runtime_classes *, mc_world_runtime_execution_code,
                                     mc_registry_classes *);
bool mc_entity_effect_damage_in_execution(mc_entity_damage_execution *, mc_entity *,
                                          mc_entity *source, int32_t amount, bool *,
                                          mc_entity_effect_error *, mc_entity_status *);
bool mc_entity_damage_execution_inspect(mc_entity_damage_execution *,
                                        mc_entity_damage_execution_view *);
/* Read only the actually selected failed child. Source callbacks require its
 * exact recorded scope/lease tuple AND actual callback cause; no returned raw
 * receipt is dereferenced before that physical protection proof. Direct typed
 * Source faults and Registry classification retain their existing authority.
 * Unknown callback classes and unqualified class-query causes remain explicitly
 * unresolved. No status/suffix/ambient error establishes a Throwable. Math is
 * the VALUE saved at an actual participating failed return, not math.failed.
 * Names/typed pointers borrow the original owners through checked cleanup. */
bool mc_entity_damage_execution_source_failure(mc_entity_damage_execution *,
                                               mc_entity_damage_failure *);
mc_world_storage_status mc_entity_damage_execution_native_close(mc_entity_damage_execution *);
mc_entity_status mc_entity_damage_execution_cleanup(mc_entity_damage_execution *,
                                                    mc_entity_effect_error *);

/* Reached shared-body bookkeeping only. No reset/forwarded owner, admission,
 * source operation or guessed close is performed. */
void mc_entity_damage_execution_select(mc_entity_damage_execution *, mc_entity_damage_child);
void mc_entity_damage_execution_note(mc_entity_damage_execution *,
                                     const mc_world_runtime_execution_result *);
void mc_entity_damage_execution_returned(mc_entity_damage_execution *);

#endif
