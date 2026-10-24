#include "generation_internal.h"

#include "../../terrain/caves_internal.h"

#include <limits.h>

static int32_t bounded(mc_java_random *random, int32_t bound) {
    int32_t value = 0;
    /* Actual fixed range8 cave forms keep every reached bound positive. */
    (void)mc_java_random_next_bounded(random, bound, &value);
    return value;
}

static int64_t signed_bits(uint64_t bits) {
    if (bits <= INT64_MAX)
        return (int64_t)bits;
    return -1 - (int64_t)(UINT64_MAX - bits);
}

static int32_t byte_load(const uint8_t *blocks, int32_t index) {
    return (int32_t)mc_java_i8_narrow((int64_t)blocks[(size_t)index]);
}

static bool water_scan(mc_chunk_provider *provider, uint8_t *blocks,
                       const mc_terrain_cave_ellipsoid *ellipse, bool *out,
                       mc_world_generation_error *error) {
    bool water = false;
    for (int32_t x = ellipse->low_x; !water && x < ellipse->high_x; x = mc_java_i32_add(x, 1)) {
        for (int32_t z = ellipse->low_z; !water && z < ellipse->high_z; z = mc_java_i32_add(z, 1)) {
            for (int32_t y = mc_java_i32_add(ellipse->high_y, 1);
                 !water && y >= mc_java_i32_sub(ellipse->low_y, 1); y = mc_java_i32_sub(y, 1)) {
                /* Source computes index791 before checking Y793..803. */
                const int32_t index = mc_java_i32_add(
                    mc_java_i32_mul(mc_java_i32_add(mc_java_i32_mul(x, 16), z), 128), y);
                if (y < 0 || y >= 128)
                    continue;
                error->cave_path_bytecode = 812;
                int32_t block = byte_load(blocks, index);
                int32_t id;
                if (!mc_world_generation_block(provider, "B", 813, 816, &error->cave_path_bytecode,
                                               &id, error))
                    return false;
                if (block == id) {
                    water = true;
                } else {
                    /* A distinct baload825 follows the first active use. */
                    error->cave_path_bytecode = 825;
                    block = byte_load(blocks, index);
                    if (!mc_world_generation_block(provider, "C", 826, 829,
                                                   &error->cave_path_bytecode, &id, error))
                        return false;
                    if (block == id)
                        water = true;
                }
                if (y != mc_java_i32_sub(ellipse->low_y, 1) && x != ellipse->low_x &&
                    x != mc_java_i32_sub(ellipse->high_x, 1) && z != ellipse->low_z &&
                    z != mc_java_i32_sub(ellipse->high_z, 1))
                    y = ellipse->low_y;
            }
        }
    }
    *out = water;
    return true;
}

static bool carve(mc_chunk_provider *provider, uint8_t *blocks,
                  const mc_terrain_cave_ellipsoid *ellipse, mc_world_generation_error *error) {
    for (int32_t x = ellipse->low_x; x < ellipse->high_x; x = mc_java_i32_add(x, 1)) {
        const double normalized_x = mc_terrain_cave_ellipsoid_x(ellipse, x);
        for (int32_t z = ellipse->low_z; z < ellipse->high_z; z = mc_java_i32_add(z, 1)) {
            mc_terrain_cave_column column;
            const bool inside =
                mc_terrain_cave_ellipsoid_column(ellipse, normalized_x, x, z, &column);
            int32_t index = column.initial_index;
            bool found_grass = false;
            if (!inside)
                continue;
            for (int32_t y = mc_java_i32_sub(ellipse->high_y, 1); y >= ellipse->low_y;
                 y = mc_java_i32_sub(y, 1), index = mc_java_i32_sub(index, 1)) {
                if (!mc_terrain_cave_ellipsoid_contains(ellipse, &column, y))
                    continue;
                error->cave_path_bytecode = 1069;
                const int32_t block = byte_load(blocks, index);
                int32_t id;
                if (!mc_world_generation_block(provider, "v", 1074, 1077,
                                               &error->cave_path_bytecode, &id, error))
                    return false;
                if (block == id)
                    found_grass = true;

                if (!mc_world_generation_block(provider, "u", 1088, 1091,
                                               &error->cave_path_bytecode, &id, error))
                    return false;
                bool replace = block == id;
                if (!replace) {
                    if (!mc_world_generation_block(provider, "w", 1099, 1102,
                                                   &error->cave_path_bytecode, &id, error))
                        return false;
                    replace = block == id;
                }
                if (!replace) {
                    if (!mc_world_generation_block(provider, "v", 1110, 1113,
                                                   &error->cave_path_bytecode, &id, error))
                        return false;
                    replace = block == id;
                }
                if (!replace)
                    continue;

                if (y < 10) {
                    if (!mc_world_generation_block(provider, "D", 1129, 1132,
                                                   &error->cave_path_bytecode, &id, error))
                        return false;
                    blocks[(size_t)index] = (uint8_t)mc_java_i8_narrow((int64_t)id);
                } else {
                    blocks[(size_t)index] = 0; /* Actual air store1144. */
                    if (found_grass) {
                        const int32_t below = mc_java_i32_sub(index, 1);
                        error->cave_path_bytecode = 1155;
                        const int32_t block_below = byte_load(blocks, below);
                        if (!mc_world_generation_block(provider, "w", 1156, 1159,
                                                       &error->cave_path_bytecode, &id, error))
                            return false;
                        if (block_below == id) {
                            if (!mc_world_generation_block(provider, "v", 1170, 1173,
                                                           &error->cave_path_bytecode, &id, error))
                                return false;
                            blocks[(size_t)below] = (uint8_t)mc_java_i8_narrow((int64_t)id);
                        }
                    }
                }
            }
        }
    }
    return true;
}

static bool path(mc_chunk_provider *provider, uint8_t *blocks,
                 const mc_terrain_cave_path_args *args, mc_world_generation_error *error) {
    mc_terrain_cave_path_frame frame;
    mc_terrain_cave_path_begin(&frame, &provider->generator.cave_random, args);
    for (; frame.current.step < frame.current.end;
         frame.current.step = mc_java_i32_add(frame.current.step, 1)) {
        mc_terrain_cave_ellipsoid ellipse;
        const mc_terrain_cave_step outcome =
            mc_terrain_cave_path_step(&frame, provider->generator.trig, &ellipse);
        if (outcome == MC_TERRAIN_CAVE_END)
            return true;
        if (outcome == MC_TERRAIN_CAVE_FORK) {
            mc_terrain_cave_path_args child;
            mc_terrain_cave_path_child(&frame, 0, &child);
            error->cave_path_bytecode = 373;
            if (!path(provider, blocks, &child, error))
                return false;
            /* A failing first child withholds both this width draw and the
             * second child's shared nextLong at its own path begin. */
            mc_terrain_cave_path_child(&frame, 1, &child);
            error->cave_path_bytecode = 412;
            return path(provider, blocks, &child, error);
        }
        if (outcome == MC_TERRAIN_CAVE_SKIP)
            continue;
        bool water;
        if (!water_scan(provider, blocks, &ellipse, &water, error))
            return false;
        if (water)
            continue;
        if (!carve(provider, blocks, &ellipse, error))
            return false;
        /* Room return1204 is reached only after the dry carve pass. */
        if (frame.room)
            return true;
    }
    return true;
}

static bool neighbor(mc_chunk_provider *provider, int32_t source_x, int32_t source_z,
                     int32_t chunk_x, int32_t chunk_z, uint8_t *blocks,
                     mc_world_generation_error *error) {
    mc_java_random *random = &provider->generator.cave_random;
    int32_t bound = bounded(random, 40) + 1;
    bound = bounded(random, bound) + 1;
    int32_t systems = bounded(random, bound);
    if (bounded(random, 15) != 0)
        systems = 0;
    for (int32_t system = 0; system < systems; system = mc_java_i32_add(system, 1)) {
        int32_t offset = bounded(random, 16);
        const double x = (double)mc_java_i32_add(mc_java_i32_mul(source_x, 16), offset);
        bound = bounded(random, 120) + 8;
        const double y = (double)bounded(random, bound);
        offset = bounded(random, 16);
        const double z = (double)mc_java_i32_add(mc_java_i32_mul(source_z, 16), offset);
        int32_t paths = 1;
        if (bounded(random, 4) == 0) {
            float width = mc_java_random_next_float(random) * 6.0f;
            width = 1.0f + width;
            const mc_terrain_cave_path_args room = {.chunk_x = chunk_x,
                                                    .chunk_z = chunk_z,
                                                    .x = x,
                                                    .y = y,
                                                    .z = z,
                                                    .vertical_ratio = 0.5,
                                                    .width = width,
                                                    .yaw = 0.0f,
                                                    .pitch = 0.0f,
                                                    .step = -1,
                                                    .end = -1};
            error->cave_chunk_bytecode = 137;
            error->cave_path_bytecode = 29;
            if (!path(provider, blocks, &room, error))
                return false;
            paths = paths + bounded(random, 4); /* Only after room returns147. */
        }
        for (int32_t index = 0; index < paths; index = mc_java_i32_add(index, 1)) {
            float yaw = mc_java_random_next_float(random) * 3.1415927f;
            yaw = yaw * 2.0f;
            float pitch = mc_java_random_next_float(random) - 0.5f;
            pitch = pitch * 2.0f;
            pitch = pitch / 8.0f;
            float width = mc_java_random_next_float(random) * 2.0f;
            const float extra_width = mc_java_random_next_float(random);
            width = width + extra_width;
            const mc_terrain_cave_path_args ordinary = {.chunk_x = chunk_x,
                                                        .chunk_z = chunk_z,
                                                        .x = x,
                                                        .y = y,
                                                        .z = z,
                                                        .vertical_ratio = 1.0,
                                                        .width = width,
                                                        .yaw = yaw,
                                                        .pitch = pitch,
                                                        .step = 0,
                                                        .end = 0};
            error->cave_chunk_bytecode = 235;
            error->cave_path_bytecode = 0;
            if (!path(provider, blocks, &ordinary, error))
                return false;
        }
    }
    return true;
}

static bool work_unit(mc_chunk_provider *provider, size_t *used, mc_world_generation_error *error) {
    if (*used == (size_t)MC_WORLD_GENERATION_CAVE_WORK_LIMIT) {
        error->cave_scheduler_bytecode = 123;
        error->cave_chunk_bytecode = 0;
        error->cave_path_bytecode = 0;
        return mc_world_generation_fail(provider, error, MC_WORLD_STORAGE_LIMIT,
                                        "GAME cave scheduler work limit");
    }
    ++*used;
    return true;
}

bool mc_world_generation_caves(mc_chunk_provider *provider, int32_t x, int32_t z, uint8_t *blocks,
                               mc_world_generation_error *error) {
    /* Enclosing generation already owns its actual World/Chunk call holds.
     * Capture source range1 once; no ordinary-coordinate whitelist is added. */
    const int32_t range = provider->generator.cave_range;
    mc_java_random *random = &provider->generator.cave_random;
    error->cave_scheduler_bytecode = 0;
    error->cave_chunk_bytecode = 0;
    error->cave_path_bytecode = 0;
    int64_t seed;
    if (!mc_world_generation_seed(provider, 11, &error->cave_scheduler_bytecode, &seed, error))
        return false;
    mc_java_random_reseed(random, seed);
    int64_t odd_x = mc_java_random_next_i64(random) / 2;
    odd_x = mc_java_i64_add(mc_java_i64_mul(odd_x, 2), 1);
    int64_t odd_z = mc_java_random_next_i64(random) / 2;
    odd_z = mc_java_i64_add(mc_java_i64_mul(odd_z, 2), 1);

    /* One unit per entered outer body and one per entered neighbor body:
     * ordinary range8 consumes17+289=306; wrapped empty-inner loops are also
     * bounded without changing their initial reseed/two-nextLong prefix. */
    size_t used = 0;
    for (int32_t source_x = mc_java_i32_sub(x, range); source_x <= mc_java_i32_add(x, range);
         source_x = mc_java_i32_add(source_x, 1)) {
        if (!work_unit(provider, &used, error))
            return false;
        for (int32_t source_z = mc_java_i32_sub(z, range); source_z <= mc_java_i32_add(z, range);
             source_z = mc_java_i32_add(source_z, 1)) {
            if (!work_unit(provider, &used, error))
                return false;
            const int64_t x_term = mc_java_i64_mul((int64_t)source_x, odd_x);
            const int64_t z_term = mc_java_i64_mul((int64_t)source_z, odd_z);
            const int64_t sum = mc_java_i64_add(x_term, z_term);
            error->cave_chunk_bytecode = 0;
            error->cave_path_bytecode = 0;
            if (!mc_world_generation_seed(provider, 105, &error->cave_scheduler_bytecode, &seed,
                                          error))
                return false;
            const uint64_t seed_bits = (uint64_t)sum ^ (uint64_t)seed;
            mc_java_random_reseed(random, signed_bits(seed_bits));
            error->cave_scheduler_bytecode = 123;
            if (!neighbor(provider, source_x, source_z, x, z, blocks, error))
                return false;
        }
    }
    return true;
}
