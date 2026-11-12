#ifndef MC_WORLD_CHUNK_ENTITY_ERRORS_H
#define MC_WORLD_CHUNK_ENTITY_ERRORS_H

#include "mc/entity/errors.h"
#include "mc/world/storage/status.h"

/* Passive direct Chunk insertion cause. No operation or owner is retained. */
typedef struct mc_world_chunk_entity_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode;
    mc_world_storage_error cause;
    /* Only a genuinely reached native Entity operation populates this.
     * A NULL source Entity, mismatch or Chunk budget failure does not invent
     * an Entity constructor/phase error. */
    mc_entity_error entity;
} mc_world_chunk_entity_error;

#endif
