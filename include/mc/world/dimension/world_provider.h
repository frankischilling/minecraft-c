#ifndef MC_WORLD_PROVIDER_DIMENSION_H
#define MC_WORLD_PROVIDER_DIMENSION_H

#include "mc/block/registry/registry.h"
#include "mc/world/biome/biome.h"
#include "mc/world/storage/status.h"

#include <stdbool.h>

typedef struct mc_world_runtime mc_world_runtime;
typedef struct mc_world_provider mc_world_provider;
typedef struct mc_world_chunk_manager mc_world_chunk_manager;

typedef enum mc_world_provider_kind {
    MC_WORLD_PROVIDER_OVERWORLD = 0,
    MC_WORLD_PROVIDER_NETHER = -1,
    MC_WORLD_PROVIDER_SKY = 1
} mc_world_provider_kind;

typedef enum mc_world_provider_phase {
    MC_WORLD_PROVIDER_RAW = 0,
    MC_WORLD_PROVIDER_CONSTRUCTING,
    MC_WORLD_PROVIDER_READY,
    MC_WORLD_PROVIDER_FAILED_CONSTRUCTION
} mc_world_provider_phase;

typedef enum mc_world_provider_status {
    MC_WORLD_PROVIDER_OK = 0,
    MC_WORLD_PROVIDER_INVALID_ARGUMENT,
    MC_WORLD_PROVIDER_INVALID_PHASE,
    MC_WORLD_PROVIDER_LIMIT,
    MC_WORLD_PROVIDER_ALLOCATION_FAILED,
    MC_WORLD_PROVIDER_WORLD_FAILED,
    MC_WORLD_CHUNK_MANAGER_FAILED,
    MC_WORLD_PROVIDER_UNSUPPORTED_FP,
    MC_WORLD_PROVIDER_REGISTRY_FAILED
} mc_world_provider_status;

typedef struct mc_world_provider_error {
    mc_world_provider_status status;
    const char *operation;
    unsigned int bytecode;
    mc_world_storage_error world;
    mc_biome_status biome;
    mc_registry_error registry;
} mc_world_provider_error;

typedef struct mc_world_provider_limits {
    size_t max_direct_allocation_bytes;
    size_t max_biome_buffer_bytes;
} mc_world_provider_limits;

/* The eight actual base fields. References/views are borrowed, not detached
 * final arrays. RAW/failed construction can have null f/h. Each successful
 * construction owns separate stable float[16] and float[4] identities. */
typedef struct mc_world_provider_fields {
    mc_world_runtime *a;
    mc_world_chunk_manager *b;
    bool c, d, e;
    const float *f;
    int32_t g;
    const float *h;
} mc_world_provider_fields;

typedef struct mc_world_provider_state {
    mc_world_provider_kind kind;
    mc_world_provider_phase phase;
    unsigned int constructor_pc, bind_pc, setup_pc;
    size_t holds, direct_allocation_bytes;
} mc_world_provider_state;

/* Fixed e/f/g hide the noise-backed superclass's same-label noise fields. A fixed
 * owner has null superclass noise and initially null a/b/c/d arrays. noise_backed is
 * NULL for fixed owners. These are the retained numerical manager/projection
 * descriptors, not copies that can be used as new owning descriptors. fixed_e
 * and d are numerical projections; biome_runtime.h exposes actual GAME e/d
 * references separately, without reconstructing them from these encodings. */
typedef struct mc_world_chunk_manager_fields {
    bool fixed;
    mc_biome_manager *noise_backed;
    const mc_biome_double_buffer *a, *b, *c;
    const mc_biome_buffer *d;
    mc_biome_id fixed_e;
    double fixed_f, fixed_g;
    size_t holds;
} mc_world_chunk_manager_fields;

const char *mc_world_provider_status_string(mc_world_provider_status status);
mc_world_provider_limits mc_world_provider_default_limits(void);

/* Allocate RAW with one hold, without game construction, entropy or I/O.
 * NULL limits selects 4MiB of direct requests and the existing 64MiB per-manager
 * buffer budget. Direct accounting includes the shared ledger, owner,
 * both float arrays and all live current/retired biome-owner requests, including
 * old+new peak during replacement. Allocator overhead, C stack, retained World
 * and separately bounded numerical biome buffers are excluded. Zero limits are
 * valid. Failure preserves out. No profile is created, reset or borrowed here. */
mc_world_provider_status mc_world_provider_allocate(mc_world_provider_kind kind,
                                                    const mc_world_provider_limits *limits,
                                                    mc_world_provider **out,
                                                    mc_world_provider_error *error);

/* Construct once in RAW. Source order is c/d/e=false, fresh f[16], g=0,
 * fresh h[4]. Subtype flags/ID and biome setup occur only during bind below.
 * A reached failure latches FAILED_CONSTRUCTION and preserves its owned prefix;
 * no retry/rollback occurs. No World, registry, provider or clock is touched. */
mc_world_provider_status mc_world_provider_construct(mc_world_provider *raw,
                                                     mc_world_provider_error *error);

/* Actual factory IDs: -1 Nether, 0 Overworld, 1 Sky. Other IDs succeed with
 * *out=NULL and no allocation. Known IDs perform allocate then construct;
 * native failure preserves out and retires only the unpublished owner. */
mc_world_provider_status mc_world_provider_create(int32_t dimension_id,
                                                  const mc_world_provider_limits *limits,
                                                  mc_world_provider **out,
                                                  mc_world_provider_error *error);

/* READY-only final bind: publish the same World at PC2, run concrete biome
 * setup, then fill the same light array. Retain a new nonnull World before
 * publication and release only this field's preceding binding afterward.
 * Same-World assignment does not add/drop a hold. A later failure retains the
 * published World and the old biome/other reached fields; bind may be called
 * again. NULL is published too: noise-backed setup then fails at its seed read,
 * while fixed setup needs no World. A successful new setup retires the old
 * biome only after publishing its replacement. External biome holds survive.
 *
 * FP admission precedes any bind mutation. Numeric construction requires the
 * java.h strict x86-64 environment; no FP controls are changed. noise-backed setup
 * uses real runtime metadata and existing seeded biome math. Fixed setup uses
 * the numerical Hell/Sky identities and actual constant fields; original
 * global biome-registry/bootstrap qualification remains separate. No fake
 * provider/top-block/registry callback or complete World capability is added. */
mc_world_provider_status mc_world_provider_bind(mc_world_provider *dimension,
                                                mc_world_runtime *world,
                                                mc_world_provider_error *error);

/* Native retirement only, separate from the game's bind(NULL): clear only a
 * and release exactly its preceding World binding. All allocated phases are
 * accepted; NULL/repeated calls are safe. Biome, flags, arrays and source PCs
 * remain unchanged. No setup, FP admission, callbacks or clock are reached. */
void mc_world_provider_unbind(mc_world_provider *dimension);

mc_world_provider_status mc_world_provider_retain(mc_world_provider *dimension);
void mc_world_provider_release(mc_world_provider *dimension);
mc_world_provider_status mc_world_provider_fields_get(const mc_world_provider *dimension,
                                                      mc_world_provider_fields *out);
mc_world_provider_status mc_world_provider_state_get(const mc_world_provider *dimension,
                                                     mc_world_provider_state *out);
mc_world_runtime *mc_world_provider_world(const mc_world_provider *dimension);
size_t mc_world_provider_hold_count(const mc_world_provider *dimension);

mc_world_provider_status mc_world_chunk_manager_retain(mc_world_chunk_manager *biome);
void mc_world_chunk_manager_release(mc_world_chunk_manager *biome);
mc_world_provider_status mc_world_chunk_manager_fields_get(const mc_world_chunk_manager *biome,
                                                           mc_world_chunk_manager_fields *out);

/* The actual a(long,float) scalar. READY required. Overworld accepts every
 * signed time and finite partial tick, with signed remainder and separately
 * rounded Java float/double operations. Nether returns .5f; Sky returns +0.0f
 * and both ignore the tick's value. Uses real libc double cos for Overworld,
 * distinct from the float lookup. New backend/original qualification is
 * pending; this is not a general nonfinite/fenv parity claim. FP admission and
 * failures preserve out. This operation reads no World and samples no clock. */
mc_world_provider_status mc_world_provider_celestial_angle(const mc_world_provider *dimension,
                                                           int64_t time, float partial_tick,
                                                           float *out,
                                                           mc_world_provider_error *error);

/* The actual virtual a(int,int) predicate. READY required. Use the World held
 * in a for the acquiring top-block query before any Block active use or missing
 * registry failure. Its immutable class binding supplies the same registry
 * through synchronous query reentry. Overworld compares the live F.bn; Nether
 * reads A.bn before rejecting air, then the actual o entry; Sky rejects air
 * without Block access, otherwise reads m[top].bA and calls material c().
 * Nullable Block/material and live array failures retain their reached source
 * points and full registry cause. The ordinary bool is separate from native
 * status; failure preserves out and all completed query/initialization effects.
 * No float admission, detached property table, new World or registry is added.
 * A native call guard protects the captured nonnull World through the entire
 * predicate, including synchronous registry resource callbacks. Its count
 * admission can fail before the query; this is not source class initialization.
 * Keep the owner and its dependencies live throughout the call. */
mc_world_provider_status mc_world_provider_can_spawn(mc_world_provider *dimension, int32_t x,
                                                     int32_t z, bool *out,
                                                     mc_world_provider_error *error);

/* All access is exclusive. Snapshots support all phases and retain nothing.
 * Hold overflow preserves counts. release(NULL) is safe; null query owners
 * reject except world/hold_count return NULL/0. Before replacing/releasing a
 * supplying owner, retain external biome aliases independently. The numerical
 * manager may be used for sampling through its existing API and separate buffer
 * budget; callers must not init/free it or replace its state, memory owner or
 * descriptors. It must stay held through all sampling/generator uses. External
 * output buffers must be freed before their supplying biome hold is released.
 * Runtime free remains BUSY until this exact World field is released. External
 * dimensions can prolong it; World cycle-aware retirement must inspect the actual World
 * and hold count, never clear the runtime's aggregate binding counter.
 * Results/errors/limits and all owner/dependency storage must be disjoint.
 * No stale-pointer detection or implicit synchronization is supplied. */

#endif
