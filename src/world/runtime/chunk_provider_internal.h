#ifndef MC_WORLD_RUNTIME_CHUNK_PROVIDER_INTERNAL_H
#define MC_WORLD_RUNTIME_CHUNK_PROVIDER_INTERNAL_H

#include "mc/core/java/int_set.h"
#include "mc/world/biome/sampling.h"
#include "mc/world/cache/acquire.h"
#include "mc/world/dimension/temperature.h"
#include "mc/world/dimension/world_provider.h"
#include "mc/world/generation/overworld.h"
#include "mc/world/runtime/runtime.h"

#define MC_CHUNK_PROVIDER_GRID_WIDTH 32u

/* Actual nullable source fields over the provider's fixed native reservation.
 * A zero span is an unpublished Java array, not the reserved C backing. r/s/t
 * are constructor-published; q/d/e/f/g/h publish only at their reached stores.
 * v borrows actual Registry-arena references, never numerical biome IDs. The
 * provider separately retains its current supplier; each entered generation
 * also holds its captured supplier across reentry. These are native lifetime
 * holds, not source fields or owning copies of reference arrays. */
typedef struct mc_chunk_provider_generation_fields {
    mc_biome_reference_array *v;
    mc_terrain_double_span q, d, e, f, g, h, r, s, t;
} mc_chunk_provider_generation_fields;

/* Borrowed views of this concrete normal-provider/cache owner. These are the
 * actual mutable descriptors and the runtime's ONE embedded fallback identity,
 * not copies of owning state. constructor_grid is a retained native 32x32 zero
 * storage projection, not Java row-object allocation/aliasing equivalence. */
typedef struct mc_chunk_provider_fields {
    mc_world_runtime *world;
    mc_chunk_acquire_policy policy;
    bool *force_load;
    mc_world_store *store;
    mc_world_chunk_manager *biome;
    mc_biome_manager *manager;
    mc_overworld_generator *generator;
    const mc_chunk_provider_generation_fields *generation;
    /* Borrowed actual field/ledger; retain arrays across replacement/reentry. */
    mc_temperature_array *w;
    mc_temperature_owner *temperature_allocations;
    mc_temperature_limits temperature_limits;
    mc_chunk_cache *cache;
    mc_chunk_pending *pending;
    mc_chunk_state *fallback;
    int32_t (*constructor_grid)[MC_CHUNK_PROVIDER_GRID_WIDTH];
    size_t max_chunks, direct_allocation_bytes, max_direct_allocation_bytes;
    mc_java_int_set_limits pending_history_limits;
} mc_chunk_provider_fields;

/* Construct the concrete Overworld provider and its enclosing empty base-cache
 * storage. The caller has already asked the real store for its loader, as the
 * original outer World.b does BEFORE the Dimension provider factory. Require
 * the same live CONSTRUCTING game runtime, its nonnull actual fields.w/x/t,
 * a successfully bound normal Dimension whose a is that runtime, and the exact
 * class trig/profile bindings. Other actual Dimension kinds stop unsupported;
 * they are never replaced by Overworld generation. No file/loader/registry,
 * population, saving, user callback or implicit World construction occurs.
 *
 * Read the current Dimension.a World seed at the factory argument position,
 * then call the real private normal constructor ONCE. Its retained cave owns
 * the actual shared-profile default Random BEFORE seeded terrain/noise. Never
 * use numerical init, copy/reseed/erase the cave or discard a successful
 * generator. Retain the Dimension's actual biome owner separately, so later
 * Dimension rebind cannot invalidate this generator's manager or buffers.
 *
 * Direct bytes count the provider (including generator/fixed scratch/grid),
 * pending keys, cache entries/list, blank data and its 32768 blocks. Exclude
 * allocator overhead, stack, borrowed World/store/trig/profile, retained biome
 * storage/buffers, independent temperature owner/arrays, and pending HashSet.
 * The latter
 * keeps existing default 8MiB retained/16MiB peak limits, with max_entries clipped
 * to min(max_chunks,65536). Zero limits are valid; max_chunks > INT32_MAX is an
 * invalid native bound. No growth/reallocation is hidden in these arrays.
 * Temperature defaults separately bound direct owner/handles to 1MiB and actual
 * Biome buffer requests to 64MiB. Live old/new aliases count simultaneously.
 * These are not a combined World/controller/Java heap or stack bound.
 * Fixed C reservation and bounded collection allocations are native policy,
 * not the original Java per-array/row/object OOM/failure sequence.
 *
 * Failure preserves *out and the runtime fallback. profile_cause is required
 * and changes only if the private generator actually enters its constructor,
 * receiving that exact profile result even when later native work fails.
 * An unpublished failed child is explicitly retired; completed shared-profile
 * effects are NOT rolled back. The enclosing World must latch its failed
 * attempt and cannot silently retry on the same World identity. Success
 * constructs &runtime->fallback in place, owning its data without publishing
 * World.v or binding contexts. Caller performs those later stores/operations.
 * Results/errors/inputs and all owners must be disjoint and exclusive. */
mc_world_storage_status mc_chunk_provider_create(mc_world_runtime *runtime,
                                                 mc_world_provider *dimension,
                                                 const mc_math_trig *trig,
                                                 mc_java_random_profile *profile, size_t max_chunks,
                                                 size_t max_direct_bytes, mc_chunk_provider **out,
                                                 mc_java_random_profile_status *profile_cause,
                                                 mc_world_storage_error *error);

/* No World retain/cycle or external provider retain is added. World/store and
 * the shared profile/trig must outlive this owner. Before free, retire all
 * entered calls, external views/leases/fallback pointers, cache registrations
 * and nonempty cache/list/pending uses; chunk identities remain store-owned.
 * Free NULL is safe. Release generator buffers before its supplying biome hold.
 * Clear runtime->fallback.data only when it still points to this owner's data;
 * never replace/copy/free the runtime's embedded chunk identity. */
void mc_chunk_provider_free(mc_chunk_provider *provider);

mc_world_storage_status mc_chunk_provider_bind_fields(mc_chunk_provider *provider,
                                                      mc_chunk_provider_fields *out,
                                                      mc_world_storage_error *error);

/* Actual current World.v presence receiver, with only inert native cache
 * metadata admission. No border, load, generation, pending removal, callbacks
 * or future-payload preflight. Caller holds the World through the complete
 * enclosing source operation and attributes this provider call at bytecode6.
 * Failure preserves out and returns its cause without changing World errors. */
mc_world_storage_status mc_world_runtime_chunk_provider_contains(mc_world_runtime *runtime,
                                                                 int32_t chunk_x, int32_t chunk_z,
                                                                 bool *out,
                                                                 mc_world_storage_error *error);

/* Actual normal yf/pk a()Z body: literal false, without World/loader callbacks.
 * This is only the underlying Overworld provider result, not enclosing cache
 * unload/save/population or a claim that those descendants are implemented.
 * Invalid arguments preserve out; valid calls write false. */
mc_world_storage_status mc_chunk_provider_maintenance(mc_chunk_provider *provider, bool *out,
                                                      mc_world_storage_error *error);

#endif
