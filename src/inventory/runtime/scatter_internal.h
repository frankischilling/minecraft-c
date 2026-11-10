#ifndef MC_INVENTORY_RUNTIME_SCATTER_INTERNAL_H
#define MC_INVENTORY_RUNTIME_SCATTER_INTERNAL_H

#include "../../entity/player/drop_execution_internal.h"
#include "mc/entity/player/death.h"
#include "mc/entity/player/drop.h"

/* The enclosing Player death work owns the reached drop until its checked
 * retirement, including after failure. IX arrays/cells are read afresh at each
 * Source site, with clears only after the genuine scatter child returns. */
mc_entity_status mc_inventory_player_scatter_entered(mc_inventory_player *, mc_registry_classes *,
                                                     mc_math_classes *, mc_player_death_state *,
                                                     mc_player_drop_call **, mc_player_drop_error *,
                                                     mc_player_drop_error *);

/* SAME IX.g all36+4 slots. Caller owns stable exclusive Inventory/Player and
 * enclosing state/drop/error/result storage until checked child retirement.
 * Bool and raw Entity/Player statuses remain independent; false/rawOK stops
 * at invoke31/78 BEFORE fresh-array clear40/87, counters or index advance.
 * Previously cleared cells and actual published Items remain reached effects.
 * Outer result receives VALUE observations only; the actual retained drop's
 * private result is canonical for explicit pending/close unwind. No Source
 * replay, after-return receipt retain or automatic arbitrary scope pop occurs.
 * Missing execution/code/outputs refuses before Source, never selects legacy.
 */
bool mc_inventory_player_scatter_entered_in_execution(
    mc_entity_tick_execution *, mc_world_runtime_classes *, mc_world_runtime_execution_code,
    mc_inventory_player *, mc_registry_classes *, mc_player_death_state *, mc_player_drop_call **,
    mc_player_drop_error *, mc_player_drop_error *, mc_entity_status *, mc_player_drop_status *,
    bool *, mc_world_runtime_execution_result *);

#endif
