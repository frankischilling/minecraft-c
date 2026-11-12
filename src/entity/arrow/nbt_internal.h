#ifndef MC_ENTITY_ARROW_NBT_INTERNAL_H
#define MC_ENTITY_ARROW_NBT_INTERNAL_H

#include "mc/entity/nbt/nbt.h"

/* Complete SL own hooks. The caller holds the genuine client Arrow; no READY,
 * shooter, World or Math admission is inserted. Read preserves c/its counted
 * occurrence and both j/k. Immutable views cannot represent a null Map or a
 * named null tag. Build returns an independent eight-field semantic tree,
 * without an id, base fields, provenance or mutable-setter/OOM equivalence.
 * Error is optional; a supplied error is fresh and disjoint from all inputs,
 * output and owners. Success retains the selected actual SL return site. */
bool mc_arrow_nbt_read_held(mc_entity *, const mc_nbt_tag *, mc_entity_nbt_error *);
bool mc_arrow_nbt_build_fields_held(mc_entity *, const mc_nbt_limits *, mc_nbt_tree **,
                                    mc_entity_nbt_error *);

#endif
