#ifndef MC_ENTITY_MOVEMENT_TURN_H
#define MC_ENTITY_MOVEMENT_TURN_H

#include "mc/entity/entity.h"

/* Actual inherited client sn.d(FF)V, on an existing held READY native Entity.
 * Arguments are the source float look deltas, not frame time or final angles.
 * Save old pitch/yaw; widen current/delta separately; multiply each delta by
 * double 0.15; add yaw/subtract pitch; narrow/store yaw then pitch; apply the
 * ordered -90/+90 pitch clamps; update previous pitch, then previous yaw by
 * the actual binary32 angle differences. Yaw is not wrapped or clamped.
 *
 * All float inputs, including signed zero, subnormals, infinities and NaNs,
 * reach the source arithmetic. Float overflow follows the captured SSE2 Java
 * operation rather than an out-of-range C cast. NaN comparisons skip clamps.
 * This requires x86-64/SSE2, nearest-even, gradual underflow, masked traps,
 * no FTZ/DAZ or excess/reassociated/fused arithmetic. Invalid native arguments,
 * phase or FP environment fail before changing any source field.
 *
 * The caller keeps this actual Entity/class owner live and exclusive. No
 * allocation, hold change, constructor diagnostic, bounds, World, watcher,
 * Random, class active use, tick or callback occurs. The same inherited body
 * applies to the existing concrete owners; this does not construct a Player
 * or supply platform mouse/camera behavior. Original comparison covers client
 * Item/Falling receivers; server turning correspondence is not established. */
mc_entity_status mc_entity_turn(mc_entity *entity, float yaw_delta, float pitch_delta);

#endif
