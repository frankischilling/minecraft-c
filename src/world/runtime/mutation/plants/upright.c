#include "plants_internal.h"

bool mc_plant_upright_tick(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                           int32_t z, mc_world_mutation_error *error) {
    const bool cactus = block->kind == MC_BLOCK_CACTUS;
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, cactus ? "or" : "ri", cactus ? "je" : "ko",
                            "a(Lfd;IIILjava/util/Random;)V", "a(Ldj;IIILjava/util/Random;)V");
    const int32_t above = mc_java_i32_add(y, 1);
    bool air;
    if (!mc_plant_air(&frame, 7, x, above, z, &air))
        return false;
    if (!air)
        return mc_plant_done(&frame);
    int32_t height = 1;
    for (;;) {
        int32_t below;
        if (!mc_plant_block(&frame, 24, x, mc_java_i32_sub(y, height), z, &below))
            return false;
        if (below != block->fields.bn)
            break;
        height = mc_java_i32_add(height, 1);
    }
    if (height >= 3)
        return mc_plant_done(&frame);
    int32_t metadata;
    if (!mc_plant_metadata(&frame, 51, x, y, z, &metadata))
        return false;
    if (metadata == 15) {
        if (!mc_plant_set(&frame, 74, x, above, z, block->fields.bn) ||
            !mc_plant_set_metadata(&frame, 84, x, y, z, 0))
            return false;
    } else if (!mc_plant_set_metadata(&frame, 99, x, y, z, mc_java_i32_add(metadata, 1))) {
        return false;
    }
    return mc_plant_done(&frame);
}
