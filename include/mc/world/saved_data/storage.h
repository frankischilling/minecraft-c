#ifndef MC_WORLD_SAVED_DATA_STORAGE_H
#define MC_WORLD_SAVED_DATA_STORAGE_H

#include "mc/storage/nbt/nbt.h"
#include "mc/world/saved_data/value.h"
#include "mc/world/storage/status.h"

#include <stdbool.h>

typedef struct mc_world_store mc_world_store;
typedef struct mc_world_saved_data mc_world_saved_data;

typedef struct mc_world_saved_data_limits {
    size_t max_ids;
    size_t max_key_units;
    size_t max_allocation_bytes;
    mc_nbt_limits nbt;
    size_t max_named_entries, max_ordered_entries;
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
    bool class_bound;
    mc_world_info_side side;
} mc_world_saved_data_view;

/* Defaults: 4096 IDs, named entries and ordered occurrences, 65536 retained
 * UTF-16 key units, 4 MiB owner/staging
 * allocations, and default NBT limits. Owner accounting includes all three
 * embedded collection descriptors, copied path, entries and keys;
 * allocator/path-conversion/stdio overhead and
 * constant stack scratch are excluded. Input files and decoded NBT use their
 * separate bounds; streamed output uses the NBT byte/tag/element/depth bounds.
 * Referenced String/value/array owners and records each have their own bounds;
 * retaining them does not charge their allocations again. Compression receives
 * an independent max_allocation_bytes budget and NBT input/output byte bounds.
 * All limits may be zero except NBT max_depth, which must be in 1..128. */
mc_world_saved_data_limits mc_world_saved_data_default_limits(void);

/* Construct a native SavedData owner, including its
 * distinct named-cache, ordered-list and ID-map descriptors. Named collections
 * start empty. A nonnull
 * store is borrowed only during this call: copy its fixed data/idcounts.dat
 * path, then read raw, uncompressed named Compound NBT. Missing files succeed
 * with an empty map. Retain only Short children, copying their UTF-16 names.
 * Neither store metadata nor session tokens are read or changed. NULL store
 * selects the original in-memory mode and performs no file operations.
 *
 * Caught I/O/malformed-data failures also succeed, with last_caught reporting
 * their diagnostic. Native allocation/limit/argument failures return an error
 * and preserve out. Input files are closed on every native path. The owner
 * survives store retirement and metadata replacement. Only the paired MapData
 * classes are constructible named subtypes; map trackers and item updates remain
 * unfinished.
 * Grant exclusive access to the owner and its file paths throughout calls. */
mc_world_storage_status mc_world_saved_data_create(const mc_world_store *store,
                                                   const mc_world_saved_data_limits *limits,
                                                   mc_world_saved_data **out,
                                                   mc_world_storage_error *error);

/* Select the actual client hc or server es base-type domain. GAME construction
 * and named-data reference scenarios use this route. The older create entry is
 * an unbound native ID/composition owner and admits either paired MapData class;
 * it does not reproduce the original manager's distinct Java base classes.
 * A bound put rejects an opposite-base value before mutations. A bound miss
 * constructs the requested class, then catches the opposite-base cast before
 * publishing the local or caching it. Cache hits still ignore requested Class.
 * Invalid side is INVALID_ARGUMENT with out unchanged. */
mc_world_storage_status
mc_world_saved_data_create_for_side(const mc_world_store *store, mc_world_info_side side,
                                    const mc_world_saved_data_limits *limits,
                                    mc_world_saved_data **out, mc_world_storage_error *error);

/* Actual named cache/list operations. Keys are retained nullable UTF-16 String
 * identities; equality follows content with null distinct from empty. Value's
 * immutable name is independent of its lookup key and selects its save file.
 * Put rejects NULL value before mutations. Replacement removes the old map
 * entry and first identity-equal list occurrence, then inserts/appends the new
 * occurrence; aliases under other keys remain. Put does not mark dirty.
 * Native resource failure preserves every completed mutation prefix.
 *
 * Get checks the actual cache before using type, including NULL type. On a
 * miss, memory mode/missing files return OK with *out=NULL. A present file
 * constructs the selected MapData, then reads gzip lowercase data. A caught
 * source-like I/O/type/bounds error after construction still publishes that
 * default or partially read value. Native argument/resource/unsupported failures
 * escape and preserve out. Every nonnull successful return supplies an owned
 * value hold, independent of map/list holds. No session or metadata operation.
 * File conversion uses the existing checked UTF-8 native transport; source
 * filename/stream failure prefixes and printed diagnostics remain unqualified.
 * Keep value/key arguments live and exclusive throughout each operation. */
mc_world_storage_status mc_world_saved_data_put(mc_world_saved_data *owner, mc_world_info_name *key,
                                                mc_saved_data_value *value,
                                                mc_world_storage_error *error);
mc_world_storage_status mc_world_saved_data_get(mc_world_saved_data *owner,
                                                const mc_saved_data_class *type,
                                                mc_world_info_name *key, mc_saved_data_value **out,
                                                mc_world_storage_error *error);

/* Passive actual named entries and ordered occurrences. Borrowed values/keys
 * remain valid only while their owning entries/occurrences remain live; retain
 * independently across mutations. Map iteration is native insertion order,
 * not a claim about the original HashMap's physical bucket traversal. */
bool mc_world_saved_data_named_at(const mc_world_saved_data *owner, size_t index,
                                  mc_world_info_name **key, mc_saved_data_value **value);
mc_saved_data_value *mc_world_saved_data_ordered_at(const mc_world_saved_data *owner, size_t index);
const mc_saved_data_error *mc_world_saved_data_last_value_error(const mc_world_saved_data *owner);

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
 * diagnostic-history effects. Visit actual ordered occurrences and save only
 * dirty values. Clear that SAME value's dirty flag after a normal or caught
 * writer result, including memory mode. Named files are direct gzip overwrites
 * of fresh lowercase data, without metadata rotation or unknown-tag retention.
 * A caught I/O/type/null encoding failure retains last_caught and still returns
 * normally. Escaping native resource failures retain dirty and completed effects.
 * NULL owner is INVALID_ARGUMENT. error receives this attempt's
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
