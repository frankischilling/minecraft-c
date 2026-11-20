#include "plants_internal.h"

#include "mc/world/runtime/population/trees.h"

typedef enum sapling_tree_kind {
    SAPLING_TREE_OAK,
    SAPLING_TREE_BIRCH,
    SAPLING_TREE_TAIGA_DENSE,
    SAPLING_TREE_BIG_OAK
} sapling_tree_kind;

/* A tree's actual failed mutation child is copied even when no setter was
 * reached. The separate tree prefix carries its source method without making
 * mutation and feature error owners recursively contain one another. */
static bool sapling_tree_failed(const mc_plant_frame *frame, unsigned int pc,
                                const mc_world_tree_error *tree) {
    mc_world_mutation_error lower = tree->mutation;
    lower.status = tree->status;
    lower.operation = tree->operation;
    lower.cause = tree->cause;
    lower.registry = tree->registry;
    lower.plant_tree = (mc_world_plant_tree_error){.reached = true,
                                                   .native_boundary = tree->native_boundary,
                                                   .feature_class = tree->feature_class,
                                                   .method = tree->method,
                                                   .bytecode = tree->bytecode,
                                                   .entry_bytecode = tree->entry_bytecode,
                                                   .profile_status = tree->profile_status};
    if (lower.cause_hook_class == NULL) {
        if (lower.hook_class != NULL) {
            lower.cause_hook_class = lower.hook_class;
            lower.cause_hook_method = lower.hook_method;
            lower.cause_hook_bytecode = lower.hook_bytecode;
        } else {
            lower.cause_hook_class = tree->feature_class;
            lower.cause_hook_method = tree->method;
            lower.cause_hook_bytecode = tree->bytecode;
        }
    }
    return mc_plant_nested(frame, pc, &lower);
}

/* he.c/eu.b do not check B or scale the newly selected tree. In particular,
 * big oak retains constructor leaf distance four rather than population's
 * scaled distance five. The masked species remains captured across every
 * acquiring setter and feature callback. */
bool mc_plant_sapling_grow(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                           int32_t z, mc_java_random *random, mc_world_mutation_error *error) {
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, "he", "eu", "c(Lfd;IIILjava/util/Random;)V",
                            "b(Ldj;IIILjava/util/Random;)V");
    int32_t metadata;
    if (!mc_plant_metadata(&frame, 5, x, y, z, &metadata))
        return false;
    const int32_t species = (int32_t)((uint32_t)metadata & 3u);
    bool ignored;
    mc_world_mutation_error lower = mc_world_mutation_empty_error();
    mc_plant_at(&frame, 18);
    if (mc_world_runtime_block_set_quiet(world, x, y, z, 0, &ignored, &lower) !=
        MC_WORLD_STORAGE_OK)
        return mc_plant_nested(&frame, 18, &lower);

    sapling_tree_kind kind = SAPLING_TREE_OAK;
    mc_world_tree_big_oak *big = NULL;
    mc_world_tree_error tree = {0};
    bool completed = false;
    if (species == 1) {
        mc_plant_at(&frame, 35); /* Actual empty ws/on constructor. */
        kind = SAPLING_TREE_TAIGA_DENSE;
    } else if (species == 2) {
        mc_plant_at(&frame, 53); /* Actual empty k/h constructor. */
        kind = SAPLING_TREE_BIRCH;
    } else {
        mc_plant_at(&frame, 65); /* Oak is constructed before the selector draw. */
        int32_t draw;
        if (!mc_plant_random(&frame, 74, random, 10, &draw))
            goto leave;
        if (draw == 0) {
            mc_plant_at(&frame, 84);
            if (mc_world_tree_big_oak_create(world, NULL, &big, &tree) != MC_WORLD_STORAGE_OK) {
                (void)sapling_tree_failed(&frame, 84, &tree);
                goto leave;
            }
            kind = SAPLING_TREE_BIG_OAK;
        }
    }

    mc_plant_at(&frame, 98);
    bool placed;
    mc_world_storage_status status;
    switch (kind) {
        case SAPLING_TREE_OAK:
            status = mc_world_tree_oak(world, random, x, y, z, &placed, &tree);
            break;
        case SAPLING_TREE_BIRCH:
            status = mc_world_tree_birch(world, random, x, y, z, &placed, &tree);
            break;
        case SAPLING_TREE_TAIGA_DENSE:
            status = mc_world_tree_taiga_dense(world, random, x, y, z, &placed, &tree);
            break;
        case SAPLING_TREE_BIG_OAK:
            status = mc_world_tree_big_oak_place(big, world, random, x, y, z, &placed, &tree);
            break;
        default:
            (void)mc_plant_fail(&frame, 98, "Sapling selected tree native kind");
            goto leave;
    }
    if (status != MC_WORLD_STORAGE_OK) {
        (void)sapling_tree_failed(&frame, 98, &tree);
        goto leave;
    }
    if (!placed) {
        mc_plant_at(&frame, 110);
        const int32_t id = block->fields.bn;
        lower = mc_world_mutation_empty_error();
        mc_plant_at(&frame, 115);
        if (mc_world_runtime_block_metadata_set_quiet(world, x, y, z, id, species, &ignored,
                                                      &lower) != MC_WORLD_STORAGE_OK) {
            (void)mc_plant_nested(&frame, 115, &lower);
            goto leave;
        }
    }
    completed = mc_plant_done(&frame);
leave:
    mc_world_tree_big_oak_dispose(big);
    return completed;
}

bool mc_plant_sapling_tick(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                           int32_t z, mc_java_random *random, mc_world_mutation_error *error) {
    const mc_plant_frame frame =
        mc_plant_frame_make(world, block, error, "he", "eu", "a(Lfd;IIILjava/util/Random;)V",
                            "a(Ldj;IIILjava/util/Random;)V");
    mc_plant_at(&frame, 1);
    if (world == NULL)
        return mc_plant_fail(&frame, 1, "Sapling tick World receiver is null");
    if (world->fields.B)
        return mc_plant_done(&frame);

    if (!mc_plant_flower_survival(world, block, x, y, z, error)) {
        const mc_plant_frame parent =
            mc_plant_frame_make(world, block, error, "wb", "nw", "a(Lfd;IIILjava/util/Random;)V",
                                "a(Ldj;IIILjava/util/Random;)V");
        (void)mc_plant_nested(&parent, 6, error);
        return mc_plant_nested(&frame, 16, error);
    }
    /* The source continues here even when the inherited helper removed its
     * receiver's cell. No current-ID check or early removal return occurs. */
    int32_t light;
    if (!mc_plant_light(&frame, 26, x, mc_java_i32_add(y, 1), z, &light))
        return false;
    if (light < 9)
        return mc_plant_done(&frame);
    int32_t draw;
    if (!mc_plant_random(&frame, 38, random, 30, &draw))
        return false;
    if (draw != 0)
        return mc_plant_done(&frame);
    int32_t metadata;
    if (!mc_plant_metadata(&frame, 49, x, y, z, &metadata))
        return false;
    if (((uint32_t)metadata & 8u) == 0) {
        if (!mc_plant_set_metadata(&frame, 72, x, y, z, metadata | 8))
            return false;
    } else if (!mc_plant_sapling_grow(world, block, x, y, z, random, error)) {
        return mc_plant_nested(&frame, 86, error);
    }
    return mc_plant_done(&frame);
}
