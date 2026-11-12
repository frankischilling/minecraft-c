#ifndef MC_WORLD_RUNTIME_SAVING_PLAYER_METADATA_INTERNAL_H
#define MC_WORLD_RUNTIME_SAVING_PLAYER_METADATA_INTERNAL_H

#include "mc/world/runtime/construction.h"
#include "mc/world/runtime/saving/errors.h"

mc_world_storage_status mc_world_runtime_build_player_metadata(mc_world_runtime *,
                                                               mc_world_info_record **,
                                                               mc_world_player_metadata_error *);

#endif
