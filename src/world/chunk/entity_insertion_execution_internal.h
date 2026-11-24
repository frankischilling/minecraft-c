#ifndef MC_WORLD_CHUNK_ENTITY_INSERTION_EXECUTION_INTERNAL_H
#define MC_WORLD_CHUNK_ENTITY_INSERTION_EXECUTION_INTERNAL_H

#include "../../core/math/execution_internal.h"
#include "object_internal.h"

/* SAME complete NORMAL LM.a(Entity)V, not virtual Blank dispatch. Caller owns
 * this exact live normal Chunk/Store lease and actual Entity, defining client
 * code graph and explicit stable IN context. No World/Chunk.d inference occurs.
 * Caller performs actual Blank return before normal-owner/Entity/Math access.
 * Required fresh stable error/status are disjoint; no owning math/context copy.
 * Bool is exact continuation independently from raw status. Reached IN failure
 * may leave status/error.status OK; actual typed Math context retains its cause.
 * q2 still precedes nullable Entity6 and IN13/25/81. All bF/bG/bH/bI stores,
 * live array/List checks and append148 keep original order and partial effects.
 * Existing wrong-location diagnostic refusal45 is unchanged, not repaired.
 * This child does not obtain/release a Store lease, World scope or Math owner;
 * parent pairs its actual lease, preserves this frame and retires real owners.
 * Missing required Math storage never selects legacy arithmetic.
 */
bool mc_world_chunk_insert_entity_in_math_execution(mc_math_execution *, mc_world_chunk *,
                                                    mc_entity *, mc_world_chunk_entity_error *,
                                                    mc_world_storage_status *);

#endif
