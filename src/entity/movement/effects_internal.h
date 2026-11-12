#ifndef MC_ENTITY_MOVEMENT_EFFECTS_INTERNAL_H
#define MC_ENTITY_MOVEMENT_EFFECTS_INTERNAL_H
#include "mc/entity/movement/effects.h"

/* Compact complete result of the actual entered UD Byte read. All outputs
 * are disjoint/live; the caller independently holds the canonical Entity.
 * Success writes only out and reports index=-1/NONE. Failure preserves out
 * and reports precisely the one diagnostic stage reached by the old getter.
 * No admission, retain, allocation, World/source callback or replay runs. */
typedef enum mc_entity_watcher_byte_stage {
    MC_ENTITY_WATCHER_BYTE_STAGE_NONE,
    MC_ENTITY_WATCHER_BYTE_STAGE_FLAG,
    MC_ENTITY_WATCHER_BYTE_STAGE_WATCHER
} mc_entity_watcher_byte_stage;

typedef struct mc_entity_watcher_byte_error {
    mc_entity_status status;
    const char *operation;
    mc_entity_effect_cause cause;
    int32_t index;
    uint16_t pc;
    mc_entity_watcher_byte_stage stage;
} mc_entity_watcher_byte_error;

bool mc_entity_watcher_byte_read_entered(mc_entity *, int32_t, int8_t *,
                                         mc_entity_watcher_byte_error *);
/* Overlay the EXACT scalar writes of the former full getter: index always;
 * status/operation/cause/source PC and the selected flag OR watcher PC only
 * on failure. Every unrelated prior diagnostic field remains untouched.
 * This never reads an owner or repeats the source getter. */
void mc_entity_effect_watcher_byte_apply(const mc_entity_watcher_byte_error *,
                                         mc_entity_effect_error *);
/* Actual UD Byte getter after a caller has independently held the Entity.
 * No READY admission or extra source operation is inserted. */
bool mc_entity_effect_watcher_byte_entered(mc_entity *, int32_t, int8_t *,
                                           struct mc_entity_effect_error *);
/* Actual SN.ai()V store2/return5. Caller owns canonical admission/lifetime;
 * no facade, allocation, Source World access or diagnostic reset is inserted. */
void mc_entity_effect_attacked_entered(mc_entity *);
#endif
