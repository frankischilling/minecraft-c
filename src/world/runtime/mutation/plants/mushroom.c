#include "plants_internal.h"

bool mc_plant_mushroom_tick(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                            int32_t z, mc_java_random *random, mc_world_mutation_error *error) {
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, "tl", "md", "a(Lfd;IIILjava/util/Random;)V",
                            "a(Ldj;IIILjava/util/Random;)V");
    int32_t draw, candidate_x, candidate_y, candidate_z;
    if (!mc_plant_random(&frame, 4, random, 100, &draw))
        return false;
    if (draw != 0)
        return mc_plant_done(&frame);
    if (!mc_plant_random(&frame, 14, random, 3, &draw))
        return false;
    candidate_x = mc_java_i32_sub(mc_java_i32_add(x, draw), 1);
    if (!mc_plant_random(&frame, 26, random, 2, &draw))
        return false;
    candidate_y = mc_java_i32_add(y, draw);
    if (!mc_plant_random(&frame, 33, random, 2, &draw))
        return false;
    candidate_y = mc_java_i32_sub(candidate_y, draw);
    if (!mc_plant_random(&frame, 44, random, 3, &draw))
        return false;
    candidate_z = mc_java_i32_sub(mc_java_i32_add(z, draw), 1);
    bool accepted;
    if (!mc_plant_air(&frame, 59, candidate_x, candidate_y, candidate_z, &accepted))
        return false;
    if (!accepted)
        return mc_plant_done(&frame);
    if (!mc_plant_survives(&frame, 73, candidate_x, candidate_y, candidate_z, &accepted))
        return false;
    if (!accepted)
        return mc_plant_done(&frame);
    /* Both draws update dead source locals. Their consumption is observable;
     * the second check still uses the ORIGINAL candidate, not those locals. */
    if (!mc_plant_random(&frame, 83, random, 3, &draw) ||
        !mc_plant_random(&frame, 95, random, 3, &draw))
        return false;
    if (!mc_plant_air(&frame, 110, candidate_x, candidate_y, candidate_z, &accepted))
        return false;
    if (!accepted)
        return mc_plant_done(&frame);
    if (!mc_plant_survives(&frame, 124, candidate_x, candidate_y, candidate_z, &accepted))
        return false;
    if (accepted &&
        !mc_plant_set(&frame, 141, candidate_x, candidate_y, candidate_z, block->fields.bn))
        return false;
    return mc_plant_done(&frame);
}
