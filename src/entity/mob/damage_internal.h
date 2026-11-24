#ifndef MC_ENTITY_MOB_DAMAGE_INTERNAL_H
#define MC_ENTITY_MOB_DAMAGE_INTERNAL_H
#include "mc/entity/movement/effects.h"
struct mc_entity_damage_execution;

/* Same complete LS.a(SN,I) body for an enclosing concrete wrapper. The caller
 * holds the canonical client Mob and owns initialized stable diagnostics.
 * Preserve the enclosing argument/wrapper fields. Only the genuine parent
 * Boolean is written; no outer wrapper return/result is published here. */
bool mc_mob_damage_living_parent_entered(mc_entity *, mc_registry_classes *, mc_math_classes *,
                                         mc_entity *, int32_t, bool *, mc_entity_effect_error *);
/* Closed canonical client Spider/Zombie/Skeleton/Creeper. These actual
 * receivers inherit GZ -> LS damage; their sound overrides stay distinct.
 * The real effect facade
 * holds the actor; the caller holds the live Registry/World graph exclusively.
 * No admission of future Random/World/String/target fields occurs here.
 * Boolean out is published only after actual GZ return. Error is caller-owned,
 * contains complete value causes and borrows only the producer's live receipt.
 * The temporary source hold drains before return. A reached successful target
 * store acquires the persistent II.d owning occurrence; ordinary replacement
 * or terminal retirement releases it. Pending child cleanup keeps its existing
 * effect-error contract. */
bool mc_mob_monster_damage_entered(mc_entity *, mc_registry_classes *, mc_math_classes *,
                                   mc_entity *source, int32_t, bool *, mc_entity_effect_error *);

/* Canonical client Sheep/Pig/Cow/Chicken/Slime/Squid/Ghast. Sheep DL.a invokes symbolic BG.a,
 * resolving inherited LS directly; the other three inherit LS.a. The effect
 * facade already holds the receiver. A nonnull source is admitted/retained
 * only at actual LS144, then kept through later callbacks and released before
 * return. Remote/dead/repeated branches never inspect source or Math.
 * Math is the actual caller-supplied class namespace, borrowed/exclusive and
 * activated only at the reached knockback sqrt. No World/trig inference,
 * substitute source, retry cap, damage rollback or completed death is added. */
bool mc_mob_living_damage_entered(mc_entity *, mc_registry_classes *, mc_math_classes *,
                                  mc_entity *source, int32_t amount, bool *,
                                  mc_entity_effect_error *);

/* Required descriptor routes through those SAME actual wrappers/LS body. */
bool mc_mob_damage_living_parent_entered_in_execution(mc_entity *, mc_entity *, int32_t, bool *,
                                                      mc_entity_effect_error *,
                                                      struct mc_entity_damage_execution *);
bool mc_mob_monster_damage_entered_in_execution(mc_entity *, mc_entity *, int32_t, bool *,
                                                mc_entity_effect_error *,
                                                struct mc_entity_damage_execution *);
bool mc_mob_living_damage_entered_in_execution(mc_entity *, mc_entity *, int32_t, bool *,
                                               mc_entity_effect_error *,
                                               struct mc_entity_damage_execution *);
bool mc_player_living_damage_entered_in_execution(mc_entity *, mc_entity *, int32_t, bool *,
                                                  mc_entity_effect_error *,
                                                  struct mc_entity_damage_execution *);
#endif
