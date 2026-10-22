#ifndef MC_ENTITY_NBT_FACTORY_H
#define MC_ENTITY_NBT_FACTORY_H

#include "mc/entity/nbt/nbt.h"

/* Independent-result form of sn/lq.c(NBT). Resolve the actual canonical runtime
 * Class name before testing dead; a dead or unmapped Entity returns OK with
 * serialized=false and leaves out unchanged. The runtime Class is the actual
 * immutable same-owner Entity literal, never a forced Item name. A true result
 * owns an independent compound containing id plus ten base/Item fields,
 * eight base/FallingSand fields, or8/9/11 base/Minecart fields selected by its
 * actual Type. It contains no retained unknown tags.
 * Entity, Stack and class owners are borrowed and excluded from tree budgets.
 * Peak allocation includes the intermediate leaf tree while copying the final
 * compound; bounded C-stack views are excluded. Output/count/error must be
 * disjoint from each other, limits and all owners. All outputs stay unchanged
 * on failure; successful false changes serialized only. This is not mutable
 * Java setter/allocation timing or original writer-order qualification. */
mc_entity_nbt_status mc_entity_save_nbt(mc_entity *entity, const mc_nbt_limits *limits,
                                        bool *serialized, mc_nbt_tree **out,
                                        mc_entity_nbt_error *error);

/* Actual jc/ga NBT factory on the supplied application class owner. NULL World
 * is supported. A matched Item, FallingSand or Minecart uses its real world-only
 * constructor, then its semantic NBT read outside the source reflection catch.
 * FallingSand has no Item Math draws or Stack substitute. IDs/profile effects
 * survive a later failure; factory-local owners are released on failure.
 * Minecart's array/earlier semantic prefixes remain real until that local
 * owner's final release. This semantic factory neither attaches storage
 * provenance nor advertises Minecart GAME-load/save support.
 * Input follows read_nbt's immutable readable-view contract and unique keys.
 * Missing id is the empty string. Unknown id returns OK/NULL after the default
 * process stdout diagnostic for lines of at most64 ASCII bytes. Default-stream
 * native I/O failures are suppressed; PrintStream.checkError/sticky state,
 * stream replacement, Unicode and arbitrary-length output are not exposed.
 * Known unsupported subclasses stop at construction45, distinct from unknown
 * names. Wrong-type/null getters stop at the reached catch diagnostic57; Java
 * reflection/Throwable stack printing and OOM timing are not implemented.
 * Nonnull World follows the existing constructor's native binding contract;
 * ordinary GAME callers select that World's bound Entity class owner. General
 * Java ClassLoader/reflection argument-type equivalence is not provided.
 * out is required and unchanged on failure; success transfers one Entity hold
 * or writes NULL. Class/World/input/out/error owners must remain exclusive,
 * live and mutually disjoint through return. No World/Chunk publication occurs.
 */
mc_entity_nbt_status mc_entity_factory_nbt(mc_entity_classes *classes, mc_world_runtime *world,
                                           const mc_nbt_tag *compound, mc_entity **out,
                                           mc_entity_nbt_error *error);

#endif
