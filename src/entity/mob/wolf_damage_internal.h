#ifndef MC_ENTITY_MOB_WOLF_DAMAGE_INTERNAL_H
#define MC_ENTITY_MOB_WOLF_DAMAGE_INTERNAL_H

#include "mc/entity/mob/wolf_damage.h"
#include "mc/entity/movement/effects.h"

/* Complete GI wrapper on an already-held canonical client Wolf. Registry and
 * Math are the actual defining namespaces and remain lazy until reached by
 * the SAME LS parent. The original caller owns all source/World dependencies.
 * Only a complete GI return publishes *out. Earlier effects survive failure;
 * newly acquired parent and World-query work stays on this actor until cleanup.
 * No preexisting producer is adopted, consumed or used to classify this call. */
bool mc_mob_wolf_damage_entered(mc_entity *, mc_registry_classes *, mc_math_classes *,
                                mc_entity *source, int32_t amount, bool *out,
                                mc_entity_effect_error *);
bool mc_mob_wolf_damage_inspect(const mc_entity *, mc_wolf_damage_state *);
mc_entity_status mc_mob_wolf_damage_cleanup(mc_entity *, mc_entity_effect_error *);

#endif
