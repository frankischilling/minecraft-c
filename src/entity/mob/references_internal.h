#ifndef MC_ENTITY_MOB_REFERENCES_INTERNAL_H
#define MC_ENTITY_MOB_REFERENCES_INTERNAL_H

#include "mc/entity/entity.h"

/* Actual nullable LS.private_b and II.d fields. Each native hold cell is
 * separate from its source slot. Reads prove that occurrence before borrowing;
 * stores retain the new value before publishing and releasing the old one.
 * No future field is preflighted and no constructor/READY gate is inserted.
 * Owners and errors remain live, exclusive and disjoint through each call. */
mc_entity_status mc_mob_look_target_get(mc_entity *, mc_entity **, mc_entity_error *);
mc_entity_status mc_mob_look_target_store(mc_entity *, mc_entity *, mc_entity_error *);
mc_entity_status mc_mob_creature_target_get(mc_entity *, mc_entity **, mc_entity_error *);
mc_entity_status mc_mob_creature_target_store(mc_entity *, mc_entity *, mc_entity_error *);

/* Native final destruction only, after a caller has established the lifetime
 * of the owner. Source fields are not repaired by call/error retirement. */
void mc_mob_references_dispose(mc_entity *);

#endif
