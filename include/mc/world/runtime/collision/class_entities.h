#ifndef MC_WORLD_RUNTIME_COLLISION_CLASS_ENTITIES_H
#define MC_WORLD_RUNTIME_COLLISION_CLASS_ENTITIES_H

#include "mc/world/runtime/collision/entities.h"

typedef struct mc_world_class_entity_list mc_world_class_entity_list;

/* Actual fd/dj.a(Class,AABB): read query bounds, construct a fresh List, scan
 * inclusive X/Z in source order using nonloading presence and real acquisition,
 * then invoke the actual normal/Blank Class gather. Never reads or clears R.
 * Preserve duplicates and source order; Class matching precedes candidate bounds.
 * No dead, player, construction-World or READY filter is added.
 *
 * Keep live GAME owners exclusive, admitting raw/constructing/failed prefixes
 * but rejecting retirement. target is NULL or a live canonical Class literal;
 * keep its defining owner live throughout the call. Different loaded owners
 * remain different Classes, independent of matching names, side or profile.
 * NULL target fails only at a reached Class invocation; an empty/Blank scan
 * can complete. bounds may be NULL until its reached World minX read; keep a
 * nonnull bounds and its actual owner live throughout the call. Arithmetic
 * requires the strict Java floating environment at its first reached operation.
 *
 * Success supplies one independently owned fresh List, including when empty.
 * Each duplicate occurrence holds its actual Entity; the List holds one local
 * World lease, making retirement/free BUSY until release. Later queries and R
 * changes leave this List untouched. Source loops have no work cap. Native
 * checked allocations do not claim Java ArrayList allocation/OOM timing.
 *
 * out/error must be fresh disjoint storage, disjoint from all owners and input
 * views. Failure preserves out and every reached World/Chunk effect, disposing
 * only this query's unreachable private result and native occurrence holds.
 * Full per-attempt acquisition children survive in error, which may be NULL. */
mc_world_storage_status mc_world_runtime_class_entities(mc_world_runtime *world,
                                                        const mc_entity_class_literal *target,
                                                        const mc_entity_bounds *bounds,
                                                        mc_world_class_entity_list **out,
                                                        mc_world_nearby_error *error);

/* Reads borrow the actual occurrence without adding an Entity hold. Keep the
 * same List live/exclusive until that borrow ends, or retain the Entity before
 * releasing it. Failure preserves out. identity is the same actual source List
 * descriptor for this owner's lifetime, not R or a copy of its contents.
 * Release balances exactly one result owner and all occurrence/World holds;
 * release(NULL) is safe. Do not copy an owning pointer or release it twice. */
mc_world_storage_status mc_world_class_entity_list_count(const mc_world_class_entity_list *list,
                                                         size_t *out);
mc_world_storage_status mc_world_class_entity_list_get(const mc_world_class_entity_list *list,
                                                       size_t index, mc_entity **out);
const mc_world_reference_list *
mc_world_class_entity_list_identity(const mc_world_class_entity_list *list);
void mc_world_class_entity_list_release(mc_world_class_entity_list *list);

#endif
