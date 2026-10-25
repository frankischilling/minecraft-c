#ifndef MC_WORLD_RUNTIME_ENTITY_ERRORS_H
#define MC_WORLD_RUNTIME_ENTITY_ERRORS_H

#include "mc/entity/errors.h"
#include "mc/world/cache/acquire.h"
#include "mc/world/storage/status.h"

#include <stddef.h>
#include <stdint.h>

typedef struct mc_world_player_sleep_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, player_bytecode;
    size_t index;
    mc_world_storage_error cause;
    mc_entity_error entity;
} mc_world_player_sleep_error;

/* Compact nested sites avoid multiplying a second complete error/cause through
 * the existing recursive mutation/publication context graph. These concrete
 * fd.y/gs.N PCs fit16bits; index fits the actual Java List's signed extent. */
typedef struct mc_world_player_sleep_context {
    uint16_t bytecode, player_bytecode;
    int32_t index;
} mc_world_player_sleep_context;

typedef struct mc_world_entity_error {
    mc_world_storage_status status;
    const char *operation;
    unsigned int bytecode, presence_bytecode, provider_bytecode;
    unsigned int chunk_bytecode, world_access_bytecode;
    unsigned int dedicated_bytecode, index_bytecode;
    mc_chunk_acquire_status acquisition;
    mc_world_storage_error cause;
    mc_entity_error entity;
    /* Player-positive fd.b invokes fd.y after d.add and before Chunk lookup. */
    mc_world_player_sleep_context sleep;
} mc_world_entity_error;

/* Separate batch occurrence from single-publication diagnostics. SIZE_MAX
 * denotes native whole-addAll admission or no reached occurrence; publication
 * retains the actual World callee/WorldAccess and typed Entity child unchanged. */
typedef struct mc_world_entity_batch_error {
    mc_world_entity_error publication;
    size_t index;
} mc_world_entity_batch_error;

mc_world_entity_batch_error mc_world_entity_batch_empty_error(void);

#endif
