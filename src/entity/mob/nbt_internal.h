#ifndef MC_ENTITY_MOB_NBT_INTERNAL_H
#define MC_ENTITY_MOB_NBT_INTERNAL_H
#include "mc/entity/nbt/nbt.h"
/* Concrete source hooks after the existing SN base read/write prefix. Caller
 * already holds the actual canonical Mob; no READY/constructor gate is added. */
bool mc_mob_nbt_read_held(mc_entity *, const mc_nbt_tag *, const mc_nbt_limits *,
                          mc_entity_nbt_error *);
bool mc_mob_nbt_build_fields_held(mc_entity *, const mc_nbt_limits *, mc_nbt_tree **,
                                  mc_entity_nbt_error *);
void mc_mob_nbt_dispose(mc_entity *);
size_t mc_mob_nbt_owner_bytes(const mc_entity *);
#endif
