#ifndef MC_ENTITY_ARROW_TICK_EXECUTION_DEPENDENCIES_H
#define MC_ENTITY_ARROW_TICK_EXECUTION_DEPENDENCIES_H

/* Concrete published owned children used by the complete parent. Each direct
 * World call uses the exact scope captured before its arguments; no NULL owner
 * chooses legacy behavior. Victim damage at 741 shares the retained damage,
 * wake, alert and death bodies. Unjoined reached descendants stop explicitly;
 * no success shim or Source replay supplies their behavior. */
#include "../../world/runtime/collision/entities_execution_internal.h"
#include "../tick/execution_internal.h"
#include "mc/entity/movement/effects.h"
#include "mc/world/runtime/collision/boxes.h"
#include "mc/world/runtime/movement.h"
#include "mc/world/runtime/presentation.h"
#include "mc/world/runtime/raytrace.h"

/* Scalar ID/metadata uses the actual published required code/result signature
 * in collision/blocks_execution_internal.h, included by tick_internal.h.
 * Sound/particle use actual execution_children_internal.h declarations.
 * Remaining declarations are explicitly OPEN, not fallback definitions. */
/* Box/state-shape now use the actual required code/site/status/result
 * declarations in blocks_execution_internal.h (already included by the parent).
 * Their bool is the continuation condition; a raw Source status is independent. */
/* Actual owned Ray entry/view, not an unresolved declaration. */
#include "../../world/runtime/raytrace_execution_internal.h"
/* Victim damage uses the same entered bodies and stable result through
 * damage_execution_internal.h. Its remaining limits belong to the actual
 * reached descendants, including unjoined recursive neighbor hooks. */

#endif
