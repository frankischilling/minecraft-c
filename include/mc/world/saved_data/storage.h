#ifndef MC_WORLD_SAVED_DATA_STORAGE_H
#define MC_WORLD_SAVED_DATA_STORAGE_H

#include "mc/storage/nbt/nbt.h"
#include "mc/world/storage/status.h"

#include <stdbool.h>

typedef struct mc_world_store mc_world_store;
typedef struct mc_world_saved_data mc_world_saved_data;

typedef struct mc_world_saved_data_limits {
    size_t max_ids;
    size_t max_key_units;
    size_t max_allocation_bytes;
    mc_nbt_limits nbt;
} mc_world_saved_data_limits;

typedef struct mc_world_saved_id {
    bool null_name;
    mc_nbt_string name;
    int16_t value;
} mc_world_saved_id;

/* Defaults: 4096 IDs, 65536 retained UTF-16 units, 4 MiB owner/staging
 * allocations, and default NBT limits. Owner accounting includes the owner,
 * copied path, entries and keys; allocator/path-conversion/stdio overhead and
 * constant stack scratch are excluded. Input files and decoded NBT use their
 * separate bounds; streamed output uses the NBT byte/tag/element/depth bounds.
 * All limits may be zero except NBT max_depth, which must be in 1..128. */
mc_world_saved_data_limits mc_world_saved_data_default_limits(void);

/* Construct the saved-data ID owner reached by World construction. A nonnull
 * store is borrowed only during this call: copy its fixed data/idcounts.dat
 * path, then read raw, uncompressed named Compound NBT. Missing files succeed
 * with an empty map. Retain only Short children, copying their UTF-16 names.
 * Neither store metadata nor session tokens are read or changed. NULL store
 * selects the original in-memory mode and performs no file operations.
 *
 * Caught I/O/malformed-data failures also succeed, with last_caught reporting
 * their diagnostic. Native allocation/limit/argument failures return an error
 * and preserve out. Input files are closed on every native path. The owner
 * survives store retirement and metadata replacement. Named SavedData subtype
 * construction/cache/list/dirty handling is not implemented by this interface.
 * Grant exclusive access to the owner and its file paths throughout calls. */
mc_world_storage_status mc_world_saved_data_create(const mc_world_store *store,
                                                   const mc_world_saved_data_limits *limits,
                                                   mc_world_saved_data **out,
                                                   mc_world_storage_error *error);

/* NULL key is the Java null key, distinct from a nonnull empty UTF-16 string.
 * A new key gets zero; an existing value increments with signed Short wrapping.
 * Commit the map before attempting a direct overwrite of raw idcounts.dat.
 * No session check, compression, rotation, implicit metadata save or rollback
 * occurs. Both null and empty keys would emit empty tag names. When both
 * coexist, persistence returns UNSUPPORTED_STATE before opening the file,
 * retaining the committed map prefix and preserving out. Their original
 * duplicate-name winner requires the actual collection profile, which is
 * unfinished. Other native emission follows insertion order; Java HashMap
 * wire/exception-prefix ordering remains unqualified. Explicit reload uses the
 * ordinary last-duplicate-wins NBT reader.
 *
 * A caught I/O/data-encoding failure returns OK and the new ID, retaining its
 * diagnostic. Native resource failures return their status with out unchanged,
 * but any completed map change remains visible. Encoding, write and byte-limit
 * failures may leave the reached truncated/partial file. Unlike the original,
 * diagnostics are retained instead of printed. Key/name/error/out storage must be disjoint
 * from the owner and each other. NULL units is valid only for zero length. */
mc_world_storage_status mc_world_saved_data_next_id(mc_world_saved_data *owner,
                                                    const mc_nbt_string *key, int32_t *out,
                                                    mc_world_storage_error *error);

/* Read-only native inspection. Entry/name views survive further next_id calls
 * until owner retirement; the value itself remains live. NULL/out-of-range
 * returns NULL; count(NULL) returns zero. No file operation or diagnostic reset
 * occurs. last_caught(NULL) returns NULL. */
size_t mc_world_saved_data_count(const mc_world_saved_data *owner);
const mc_world_saved_id *mc_world_saved_data_id_at(const mc_world_saved_data *owner, size_t index);
const mc_world_storage_error *mc_world_saved_data_last_caught(const mc_world_saved_data *owner);
void mc_world_saved_data_free(mc_world_saved_data *owner);

#endif
