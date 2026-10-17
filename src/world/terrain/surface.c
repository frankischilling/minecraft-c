#include "mc/world/terrain/surface.h"

enum surface_block {
    SURFACE_AIR = 0,
    SURFACE_STONE = 1,
    SURFACE_BEDROCK = 7,
    SURFACE_WATER = 9,
    SURFACE_SAND = 12,
    SURFACE_GRAVEL = 13,
    SURFACE_SANDSTONE = 24
};

int64_t mc_terrain_normal_chunk_seed(int32_t chunk_x, int32_t chunk_z)
{
    int64_t x = mc_java_i64_mul((int64_t)chunk_x, INT64_C(341873128712));
    int64_t z = mc_java_i64_mul((int64_t)chunk_z, INT64_C(132897987541));
    return mc_java_i64_add(x, z);
}

static bool surface_span_valid(mc_terrain_double_span span)
{
    return span.data != NULL && span.length >= MC_TERRAIN_NORMAL_SURFACE_VALUES &&
           span.length <= SIZE_MAX / sizeof(*span.data);
}

static bool surface_fields_valid(const mc_terrain_normal_surface_workspace *fields)
{
    return fields != NULL && surface_span_valid(fields->sand) &&
           surface_span_valid(fields->gravel) && surface_span_valid(fields->thickness);
}

static bool surface_apply_valid(const mc_terrain_normal_surface_workspace *fields,
                                const mc_terrain_surface_material *materials,
                                size_t materials_length, const mc_java_random *random,
                                const uint8_t *blocks, size_t blocks_length)
{
    return surface_fields_valid(fields) && materials != NULL &&
           materials_length >= MC_TERRAIN_NORMAL_SURFACE_VALUES &&
           materials_length <= SIZE_MAX / sizeof(*materials) && random != NULL && blocks != NULL &&
           blocks_length >= MC_TERRAIN_NORMAL_BLOCKS;
}

static void surface_noise(const mc_terrain_normal_state *state,
                          const mc_terrain_normal_surface_workspace *workspace, int32_t chunk_x,
                          int32_t chunk_z)
{
    double origin_x = (double)mc_java_i32_mul(chunk_x, 16);
    double origin_z = (double)mc_java_i32_mul(chunk_z, 16);
    double scale = 0.03125;
    mc_noise_octaves channel = {state->surface_primary_layers, 4};
    mc_noise_grid grid = {origin_x, origin_z, 0.0, 16, 16, 1, scale, scale, 1.0};
    /* Fixed shapes/counts and all full spans were validated before mutation;
     * each original octave request now has no remaining failure. */
    (void)mc_noise_octaves_fill(&channel, &grid, workspace->sand.data, workspace->sand.length);

    grid.origin_y = 109.0134;
    grid.origin_z = origin_z;
    grid.size_y = 1;
    grid.size_z = 16;
    grid.scale_y = 1.0;
    grid.scale_z = scale;
    (void)mc_noise_octaves_fill(&channel, &grid, workspace->gravel.data, workspace->gravel.length);

    channel.layers = state->surface_secondary_layers;
    grid.origin_y = origin_z;
    grid.origin_z = 0.0;
    grid.size_y = 16;
    grid.size_z = 1;
    grid.scale_x = scale * 2.0;
    grid.scale_y = scale * 2.0;
    grid.scale_z = scale * 2.0;
    (void)mc_noise_octaves_fill(&channel, &grid, workspace->thickness.data,
                                workspace->thickness.length);
}

static void surface_apply(const mc_terrain_normal_surface_workspace *fields,
                          const mc_terrain_surface_material *materials, mc_java_random *random,
                          uint8_t *blocks)
{
    for (size_t z = 0; z < 16; ++z) {
        for (size_t x = 0; x < 16; ++x) {
            size_t column = x * 16u + z;
            double sand_jitter = mc_java_random_next_double(random) * 0.2;
            bool sand = fields->sand.data[column] + sand_jitter > 0.0;
            double gravel_jitter = mc_java_random_next_double(random) * 0.2;
            bool gravel = fields->gravel.data[column] + gravel_jitter > 3.0;
            double thickness_jitter = mc_java_random_next_double(random) * 0.25;
            int32_t thickness = mc_java_double_to_i32(
                ((fields->thickness.data[column] / 3.0) + 3.0) + thickness_jitter);
            int32_t remaining = -1;
            uint8_t top = materials[column].top;
            uint8_t filler = materials[column].filler;
            for (int32_t y = 127; y >= 0; --y) {
                size_t index = column * 128u + (size_t)y;
                int32_t bedrock = 0;
                (void)mc_java_random_next_bounded(random, 5, &bedrock);
                if (y <= bedrock) {
                    blocks[index] = SURFACE_BEDROCK;
                    continue;
                }
                uint8_t block = blocks[index];
                if (block == SURFACE_AIR) {
                    remaining = -1;
                    continue;
                }
                if (block != SURFACE_STONE) {
                    continue;
                }
                if (remaining == -1) {
                    if (thickness <= 0) {
                        top = SURFACE_AIR;
                        filler = SURFACE_STONE;
                    } else if (y >= 60 && y <= 65) {
                        top = materials[column].top;
                        filler = materials[column].filler;
                        if (gravel) {
                            top = SURFACE_AIR;
                            filler = SURFACE_GRAVEL;
                        }
                        if (sand) {
                            top = SURFACE_SAND;
                            filler = SURFACE_SAND;
                        }
                    }
                    if (y < 64 && top == SURFACE_AIR) {
                        top = SURFACE_WATER;
                    }
                    remaining = thickness;
                    blocks[index] = y >= 63 ? top : filler;
                } else if (remaining > 0) {
                    --remaining;
                    blocks[index] = filler;
                    if (remaining == 0 && filler == SURFACE_SAND) {
                        (void)mc_java_random_next_bounded(random, 4, &remaining);
                        filler = SURFACE_SANDSTONE;
                    }
                }
            }
        }
    }
}

bool mc_terrain_normal_surface_noise(const mc_terrain_normal_state *state,
                                     const mc_terrain_normal_surface_workspace *workspace,
                                     int32_t chunk_x, int32_t chunk_z)
{
    if (state == NULL || !surface_fields_valid(workspace)) {
        return false;
    }
    surface_noise(state, workspace, chunk_x, chunk_z);
    return true;
}

bool mc_terrain_normal_surface_apply(const mc_terrain_normal_surface_workspace *fields,
                                     const mc_terrain_surface_material *materials,
                                     size_t materials_length, mc_java_random *random,
                                     uint8_t *blocks, size_t blocks_length)
{
    if (!surface_apply_valid(fields, materials, materials_length, random, blocks, blocks_length)) {
        return false;
    }
    surface_apply(fields, materials, random, blocks);
    return true;
}

bool mc_terrain_normal_surface(const mc_terrain_normal_state *state,
                               const mc_terrain_normal_surface_workspace *workspace,
                               int32_t chunk_x, int32_t chunk_z,
                               const mc_terrain_surface_material *materials,
                               size_t materials_length, mc_java_random *random, uint8_t *blocks,
                               size_t blocks_length)
{
    if (state == NULL || !surface_apply_valid(workspace, materials, materials_length, random,
                                              blocks, blocks_length)) {
        return false;
    }
    surface_noise(state, workspace, chunk_x, chunk_z);
    surface_apply(workspace, materials, random, blocks);
    return true;
}
