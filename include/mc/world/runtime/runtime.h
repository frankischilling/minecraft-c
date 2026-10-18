#ifndef MC_WORLD_RUNTIME_RUNTIME_H
#define MC_WORLD_RUNTIME_RUNTIME_H

#include "mc/world/cache/lifecycle.h"
#include "mc/world/generation/normal.h"
#include "mc/world/storage/store.h"

typedef struct mc_world_runtime mc_world_runtime;

/* Stable mutable scalar state, separate from expiring metadata views. The
 * runtime initially copies these values from current metadata; force/save-off
 * flags start false. Edits require exclusive access or synchronous callbacks.
 * Every metadata save overlays time/spawn on a fresh store view, retaining live
 * SizeOnDisk and the loaded last_played field. No world tick is implemented. */
typedef struct mc_world_runtime_state {
    int64_t time;
    int32_t spawn_x, spawn_y, spawn_z;
    bool world_force_load, provider_force_load, saving_disabled;
} mc_world_runtime_state;

typedef uint32_t (*mc_world_region_clock)(void *owner);
typedef int64_t (*mc_world_metadata_clock)(void *owner);

/* Clock callbacks only sample and return a value. They must not reenter,
 * invalidate borrowed metadata or mutate runtime/store/dependency state. */

typedef struct mc_world_runtime_options {
    mc_world_store *store;
    mc_chunk_cache *cache;
    mc_chunk_pending *pending;
    int32_t dimension;
    mc_chunk_acquire_policy policy;
    mc_normal_generator *generator;
    const mc_normal_generation_resources *generation;
    void *provider;
    mc_chunk_populate_callback populate;
    mc_chunk_maintenance_callback provider_maintenance;
    void *auxiliary;
    mc_world_save_callback save_auxiliary;
    void *progress;
    mc_world_save_progress_callback report_progress;
    void *clock;
    mc_world_region_clock region_timestamp;
    mc_world_metadata_clock metadata_timestamp;
} mc_world_runtime_options;

/* Compose storage, acquisition, normal generation, save/unload and owner leases.
 * Borrow store, validated cache/ordered pending and all optional dependencies;
 * they must outlive this owner. cache/list/pending must initially be empty and
 * the store must already have metadata. The runtime registers its cache and its
 * internal blank fallback with the store, and owns stable context/clock storage.
 * No world files are touched by create. Failure preserves out and borrowed
 * state; an allocated registration is undone if construction cannot complete.
 *
 * Dimensions 0 and -1 are accepted. An optional initialized normal generator is
 * supported only for dimension0, with matching resources/cache/world seed and
 * explicit populate callback. Missing generation is load-only: missing records
 * stop instead of publishing a fabricated fallback. Actual feature population
 * remains the supplied callback; its failure retains coordinator partial effects.
 * provider_maintenance and save_auxiliary are explicit even for empty/no-op
 * implementations. Normal region-loader extra/flush/maintenance are native
 * no-ops, matching the represented loaders.
 *
 * Only known-empty entity/tile runtime chunks activate here. Opaque records are
 * refused by the existing loader. Native registration/detachment callbacks are
 * therefore empty, but the coordinator still invokes each required boundary.
 * Generic lifecycle.h supports separately observed external feature seams.
 *
 * All native storage failures escape as fatal callback outcomes with last_error;
 * this preserves the foundation's safer error policy instead of reproducing
 * swallowed original I/O loss. Earlier file/runtime effects are not rolled back.
 * Result pointers and borrowed storage must satisfy the underlying disjointness
 * and exclusive-access contracts. Keep options/dependencies bound for the owner's
 * lifetime; no arbitrary raw-pointer liveness detection is promised. */
mc_world_storage_status mc_world_runtime_create(const mc_world_runtime_options *options,
                                                mc_world_runtime **out,
                                                mc_world_storage_error *error);

mc_world_runtime_state *mc_world_runtime_live(mc_world_runtime *runtime);
/* Snapshot current world metadata: copy a fresh store view, then overlay stable
 * live time/spawn. SizeOnDisk and loaded last_played come from that current view.
 * Strings/Player remain borrowed until the next successful store load/save or
 * runtime/store destruction. Queries do not change last_error. Failure preserves
 * out. This is distinct from the store's last retained persistence view. */
mc_world_storage_status mc_world_runtime_read_info(const mc_world_runtime *runtime,
                                                   mc_level_info *out,
                                                   mc_world_storage_error *error);
/* Contexts are borrowed inspection views. Enter top-level operations through
 * the runtime functions below so owner destruction remains blocked across every
 * callback, including callbacks without a current chunk. Synchronous recursive
 * generic operations within such a callback retain the outer call protection. */
const mc_chunk_acquire_context *mc_world_runtime_acquisition(const mc_world_runtime *runtime);
const mc_chunk_lifecycle_context *mc_world_runtime_lifecycle(const mc_world_runtime *runtime);
const mc_world_save_context *mc_world_runtime_saving(const mc_world_runtime *runtime);
const mc_world_storage_error *mc_world_runtime_last_error(const mc_world_runtime *runtime);

mc_chunk_acquire_status mc_world_runtime_acquire(mc_world_runtime *runtime, int32_t x, int32_t z,
                                                 mc_chunk_state **out);
mc_chunk_acquire_status mc_world_runtime_get(mc_world_runtime *runtime, int32_t x, int32_t z,
                                             mc_chunk_state **out);
mc_chunk_acquire_status mc_world_runtime_populate(mc_world_runtime *runtime, int32_t x, int32_t z);
mc_chunk_acquire_status mc_world_runtime_mark_drop(mc_world_runtime *runtime, int32_t x, int32_t z);
mc_chunk_lifecycle_status mc_world_runtime_save_chunks(mc_world_runtime *runtime, bool force,
                                                       bool *completed);
mc_chunk_lifecycle_status mc_world_runtime_save(mc_world_runtime *runtime, bool force);
mc_chunk_lifecycle_status mc_world_runtime_save_client(mc_world_runtime *runtime, int32_t pass,
                                                       bool *completed);
mc_chunk_lifecycle_status mc_world_runtime_maintain(mc_world_runtime *runtime,
                                                    bool *provider_result);

/* Native generation reserves one owner/link slot before mutating generator,
 * manager or lighting state, creates unpublished staging data, then retains its
 * owned copy and returns only that stable identity. Reservation cannot reserve
 * all later payload allocations: retention failure after successful generation
 * leaves RNG/manager/queue effects visible, cancels the reservation, frees only
 * staging storage and reports failure. Loaded source NBT is never cloned through
 * this generated-record path. These rules apply to the acquisition adapter above.
 *
 * Raw returned contexts/views expire at free. Before free, retire their uses and
 * all external fallback pointers. Free returns BUSY while map/list/pending are
 * nonempty or an entered call/local lease remains active; it neither saves,
 * unpublishes nor frees retained store chunks. Empty
 * contexts are unregistered and this owner alone is freed. NULL is accepted. */
mc_world_storage_status mc_world_runtime_free(mc_world_runtime *runtime,
                                              mc_world_storage_error *error);

#endif
