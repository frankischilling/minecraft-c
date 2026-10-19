#include "generation_internal.h"

#include "../../terrain/surface_internal.h"

static bool reference_failure(mc_world_provider *provider, mc_world_generation_error *error,
                              mc_registry_status status, unsigned int pc, const char *operation)
{
    error->surface_bytecode = pc;
    error->registry = (mc_registry_error){.status = status,
                                          .message = operation,
                                          .initializer_pc = -1,
                                          .block_id = -1,
                                          .constructor_pc = -1,
                                          .active_initializer_pc = -1,
                                          .item_argument = -1};
    return mc_world_generation_registry_fail(provider, error, status, operation);
}

bool mc_world_generation_surface(mc_world_provider *provider, int32_t x, int32_t z, uint8_t *blocks,
                                 const mc_biome_reference_array *biomes,
                                 mc_world_generation_error *error)
{
    mc_normal_generator *generator = &provider->generator;
    mc_java_random *random = &generator->generator_random;

    /* Each request completes before its actual r42/s80/t129 publication.
     * Fixed native backing is not an entry-wide future-source preflight. */
    const mc_terrain_double_span sand_output = {generator->sand, MC_TERRAIN_NORMAL_SURFACE_VALUES};
    if (!mc_terrain_normal_surface_channel(&generator->terrain, MC_TERRAIN_SURFACE_SAND, x, z,
                                           sand_output)) {
        error->surface_bytecode = 0;
        return mc_world_generation_fail(provider, error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                        "surface sand native backing");
    }
    provider->generation.r = sand_output;

    const mc_terrain_double_span gravel_output = {generator->gravel,
                                                  MC_TERRAIN_NORMAL_SURFACE_VALUES};
    if (!mc_terrain_normal_surface_channel(&generator->terrain, MC_TERRAIN_SURFACE_GRAVEL, x, z,
                                           gravel_output)) {
        error->surface_bytecode = 0;
        return mc_world_generation_fail(provider, error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                        "surface gravel native backing");
    }
    provider->generation.s = gravel_output;

    const mc_terrain_double_span thickness_output = {generator->thickness,
                                                     MC_TERRAIN_NORMAL_SURFACE_VALUES};
    if (!mc_terrain_normal_surface_channel(&generator->terrain, MC_TERRAIN_SURFACE_THICKNESS, x, z,
                                           thickness_output)) {
        error->surface_bytecode = 0;
        return mc_world_generation_fail(provider, error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                        "surface thickness native backing");
    }
    provider->generation.t = thickness_output;

    for (size_t local_z = 0; local_z < 16; ++local_z) {
        for (size_t local_x = 0; local_x < 16; ++local_x) {
            const size_t column = local_z + local_x * 16u;
            /* Source aaload162 captures this column's receiver before any
             * of its three draws. A NULL CELL remains valid until p282. */
            if (biomes == NULL)
                return reference_failure(provider, error, MC_REGISTRY_NULL_REFERENCE, 162,
                                         "surface Biome array is null");
            if (column >= mc_biome_reference_array_length(biomes))
                return reference_failure(provider, error, MC_REGISTRY_INDEX_OUT_OF_RANGE, 162,
                                         "surface Biome array index");
            const mc_biome *biome = mc_biome_reference_array_data(biomes)[column];

            const double sand_noise = provider->generation.r.data[column];
            const double sand_uniform = mc_java_random_next_double(random); /*182*/
            const bool sand = mc_terrain_surface_patch(sand_noise, sand_uniform, 0.0);
            const double gravel_noise = provider->generation.s.data[column];
            const double gravel_uniform = mc_java_random_next_double(random); /*219*/
            const bool gravel = mc_terrain_surface_patch(gravel_noise, gravel_uniform, 3.0);
            const double thickness_base =
                mc_terrain_surface_thickness_base(provider->generation.t.data[column]);
            const double thickness_uniform = mc_java_random_next_double(random); /*266*/
            const int32_t thickness =
                mc_terrain_surface_thickness_finish(thickness_base, thickness_uniform);
            int32_t remaining = -1;

            const mc_biome_fields *biome_fields = mc_biome_fields_get(biome);
            if (biome_fields == NULL)
                return reference_failure(provider, error, MC_REGISTRY_NULL_REFERENCE, 282,
                                         "surface Biome p receiver is null");
            int32_t top = (int32_t)biome_fields->p;
            int32_t filler = (int32_t)biome_fields->q;

            for (int32_t y = 127; y >= 0; --y) {
                const size_t index = column * 128u + (size_t)y;
                int32_t bedrock = 0;
                (void)mc_java_random_next_bounded(random, 5, &bedrock); /*328*/
                int32_t id;
                if (y <= bedrock) {
                    if (!mc_world_generation_block(provider, "A", 338, 341,
                                                   &error->surface_bytecode, &id, error))
                        return false;
                    blocks[index] = (uint8_t)mc_java_i8_narrow(id); /*344/345*/
                    continue;
                }

                const int32_t block = (int32_t)mc_java_i8_narrow(blocks[index]); /*352*/
                if (block == 0) {
                    remaining = -1;
                    continue;
                }
                if (!mc_world_generation_block(provider, "u", 368, 371, &error->surface_bytecode,
                                               &id, error))
                    return false;
                if (block != id)
                    continue;

                if (remaining == -1) {
                    if (thickness <= 0) {
                        top = 0;
                        if (!mc_world_generation_block(provider, "u", 391, 394,
                                                       &error->surface_bytecode, &id, error))
                            return false;
                        filler = (int32_t)mc_java_i8_narrow(id); /*397*/
                    } else if (y >= 60 && y <= 65) {
                        /* Instance reads423/430 use the SAME captured cell,
                         * after any reached Block active-use callbacks. */
                        biome_fields = mc_biome_fields_get(biome);
                        if (biome_fields == NULL)
                            return reference_failure(provider, error, MC_REGISTRY_NULL_REFERENCE,
                                                     423, "surface Biome p receiver is null");
                        top = (int32_t)biome_fields->p;
                        filler = (int32_t)biome_fields->q;
                        if (gravel) {
                            top = 0;
                            if (!mc_world_generation_block(provider, "G", 448, 451,
                                                           &error->surface_bytecode, &id, error))
                                return false;
                            filler = (int32_t)mc_java_i8_narrow(id); /*454*/
                        }
                        if (sand) {
                            if (!mc_world_generation_block(provider, "F", 462, 465,
                                                           &error->surface_bytecode, &id, error))
                                return false;
                            top = (int32_t)mc_java_i8_narrow(id); /*468*/
                            if (!mc_world_generation_block(provider, "F", 476, 479,
                                                           &error->surface_bytecode, &id, error))
                                return false;
                            filler = (int32_t)mc_java_i8_narrow(id); /*482*/
                        }
                    }
                    if (y < 64 && top == 0) {
                        if (!mc_world_generation_block(provider, "C", 497, 500,
                                                       &error->surface_bytecode, &id, error))
                            return false;
                        top = (int32_t)mc_java_i8_narrow(id); /*503*/
                    }
                    remaining = thickness;
                    blocks[index] = (uint8_t)(y >= 63 ? top : filler); /*524/533*/
                } else if (remaining > 0) {
                    --remaining;
                    blocks[index] = (uint8_t)filler; /*550 precedes F558*/
                    if (remaining == 0) {
                        if (!mc_world_generation_block(provider, "F", 558, 561,
                                                       &error->surface_bytecode, &id, error))
                            return false;
                        if (filler == id) {
                            (void)mc_java_random_next_bounded(random, 4, &remaining); /*572*/
                            if (!mc_world_generation_block(provider, "R", 577, 580,
                                                           &error->surface_bytecode, &id, error))
                                return false;
                            filler = (int32_t)mc_java_i8_narrow(id); /*583*/
                        }
                    }
                }
            }
        }
    }
    error->surface_bytecode = 0;
    return true;
}
