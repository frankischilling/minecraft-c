#include "mc/world/chunk/nibbles.h"

static bool nibbles_coordinates_valid(int32_t x, int32_t y, int32_t z)
{
    return x >= 0 && x < (int32_t)MC_CHUNK_WIDTH && y >= 0 && y < (int32_t)MC_CHUNK_HEIGHT &&
           z >= 0 && z < (int32_t)MC_CHUNK_WIDTH;
}

static size_t nibbles_packed_index(int32_t x, int32_t y, int32_t z)
{
    size_t block_index =
        (size_t)x * MC_CHUNK_WIDTH * MC_CHUNK_HEIGHT + (size_t)z * MC_CHUNK_HEIGHT + (size_t)y;
    return block_index / 2u;
}

bool mc_chunk_nibbles_get(const uint8_t *packed, size_t capacity, int32_t x, int32_t y, int32_t z,
                          uint8_t *out)
{
    if (packed == NULL || out == NULL || capacity < MC_CHUNK_NIBBLE_BYTES ||
        !nibbles_coordinates_valid(x, y, z)) {
        return false;
    }

    uint32_t value = packed[nibbles_packed_index(x, y, z)];
    if (((uint32_t)y & UINT32_C(1)) != 0) {
        value /= UINT32_C(16);
    }
    *out = (uint8_t)(value & UINT32_C(15));
    return true;
}

bool mc_chunk_nibbles_set(uint8_t *packed, size_t capacity, int32_t x, int32_t y, int32_t z,
                          int32_t value)
{
    if (packed == NULL || capacity < MC_CHUNK_NIBBLE_BYTES || !nibbles_coordinates_valid(x, y, z)) {
        return false;
    }

    size_t index = nibbles_packed_index(x, y, z);
    uint32_t previous = packed[index];
    uint32_t nibble = (uint32_t)value & UINT32_C(15);
    if (((uint32_t)y & UINT32_C(1)) == 0) {
        packed[index] = (uint8_t)((previous & UINT32_C(240)) | nibble);
    } else {
        packed[index] = (uint8_t)((previous & UINT32_C(15)) | (nibble * UINT32_C(16)));
    }
    return true;
}
