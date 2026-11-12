#ifndef MC_ENTITY_MOB_DEATH_EXECUTION_INTERNAL_H
#define MC_ENTITY_MOB_DEATH_EXECUTION_INTERNAL_H

#include "../drop/execution_internal.h"
#include "death_internal.h"

typedef struct mc_mob_death_execution_view {
    mc_entity_tick_execution *execution;
    mc_world_runtime_execution_result *result;
    mc_entity_drop_call *drop;
    mc_entity_drop_execution_view child;
    mc_entity_status drop_status;
    bool drop_returned, has_child, completed;
} mc_mob_death_execution_view;

/* SAME complete LS death/q and concrete Sheep/Skeleton/Squid/Creeper bodies.
 * Required initialized stable effect scratch and fresh/disjoint outer result;
 * Bool preserves native/Source continuation independently from raw status.
 * Missing context/code/outputs refuses before Source, never selects legacy.
 * No Math ensure occurs before a real drop's reached FD/LM IN invocation.
 * Scalar B and literal FD status0 keep their actual independent World holds;
 * no execution scope or unreached Container close is invented around them.
 *
 * Retained actor-resident work owns its actual current Stack/drop and stable
 * result on ANY false, including raw OK. No drop-return count, loop index,
 * Stack release or later status/tail operation follows such a child. The outer
 * result receives only VALUE observations, never an owner or close authority.
 * Caller owns exclusive stable execution/shared Math, defining code/Registry /
 * Entity graph and full external provider/Store/import/generation dependencies.
 * Complete Arrow/victim/Player dispatch remains a separate caller join.
 */
bool mc_mob_death_entered_in_execution(mc_entity_tick_execution *, mc_world_runtime_classes *,
                                       mc_world_runtime_execution_code, mc_entity *,
                                       mc_registry_classes *, mc_entity *source, mc_entity_status *,
                                       mc_entity_effect_error *,
                                       mc_world_runtime_execution_result *);

/* Only the SAME actual pending owned death. result is the original forwarded
 * child return VALUE; child.result is the canonical stable generic-drop result
 * used for explicit finish_pending/retry_close. Keep the actual drop and typed
 * acquisition/Chunk/receipt views live through exact unwind. Do not infer a
 * pending child or close right from an ambient context/error/other invocation.
 * After its explicit descendants drain, existing death/effect cleanup invokes
 * native_close on only this recorded drop, then checked retirement. Refusal
 * frees nothing. Cleanup never replays Source or promotes original false to
 * success. Expire all typed views before successful cleanup consumes the work.
 */
bool mc_mob_death_execution_inspect(mc_entity *, mc_mob_death_execution_view *);

#endif
