#ifndef MC_WORLD_STORAGE_STORE_H
#define MC_WORLD_STORAGE_STORE_H

#include "mc/storage/level/level.h"
#include "mc/world/cache/acquire.h"
#include "mc/world/storage/chunk.h"
#include "mc/world/storage/regions.h"

typedef struct mc_world_store mc_world_store;
typedef struct mc_world_store_reservation mc_world_store_reservation;

#define MC_WORLD_STORE_MAX_REGISTERED_CACHES 256u

typedef struct mc_world_store_limits {
    size_t max_loaded_chunks;
    mc_nbt_limits nbt;
    mc_compression_limits compression; /* Metadata gzip; chunks use regions' limits. */
} mc_world_store_limits;

typedef struct mc_world_store_options {
    bool create_players;
    int64_t session_token;
    const uint8_t *block_map;
    size_t block_map_length;
    mc_world_store_limits limits;
} mc_world_store_options;

/* Defaults: 512 retained/reserved identities and default NBT/compression limits.
 * Each codec operation and retained chunk document has its own allocation
 * budget; max_loaded_chunks bounds owned plus reserved slots, not a shared
 * process budget. Individual retirement is explicit and checks registered
 * references; arbitrary external borrowed views remain a caller obligation. */
mc_world_store_limits mc_world_store_default_limits(void);

/* Open an explicitly selected read/write world root. regions is borrowed until
 * free and may be shared by several stores. root is a bounded UTF-8 native path.
 * options is required: copy its frozen first 256 block-map bytes (each maps to
 * itself or 0), create root/data and optionally players, then overwrite the
 * eight-byte big-endian session.lock token. create_players=false still writes
 * that token, as the original handler does. This is timestamp-token checking,
 * not OS exclusion. Grant exclusive access to these paths across all calls.
 *
 * No metadata is implicitly loaded or created. Failure leaves out unchanged;
 * earlier directory/lock effects may remain. No personal-world copy is made by
 * this library: callers running compatibility tests must supply owned copies.
 * Options, source paths and result/error storage must be mutually disjoint. */
mc_world_storage_status mc_world_store_open(const char *root, mc_world_regions *regions,
                                            const mc_world_store_options *options,
                                            mc_world_store **out, mc_world_storage_error *error);

/* Compare exactly the first eight session bytes, allowing arbitrary trailing
 * bytes. Missing/short/unreadable or unequal tokens return SESSION_MISMATCH;
 * allocation/argument errors remain distinct. No lock file is deleted. */
mc_world_storage_status mc_world_store_check_session(const mc_world_store *store,
                                                     mc_world_storage_error *error);

/* Load gzip metadata from level.dat, then level.dat_old when the primary is
 * missing or throws an equivalent data/I/O error. level.dat_new is ignored.
 * A missing Data child succeeds with defaults and prevents fallback. Resource
 * failures stop instead of silently selecting an older world. Both missing
 * return MISSING; no usable record with corruption/I/O returns an explicit
 * error rather than silently creating a fresh world. Existing metadata remains
 * unchanged on failure. No session check occurs during this read.
 *
 * info/root are borrowed immutable views until the next successful load/save
 * or free. info is NULL before successful load/save; root is the last retained
 * metadata tree. Input strings/Player may borrow these views during save. */
mc_world_storage_status mc_world_store_load_info(mc_world_store *store,
                                                 mc_world_storage_error *error);
const mc_level_info *mc_world_store_info(const mc_world_store *store);
const mc_nbt_tag *mc_world_store_metadata_root(const mc_world_store *store);
const char *mc_world_store_root(const mc_world_store *store);

/* World-save metadata route: check the session, build gzip Data with version
 * 19132 and explicit LastPlayed clock, then checked new/current/old rotation.
 * A completed new file precedes deleting old; current moves to old if present;
 * new replaces current. Stop at the first failed operation and retain recovery
 * files. Original ignored rename/delete results are not reproduced. No fsync
 * or crash-safe transaction is promised. A failure may leave level.dat_new or
 * a completed current-to-old move, but never intentionally deletes current
 * after a failed move. No automatic chunk save occurs.
 *
 * Preserve unknown root/Data fields and the currently retained opaque Player.
 * Changing/dropping/introducing Player through this intermediate owner returns
 * UNSUPPORTED_STATE; use of an empty live-player list must not silently erase
 * unsupported data. This differs from original empty-list metadata saving.
 * The input info is not mutated. After success, the store view retains input
 * last_played (the loaded field), while output LastPlayed uses saved_at_ms.
 * SizeOnDisk is copied from info; chunk writes update the store's live value.
 */
mc_world_storage_status mc_world_store_save_info(mc_world_store *store, const mc_level_info *info,
                                                 int64_t saved_at_ms,
                                                 mc_world_storage_error *error);

/* Dimension 0 uses the root; -1 uses DIM-1. Other dimensions return
 * UNSUPPORTED_STATE. Load owns a new stable chunk identity on every success,
 * applies the frozen postload block map, and leaves all transient load fields
 * at their decoded defaults. Coordinates remain those stored in Level even
 * when they differ from the requested slot. No activation/registration or
 * cache time stamp occurs. Opaque entity/tile data is retained for safe storage
 * round trips. Missing slot returns MISSING. Outputs are unchanged on failure.
 *
 * retain_chunk copies a newly generated normal chunk into store ownership;
 * it cannot introduce entity/tile records. write_chunk accepts only a state
 * retained by this store, checks the session, writes schema at explicit world
 * time and region timestamp, then adds growth to live SizeOnDisk with Java long
 * wrapping. Successful metadata must already exist for accounting. The caller
 * owns save eligibility, dirty reset and last_save_time: transport changes none.
 */
mc_world_storage_status mc_world_store_load_chunk(mc_world_store *store, int32_t dimension,
                                                  int32_t x, int32_t z, mc_chunk_state **out,
                                                  mc_world_storage_error *error);
mc_world_storage_status mc_world_store_retain_chunk(mc_world_store *store, int32_t dimension,
                                                    const mc_chunk_state *chunk,
                                                    mc_chunk_state **out,
                                                    mc_world_storage_error *error);
mc_world_storage_status mc_world_store_write_chunk(mc_world_store *store, mc_chunk_state *chunk,
                                                   int64_t world_time, uint32_t timestamp,
                                                   uint32_t *growth, mc_world_storage_error *error);
size_t mc_world_store_chunk_count(const mc_world_store *store);

/* Identity queries use the exact retained state pointer, never coordinates or a
 * packed key. NULL/unknown inputs return false. Lease/release reject unknown
 * identities and unbalanced releases with INVALID_ARGUMENT. A lease-count
 * overflow returns LIMIT without mutation. These calls perform no I/O or
 * callbacks and are suitable for synchronous coordinator reference hooks. */
bool mc_world_store_owns_chunk(const mc_world_store *store, const mc_chunk_state *chunk);
mc_world_storage_status mc_world_store_lease_chunk(mc_world_store *store,
                                                   const mc_chunk_state *chunk,
                                                   mc_world_storage_error *error);
mc_world_storage_status mc_world_store_release_chunk(mc_world_store *store,
                                                     const mc_chunk_state *chunk,
                                                     mc_world_storage_error *error);

/* Borrow a live cache descriptor and an optional stable fallback-pointer slot.
 * Every provided slot must contain a valid nonnull normal/blank state. Keep the
 * descriptor, arrays, slot and referenced identities live until unregister.
 * Mutating valid contents is permitted under exclusive access; their actual
 * current identities are rescanned at retirement. The same cache/slot pair is
 * idempotent; reusing a cache with a different slot is INVALID_ARGUMENT. At most
 * 256 distinct caches may be registered. Registration allocates only its link.
 * Invalid metadata or allocation/registration limits preserve the registry.
 * Unregister rejects an unknown cache but never frees any referenced chunk;
 * it can remove a registration whose descriptor metadata has become invalid. */
mc_world_storage_status mc_world_store_register_cache(mc_world_store *store,
                                                      const mc_chunk_cache *cache,
                                                      mc_chunk_state *const *fallback,
                                                      mc_world_storage_error *error);
mc_world_storage_status mc_world_store_unregister_cache(mc_world_store *store,
                                                        const mc_chunk_cache *cache,
                                                        mc_world_storage_error *error);

/* Retire one exact owned identity only after all registered metadata validates.
 * Invalid cache/fallback metadata returns INVALID_ARGUMENT without freeing.
 * Leases, active=true, or any map/list/fallback occurrence return BUSY. Repeated
 * list entries, overwritten packed aliases and references from several caches
 * all count. Success unlinks/frees this owner and recovers one admission slot.
 * No save, file/session/metadata change or shared-region clear occurs.
 *
 * The caller must first retire every unregistered raw pointer/view, including
 * deferred external registration effects. Registrations/leases cannot detect
 * arbitrary pointers and are not a Java GC model. All calls require exclusive
 * owner/descriptor access. Discard the state and its borrowed data after OK. */
mc_world_storage_status mc_world_store_retire_chunk(mc_world_store *store, mc_chunk_state *chunk,
                                                    mc_world_storage_error *error);

/* Reserve one slot and preallocate its ownership link before generation.
 * Only dimensions 0/-1 are supported. out is required and unchanged on failure;
 * the returned opaque token belongs to this store until canceled/consumed.
 * Ordinary load/retain and other reservations share the same admission bound.
 * No payload/codec allocation is reserved, no generation runs, and no files are
 * changed. A later generated-payload copy can still fail after RNG/queue effects;
 * those effects are outside this reservation and are not rolled back. */
mc_world_storage_status mc_world_store_reserve_chunk(mc_world_store *store, int32_t dimension,
                                                     mc_world_store_reservation **out,
                                                     mc_world_storage_error *error);
/* Cancel frees exactly this token/link. retain_reserved copies the same normal
 * generated state accepted by retain_chunk using the reserved link/dimension.
 * Success consumes the token and returns a fresh stable identity; failure
 * preserves the token, reservation count and out. It does not normalize, finish
 * or publish to a cache. Unknown/foreign tokens are INVALID_ARGUMENT. Discard
 * tokens after successful cancel/retain; never use a stale token again. */
mc_world_storage_status mc_world_store_cancel_reservation(mc_world_store *store,
                                                          mc_world_store_reservation *reservation,
                                                          mc_world_storage_error *error);
mc_world_storage_status mc_world_store_retain_reserved(mc_world_store *store,
                                                       mc_world_store_reservation *reservation,
                                                       const mc_chunk_state *chunk,
                                                       mc_chunk_state **out,
                                                       mc_world_storage_error *error);
size_t mc_world_store_reservation_count(const mc_world_store *store);

/* Empty-runtime serializer seam. Arguments/accounting match write_chunk.
 * After ownership/metadata admission, check the session and refuse nonempty
 * retained Entities/TileEntities before changing has_entities. A known-empty
 * runtime resets has_entities=false before serialization/transport; this reset
 * remains visible on a later failure. Growth/output remain unchanged on failure,
 * and dirty/last_save_time remain caller-owned. No entity/tile implementation is
 * supplied. Generic write_chunk retains its unchanged opaque transport policy. */
mc_world_storage_status mc_world_store_write_runtime_chunk(mc_world_store *store,
                                                           mc_chunk_state *chunk,
                                                           int64_t world_time, uint32_t timestamp,
                                                           uint32_t *growth,
                                                           mc_world_storage_error *error);

/* Real acquisition loader adapter. Keep this caller-owned binding stable during
 * a call. Missing records alone permit generation; unreadable/unsupported data
 * is fatal to prevent replacement by generated chunks. Activation also refuses
 * nonempty opaque entity/tile lists because their world objects are not yet
 * implemented. The generic storage load above still permits safe preservation.
 * last_error retains diagnostics. The coordinator alone stamps loaded time and
 * publishes/activates the identity after a successful callback. */
typedef struct mc_world_store_loader {
    mc_world_store *store;
    int32_t dimension;
    mc_world_storage_error last_error;
} mc_world_store_loader;

mc_chunk_load_result mc_world_store_load_callback(void *loader,
                                                  const mc_chunk_acquire_context *context,
                                                  int32_t x, int32_t z, mc_chunk_state **out);

/* Retire every external cache/list/state/view/token reference first. Free also
 * releases registry links and outstanding reservation tokens/links. No implicit
 * save, session-lock deletion or shared-region-cache clearing occurs. NULL is
 * valid. Explicitly clear/free regions only after every borrowing store stops. */
void mc_world_store_free(mc_world_store *store);

#endif
