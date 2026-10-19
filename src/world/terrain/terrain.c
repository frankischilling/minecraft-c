#include "terrain_internal.h"

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

bool mc_terrain_normal_density_channel(const mc_terrain_normal_state *state,
                                       mc_terrain_density_channel requested, int32_t chunk_x,
                                       int32_t chunk_z, mc_terrain_double_span out)
{
    if (state == NULL) {
        return false;
    }
    mc_noise_octaves channel;
    size_t minimum;
    switch (requested) {
        case MC_TERRAIN_DENSITY_SCALE:
            channel = (mc_noise_octaves){state->column_scale_layers, 10};
            minimum = MC_TERRAIN_NORMAL_COLUMN_VALUES;
            break;
        case MC_TERRAIN_DENSITY_BIAS:
            channel = (mc_noise_octaves){state->column_bias_layers, 16};
            minimum = MC_TERRAIN_NORMAL_COLUMN_VALUES;
            break;
        case MC_TERRAIN_DENSITY_MIX:
            channel = (mc_noise_octaves){state->mix_layers, 8};
            minimum = MC_TERRAIN_NORMAL_DENSITY_VALUES;
            break;
        case MC_TERRAIN_DENSITY_LOW:
            channel = (mc_noise_octaves){state->low_layers, 16};
            minimum = MC_TERRAIN_NORMAL_DENSITY_VALUES;
            break;
        case MC_TERRAIN_DENSITY_HIGH:
            channel = (mc_noise_octaves){state->high_layers, 16};
            minimum = MC_TERRAIN_NORMAL_DENSITY_VALUES;
            break;
        default:
            return false;
    }
    if (!terrain_span_valid(out.data, out.length, minimum)) {
        return false;
    }
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
    double horizontal_scale = 684.412;
    double vertical_scale = 684.412;
    if (requested == MC_TERRAIN_DENSITY_BIAS) {
        grid.scale_x = 200.0;
        grid.scale_z = 200.0;
    } else if (requested >= MC_TERRAIN_DENSITY_MIX) {
        grid.origin_y = 0.0;
        grid.size_y = 17;
        grid.scale_x = horizontal_scale;
        grid.scale_y = vertical_scale;
        grid.scale_z = horizontal_scale;
        if (requested == MC_TERRAIN_DENSITY_MIX) {
            grid.scale_x = horizontal_scale / 80.0;
            grid.scale_y = vertical_scale / 160.0;
            grid.scale_z = horizontal_scale / 80.0;
        }
    }
    return mc_noise_octaves_fill(&channel, &grid, out.data, out.length);
}

static void terrain_channels(const mc_terrain_normal_state *state,
                             const mc_terrain_normal_workspace *workspace, int32_t chunk_x,
                             int32_t chunk_z)
{
    /* All counts and spans were validated before mutation. The short original
     * overload supplies Y=10, height=1, Y scale=1; its last double is unused. */
    (void)mc_terrain_normal_density_channel(state, MC_TERRAIN_DENSITY_SCALE, chunk_x, chunk_z,
                                            workspace->column_scale);
    (void)mc_terrain_normal_density_channel(state, MC_TERRAIN_DENSITY_BIAS, chunk_x, chunk_z,
                                            workspace->column_bias);
    (void)mc_terrain_normal_density_channel(state, MC_TERRAIN_DENSITY_MIX, chunk_x, chunk_z,
                                            workspace->mix);
    (void)mc_terrain_normal_density_channel(state, MC_TERRAIN_DENSITY_LOW, chunk_x, chunk_z,
                                            workspace->low);
    (void)mc_terrain_normal_density_channel(state, MC_TERRAIN_DENSITY_HIGH, chunk_x, chunk_z,
                                            workspace->high);
}

void mc_terrain_normal_density_column(const mc_terrain_normal_workspace *workspace, size_t column,
                                      double temperature, double rainfall, double *density)
{
    size_t index = column * 17;
    double wetness = rainfall * temperature;
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

static void terrain_compose(const mc_terrain_normal_workspace *workspace,
                            const mc_terrain_normal_climate *climate, double *density)
{
    size_t column = 0;
    for (size_t x = 0; x < 5; ++x) {
        size_t climate_x = x * 3 + 1;
        for (size_t z = 0; z < 5; ++z) {
            size_t climate_z = z * 3 + 1;
            size_t climate_index = climate_x * 16 + climate_z;
            double temperature = climate->temperature[climate_index];
            double rainfall = climate->rainfall[climate_index];
            mc_terrain_normal_density_column(workspace, column, temperature, rainfall, density);
            ++column;
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

static void terrain_base_cursor_row(mc_terrain_base_cursor *cursor)
{
    cursor->value = cursor->edge0;
    cursor->step = (cursor->edge1 - cursor->edge0) * 0.25;
}

static void terrain_base_cursor_slice(mc_terrain_base_cursor *cursor)
{
    cursor->edge0 = cursor->corner00;
    cursor->edge1 = cursor->corner01;
    cursor->edge_step0 = (cursor->corner10 - cursor->corner00) * 0.25;
    cursor->edge_step1 = (cursor->corner11 - cursor->corner01) * 0.25;
    terrain_base_cursor_row(cursor);
}

static void terrain_base_cursor_cell(mc_terrain_base_cursor *cursor)
{
    size_t left = (cursor->coarse_x * 5 + cursor->coarse_z) * 17 + cursor->coarse_y;
    size_t right = ((cursor->coarse_x + 1) * 5 + cursor->coarse_z) * 17 + cursor->coarse_y;
    cursor->corner00 = cursor->density[left];
    cursor->corner01 = cursor->density[left + 17];
    cursor->corner10 = cursor->density[right];
    cursor->corner11 = cursor->density[right + 17];
    cursor->step00 = (cursor->density[left + 1] - cursor->corner00) * 0.125;
    cursor->step01 = (cursor->density[left + 18] - cursor->corner01) * 0.125;
    cursor->step10 = (cursor->density[right + 1] - cursor->corner10) * 0.125;
    cursor->step11 = (cursor->density[right + 18] - cursor->corner11) * 0.125;
    terrain_base_cursor_slice(cursor);
}

void mc_terrain_normal_base_cursor_init(mc_terrain_base_cursor *cursor, const double *density)
{
    *cursor = (mc_terrain_base_cursor){.density = density};
    terrain_base_cursor_cell(cursor);
}

bool mc_terrain_normal_base_cursor_get(const mc_terrain_base_cursor *cursor,
                                       mc_terrain_base_voxel *out)
{
    if (cursor->done) {
        return false;
    }
    size_t x = cursor->coarse_x * 4 + cursor->sub_x;
    size_t y = cursor->coarse_y * 8 + cursor->sub_y;
    size_t z = cursor->coarse_z * 4 + cursor->sub_z;
    *out = (mc_terrain_base_voxel){
        .index = (x << 11) | (z << 7) | y,
        .temperature_index = x * 16 + z,
        .y = (int32_t)y,
        .density = cursor->value,
    };
    return true;
}

void mc_terrain_normal_base_cursor_advance(mc_terrain_base_cursor *cursor)
{
    if (cursor->done) {
        return;
    }
    cursor->value = cursor->value + cursor->step;
    if (++cursor->sub_z < 4) {
        return;
    }
    cursor->sub_z = 0;
    cursor->edge0 = cursor->edge0 + cursor->edge_step0;
    cursor->edge1 = cursor->edge1 + cursor->edge_step1;
    if (++cursor->sub_x < 4) {
        terrain_base_cursor_row(cursor);
        return;
    }
    cursor->sub_x = 0;
    cursor->corner00 = cursor->corner00 + cursor->step00;
    cursor->corner01 = cursor->corner01 + cursor->step01;
    cursor->corner10 = cursor->corner10 + cursor->step10;
    cursor->corner11 = cursor->corner11 + cursor->step11;
    if (++cursor->sub_y < 8) {
        terrain_base_cursor_slice(cursor);
        return;
    }
    cursor->sub_y = 0;
    if (++cursor->coarse_y < 16) {
        terrain_base_cursor_cell(cursor);
        return;
    }
    cursor->coarse_y = 0;
    if (++cursor->coarse_z < 4) {
        terrain_base_cursor_cell(cursor);
        return;
    }
    cursor->coarse_z = 0;
    if (++cursor->coarse_x < 4) {
        terrain_base_cursor_cell(cursor);
        return;
    }
    cursor->done = true;
}

static void terrain_base_fill(const double *density, const double *temperature, uint8_t *blocks)
{
    mc_terrain_base_cursor cursor;
    mc_terrain_normal_base_cursor_init(&cursor, density);
    mc_terrain_base_voxel voxel;
    while (mc_terrain_normal_base_cursor_get(&cursor, &voxel)) {
        double heat = temperature[voxel.temperature_index];
        uint8_t block = 0;
        if (voxel.y < 64) {
            block = heat < 0.5 && voxel.y >= 63 ? 79 : 9;
        }
        if (voxel.density > 0.0) {
            block = 1;
        }
        blocks[voxel.index] = block;
        mc_terrain_normal_base_cursor_advance(&cursor);
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
