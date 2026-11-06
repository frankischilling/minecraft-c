#include "plants_internal.h"

#include <emmintrin.h>
#include <float.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && sizeof(float) == 4 && FLT_EVAL_METHOD == 0,
               "Crop growth requires source-width binary32 evaluation");

/* ni.i/ij.h reads all eight crop neighbors before inspecting any of their
 * equality relationships. Absolute loop bounds/increments use Java wrapping;
 * neither cached farmland IDs nor fixed-offset iteration replaces the source. */
static bool growth_factor(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                          int32_t z, float *out, mc_world_mutation_error *error) {
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, "ni", "ij", "i(Lfd;III)F", "h(Ldj;III)F");
    int32_t adjacent[8];
    const int32_t min_x = mc_java_i32_sub(x, 1), max_x = mc_java_i32_add(x, 1);
    const int32_t min_z = mc_java_i32_sub(z, 1), max_z = mc_java_i32_add(z, 1);
    if (!mc_plant_block(&frame, 10, x, y, min_z, &adjacent[0]) ||
        !mc_plant_block(&frame, 22, x, y, max_z, &adjacent[1]) ||
        !mc_plant_block(&frame, 34, min_x, y, z, &adjacent[2]) ||
        !mc_plant_block(&frame, 46, max_x, y, z, &adjacent[3]) ||
        !mc_plant_block(&frame, 60, min_x, y, min_z, &adjacent[4]) ||
        !mc_plant_block(&frame, 74, max_x, y, min_z, &adjacent[5]) ||
        !mc_plant_block(&frame, 88, max_x, y, max_z, &adjacent[6]) ||
        !mc_plant_block(&frame, 102, min_x, y, max_z, &adjacent[7]))
        return false;
    const bool x_crop = adjacent[2] == block->fields.bn || adjacent[3] == block->fields.bn;
    const bool z_crop = adjacent[0] == block->fields.bn || adjacent[1] == block->fields.bn;
    const bool diagonal = adjacent[4] == block->fields.bn || adjacent[5] == block->fields.bn ||
                          adjacent[6] == block->fields.bn || adjacent[7] == block->fields.bn;
    float factor = 1.0f;
    for (int32_t i = min_x; i <= max_x; i = mc_java_i32_add(i, 1)) {
        for (int32_t j = min_z; j <= max_z; j = mc_java_i32_add(j, 1)) {
            int32_t below;
            const int32_t below_y = mc_java_i32_sub(y, 1);
            if (!mc_plant_block(&frame, 236, i, below_y, j, &below))
                return false;
            const mc_block *farmland;
            mc_registry_error registry = mc_world_mutation_empty_registry();
            mc_plant_at(&frame, 246);
            if (mc_registry_block_static_get(block->classes, "aB", &farmland, &registry) !=
                MC_REGISTRY_OK) {
                return mc_world_mutation_registry_fail(world, error, &registry,
                                                       "Crop growth farmland static");
            }
            if (farmland == NULL) {
                mc_plant_at(&frame, 249);
                registry.status = MC_REGISTRY_NULL_REFERENCE;
                registry.message = "Crop growth farmland receiver is null";
                return mc_world_mutation_registry_fail(world, error, &registry, registry.message);
            }
            float contribution = 0.0f;
            if (below == farmland->fields.bn) {
                contribution = 1.0f;
                int32_t moisture;
                if (!mc_plant_metadata(&frame, 266, i, below_y, j, &moisture))
                    return false;
                if (moisture > 0)
                    contribution = 3.0f;
            }
            if (i != x || j != z)
                contribution = contribution / 4.0f;
            factor = factor + contribution;
        }
    }
    if (diagonal || (x_crop && z_crop))
        factor = factor / 2.0f;
    *out = factor;
    return mc_plant_done(&frame);
}

bool mc_plant_crop_tick(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                        int32_t z, mc_java_random *random, mc_world_mutation_error *error) {
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, "ni", "ij", "a(Lfd;IIILjava/util/Random;)V",
                            "a(Ldj;IIILjava/util/Random;)V");
    /* The parent tick does not consume Random. Its removal does not prevent
     * the subclass from executing its subsequent light and metadata queries. */
    if (!mc_plant_flower_survival(world, block, x, y, z, error))
        return mc_plant_nested(&frame, 8, error);
    int32_t light;
    if (!mc_plant_light(&frame, 18, x, mc_java_i32_add(y, 1), z, &light))
        return false;
    if (light < 9)
        return mc_plant_done(&frame);
    int32_t metadata;
    if (!mc_plant_metadata(&frame, 31, x, y, z, &metadata))
        return false;
    if (metadata >= 7)
        return mc_plant_done(&frame);
    float factor;
    if (!growth_factor(world, block, x, y, z, &factor, error))
        return mc_plant_nested(&frame, 49, error);
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return mc_plant_fail(&frame, 60, "Crop growth floating environment");
    const int32_t bound = mc_java_float_to_i32(100.0f / factor);
    int32_t draw;
    if (!mc_plant_random(&frame, 62, random, bound, &draw))
        return false;
    if (draw == 0 && !mc_plant_set_metadata(&frame, 78, x, y, z, mc_java_i32_add(metadata, 1)))
        return false;
    return mc_plant_done(&frame);
}
