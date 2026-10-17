#ifndef MC_WORLD_BIOME_BIOME_H
#define MC_WORLD_BIOME_BIOME_H

#include "mc/world/noise/simplex.h"

#include <stddef.h>
#include <stdint.h>

#define MC_BIOME_TABLE_ENTRIES 4096u
#define MC_BIOME_DEFAULT_ALLOCATION_BYTES (64u * 1024u * 1024u)

/* Project encodings for the original static identities a..m, not registry IDs.
 * NONE represents an unset array element, not an original biome identity. */
typedef enum mc_biome_id {
    MC_BIOME_NONE = 0,
    MC_BIOME_RAINFOREST,
    MC_BIOME_SWAMPLAND,
    MC_BIOME_SEASONAL_FOREST,
    MC_BIOME_FOREST,
    MC_BIOME_SAVANNA,
    MC_BIOME_SHRUBLAND,
    MC_BIOME_TAIGA,
    MC_BIOME_DESERT,
    MC_BIOME_PLAINS,
    MC_BIOME_ICE_DESERT,
    MC_BIOME_TUNDRA,
    MC_BIOME_HELL,
    MC_BIOME_SKY
} mc_biome_id;

typedef enum mc_biome_status {
    MC_BIOME_OK = 0,
    MC_BIOME_INVALID_ARGUMENT,
    MC_BIOME_LIMIT,
    MC_BIOME_ALLOCATION_FAILED
} mc_biome_status;

const char *mc_biome_status_string(mc_biome_status status);
const char *mc_biome_name(mc_biome_id biome); /* NULL for NONE/invalid values. */
char mc_biome_field(mc_biome_id biome);       /* '\0' for NONE/invalid values. */

typedef struct mc_biome_classifier {
    mc_biome_id table[MC_BIOME_TABLE_ENTRIES]; /* temperature + rainfall*64 */
} mc_biome_classifier;

/* Explicit table construction uses binary32 i/63.0f and the float selector.
 * The double selector truncates T*63 and R*63, then indexes this table.
 * Both selectors require finite unit-interval inputs and a nonnull result.
 * Lookup requires an initialized, unmodified table and disjoint result storage.
 * Failures preserve outputs. Exact results assume binary32/binary64
 * round-to-nearest on tested x86-64 targets. */
mc_biome_status mc_biome_classifier_init(mc_biome_classifier *classifier);
mc_biome_status mc_biome_classify_float(float temperature, float rainfall, mc_biome_id *out);
mc_biome_status mc_biome_lookup(const mc_biome_classifier *classifier, double temperature,
                                double rainfall, mc_biome_id *out);

/* Caller-owned allocation accounting, shared by a manager and its external
 * buffers. Zero-initialize before first use. init accepts a new limit only
 * with no live allocations. Keep this object alive until all buffers are freed.
 * Fields are read-only outside this API. The peak live limit includes module
 * allocation headers and old+new storage during growth, excluding system malloc
 * overhead and caller-owned manager/classifier/noise storage. Zero is valid. */
typedef struct mc_biome_memory {
    size_t max_allocation_bytes;
    size_t live_allocation_bytes;
} mc_biome_memory;

mc_biome_status mc_biome_memory_init(mc_biome_memory *memory, size_t max_allocation_bytes);

/* Zero-initialized descriptors are null arrays. A successful resize creates an
 * exact-length zeroed replacement when null or differently sized; an existing
 * exact-length array is retained. Allocated length zero has nonnull data and
 * distinct identity from a null descriptor. Lengths must fit INT32_MAX and the
 * byte span. Every allocation belongs to the supplied memory owner.
 * Descriptor fields are read-only; callers may inspect/prefill elements. Do not
 * operate on owning copies, use stale views after replacement, or overlap live
 * buffers. Failed calls preserve descriptors, contents and live accounting.
 * free accepts NULL and resets a descriptor to null. */
typedef struct mc_biome_double_buffer {
    double *data;
    size_t length;
} mc_biome_double_buffer;

typedef struct mc_biome_buffer {
    mc_biome_id *data;
    size_t length;
} mc_biome_buffer;

mc_biome_status mc_biome_double_buffer_resize(mc_biome_memory *memory,
                                              mc_biome_double_buffer *buffer, size_t length);
mc_biome_status mc_biome_buffer_resize(mc_biome_memory *memory, mc_biome_buffer *buffer,
                                       size_t length);
void mc_biome_double_buffer_free(mc_biome_double_buffer *buffer);
void mc_biome_buffer_free(mc_biome_buffer *buffer);

/* Zero-initialize before init. Reinitialization requires manager_free first.
 * The three channels use independent wrapped seed products 9871/39811/543321
 * and 4/4/2 simplex layers. State/table/descriptor fields are read-only; buffer
 * elements may be inspected/prefilled between calls. No retained world object.
 * a/b/c/d correspond to the original exposed reusable arrays: processed
 * temperature, processed rainfall, raw auxiliary noise, and cached biomes.
 * Calls recompute; these are reusable arrays rather than coordinate caches.
 * One owner per mutable manager; concurrent use requires synchronization. */
typedef struct mc_biome_manager {
    mc_noise_simplex temperature_layers[4];
    mc_noise_simplex rainfall_layers[4];
    mc_noise_simplex auxiliary_layers[2];
    mc_biome_classifier classifier;
    mc_biome_memory *memory;
    mc_biome_double_buffer a;
    mc_biome_double_buffer b;
    mc_biome_double_buffer c;
    mc_biome_buffer d;
    int initialized;
} mc_biome_manager;

mc_biome_status mc_biome_manager_init(mc_biome_manager *manager, int64_t seed,
                                      mc_biome_memory *memory);
/* Releases a/b/c/d only, not separately owned external outputs. Accepts NULL;
 * resets the manager to its zero-initialized state. Memory owner must be live. */
void mc_biome_manager_free(mc_biome_manager *manager);

/* Sampling requires initialized state. All dimensions and their product must
 * fit INT32_MAX; zero dimensions are valid. Null/short arrays grow to the exact
 * product and larger arrays retain their length/identity. Zero-size calls still
 * materialize null arrays as allocated empty arrays. All needed replacements
 * are staged before any mutation; failures preserve all buffers and state.
 * External outputs must be module-owned by manager->memory or null, and must
 * be disjoint from the manager, its buffers and scalar result storage.
 * No buffer aliases are accepted as an implicit alternate original operation. */

/* Rectangle supported. Writes external output and c, leaving a/b/d untouched.
 * Their full retained spans are cleared; only the requested prefix is blended. */
mc_biome_status mc_biome_temperatures(mc_biome_manager *manager, int32_t x, int32_t z, size_t width,
                                      size_t height, mc_biome_double_buffer *output);

/* Square domain only: the original rectangular biome method samples width
 * twice before consuming width*height. Direct output retains its biome suffix;
 * a/b/c are fully cleared then their requested prefix is sampled/processed.
 * The direct call leaves d untouched; cached_square uses and assigns d. */
mc_biome_status mc_biome_sample_square(mc_biome_manager *manager, int32_t x, int32_t z, size_t size,
                                       mc_biome_buffer *output);
mc_biome_status mc_biome_cached_square(mc_biome_manager *manager, int32_t x, int32_t z,
                                       size_t size);
mc_biome_status mc_biome_point(mc_biome_manager *manager, int32_t x, int32_t z, mc_biome_id *out);
mc_biome_status mc_biome_chunk(mc_biome_manager *manager, int32_t chunk_x, int32_t chunk_z,
                               mc_biome_id *out);

/* Client-only original xv.b(II)D: raw channel e, 1x1, frequency factor 0.5.
 * Reuses/clears a only; no processing/clamp and no server counterpart claimed. */
mc_biome_status mc_biome_client_raw_temperature(mc_biome_manager *manager, int32_t x, int32_t z,
                                                double *out);

#endif
