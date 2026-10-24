#ifndef MC_WORLD_CACHE_LIFECYCLE_H
#define MC_WORLD_CACHE_LIFECYCLE_H

#include "mc/world/cache/acquire.h"
#include "mc/world/chunk/references.h"

typedef enum mc_chunk_lifecycle_status {
    MC_CHUNK_LIFECYCLE_OK = 0,
    MC_CHUNK_LIFECYCLE_INVALID_ARGUMENT,
    MC_CHUNK_LIFECYCLE_LIMIT,
    MC_CHUNK_LIFECYCLE_CALLBACK_FAILED,
    MC_CHUNK_LIFECYCLE_STATE_ERROR
} mc_chunk_lifecycle_status;

/* Explicit original exception boundaries, not a C exception mechanism. Main
 * save catches IO_ERROR only; extra save catches IO_ERROR and EXCEPTION. Flush,
 * detachment and maintenance do not catch them. FATAL always escapes. A callback
 * adapter owns diagnostics and must report its actual outcome. In particular,
 * an adapter choosing safer fatal storage errors must not label them caught. */
typedef enum mc_chunk_callback_result {
    MC_CHUNK_CALLBACK_OK = 0,
    MC_CHUNK_CALLBACK_IO_ERROR,
    MC_CHUNK_CALLBACK_EXCEPTION,
    MC_CHUNK_CALLBACK_FATAL
} mc_chunk_callback_result;

typedef struct mc_chunk_lifecycle_context mc_chunk_lifecycle_context;
typedef mc_chunk_callback_result (*mc_chunk_save_callback)(
    void *loader, const mc_chunk_lifecycle_context *context, mc_chunk_state *chunk);
typedef mc_chunk_callback_result (*mc_chunk_loader_callback)(
    void *loader, const mc_chunk_lifecycle_context *context);
typedef mc_chunk_callback_result (*mc_chunk_detach_tile_entities_callback)(
    void *world, const mc_chunk_lifecycle_context *context, mc_chunk_state *chunk);
typedef mc_chunk_callback_result (*mc_chunk_defer_entities_callback)(
    void *world, const mc_chunk_lifecycle_context *context, mc_chunk_state *chunk, size_t slice);
typedef mc_chunk_callback_result (*mc_chunk_maintenance_callback)(
    void *provider, const mc_chunk_lifecycle_context *context, bool *result);

/* Borrowed synchronous bindings. Keep this descriptor, its bindings, policy and
 * optional references descriptor unchanged through nested calls. Callback owners
 * may be NULL. Cache/list/pending contents and the clock may change in callbacks;
 * they must remain valid and all retained identities must stay live. No graph is
 * copied or rolled back. Output storage must be disjoint from all runtime state.
 * Exclusive access is required except for synchronous callback reentry.
 *
 * The four loader callbacks are either all present or all absent. Missing loader
 * skips its work and does not stamp last_save_time. Detach callbacks are explicit
 * even for empty normal collections. Maintenance requires provider_maintenance;
 * the original unconditional provider call has no safe absent-provider result.
 * Dedicated policy borrows saving_disabled; base policy ignores it. Saving-off
 * gates world saving and dedicated unload/loader maintenance, not direct save.
 *
 * references optionally accounts for each retained local across callbacks. With
 * NULL, lifetime preservation is entirely the caller's precondition. Neither
 * mode frees chunks, arrays, pending history, registrations or callback owners. */
struct mc_chunk_lifecycle_context {
    mc_chunk_cache *cache;
    mc_chunk_pending *pending;
    const int64_t *world_time;
    const bool *saving_disabled;
    mc_chunk_acquire_policy policy;
    void *world;
    void *loader;
    void *provider;
    mc_chunk_save_callback save_main;
    mc_chunk_save_callback save_extra;
    mc_chunk_loader_callback flush;
    mc_chunk_loader_callback loader_maintenance;
    mc_chunk_detach_tile_entities_callback invalidate_tile_entities;
    mc_chunk_defer_entities_callback defer_entities;
    mc_chunk_maintenance_callback provider_maintenance;
    const mc_chunk_references *references;
};

const char *mc_chunk_lifecycle_status_string(mc_chunk_lifecycle_status status);
bool mc_chunk_lifecycle_validate(const mc_chunk_lifecycle_context *context);
bool mc_chunk_lifecycle_saving_enabled(const mc_chunk_lifecycle_context *context);

/* NULL/blank/never-save is false. Normal force: dirty || (has_entities &&
 * now != last_save_time). Incremental uses signed now >= wrap(last_save_time+
 * 600), rather than subtraction. Active/populated do not gate eligibility. */
bool mc_chunk_save_eligible(const mc_chunk_state *chunk, int64_t now, bool force);

/* Live indexed insertion-list traversal. Forced !never_save extra precedes
 * eligibility. With a loader, main stamps the current clock before its callback;
 * a returning main wrapper clears dirty and counts an eligible attempt, even
 * after caught I/O or with no loader. Incremental attempt 24 returns completed=
 * false immediately, including at the last list entry. A complete forced pass
 * calls flush. No other pass reports progress or restarts automatically.
 *
 * Errors preserve *completed and all completed effects. A later resource limit
 * does not roll back earlier saves/callbacks. No chunk identity is retired. */
mc_chunk_lifecycle_status mc_chunk_lifecycle_save(const mc_chunk_lifecycle_context *context,
                                                  bool force, bool *completed);

/* Direct chunk detachment, including identities outside the cache. Requires a
 * valid context/chunk and both detach callbacks. Normal chunks set active=false
 * before invalidating TileEntities, then defer all eight entity slices in order, even
 * when already inactive. Blank chunks have a virtual no-op body. References,
 * when supplied, retain the selected identity once and release it on every
 * return after a successful retain. Retain failure returns LIMIT before effects.
 *
 * Callback failures preserve earlier effects. This entry does not save,
 * unpublish, alter pending/map/list membership or gate on saving_disabled; it
 * invokes no loader/provider callbacks. Callback effects remain caller-owned.
 * Raw views and deferred registrations still require separate lifetime
 * accounting under the context's borrowed-storage contract. */
mc_chunk_lifecycle_status mc_chunk_lifecycle_detach(const mc_chunk_lifecycle_context *context,
                                                    mc_chunk_state *chunk);

/* Ordered pending mode only. Up to 100 iterations each select a fresh first key,
 * retain its mapped identity, detach normal active=false/TileEntities/eight deferred
 * slices, then main/extra, remove selected pending key, remove selected map key
 * unconditionally, and remove only the first same-identity list occurrence.
 * No eligibility or dirty reset occurs. Missing selected map value is a bounded
 * STATE_ERROR, replacing the original null dereference. Earlier effects survive.
 * Dedicated saving-off skips that loop and loader maintenance; provider still
 * runs. The returned provider Boolean is independent of pending emptiness.
 * Errors preserve *provider_result. Normal detachment never clears TileEntity/entity
 * collections; deferred registrations need their own lifetime accounting. */
mc_chunk_lifecycle_status mc_chunk_lifecycle_maintain(const mc_chunk_lifecycle_context *context,
                                                      bool *provider_result);

typedef enum mc_world_save_stage {
    MC_WORLD_SAVE_LEVEL = 0,
    MC_WORLD_SAVE_CHUNKS
} mc_world_save_stage;

typedef struct mc_world_save_context mc_world_save_context;
typedef mc_chunk_callback_result (*mc_world_save_callback)(void *world,
                                                           const mc_world_save_context *context);
typedef mc_chunk_callback_result (*mc_world_save_progress_callback)(
    void *progress, const mc_world_save_context *context, mc_world_save_stage stage);

/* Stable borrowed world-save bindings, with the same lifetime/reentry and
 * disjoint-output rules as chunks. All three metadata callbacks are required:
 * check_session, save_metadata, save_auxiliary run in that order. Explicit empty
 * auxiliary implementations are permitted. They receive world unchanged.
 * progress is optional and receives its own opaque owner. No exception boundary
 * catches callback failures here. Metadata views may expire after save_metadata;
 * adapters must obtain fresh live views before every later use. world_time must
 * therefore point to a stable live clock, never into an expiring metadata view. */
struct mc_world_save_context {
    const mc_chunk_lifecycle_context *chunks;
    void *world;
    void *progress;
    mc_world_save_callback check_session;
    mc_world_save_callback save_metadata;
    mc_world_save_callback save_auxiliary;
    mc_world_save_progress_callback report_progress;
};

bool mc_world_save_validate(const mc_world_save_context *context);

/* Original outer world route: gate save-enabled, optional LEVEL progress,
 * session/metadata/auxiliary, optional CHUNKS progress, then chunk save. Discard
 * its completed Boolean, preserving the original outer void/cutoff distinction.
 * Earlier metadata/chunk effects remain after any later error. */
mc_chunk_lifecycle_status mc_world_lifecycle_save(const mc_world_save_context *context, bool force);

/* Client-only incremental route: disabled returns completed=true; pass==0 runs
 * session/metadata/auxiliary first, other signed pass values skip it. Then expose
 * the incremental chunk-pass Boolean directly. No progress is reported. Errors
 * preserve *completed. Dedicated policy is rejected: no matching server entry
 * has been established. This function does not implement the world tick. */
mc_chunk_lifecycle_status mc_world_lifecycle_save_client(const mc_world_save_context *context,
                                                         int32_t pass, bool *completed);

#endif
