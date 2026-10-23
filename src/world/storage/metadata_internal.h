#ifndef MC_WORLD_STORAGE_METADATA_INTERNAL_H
#define MC_WORLD_STORAGE_METADATA_INTERNAL_H

#include "mc/world/storage/store.h"

/* Reached handler transport after the caller's actual session check and Info
 * serialization. Borrow the completed record for this call. Do not sample a
 * clock, change Info, check the session again or select a Player. The checked
 * native encoding/rotation and unknown-field retention policies still apply. */
mc_world_storage_status mc_world_store_write_info_record(mc_world_store *store, mc_world_info *info,
                                                         const mc_world_info_record *record,
                                                         mc_world_storage_error *error);

#endif
