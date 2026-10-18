#ifndef MC_WORLD_CACHE_ACQUIRE_H
#define MC_WORLD_CACHE_ACQUIRE_H

#include "mc/core/java/int_set.h"
#include "mc/world/cache/cache.h"
#include "mc/world/chunk/references.h"

typedef enum mc_chunk_acquire_status {
    MC_CHUNK_ACQUIRE_OK = 0,
    MC_CHUNK_ACQUIRE_INVALID_ARGUMENT,
    MC_CHUNK_ACQUIRE_LIMIT,
    MC_CHUNK_ACQUIRE_PARTIAL_LIMIT,
    MC_CHUNK_ACQUIRE_CALLBACK_FAILED,
    MC_CHUNK_ACQUIRE_STATE_ERROR,
    MC_CHUNK_ACQUIRE_EMPTY
} mc_chunk_acquire_status;

const char *mc_chunk_acquire_status_string(mc_chunk_acquire_status status);

/* Caller-owned unique packed keys. The array retains insertion-survivor order,
 * independently of optional JDK-compatible selection history. Legacy mode has
 * NULL history and supplies membership only. Ordered mode borrows one exclusive
 * history owner, which must outlive this descriptor and every active callback.
 * Do not share that history with another descriptor or mutate it separately.
 * No operation here frees the history owner.
 * Imported descriptors must pass validate before use. */
typedef struct mc_chunk_pending {
    int32_t *keys;
    size_t capacity, count;
    mc_java_int_set *history;
} mc_chunk_pending;

/* Failure preserves the descriptor and every supplied input. Legacy init starts
 * empty with NULL history. Ordered init requires an empty nonnull history and
 * preserves its existing logical capacity; it does not recreate a fresh set. */
bool mc_chunk_pending_init(mc_chunk_pending *pending, int32_t *keys, size_t capacity);
bool mc_chunk_pending_init_ordered(mc_chunk_pending *pending, int32_t *keys, size_t capacity,
                                   mc_java_int_set *history);
bool mc_chunk_pending_validate(const mc_chunk_pending *pending);
/* Requires a validated set. Invalid metadata returns false. */
bool mc_chunk_pending_contains(const mc_chunk_pending *pending, int32_t key);

/* All ordered-mode changes, including callback changes, must use these
 * operations. Direct edits are outside this contract even when validation still
 * matches current membership. Validation cannot reconstruct past mutations;
 * restoring membership from a snapshot cannot restore history. Restore ordered
 * state by replaying its complete history from the appropriate initial state.
 * Arguments/results must be disjoint from pending, its keys and history.
 *
 * changed is required. Duplicate add/absent remove succeed with false. Invalid
 * metadata/pairing returns INVALID_ARGUMENT. Capacity, history-budget and
 * history-allocation failures return LIMIT. Every failure preserves both
 * representations and outputs. Removal allocates nothing and preserves array
 * survivor order; unused array bytes are not cleared. */
mc_chunk_acquire_status mc_chunk_pending_add(mc_chunk_pending *pending, int32_t key, bool *changed);
mc_chunk_acquire_status mc_chunk_pending_remove(mc_chunk_pending *pending, int32_t key,
                                                bool *changed);
/* Ordered mode only. EMPTY preserves *key; legacy mode is INVALID_ARGUMENT.
 * Each call observes a fresh first value without consuming an iterator. */
mc_chunk_acquire_status mc_chunk_pending_first(const mc_chunk_pending *pending, int32_t *key);
/* Retains the array bytes and history's logical capacity, setting count to zero.
 * Invalid metadata/pairing preserves all state. No history owner is freed. */
mc_chunk_acquire_status mc_chunk_pending_clear(mc_chunk_pending *pending);

typedef enum mc_chunk_acquire_policy {
    MC_CHUNK_ACQUIRE_BASE = 0,
    MC_CHUNK_ACQUIRE_DEDICATED
} mc_chunk_acquire_policy;

typedef enum mc_chunk_load_result {
    MC_CHUNK_LOAD_LOADED = 0,
    MC_CHUNK_LOAD_MISS,
    MC_CHUNK_LOAD_CAUGHT_ERROR,
    MC_CHUNK_LOAD_FATAL
} mc_chunk_load_result;

typedef struct mc_chunk_acquire_context mc_chunk_acquire_context;

/* Callback owners are opaque borrowed values and may be NULL. Only LOADED may
 * supply a result: it must be a live nonnull valid identity. MISS and the
 * original caught-Exception boundary continue to generation/fallback; the
 * loader owns any caught-error diagnostic. FATAL stops acquisition. */
typedef mc_chunk_load_result (*mc_chunk_load_callback)(void *loader,
                                                       const mc_chunk_acquire_context *context,
                                                       int32_t x, int32_t z,
                                                       mc_chunk_state **loaded);
/* Success requires a complete finished nonnull stable result. Returning NULL
 * on success is a defensive STATE_ERROR, not fallback or full original failure
 * parity: this coordinator deliberately never publishes NULL map values. */
typedef bool (*mc_chunk_generate_callback)(void *provider, const mc_chunk_acquire_context *context,
                                           int32_t x, int32_t z, mc_chunk_state **generated);
typedef bool (*mc_chunk_register_tiles_callback)(void *world,
                                                 const mc_chunk_acquire_context *context,
                                                 mc_chunk_state *chunk);
typedef bool (*mc_chunk_register_entities_callback)(void *world,
                                                    const mc_chunk_acquire_context *context,
                                                    mc_chunk_state *chunk, size_t slice);
/* cache_provider is the same coordinator identity that triggered population,
 * matching the original acquisition caller's provider argument. */
typedef bool (*mc_chunk_populate_callback)(void *provider,
                                           const mc_chunk_acquire_context *cache_provider,
                                           int32_t x, int32_t z);

/* Stable caller-owned synchronous coordinator. It owns none of its inputs and
 * never allocates/frees cache or chunk storage. Ordered pending changes may
 * allocate/free nodes through the borrowed history owner and its budgets.
 * Keep this descriptor and callback bindings unchanged while a call or
 * recursive call is active. Borrowed cache/pending descriptors, identities,
 * payloads and fallback must remain live and disjoint; replacement never frees
 * an older identity retained in the insertion list. Callbacks may mutate live
 * cache/pending/world state and recursively acquire, but must return valid
 * descriptors. Callers must supply capacity for nested operations themselves.
 * General storage disjointness is a caller precondition, without pointer-range
 * ordering checks. Result-pointer output storage must also be disjoint from
 * runtime state and callback writes. Exclusive access is required outside
 * synchronous callbacks.
 *
 * generate and populate are either both present (a provider) or both absent.
 * load is independently optional. Both registration callbacks are explicit and
 * required, including empty collections. world_time is sampled after a loaded
 * callback returns, never for generated/fallback results. Dedicated policy also
 * borrows live world/provider force flags and spawn coordinates. Base policy
 * ignores those four pointers and permits NULL for them. Opaque owners are
 * passed unchanged, and need not be nonnull when their callbacks need no owner.
 * Chunk never_save/has_entities and all other flags/time remain unchanged except
 * active, terrain_populated, dirty and loaded last_save_time specified below. */
struct mc_chunk_acquire_context {
    mc_chunk_cache *cache;
    mc_chunk_pending *pending;
    mc_chunk_state *fallback;
    const int64_t *world_time;
    const bool *world_force_load;
    const bool *provider_force_load;
    const int32_t *spawn_x;
    const int32_t *spawn_z;
    mc_chunk_acquire_policy policy;
    void *world;
    void *loader;
    void *provider;
    mc_chunk_load_callback load;
    mc_chunk_generate_callback generate;
    mc_chunk_register_tiles_callback register_tiles;
    mc_chunk_register_entities_callback register_entities;
    mc_chunk_populate_callback populate;
    /* Optional lease accounting for loaded/generated/current population locals
     * across callbacks. Both hooks must be present and their descriptor must
     * remain stable. A failed lease after acquisition began is PARTIAL_LIMIT.
     * NULL leaves lifetime preservation entirely to the caller. Returned raw
     * state pointers remain borrowed after the balanced local lease ends. */
    const mc_chunk_references *references;
};

bool mc_chunk_acquire_validate(const mc_chunk_acquire_context *context);

/* Original c entry: remove pending even on a hit. On a miss, load then optional
 * generate/fallback; publish the original requested key unconditionally after
 * callbacks, append the same retained result, activate, then evaluate all four
 * ordered population gates against live state. There is no placeholder or
 * post-callback replacement of the retained outer result by a lookup result.
 * Normal activation sets active before tiles, then calls all eight slices in
 * order. Blank activation and dirty marking are no-ops. The first normal
 * pre-activation method is already a no-op; it adds no callback here.
 *
 * INVALID_ARGUMENT and preflight LIMIT preserve all state. A known miss reserves
 * one map slot and one list slot before pending removal. After callbacks,
 * metadata/capacity is checked again; PARTIAL_LIMIT preserves their completed
 * effects and pending removal. CALLBACK_FAILED/STATE_ERROR may also leave
 * visible partial state. No rollback occurs. Published objects stay live after
 * later activation/population failure. *out is assigned only on OK. */
mc_chunk_acquire_status mc_chunk_acquire(const mc_chunk_acquire_context *context, int32_t x,
                                         int32_t z, mc_chunk_state **out);

/* Original b entry: nonnull hit preserves pending. A base miss always acquires;
 * a dedicated miss acquires only when live world_force_load or (otherwise)
 * provider_force_load is true. A denied miss returns fallback without pending
 * removal, publication, callbacks or activation. */
mc_chunk_acquire_status mc_chunk_acquire_get(const mc_chunk_acquire_context *context, int32_t x,
                                             int32_t z, mc_chunk_state **out);

/* Original cache population helper: resolve through b, return if populated,
 * otherwise set terrain_populated before invoking the optional provider. After
 * provider success mark only a normal target dirty. Provider absence is flag
 * only. Callback failure leaves the flag and all callback effects visible. */
mc_chunk_acquire_status mc_chunk_acquire_populate(const mc_chunk_acquire_context *context,
                                                  int32_t x, int32_t z);

/* Dedicated he.d producer only. Java-wrapped x*16+8-spawn_x / z*16+8-spawn_z
 * outside inclusive [-128,128] adds the packed key, irrespective of map presence.
 * An already-present key or an in-range request succeeds unchanged. No unload,
 * save, iteration order or pending map-membership requirement is implemented. */
mc_chunk_acquire_status mc_chunk_acquire_mark_drop(const mc_chunk_acquire_context *context,
                                                   int32_t x, int32_t z);

#endif
