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

void mc_terrain_cave_path(mc_terrain_cave_context *context, double x, double y, double z,
                          float width, float yaw, float pitch, int32_t step, int32_t end,
                          double vertical_ratio)
{
    int32_t origin_x = mc_java_i32_mul(context->chunk_x, 16);
    int32_t origin_z = mc_java_i32_mul(context->chunk_z, 16);
    double target_x = (double)mc_java_i32_add(origin_x, 8);
    double target_z = (double)mc_java_i32_add(origin_z, 8);
    float yaw_velocity = 0.0f;
    float pitch_velocity = 0.0f;
    mc_java_random local;
    mc_java_random_seed(&local, mc_java_random_next_i64(context->random));
    if (end <= 0) {
        end = 112 - cave_bounded(&local, 28);
    }
    bool room = false;
    if (step == -1) {
        step = end / 2;
        room = true;
    }
    int32_t fork_step = cave_bounded(&local, end / 2) + end / 4;
    bool slow_pitch = cave_bounded(&local, 6) == 0;
    for (; step < end; ++step) {
        float phase = (float)step * 3.1415927f;
        phase = phase / (float)end;
        float radius_wave = mc_math_sin(context->table, phase) * width;
        radius_wave = radius_wave * 1.0f;
        double radius = 1.5 + (double)radius_wave;
        double vertical_radius = radius * vertical_ratio;
        float cosine_pitch = mc_math_cos(context->table, pitch);
        float sine_pitch = mc_math_sin(context->table, pitch);
        float movement_x = mc_math_cos(context->table, yaw) * cosine_pitch;
        x = x + (double)movement_x;
        y = y + (double)sine_pitch;
        float movement_z = mc_math_sin(context->table, yaw) * cosine_pitch;
        z = z + (double)movement_z;
        pitch = pitch * (slow_pitch ? 0.92f : 0.7f);
        float pitch_change = pitch_velocity * 0.1f;
        pitch = pitch + pitch_change;
        float yaw_change = yaw_velocity * 0.1f;
        yaw = yaw + yaw_change;
        pitch_velocity = pitch_velocity * 0.9f;
        yaw_velocity = yaw_velocity * 0.75f;

        float first = mc_java_random_next_float(&local);
        float second = mc_java_random_next_float(&local);
        float third = mc_java_random_next_float(&local);
        float velocity_change = first - second;
        velocity_change = velocity_change * third;
        velocity_change = velocity_change * 2.0f;
        pitch_velocity = pitch_velocity + velocity_change;
        first = mc_java_random_next_float(&local);
        second = mc_java_random_next_float(&local);
        third = mc_java_random_next_float(&local);
        velocity_change = first - second;
        velocity_change = velocity_change * third;
        velocity_change = velocity_change * 4.0f;
        yaw_velocity = yaw_velocity + velocity_change;

        if (!room && step == fork_step && width > 1.0f) {
            float child_width = mc_java_random_next_float(&local) * 0.5f;
            child_width = child_width + 0.5f;
            float child_yaw = yaw - 1.5707964f;
            float child_pitch = pitch / 3.0f;
            mc_terrain_cave_path(context, x, y, z, child_width, child_yaw, child_pitch, step, end,
                                 1.0);
            child_width = mc_java_random_next_float(&local) * 0.5f;
            child_width = child_width + 0.5f;
            child_yaw = yaw + 1.5707964f;
            child_pitch = pitch / 3.0f;
            mc_terrain_cave_path(context, x, y, z, child_width, child_yaw, child_pitch, step, end,
                                 1.0);
            return;
        }
        if (!room && cave_bounded(&local, 4) == 0) {
            continue;
        }
        double delta_x = x - target_x;
        double delta_z = z - target_z;
        double remaining = (double)(end - step);
        float reach_float = width + 2.0f;
        reach_float = reach_float + 16.0f;
        double reach = (double)reach_float;
        double distance = delta_x * delta_x + delta_z * delta_z;
        distance = distance - remaining * remaining;
        if (distance > reach * reach) {
            return;
        }
        if (x < target_x - 16.0 - radius * 2.0 || z < target_z - 16.0 - radius * 2.0 ||
            x > target_x + 16.0 + radius * 2.0 || z > target_z + 16.0 + radius * 2.0) {
            continue;
        }

        int32_t low_x =
            mc_java_i32_sub(mc_java_i32_sub(mc_math_floor_double(x - radius), origin_x), 1);
        int32_t high_x =
            mc_java_i32_add(mc_java_i32_sub(mc_math_floor_double(x + radius), origin_x), 1);
        int32_t low_y = mc_java_i32_sub(mc_math_floor_double(y - vertical_radius), 1);
        int32_t high_y = mc_java_i32_add(mc_math_floor_double(y + vertical_radius), 1);
        int32_t low_z =
            mc_java_i32_sub(mc_java_i32_sub(mc_math_floor_double(z - radius), origin_z), 1);
        int32_t high_z =
            mc_java_i32_add(mc_java_i32_sub(mc_math_floor_double(z + radius), origin_z), 1);
        if (low_x < 0) {
            low_x = 0;
        }
        if (high_x > 16) {
            high_x = 16;
        }
        if (low_y < 1) {
            low_y = 1;
        }
        if (high_y > 120) {
            high_y = 120;
        }
        if (low_z < 0) {
            low_z = 0;
        }
        if (high_z > 16) {
            high_z = 16;
        }

        bool water = false;
        for (int32_t local_x = low_x; !water && local_x < high_x; ++local_x) {
            for (int32_t local_z = low_z; !water && local_z < high_z; ++local_z) {
                for (int32_t water_y = high_y + 1; !water && water_y >= low_y - 1; --water_y) {
                    if (water_y < 0 || water_y >= 128) {
                        continue;
                    }
                    size_t index = (size_t)((local_x * 16 + local_z) * 128 + water_y);
                    uint8_t block = context->blocks[index];
                    if (block == CAVE_FLOWING_WATER || block == CAVE_WATER) {
                        water = true;
                    }
                    if (water_y != low_y - 1 && local_x != low_x && local_x != high_x - 1 &&
                        local_z != low_z && local_z != high_z - 1) {
                        water_y = low_y;
                    }
                }
            }
        }
        if (water) {
            continue;
        }
        for (int32_t local_x = low_x; local_x < high_x; ++local_x) {
            double normalized_x = (double)mc_java_i32_add(local_x, origin_x) + 0.5;
            normalized_x = normalized_x - x;
            normalized_x = normalized_x / radius;
            for (int32_t local_z = low_z; local_z < high_z; ++local_z) {
                double normalized_z = (double)mc_java_i32_add(local_z, origin_z) + 0.5;
                normalized_z = normalized_z - z;
                normalized_z = normalized_z / radius;
                int32_t index = (local_x * 16 + local_z) * 128 + high_y;
                bool found_grass = false;
                if (normalized_x * normalized_x + normalized_z * normalized_z < 1.0) {
                    for (int32_t geometric_y = high_y - 1; geometric_y >= low_y;
                         --geometric_y, --index) {
                        double normalized_y = (double)geometric_y + 0.5;
                        normalized_y = normalized_y - y;
                        normalized_y = normalized_y / vertical_radius;
                        if (!(normalized_y > -0.7)) {
                            continue;
                        }
                        double ellipse = normalized_x * normalized_x + normalized_y * normalized_y;
                        ellipse = ellipse + normalized_z * normalized_z;
                        if (ellipse < 1.0) {
                            uint8_t block = context->blocks[(size_t)index];
                            if (block == CAVE_GRASS) {
                                found_grass = true;
                            }
                            if (block == CAVE_STONE || block == CAVE_DIRT || block == CAVE_GRASS) {
                                if (geometric_y < 10) {
                                    context->blocks[(size_t)index] = CAVE_FLOWING_LAVA;
                                } else {
                                    context->blocks[(size_t)index] = CAVE_AIR;
                                    if (found_grass &&
                                        context->blocks[(size_t)(index - 1)] == CAVE_DIRT) {
                                        context->blocks[(size_t)(index - 1)] = CAVE_GRASS;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if (room) {
            break;
        }
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

bool mc_terrain_normal_caves(const mc_math_trig *table, int64_t world_seed, int32_t chunk_x,
                             int32_t chunk_z, mc_java_random *cave_random, uint8_t *blocks,
                             size_t blocks_length)
{
    if (table == NULL || cave_random == NULL || blocks == NULL ||
        blocks_length < MC_TERRAIN_CAVE_BLOCKS || chunk_x < INT32_MIN + 8 ||
        chunk_x > INT32_MAX - 9 || chunk_z < INT32_MIN + 8 || chunk_z > INT32_MAX - 9) {
        return false;
    }
    mc_terrain_cave_context context = {table, cave_random, blocks, chunk_x, chunk_z};
    mc_java_random_seed(cave_random, world_seed);
    int64_t odd_x = mc_java_random_next_i64(cave_random) / 2;
    odd_x = mc_java_i64_add(mc_java_i64_mul(odd_x, 2), 1);
    int64_t odd_z = mc_java_random_next_i64(cave_random) / 2;
    odd_z = mc_java_i64_add(mc_java_i64_mul(odd_z, 2), 1);
    for (int32_t source_x = chunk_x - 8; source_x <= chunk_x + 8; ++source_x) {
        for (int32_t source_z = chunk_z - 8; source_z <= chunk_z + 8; ++source_z) {
            int64_t x_term = mc_java_i64_mul((int64_t)source_x, odd_x);
            int64_t z_term = mc_java_i64_mul((int64_t)source_z, odd_z);
            uint64_t seed_bits = (uint64_t)mc_java_i64_add(x_term, z_term) ^ (uint64_t)world_seed;
            mc_java_random_seed(cave_random, cave_i64_from_bits(seed_bits));
            cave_neighbor(&context, source_x, source_z);
        }
    }
    return true;
}
