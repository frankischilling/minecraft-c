#ifndef MC_WORLD_RUNTIME_ENTITY_ERRORS_H
#define MC_WORLD_RUNTIME_ENTITY_ERRORS_H

#include "mc/entity/errors.h"
#include "mc/world/cache/acquire.h"
#include "mc/world/storage/status.h"

#include <stddef.h>

typedef struct mc_world_entity_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, presence_bytecode, provider_bytecode;
    unsigned int chunk_bytecode, observer_bytecode;
    mc_chunk_acquire_status acquisition;
    mc_world_storage_error cause;
    mc_entity_error entity;
} mc_world_entity_error;

/* Separate batch occurrence from single-publication diagnostics. SIZE_MAX
 * denotes native whole-addAll admission or no reached occurrence; publication
 * retains the actual World callee/observer and typed Entity child unchanged. */
typedef struct mc_world_entity_batch_error {
    mc_world_entity_error publication;
    size_t index;
} mc_world_entity_batch_error;

mc_world_entity_batch_error mc_world_entity_batch_empty_error(void);

#endif
