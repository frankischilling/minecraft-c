#include "surface_internal.h"

enum surface_block {
    SURFACE_AIR = 0,
    SURFACE_STONE = 1,
    SURFACE_BEDROCK = 7,
    SURFACE_WATER = 9,
    SURFACE_SAND = 12,
    SURFACE_GRAVEL = 13,
    SURFACE_SANDSTONE = 24
};

int64_t mc_terrain_overworld_chunk_seed(int32_t chunk_x, int32_t chunk_z) {
    int64_t x = mc_java_i64_mul((int64_t)chunk_x, INT64_C(341873128712));
    int64_t z = mc_java_i64_mul((int64_t)chunk_z, INT64_C(132897987541));
    return mc_java_i64_add(x, z);
}

static bool surface_span_valid(mc_terrain_double_span span) {
    return span.data != NULL && span.length >= MC_TERRAIN_OVERWORLD_SURFACE_VALUES &&
           span.length <= SIZE_MAX / sizeof(*span.data);
}

static bool surface_fields_valid(const mc_terrain_overworld_surface_workspace *fields) {
    return fields != NULL && surface_span_valid(fields->sand) &&
           surface_span_valid(fields->gravel) && surface_span_valid(fields->thickness);
}

static bool surface_apply_valid(const mc_terrain_overworld_surface_workspace *fields,
                                const mc_terrain_surface_material *materials,
                                size_t materials_length, const mc_java_random *random,
                                const uint8_t *blocks, size_t blocks_length) {
    return surface_fields_valid(fields) && materials != NULL &&
           materials_length >= MC_TERRAIN_OVERWORLD_SURFACE_VALUES &&
           materials_length <= SIZE_MAX / sizeof(*materials) && random != NULL && blocks != NULL &&
           blocks_length >= MC_TERRAIN_OVERWORLD_BLOCKS;
}

bool mc_terrain_overworld_surface_channel(const mc_terrain_overworld_state *state,
                                          mc_terrain_surface_channel selected, int32_t chunk_x,
                                          int32_t chunk_z, mc_terrain_double_span out) {
    if (state == NULL || !surface_span_valid(out)) {
        return false;
    }
    double origin_x = (double)mc_java_i32_mul(chunk_x, 16);
    double origin_z = (double)mc_java_i32_mul(chunk_z, 16);
    double scale = 0.03125;
    mc_noise_octaves channel = {state->surface_primary_layers, 4};
    mc_noise_grid grid = {origin_x, origin_z, 0.0, 16, 16, 1, scale, scale, 1.0};
    switch (selected) {
        case MC_TERRAIN_SURFACE_SAND:
            break;
        case MC_TERRAIN_SURFACE_GRAVEL:
            grid.origin_y = 109.0134;
            grid.origin_z = origin_z;
            grid.size_y = 1;
            grid.size_z = 16;
            grid.scale_y = 1.0;
            grid.scale_z = scale;
            break;
        case MC_TERRAIN_SURFACE_THICKNESS:
            channel.layers = state->surface_secondary_layers;
            grid.scale_x = scale * 2.0;
            grid.scale_y = scale * 2.0;
            grid.scale_z = scale * 2.0;
            break;
        default:
            return false;
    }
    return mc_noise_octaves_fill(&channel, &grid, out.data, out.length);
}

static void surface_noise(const mc_terrain_overworld_state *state,
                          const mc_terrain_overworld_surface_workspace *workspace, int32_t chunk_x,
                          int32_t chunk_z) {
    /* Fixed shapes/counts and all full spans were validated before mutation;
     * each original octave request now has no remaining failure. */
    /* The public numerical operations validate ALL spans before mutation.
     * GAME instead uses the same requests one at a time at its source calls. */
    (void)mc_terrain_overworld_surface_channel(state, MC_TERRAIN_SURFACE_SAND, chunk_x, chunk_z,
                                               workspace->sand);
    (void)mc_terrain_overworld_surface_channel(state, MC_TERRAIN_SURFACE_GRAVEL, chunk_x, chunk_z,
                                               workspace->gravel);
    (void)mc_terrain_overworld_surface_channel(state, MC_TERRAIN_SURFACE_THICKNESS, chunk_x,
                                               chunk_z, workspace->thickness);
}

static void surface_apply(const mc_terrain_overworld_surface_workspace *fields,
                          const mc_terrain_surface_material *materials, mc_java_random *random,
                          uint8_t *blocks) {
    for (size_t z = 0; z < 16; ++z) {
        for (size_t x = 0; x < 16; ++x) {
            size_t column = x * 16u + z;
            double sand_noise = fields->sand.data[column];
            double sand_uniform = mc_java_random_next_double(random);
            bool sand = mc_terrain_surface_patch(sand_noise, sand_uniform, 0.0);
            double gravel_noise = fields->gravel.data[column];
            double gravel_uniform = mc_java_random_next_double(random);
            bool gravel = mc_terrain_surface_patch(gravel_noise, gravel_uniform, 3.0);
            double thickness_base =
                mc_terrain_surface_thickness_base(fields->thickness.data[column]);
            double thickness_uniform = mc_java_random_next_double(random);
            int32_t thickness =
                mc_terrain_surface_thickness_finish(thickness_base, thickness_uniform);
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

bool mc_terrain_overworld_surface_noise(const mc_terrain_overworld_state *state,
                                        const mc_terrain_overworld_surface_workspace *workspace,
                                        int32_t chunk_x, int32_t chunk_z) {
    if (state == NULL || !surface_fields_valid(workspace)) {
        return false;
    }
    surface_noise(state, workspace, chunk_x, chunk_z);
    return true;
}

bool mc_terrain_overworld_surface_apply(const mc_terrain_overworld_surface_workspace *fields,
                                        const mc_terrain_surface_material *materials,
                                        size_t materials_length, mc_java_random *random,
                                        uint8_t *blocks, size_t blocks_length) {
    if (!surface_apply_valid(fields, materials, materials_length, random, blocks, blocks_length)) {
        return false;
    }
    surface_apply(fields, materials, random, blocks);
    return true;
}

bool mc_terrain_overworld_surface(const mc_terrain_overworld_state *state,
                                  const mc_terrain_overworld_surface_workspace *workspace,
                                  int32_t chunk_x, int32_t chunk_z,
                                  const mc_terrain_surface_material *materials,
                                  size_t materials_length, mc_java_random *random, uint8_t *blocks,
                                  size_t blocks_length) {
    if (state == NULL || !surface_apply_valid(workspace, materials, materials_length, random,
                                              blocks, blocks_length)) {
        return false;
    }
    surface_noise(state, workspace, chunk_x, chunk_z);
    surface_apply(workspace, materials, random, blocks);
    return true;
}
