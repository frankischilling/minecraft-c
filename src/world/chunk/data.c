#include "mc/world/chunk/data.h"

#include <string.h>

bool mc_chunk_data_init(mc_chunk_data *data, uint8_t *blocks, size_t blocks_capacity)
{
    if (data == NULL || blocks == NULL || blocks_capacity < MC_CHUNK_BLOCKS) {
        return false;
    }

    data->blocks = blocks;
    data->blocks_capacity = blocks_capacity;
    memset(data->metadata, 0, sizeof(data->metadata));
    memset(data->skylight, 0, sizeof(data->skylight));
    memset(data->block_light, 0, sizeof(data->block_light));
    memset(data->heightmap, 0, sizeof(data->heightmap));
    data->minimum_height = 0;
    return true;
}

bool mc_chunk_data_rebuild_height_skylight(mc_chunk_data *data, const int32_t *opacity,
                                           size_t opacity_length, bool skylight_enabled)
{
    if (data == NULL || data->blocks == NULL || data->blocks_capacity < MC_CHUNK_BLOCKS ||
        opacity == NULL || opacity_length < 256u) {
        return false;
    }
    for (size_t id = 0; id < 256u; ++id) {
        if (opacity[id] < 0) {
            return false;
        }
    }

    int32_t minimum = (int32_t)MC_CHUNK_HEIGHT - 1;
    for (int32_t x = 0; x < (int32_t)MC_CHUNK_WIDTH; ++x) {
        for (int32_t z = 0; z < (int32_t)MC_CHUNK_WIDTH; ++z) {
            size_t base =
                (size_t)x * MC_CHUNK_WIDTH * MC_CHUNK_HEIGHT + (size_t)z * MC_CHUNK_HEIGHT;
            int32_t height = (int32_t)MC_CHUNK_HEIGHT - 1;
            while (height > 0 && opacity[data->blocks[base + (size_t)height - 1u]] == 0) {
                --height;
            }
            data->heightmap[(size_t)z * MC_CHUNK_WIDTH + (size_t)x] = (uint8_t)height;
            if (height < minimum) {
                minimum = height;
            }

            if (skylight_enabled) {
                int32_t light = 15;
                for (int32_t y = (int32_t)MC_CHUNK_HEIGHT - 1; y > 0 && light > 0; --y) {
                    /* Entry light is 1..15 and opacity is 0..INT32_MAX,
                     * so this signed subtraction is representable. */
                    light -= opacity[data->blocks[base + (size_t)y]];
                    if (light > 0) {
                        /* Fixed array capacity and loop coordinates were
                         * validated before mutation; this set cannot fail. */
                        (void)mc_chunk_nibbles_set(data->skylight, sizeof(data->skylight), x, y, z,
                                                   light);
                    }
                }
            }
        }
    }
    data->minimum_height = minimum;
    return true;
}
