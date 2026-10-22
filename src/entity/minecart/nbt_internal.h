#ifndef MC_ENTITY_MINECART_NBT_INTERNAL_H
#define MC_ENTITY_MINECART_NBT_INTERNAL_H

#include "mc/entity/nbt/nbt.h"

/* Concrete late sn/lq hooks. The caller owns the existing temporary Entity
 * hold and a fresh enclosing NBT frame; these methods add only their reached
 * own/lower sites. No class/phase/World preflight or provenance is implicit. */
bool mc_minecart_nbt_read_held(mc_entity *entity, const mc_nbt_tag *compound,
                               mc_entity_nbt_error *frame);

/* Independent own-field tree only: Type; Type+Items; or Type+PushX+PushZ+Fuel.
 * The caller charges this retained tree while copying its final base+own tree
 * and frees it on every outcome. out is unchanged on failure. */
bool mc_minecart_nbt_build_fields_held(mc_entity *entity, const mc_nbt_limits *limits,
                                       mc_nbt_tree **out, mc_entity_nbt_error *frame);

#endif
