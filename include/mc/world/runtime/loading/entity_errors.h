#ifndef MC_WORLD_RUNTIME_LOADING_ENTITY_ERRORS_H
#define MC_WORLD_RUNTIME_LOADING_ENTITY_ERRORS_H

#include "mc/entity/nbt/errors.h"
#include "mc/world/chunk/entity_errors.h"

/* One ordinary GAME Entity import attempt. The source factory, direct Chunk
 * insertion and extra native provenance are independent reached children.
 * index is SIZE_MAX before an occurrence is selected. This record owns no
 * source, Entity, Chunk or Throwable; all operation strings are static. */
typedef struct mc_world_entity_load_error {
    size_t index;
    mc_entity_nbt_error factory, provenance;
    mc_world_chunk_entity_error insertion;
    mc_world_storage_error native;
} mc_world_entity_load_error;

mc_world_entity_load_error mc_world_entity_load_empty_error(void);

#endif
