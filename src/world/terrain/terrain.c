#include "mc/world/terrain/terrain.h"

bool mc_terrain_normal_init(mc_terrain_normal_state *state, int64_t seed)
{
    if (state == NULL) {
        return false;
    }
    mc_java_random random;
    mc_java_random_seed(&random, seed);
    mc_noise_octaves channel;
    (void)mc_noise_octaves_init(&channel, state->low_layers, 16, &random);
    (void)mc_noise_octaves_init(&channel, state->high_layers, 16, &random);
    (void)mc_noise_octaves_init(&channel, state->mix_layers, 8, &random);
    (void)mc_noise_octaves_init(&channel, state->surface_primary_layers, 4, &random);
    (void)mc_noise_octaves_init(&channel, state->surface_secondary_layers, 4, &random);
    (void)mc_noise_octaves_init(&channel, state->column_scale_layers, 10, &random);
    (void)mc_noise_octaves_init(&channel, state->column_bias_layers, 16, &random);
    (void)mc_noise_octaves_init(&channel, state->later_layers, 8, &random);
    state->random_after_init = random;
    return true;
}

static bool terrain_span_valid(const double *data, size_t length, size_t minimum)
{
    return data != NULL && length >= minimum && length <= SIZE_MAX / sizeof(*data);
}

static bool terrain_density_valid(const mc_terrain_normal_state *state,
                                  const mc_terrain_normal_workspace *workspace,
                                  const mc_terrain_normal_climate *climate,
                                  mc_terrain_double_span density)
{
    return state != NULL && workspace != NULL && climate != NULL &&
           terrain_span_valid(climate->temperature, climate->temperature_length,
                              MC_TERRAIN_NORMAL_CLIMATE_VALUES) &&
           terrain_span_valid(climate->rainfall, climate->rainfall_length,
                              MC_TERRAIN_NORMAL_CLIMATE_VALUES) &&
           terrain_span_valid(workspace->mix.data, workspace->mix.length,
                              MC_TERRAIN_NORMAL_DENSITY_VALUES) &&
           terrain_span_valid(workspace->low.data, workspace->low.length,
                              MC_TERRAIN_NORMAL_DENSITY_VALUES) &&
           terrain_span_valid(workspace->high.data, workspace->high.length,
                              MC_TERRAIN_NORMAL_DENSITY_VALUES) &&
           terrain_span_valid(workspace->column_scale.data, workspace->column_scale.length,
                              MC_TERRAIN_NORMAL_COLUMN_VALUES) &&
           terrain_span_valid(workspace->column_bias.data, workspace->column_bias.length,
                              MC_TERRAIN_NORMAL_COLUMN_VALUES) &&
           terrain_span_valid(density.data, density.length, MC_TERRAIN_NORMAL_DENSITY_VALUES);
}

static bool terrain_base_valid(const double *density, size_t density_length,
                               const double *temperature, size_t temperature_length,
                               const uint8_t *blocks, size_t blocks_length)
{
    return terrain_span_valid(density, density_length, MC_TERRAIN_NORMAL_DENSITY_VALUES) &&
           terrain_span_valid(temperature, temperature_length, MC_TERRAIN_NORMAL_CLIMATE_VALUES) &&
           blocks != NULL && blocks_length >= MC_TERRAIN_NORMAL_BLOCKS;
}

static void terrain_channels(const mc_terrain_normal_state *state,
                             const mc_terrain_normal_workspace *workspace, int32_t chunk_x,
                             int32_t chunk_z)
{
    mc_noise_grid grid = {
        (double)mc_java_i32_mul(chunk_x, 4),
        10.0,
        (double)mc_java_i32_mul(chunk_z, 4),
        5,
        1,
        5,
        1.121,
        1.0,
        1.121,
    };
    /* All counts and spans were validated before mutation. The short original
     * overload supplies Y=10, height=1, Y scale=1; its last double is unused. */
    mc_noise_octaves channel = {state->column_scale_layers, 10};
    (void)mc_noise_octaves_fill(&channel, &grid, workspace->column_scale.data,
                                workspace->column_scale.length);
    channel.layers = state->column_bias_layers;
    channel.count = 16;
    grid.scale_x = 200.0;
    grid.scale_z = 200.0;
    (void)mc_noise_octaves_fill(&channel, &grid, workspace->column_bias.data,
                                workspace->column_bias.length);

    double horizontal_scale = 684.412;
    double vertical_scale = 684.412;
    grid.origin_y = 0.0;
    grid.size_y = 17;
    grid.scale_x = horizontal_scale / 80.0;
    grid.scale_y = vertical_scale / 160.0;
    grid.scale_z = horizontal_scale / 80.0;
    channel.layers = state->mix_layers;
    channel.count = 8;
    (void)mc_noise_octaves_fill(&channel, &grid, workspace->mix.data, workspace->mix.length);

    grid.scale_x = horizontal_scale;
    grid.scale_y = vertical_scale;
    grid.scale_z = horizontal_scale;
    channel.layers = state->low_layers;
    channel.count = 16;
    (void)mc_noise_octaves_fill(&channel, &grid, workspace->low.data, workspace->low.length);
    channel.layers = state->high_layers;
    (void)mc_noise_octaves_fill(&channel, &grid, workspace->high.data, workspace->high.length);
}

static void terrain_compose(const mc_terrain_normal_workspace *workspace,
                            const mc_terrain_normal_climate *climate, double *density)
{
    size_t column = 0;
    size_t index = 0;
    for (size_t x = 0; x < 5; ++x) {
        size_t climate_x = x * 3 + 1;
        for (size_t z = 0; z < 5; ++z) {
            size_t climate_z = z * 3 + 1;
            size_t climate_index = climate_x * 16 + climate_z;
            double temperature = climate->temperature[climate_index];
            double wetness = climate->rainfall[climate_index] * temperature;
            wetness = 1.0 - wetness;
            wetness = wetness * wetness;
            wetness = wetness * wetness;
            wetness = 1.0 - wetness;

            double scale = (workspace->column_scale.data[column] + 256.0) / 512.0;
            scale = scale * wetness;
            if (scale > 1.0) {
                scale = 1.0;
            }
            double bias = workspace->column_bias.data[column] / 8000.0;
            if (bias < 0.0) {
                bias = -bias * 0.3;
            }
            bias = bias * 3.0 - 2.0;
            if (bias < 0.0) {
                bias = bias / 2.0;
                if (bias < -1.0) {
                    bias = -1.0;
                }
                bias = bias / 1.4;
                bias = bias / 2.0;
                scale = 0.0;
            } else {
                if (bias > 1.0) {
                    bias = 1.0;
                }
                bias = bias / 8.0;
            }
            if (scale < 0.0) {
                scale = 0.0;
            }
            scale = scale + 0.5;
            bias = bias * 17.0 / 16.0;
            double center = 17.0 / 2.0 + bias * 4.0;
            ++column;

            for (size_t y = 0; y < 17; ++y) {
                double vertical = ((double)y - center) * 12.0 / scale;
                if (vertical < 0.0) {
                    vertical = vertical * 4.0;
                }
                double low = workspace->low.data[index] / 512.0;
                double high = workspace->high.data[index] / 512.0;
                double mix = (workspace->mix.data[index] / 10.0 + 1.0) / 2.0;
                double value;
                if (mix < 0.0) {
                    value = low;
                } else if (mix > 1.0) {
                    value = high;
                } else {
                    value = low + (high - low) * mix;
                }
                value = value - vertical;
                if (y > 13) {
                    /* Java divides in binary32 before promoting the fade. */
                    float fade_float = (float)(y - 13) / 3.0f;
                    double fade = (double)fade_float;
                    value = value * (1.0 - fade) + -10.0 * fade;
                }
                density[index] = value;
                ++index;
            }
        }
    }
}

static void terrain_density_fill(const mc_terrain_normal_state *state,
                                 const mc_terrain_normal_workspace *workspace, int32_t chunk_x,
                                 int32_t chunk_z, const mc_terrain_normal_climate *climate,
                                 double *density)
{
    terrain_channels(state, workspace, chunk_x, chunk_z);
    terrain_compose(workspace, climate, density);
}

bool mc_terrain_normal_density(const mc_terrain_normal_state *state,
                               const mc_terrain_normal_workspace *workspace, int32_t chunk_x,
                               int32_t chunk_z, const mc_terrain_normal_climate *climate,
                               mc_terrain_double_span density)
{
    if (!terrain_density_valid(state, workspace, climate, density)) {
        return false;
    }
    terrain_density_fill(state, workspace, chunk_x, chunk_z, climate, density.data);
    return true;
}

static void terrain_base_fill(const double *density, const double *temperature, uint8_t *blocks)
{
    for (size_t coarse_x = 0; coarse_x < 4; ++coarse_x) {
        for (size_t coarse_z = 0; coarse_z < 4; ++coarse_z) {
            for (size_t coarse_y = 0; coarse_y < 16; ++coarse_y) {
                size_t left = (coarse_x * 5 + coarse_z) * 17 + coarse_y;
                size_t right = ((coarse_x + 1) * 5 + coarse_z) * 17 + coarse_y;
                double corner00 = density[left];
                double corner01 = density[left + 17];
                double corner10 = density[right];
                double corner11 = density[right + 17];
                double step00 = (density[left + 1] - corner00) * 0.125;
                double step01 = (density[left + 18] - corner01) * 0.125;
                double step10 = (density[right + 1] - corner10) * 0.125;
                double step11 = (density[right + 18] - corner11) * 0.125;

                for (size_t sub_y = 0; sub_y < 8; ++sub_y) {
                    double edge0 = corner00;
                    double edge1 = corner01;
                    double edge_step0 = (corner10 - corner00) * 0.25;
                    double edge_step1 = (corner11 - corner01) * 0.25;
                    for (size_t sub_x = 0; sub_x < 4; ++sub_x) {
                        size_t x = coarse_x * 4 + sub_x;
                        size_t y = coarse_y * 8 + sub_y;
                        size_t block_index = (x << 11) | (coarse_z * 4 << 7) | y;
                        double value = edge0;
                        double step = (edge1 - edge0) * 0.25;
                        for (size_t sub_z = 0; sub_z < 4; ++sub_z) {
                            size_t z = coarse_z * 4 + sub_z;
                            double heat = temperature[x * 16 + z];
                            uint8_t block = 0;
                            if (y < 64) {
                                block = heat < 0.5 && y >= 63 ? 79 : 9;
                            }
                            if (value > 0.0) {
                                block = 1;
                            }
                            blocks[block_index] = block;
                            block_index += 128;
                            value = value + step;
                        }
                        edge0 = edge0 + edge_step0;
                        edge1 = edge1 + edge_step1;
                    }
                    corner00 = corner00 + step00;
                    corner01 = corner01 + step01;
                    corner10 = corner10 + step10;
                    corner11 = corner11 + step11;
                }
            }
        }
    }
}

bool mc_terrain_normal_base(const double *density, size_t density_length, const double *temperature,
                            size_t temperature_length, uint8_t *blocks, size_t blocks_length)
{
    if (!terrain_base_valid(density, density_length, temperature, temperature_length, blocks,
                            blocks_length)) {
        return false;
    }
    terrain_base_fill(density, temperature, blocks);
    return true;
}

bool mc_terrain_normal_generate(const mc_terrain_normal_state *state,
                                const mc_terrain_normal_workspace *workspace, int32_t chunk_x,
                                int32_t chunk_z, const mc_terrain_normal_climate *climate,
                                mc_terrain_double_span density, uint8_t *blocks,
                                size_t blocks_length)
{
    if (!terrain_density_valid(state, workspace, climate, density) ||
        !terrain_base_valid(density.data, density.length, climate->temperature,
                            climate->temperature_length, blocks, blocks_length)) {
        return false;
    }
    terrain_density_fill(state, workspace, chunk_x, chunk_z, climate, density.data);
    terrain_base_fill(density.data, climate->temperature, blocks);
    return true;
}
