#ifndef MC_WORLD_RUNTIME_TILE_ENTITIES_IDENTITY_INTERNAL_H
#define MC_WORLD_RUNTIME_TILE_ENTITIES_IDENTITY_INTERNAL_H

#include "mc/world/runtime/tile_entities/tile_entities.h"

#include "../../../core/java/object_identity_internal.h"

/* Private Object identity on actual held owners. A disjoint output is required
 * on every call. A null Tile returns zero without accessing Thread/profile,
 * even with a null Thread. Nonnull Tiles and canonical Class receivers require
 * an admitted nonnull current logical Thread, including cached calls; none is
 * discovered through a Tile's mutable World. RAW, READY and failed-construction
 * Tiles use the same cell. A fresh cell draws once; a cached cell draws nothing.
 *
 * Callers keep the actual Tile held or a counted canonical Class namespace
 * borrow live, with exclusive access to the objects and Thread. Dangling or
 * synthesized owners are not admitted. Invalid native arguments preserve out
 * and all cells/Thread words. No operation allocates, initializes a Source
 * class, publishes a World or consumes application Random state. */
mc_tile_entity_status mc_tile_entity_identity_hash(mc_tile_entity *nullable_tile,
                                                   mc_java_identity_thread *current_thread,
                                                   uint32_t *out);
mc_tile_entity_status
mc_tile_entity_class_literal_identity_hash(const mc_tile_entity_class_literal *literal,
                                           mc_java_identity_thread *current_thread, uint32_t *out);

/* Borrow the exact represented Class.getName ASCII bytes for the nine actual
 * Tile classes, separately qualified from the diagnostic public view. These
 * root-package names compare bytewise exactly as their UTF-16 names do; this
 * does not allocate a Java String or supply String object identity. For a
 * Class mirror used as a key, key.getClass().getName is java.lang.Class,
 * independently of the represented Tile class name returned here.
 * Requires the same live canonical namespace borrow and preserves out on
 * failure. No Source active use occurs in any class phase. */
mc_tile_entity_status
mc_tile_entity_class_literal_source_name(const mc_tile_entity_class_literal *literal,
                                         const char **out);

/* Internal borrowed cell access, with canonical namespace freshness checked in
 * the owning translation unit. This adds no hold and expires with the owner.
 * The cell is never a serialization field or a constructor/reset input;
 * hash installation uses only the admitted current Thread transformation. */
mc_tile_entity_status
mc_tile_entity_class_literal_identity_cell(const mc_tile_entity_class_literal *literal,
                                           mc_java_identity_cell **out);

#endif
