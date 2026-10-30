#ifndef MC_ENTITY_CONSTRUCTION_INTERNAL_H
#define MC_ENTITY_CONSTRUCTION_INTERNAL_H

#include "mc/entity/entity.h"

/* Concrete callers are Item, FallingSand, Minecart, local Player, pickup
 * particle and the five client hostile Mobs. This is not a public generic-kind
 * factory. Preserve actual active use before live count/budget and header
 * allocation. Mob callers perform their selected initializer first; Minecart's
 * branch performs its own initializer here. No fabricated subtype is admitted. */
mc_entity_status mc_entity_allocate_canonical(mc_entity_classes *classes, mc_entity_class_kind kind,
                                              const char *operation, mc_entity **out,
                                              mc_entity_error *error);

/* Shared exact kind -> RAW -> nullable World profile admission, then
 * CONSTRUCTING store. No same-side/class/READY World
 * gate or source invocation. A failed begin preserves the existing phase and
 * leaves the new own diagnostic sites neutral. */
mc_entity_status mc_entity_constructor_begin(mc_entity *entity, mc_entity_class_kind kind,
                                             mc_world_runtime *world, const char *operation,
                                             const char *profile_operation, mc_entity_error *error);

/* Shared base body owns ID/bounds/Random/watcher/World binding/position/Byte0.
 * Its early virtual b() dispatch reaches the real DC or selected Mob hook;
 * Item/Falling/Minecart and client particle targets are verified empty returns. */
mc_entity_status mc_entity_construct_base(mc_entity *entity, mc_world_runtime *world,
                                          mc_entity_error *error);

/* Exact typed Byte registration used by base index0 and the real gs.b hook
 * index16. Called only on the constructor-owned actual watcher. */
mc_entity_status mc_entity_construct_watch_byte_zero(mc_entity *entity, int32_t index,
                                                     mc_entity_error *error);
mc_entity_status mc_entity_construct_watch_byte(mc_entity *, int32_t index, int8_t value,
                                                mc_entity_error *);

/* Called only after successful begin, once per public concrete constructor.
 * Failure stores FAILED_CONSTRUCTION and preserves the entire supplied error.
 * Success follows the caller's actual return PC, stores READY and clears error. */
mc_entity_status mc_entity_constructor_finish(mc_entity *entity, mc_entity_status status,
                                              mc_entity_error *error);

/* Thin instance-ledger wrappers over class memory: increment/decrement this
 * Entity's direct_bytes for exactly the same successful requests/retirements.
 * Constructor disposal uses the same pair; no extra native owners are hidden. */
void *mc_entity_construction_allocate(mc_entity *entity, size_t bytes, mc_entity_error *error,
                                      const char *operation, uint16_t bytecode);
void mc_entity_construction_retire(mc_entity *entity, void *allocation, size_t bytes);

#endif
