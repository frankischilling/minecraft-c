#ifndef MC_WORLD_CHUNK_DATA_H
#define MC_WORLD_CHUNK_DATA_H

#include "mc/world/chunk/nibbles.h"

/* Caller-owned voxel data with a borrowed mutable block array. This stores
 * the fixed normal shape; excess block capacity is retained storage only.
 * Keep data, blocks and any opacity table valid and disjoint. The caller
 * owns their lifetimes and grants exclusive access during mutation.
 */
typedef struct mc_chunk_data {
    uint8_t *blocks;
    size_t blocks_capacity;
    uint8_t metadata[MC_CHUNK_NIBBLE_BYTES];
    uint8_t skylight[MC_CHUNK_NIBBLE_BYTES];
    uint8_t block_light[MC_CHUNK_NIBBLE_BYTES];
    uint8_t heightmap[MC_CHUNK_HEIGHTMAP_BYTES];
    int32_t minimum_height;
} mc_chunk_data;

/* Rejects null data/blocks or short block capacity before mutation. Success
 * retains the borrowed pointer/capacity and every block byte, zeros the
 * three packed arrays and heightmap, and sets minimum_height to zero.
 * Performs no allocation or I/O.
 */
bool mc_chunk_data_init(mc_chunk_data *data, uint8_t *blocks, size_t blocks_capacity);

/* Local height/direct-skylight computation over initialized valid data.
 * The first 256 opacity entries must exist and be nonnegative int32_t
 * values; a table suffix is ignored. Null/short data or table, or a negative
 * entry in that prefix, returns false before any mutation.
 *
 * Heightmap order is z*16 + x. Height starts at 127 and tests Y126 down to
 * Y0; the stored result is 0..127. Direct sky starts at light 15/Y127,
 * subtracts the current opacity and writes only a positive result. Y0 is
 * never written. Existing skylight is retained at every skipped position,
 * including all positions when skylight_enabled is false.
 *
 * Success changes only heightmap, minimum_height and conditional skylight
 * nibbles. Blocks, metadata, block_light, table and borrowed pointer/capacity
 * remain unchanged. Separate owners may share an immutable opacity table.
 * There is no allocation, RNG, world-height query, queue or dirty state.
 */
bool mc_chunk_data_rebuild_height_skylight(mc_chunk_data *data, const int32_t *opacity,
                                           size_t opacity_length, bool skylight_enabled);

#endif
