#ifndef MC_ENTITY_PLAYER_CONTAINER_INTERACTION_H
#define MC_ENTITY_PLAYER_CONTAINER_INTERACTION_H

#include "mc/entity/player/drop.h"
#include "mc/entity/player/statistics.h"
#include "mc/inventory/container_interaction.h"

typedef struct mc_player_container_call mc_player_container_call;

typedef struct mc_player_container_error {
    mc_entity_status status;
    const char *operation, *exception_class;
    mc_player_site site, child;
    bool native_policy, source_exception, source_error;
    mc_container_interaction_error interaction;
    mc_player_drop_error drop;
    mc_player_statistic_error statistic;
    mc_registry_error registry;
} mc_player_container_error;

typedef struct mc_player_container_state {
    mc_entity *player;
    mc_container *container;
    const mc_item_stack *result;
    const mc_player_container_error *error;
    const mc_container_interaction_state *interaction;
    bool close, active, returned, active_container_restored, direct_target;
    int32_t window_id;
    size_t drop_calls;
} mc_player_container_state;

/* Source GS.d/e captures; no phase gate/initializer/native hold is inserted.
 * Caller keeps the actual Player and returned Container alive/exclusive. */
mc_entity_status mc_player_container_default_get(mc_entity *, mc_container **);
mc_entity_status mc_player_container_active_get(mc_entity *, mc_container **);
/* Entered actual GS.r()V: read d2, store e5, return8. No display, phase gate,
 * allocation or Container work; caller keeps the actual source graph live. */
mc_entity_status mc_player_container_restore_default(mc_entity *, mc_player_container_error *);

/* Actual singleplayer OB forwarding. window_id is the actual ignored source
 * argument. The current Player.e receiver is captured when reached, never
 * replaced with the default Container. Close restores e=d only after source
 * close returns. Reached drops use actual GS drop calls retained until checked
 * retirement; source failures preserve cursor/grid/publication/stat prefixes.
 * out starts NULL. Errors are optional and cannot alias live owner storage. */
mc_entity_status mc_player_container_click_begin(mc_registry_classes *, mc_math_classes *,
                                                 mc_entity *player, int32_t window_id, int32_t slot,
                                                 int32_t button, bool shift,
                                                 mc_player_container_call **,
                                                 mc_player_container_error *);
mc_entity_status mc_player_container_close_begin(mc_registry_classes *, mc_math_classes *,
                                                 mc_entity *player, int32_t window_id,
                                                 mc_player_container_call **,
                                                 mc_player_container_error *);
/* Enter the supplied actual Container's virtual close directly, as OO.h15.
 * The caller owns its enclosing invocation site. No OB frame, Player.e read,
 * window-id read or e=d store is inserted. NULL Player reaches DW's GS.c1;
 * NULL Container fails the caller's invocation before the virtual body.
 * Child drops and the reached IT World occurrence retain the same checked
 * ownership contract as the controller adapters above. */
mc_entity_status mc_player_container_direct_close_begin(mc_registry_classes *, mc_math_classes *,
                                                        mc_entity *player, mc_container *,
                                                        mc_player_container_call **,
                                                        mc_player_container_error *);
bool mc_player_container_inspect(const mc_player_container_call *, mc_player_container_state *);
/* Read-only reached GS child; borrowed views end at checked retirement. */
bool mc_player_container_drop_inspect(const mc_player_container_call *, size_t,
                                      mc_player_drop_state *);
mc_entity_status mc_player_container_take_result(mc_player_container_call *, mc_item_stack **);
mc_entity_status mc_player_container_retire(mc_player_container_call **,
                                            mc_player_container_error *);

#endif
