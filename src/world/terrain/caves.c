#include "caves_internal.h"

#include <limits.h>

enum cave_block {
    CAVE_AIR = 0,
    CAVE_STONE = 1,
    CAVE_GRASS = 2,
    CAVE_DIRT = 3,
    CAVE_FLOWING_WATER = 8,
    CAVE_WATER = 9,
    CAVE_FLOWING_LAVA = 10
};

static int32_t cave_bounded(mc_java_random *random, int32_t bound)
{
    int32_t result = 0;
    /* Normal runtime and the private adapter domain keep every bound positive. */
    (void)mc_java_random_next_bounded(random, bound, &result);
    return result;
}

static int64_t cave_i64_from_bits(uint64_t bits)
{
    if (bits <= INT64_MAX) {
        return (int64_t)bits;
    }
    return -1 - (int64_t)(UINT64_MAX - bits);
}

void mc_terrain_cave_room(mc_terrain_cave_context *context, double x, double y, double z)
{
    float width = mc_java_random_next_float(context->random) * 6.0f;
    width = 1.0f + width;
    mc_terrain_cave_path(context, x, y, z, width, 0.0f, 0.0f, -1, -1, 0.5);
}

void mc_terrain_cave_path_begin(mc_terrain_cave_path_frame *frame, mc_java_random *shared,
                                const mc_terrain_cave_path_args *args)
{
    frame->current = *args;
    frame->origin_x = mc_java_i32_mul(args->chunk_x, 16);
    frame->origin_z = mc_java_i32_mul(args->chunk_z, 16);
    frame->target_x = (double)mc_java_i32_add(frame->origin_x, 8);
    frame->target_z = (double)mc_java_i32_add(frame->origin_z, 8);
    frame->yaw_velocity = 0.0f;
    frame->pitch_velocity = 0.0f;
    const int64_t seed = mc_java_random_next_i64(shared);
    mc_java_random_seed(&frame->local, seed);
    if (frame->current.end <= 0)
        frame->current.end = 112 - cave_bounded(&frame->local, 28);
    frame->room = false;
    if (frame->current.step == -1) {
        frame->current.step = frame->current.end / 2;
        frame->room = true;
    }
    frame->fork_step = cave_bounded(&frame->local, frame->current.end / 2) + frame->current.end / 4;
    frame->slow_pitch = cave_bounded(&frame->local, 6) == 0;
}

mc_terrain_cave_step mc_terrain_cave_path_step(mc_terrain_cave_path_frame *frame,
                                               const mc_math_trig *trig,
                                               mc_terrain_cave_ellipsoid *out)
{
    mc_terrain_cave_path_args *current = &frame->current;
    if (current->step >= current->end)
        return MC_TERRAIN_CAVE_END;
    float phase = (float)current->step * 3.1415927f;
    phase = phase / (float)current->end;
    float radius_wave = mc_math_sin(trig, phase) * current->width;
    radius_wave = radius_wave * 1.0f;
    double radius = 1.5 + (double)radius_wave;
    double vertical_radius = radius * current->vertical_ratio;
    float cosine_pitch = mc_math_cos(trig, current->pitch);
    float sine_pitch = mc_math_sin(trig, current->pitch);
    float movement_x = mc_math_cos(trig, current->yaw) * cosine_pitch;
    current->x = current->x + (double)movement_x;
    current->y = current->y + (double)sine_pitch;
    float movement_z = mc_math_sin(trig, current->yaw) * cosine_pitch;
    current->z = current->z + (double)movement_z;
    current->pitch = current->pitch * (frame->slow_pitch ? 0.92f : 0.7f);
    float pitch_change = frame->pitch_velocity * 0.1f;
    current->pitch = current->pitch + pitch_change;
    float yaw_change = frame->yaw_velocity * 0.1f;
    current->yaw = current->yaw + yaw_change;
    frame->pitch_velocity = frame->pitch_velocity * 0.9f;
    frame->yaw_velocity = frame->yaw_velocity * 0.75f;

    float first = mc_java_random_next_float(&frame->local);
    float second = mc_java_random_next_float(&frame->local);
    float third = mc_java_random_next_float(&frame->local);
    float velocity_change = first - second;
    velocity_change = velocity_change * third;
    velocity_change = velocity_change * 2.0f;
    frame->pitch_velocity = frame->pitch_velocity + velocity_change;
    first = mc_java_random_next_float(&frame->local);
    second = mc_java_random_next_float(&frame->local);
    third = mc_java_random_next_float(&frame->local);
    velocity_change = first - second;
    velocity_change = velocity_change * third;
    velocity_change = velocity_change * 4.0f;
    frame->yaw_velocity = frame->yaw_velocity + velocity_change;

    if (!frame->room && current->step == frame->fork_step && current->width > 1.0f)
        return MC_TERRAIN_CAVE_FORK;
    if (!frame->room && cave_bounded(&frame->local, 4) == 0)
        return MC_TERRAIN_CAVE_SKIP;
    double delta_x = current->x - frame->target_x;
    double delta_z = current->z - frame->target_z;
    double remaining = (double)mc_java_i32_sub(current->end, current->step);
    float reach_float = current->width + 2.0f;
    reach_float = reach_float + 16.0f;
    double reach = (double)reach_float;
    double distance = delta_x * delta_x + delta_z * delta_z;
    distance = distance - remaining * remaining;
    if (distance > reach * reach)
        return MC_TERRAIN_CAVE_END;
    if (current->x < frame->target_x - 16.0 - radius * 2.0 ||
        current->z < frame->target_z - 16.0 - radius * 2.0 ||
        current->x > frame->target_x + 16.0 + radius * 2.0 ||
        current->z > frame->target_z + 16.0 + radius * 2.0)
        return MC_TERRAIN_CAVE_SKIP;

    int32_t low_x = mc_java_i32_sub(
        mc_java_i32_sub(mc_math_floor_double(current->x - radius), frame->origin_x), 1);
    int32_t high_x = mc_java_i32_add(
        mc_java_i32_sub(mc_math_floor_double(current->x + radius), frame->origin_x), 1);
    int32_t low_y = mc_java_i32_sub(mc_math_floor_double(current->y - vertical_radius), 1);
    int32_t high_y = mc_java_i32_add(mc_math_floor_double(current->y + vertical_radius), 1);
    int32_t low_z = mc_java_i32_sub(
        mc_java_i32_sub(mc_math_floor_double(current->z - radius), frame->origin_z), 1);
    int32_t high_z = mc_java_i32_add(
        mc_java_i32_sub(mc_math_floor_double(current->z + radius), frame->origin_z), 1);
    if (low_x < 0)
        low_x = 0;
    if (high_x > 16)
        high_x = 16;
    if (low_y < 1)
        low_y = 1;
    if (high_y > 120)
        high_y = 120;
    if (low_z < 0)
        low_z = 0;
    if (high_z > 16)
        high_z = 16;
    *out = (mc_terrain_cave_ellipsoid){.x = current->x,
                                       .y = current->y,
                                       .z = current->z,
                                       .radius = radius,
                                       .vertical_radius = vertical_radius,
                                       .origin_x = frame->origin_x,
                                       .origin_z = frame->origin_z,
                                       .low_x = low_x,
                                       .high_x = high_x,
                                       .low_y = low_y,
                                       .high_y = high_y,
                                       .low_z = low_z,
                                       .high_z = high_z};
    return MC_TERRAIN_CAVE_ELLIPSOID;
}

void mc_terrain_cave_path_child(mc_terrain_cave_path_frame *parent, unsigned int index,
                                mc_terrain_cave_path_args *out)
{
    float width = mc_java_random_next_float(&parent->local) * 0.5f;
    width = width + 0.5f;
    float yaw = index == 0 ? parent->current.yaw - 1.5707964f : parent->current.yaw + 1.5707964f;
    float pitch = parent->current.pitch / 3.0f;
    *out = parent->current;
    out->width = width;
    out->yaw = yaw;
    out->pitch = pitch;
    out->vertical_ratio = 1.0;
}

double mc_terrain_cave_ellipsoid_x(const mc_terrain_cave_ellipsoid *ellipse, int32_t local_x)
{
    double normalized = (double)mc_java_i32_add(local_x, ellipse->origin_x) + 0.5;
    normalized = normalized - ellipse->x;
    normalized = normalized / ellipse->radius;
    return normalized;
}

bool mc_terrain_cave_ellipsoid_column(const mc_terrain_cave_ellipsoid *ellipse, double normalized_x,
                                      int32_t local_x, int32_t local_z, mc_terrain_cave_column *out)
{
    double normalized_z = (double)mc_java_i32_add(local_z, ellipse->origin_z) + 0.5;
    normalized_z = normalized_z - ellipse->z;
    normalized_z = normalized_z / ellipse->radius;
    int32_t index = mc_java_i32_add(
        mc_java_i32_mul(mc_java_i32_add(mc_java_i32_mul(local_x, 16), local_z), 128),
        ellipse->high_y);
    *out = (mc_terrain_cave_column){normalized_x, normalized_z, index};
    return normalized_x * normalized_x + normalized_z * normalized_z < 1.0;
}

bool mc_terrain_cave_ellipsoid_contains(const mc_terrain_cave_ellipsoid *ellipse,
                                        const mc_terrain_cave_column *column, int32_t geometric_y)
{
    double normalized_y = (double)geometric_y + 0.5;
    normalized_y = normalized_y - ellipse->y;
    normalized_y = normalized_y / ellipse->vertical_radius;
    if (!(normalized_y > -0.7))
        return false;
    double value = column->normalized_x * column->normalized_x + normalized_y * normalized_y;
    value = value + column->normalized_z * column->normalized_z;
    return value < 1.0;
}

void mc_terrain_cave_path(mc_terrain_cave_context *context, double x, double y, double z,
                          float width, float yaw, float pitch, int32_t step, int32_t end,
                          double vertical_ratio)
{
    const mc_terrain_cave_path_args args = {.chunk_x = context->chunk_x,
                                            .chunk_z = context->chunk_z,
                                            .x = x,
                                            .y = y,
                                            .z = z,
                                            .vertical_ratio = vertical_ratio,
                                            .width = width,
                                            .yaw = yaw,
                                            .pitch = pitch,
                                            .step = step,
                                            .end = end};
    mc_terrain_cave_path_frame frame;
    mc_terrain_cave_path_begin(&frame, context->random, &args);
    for (; frame.current.step < frame.current.end;
         frame.current.step = mc_java_i32_add(frame.current.step, 1)) {
        mc_terrain_cave_ellipsoid ellipse;
        mc_terrain_cave_step outcome = mc_terrain_cave_path_step(&frame, context->table, &ellipse);
        if (outcome == MC_TERRAIN_CAVE_END)
            return;
        if (outcome == MC_TERRAIN_CAVE_FORK) {
            mc_terrain_cave_path_args child;
            mc_terrain_cave_path_child(&frame, 0, &child);
            mc_terrain_cave_path(context, child.x, child.y, child.z, child.width, child.yaw,
                                 child.pitch, child.step, child.end, child.vertical_ratio);
            mc_terrain_cave_path_child(&frame, 1, &child);
            mc_terrain_cave_path(context, child.x, child.y, child.z, child.width, child.yaw,
                                 child.pitch, child.step, child.end, child.vertical_ratio);
            return;
        }
        if (outcome == MC_TERRAIN_CAVE_SKIP)
            continue;

        bool water = false;
        for (int32_t local_x = ellipse.low_x; !water && local_x < ellipse.high_x; ++local_x) {
            for (int32_t local_z = ellipse.low_z; !water && local_z < ellipse.high_z; ++local_z) {
                for (int32_t water_y = ellipse.high_y + 1; !water && water_y >= ellipse.low_y - 1;
                     --water_y) {
                    if (water_y < 0 || water_y >= 128)
                        continue;
                    size_t index = (size_t)((local_x * 16 + local_z) * 128 + water_y);
                    uint8_t block = context->blocks[index];
                    if (block == CAVE_FLOWING_WATER || block == CAVE_WATER)
                        water = true;
                    if (water_y != ellipse.low_y - 1 && local_x != ellipse.low_x &&
                        local_x != ellipse.high_x - 1 && local_z != ellipse.low_z &&
                        local_z != ellipse.high_z - 1)
                        water_y = ellipse.low_y;
                }
            }
        }
        if (water)
            continue;
        for (int32_t local_x = ellipse.low_x; local_x < ellipse.high_x; ++local_x) {
            double normalized_x = mc_terrain_cave_ellipsoid_x(&ellipse, local_x);
            for (int32_t local_z = ellipse.low_z; local_z < ellipse.high_z; ++local_z) {
                mc_terrain_cave_column column;
                bool inside = mc_terrain_cave_ellipsoid_column(&ellipse, normalized_x, local_x,
                                                               local_z, &column);
                int32_t index = column.initial_index;
                bool found_grass = false;
                if (inside) {
                    for (int32_t geometric_y = ellipse.high_y - 1; geometric_y >= ellipse.low_y;
                         --geometric_y, --index) {
                        if (!mc_terrain_cave_ellipsoid_contains(&ellipse, &column, geometric_y))
                            continue;
                        uint8_t block = context->blocks[(size_t)index];
                        if (block == CAVE_GRASS)
                            found_grass = true;
                        if (block == CAVE_STONE || block == CAVE_DIRT || block == CAVE_GRASS) {
                            if (geometric_y < 10) {
                                context->blocks[(size_t)index] = CAVE_FLOWING_LAVA;
                            } else {
                                context->blocks[(size_t)index] = CAVE_AIR;
                                if (found_grass &&
                                    context->blocks[(size_t)(index - 1)] == CAVE_DIRT)
                                    context->blocks[(size_t)(index - 1)] = CAVE_GRASS;
                            }
                        }
                    }
                }
            }
        }
        if (frame.room)
            break;
    }
}

static void cave_neighbor(mc_terrain_cave_context *context, int32_t source_x, int32_t source_z)
{
    int32_t bound = cave_bounded(context->random, 40) + 1;
    bound = cave_bounded(context->random, bound) + 1;
    int32_t systems = cave_bounded(context->random, bound);
    if (cave_bounded(context->random, 15) != 0) {
        systems = 0;
    }
    for (int32_t system = 0; system < systems; ++system) {
        int32_t offset = cave_bounded(context->random, 16);
        double x = (double)mc_java_i32_add(mc_java_i32_mul(source_x, 16), offset);
        bound = cave_bounded(context->random, 120) + 8;
        double y = (double)cave_bounded(context->random, bound);
        offset = cave_bounded(context->random, 16);
        double z = (double)mc_java_i32_add(mc_java_i32_mul(source_z, 16), offset);
        int32_t paths = 1;
        if (cave_bounded(context->random, 4) == 0) {
            mc_terrain_cave_room(context, x, y, z);
            paths = paths + cave_bounded(context->random, 4);
        }
        for (int32_t path = 0; path < paths; ++path) {
            float yaw = mc_java_random_next_float(context->random) * 3.1415927f;
            yaw = yaw * 2.0f;
            float pitch = mc_java_random_next_float(context->random) - 0.5f;
            pitch = pitch * 2.0f;
            pitch = pitch / 8.0f;
            float width = mc_java_random_next_float(context->random) * 2.0f;
            float extra_width = mc_java_random_next_float(context->random);
            width = width + extra_width;
            mc_terrain_cave_path(context, x, y, z, width, yaw, pitch, 0, 0, 1.0);
        }
    }
}

static bool cave_normal(const mc_math_trig *table, int64_t world_seed, int32_t chunk_x,
                        int32_t chunk_z, mc_java_random *cave_random, uint8_t *blocks,
                        size_t blocks_length, bool fresh)
{
    if (table == NULL || cave_random == NULL || blocks == NULL ||
        blocks_length < MC_TERRAIN_CAVE_BLOCKS || chunk_x < INT32_MIN + 8 ||
        chunk_x > INT32_MAX - 9 || chunk_z < INT32_MIN + 8 || chunk_z > INT32_MAX - 9) {
        return false;
    }
    mc_terrain_cave_context context = {table, cave_random, blocks, chunk_x, chunk_z};
    if (fresh)
        mc_java_random_seed(cave_random, world_seed);
    else
        mc_java_random_reseed(cave_random, world_seed);
    int64_t odd_x = mc_java_random_next_i64(cave_random) / 2;
    odd_x = mc_java_i64_add(mc_java_i64_mul(odd_x, 2), 1);
    int64_t odd_z = mc_java_random_next_i64(cave_random) / 2;
    odd_z = mc_java_i64_add(mc_java_i64_mul(odd_z, 2), 1);
    for (int32_t source_x = chunk_x - 8; source_x <= chunk_x + 8; ++source_x) {
        for (int32_t source_z = chunk_z - 8; source_z <= chunk_z + 8; ++source_z) {
            int64_t x_term = mc_java_i64_mul((int64_t)source_x, odd_x);
            int64_t z_term = mc_java_i64_mul((int64_t)source_z, odd_z);
            uint64_t seed_bits = (uint64_t)mc_java_i64_add(x_term, z_term) ^ (uint64_t)world_seed;
            mc_java_random_reseed(cave_random, cave_i64_from_bits(seed_bits));
            cave_neighbor(&context, source_x, source_z);
        }
    }
    return true;
}

bool mc_terrain_normal_caves(const mc_math_trig *table, int64_t world_seed, int32_t chunk_x,
                             int32_t chunk_z, mc_java_random *cave_random, uint8_t *blocks,
                             size_t blocks_length)
{
    return cave_normal(table, world_seed, chunk_x, chunk_z, cave_random, blocks, blocks_length,
                       true);
}

bool mc_terrain_normal_caves_reseed(const mc_math_trig *table, int64_t world_seed, int32_t chunk_x,
                                    int32_t chunk_z, mc_java_random *cave_random, uint8_t *blocks,
                                    size_t blocks_length)
{
    return cave_normal(table, world_seed, chunk_x, chunk_z, cave_random, blocks, blocks_length,
                       false);
}
