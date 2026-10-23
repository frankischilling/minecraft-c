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

typedef struct mc_world_saved_data_view {
    const void *storage;
    const void *named_cache, *ordered_list, *id_map;
    size_t named_count, ordered_count, id_count;
} mc_world_saved_data_view;

/* Defaults: 4096 IDs, 65536 retained UTF-16 units, 4 MiB owner/staging
 * allocations, and default NBT limits. Owner accounting includes all three
 * embedded collection descriptors, copied path, entries and keys;
 * allocator/path-conversion/stdio overhead and
 * constant stack scratch are excluded. Input files and decoded NBT use their
 * separate bounds; streamed output uses the NBT byte/tag/element/depth bounds.
 * All limits may be zero except NBT max_depth, which must be in 1..128. */
mc_world_saved_data_limits mc_world_saved_data_default_limits(void);

/* Construct the SavedData owner reached by World construction, including its
 * distinct named-cache, ordered-list and ID-map descriptors. Named collections
 * start empty; only ID mutation is currently implemented. A nonnull
 * store is borrowed only during this call: copy its fixed data/idcounts.dat
 * path, then read raw, uncompressed named Compound NBT. Missing files succeed
 * with an empty map. Retain only Short children, copying their UTF-16 names.
 * Neither store metadata nor session tokens are read or changed. NULL store
 * selects the original in-memory mode and performs no file operations.
 *
 * Caught I/O/malformed-data failures also succeed, with last_caught reporting
 * their diagnostic. Native allocation/limit/argument failures return an error
 * and preserve out. Input files are closed on every native path. The owner
 * survives store retirement and metadata replacement. Named subtype construction,
 * cache lookup/put, list mutation and dirty handling remain unfinished.
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

/* Actual hc/es.a()V ordered-list save. An empty actual list returns normally,
 * independently of ID-map contents and without file, clock, Random, session or
 * diagnostic-history effects. A nonempty named list is UNSUPPORTED_STATE until
 * subtype/dirty/serialization behavior is implemented; it is never skipped as
 * successful work. NULL owner is INVALID_ARGUMENT. error receives this attempt's
 * result and must be disjoint from owner storage, including last_caught. */
mc_world_storage_status mc_world_saved_data_save(mc_world_saved_data *owner,
                                                 mc_world_storage_error *error);

/* Passive snapshot of actual owned descriptors and their current counts.
 * Identities are distinct, stable through ID mutation, and borrowed until free.
 * storage is NULL in memory mode, otherwise this owner's copy-owned path binding
 * used for ID transport. It is not a retained Store pointer or a Java handler
 * identity; separate managers have separate bindings even for the same path.
 * No allocation, file operation or diagnostic reset occurs. NULL arguments
 * return INVALID_ARGUMENT and preserve out. Output must be disjoint from owner
 * storage. Named-count zero is the actual named cache, not ID-map emptiness. */
mc_world_storage_status mc_world_saved_data_inspect(const mc_world_saved_data *owner,
                                                    mc_world_saved_data_view *out);

/* Read-only native inspection. Entry/name views survive further next_id calls
 * until owner retirement; the value itself remains live. NULL/out-of-range
 * returns NULL; count(NULL) returns zero. No file operation or diagnostic reset
 * occurs. last_caught(NULL) returns NULL. */
size_t mc_world_saved_data_count(const mc_world_saved_data *owner);
const mc_world_saved_id *mc_world_saved_data_id_at(const mc_world_saved_data *owner, size_t index);
const mc_world_storage_error *mc_world_saved_data_last_caught(const mc_world_saved_data *owner);
void mc_world_saved_data_free(mc_world_saved_data *owner);

#endif
