#ifndef MC_WORLD_CHUNK_NIBBLES_H
#define MC_WORLD_CHUNK_NIBBLES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Fixed normal chunk shape, shared by packed arrays and voxel data. */
#define MC_CHUNK_WIDTH 16u
#define MC_CHUNK_HEIGHT 128u
#define MC_CHUNK_BLOCKS (MC_CHUNK_WIDTH * MC_CHUNK_WIDTH * MC_CHUNK_HEIGHT)
#define MC_CHUNK_NIBBLE_BYTES (MC_CHUNK_BLOCKS / 2u)
#define MC_CHUNK_HEIGHTMAP_BYTES (MC_CHUNK_WIDTH * MC_CHUNK_WIDTH)

/* Coordinates are local: x/z 0..15 and y 0..127. Block order is X/Z/Y,
 * x*2048 + z*128 + y. Even Y uses the low nibble; odd Y uses the high.
 * Storage must be valid for its stated capacity. The getter's output must
 * be valid disjoint storage. Access is exclusive during a set operation.
 *
 * Null storage/output, short capacity or invalid coordinates return false
 * before mutation. A failed get retains *out. Only the logical packed
 * prefix is read or written; any capacity suffix is retained.
 */
bool mc_chunk_nibbles_get(const uint8_t *packed, size_t capacity, int32_t x, int32_t y, int32_t z,
                          uint8_t *out);

/* Every int32_t value is accepted; its low four bits replace only the
 * selected nibble, preserving the other nibble in the same byte.
 */
bool mc_chunk_nibbles_set(uint8_t *packed, size_t capacity, int32_t x, int32_t y, int32_t z,
                          int32_t value);

#endif
