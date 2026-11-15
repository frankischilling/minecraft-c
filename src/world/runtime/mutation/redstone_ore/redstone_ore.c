#include "redstone_ore_internal.h"

#include "../../../../block/registry/registry_internal.h"
#include "../mutation_internal.h"
#include "mc/block/registry/runtime.h"

#include <stdlib.h>

static void at(mc_world_mutation_error *error, unsigned int pc) {
    error->hook_class = "bs";
    error->hook_method = "a(Lfd;IIILjava/util/Random;)V";
    error->hook_bytecode = pc;
    error->redstone_ore_tick_bytecode = pc;
}

static bool null_receiver(mc_world_runtime *world, mc_world_mutation_error *error, unsigned int pc,
                          const char *operation) {
    at(error, pc);
    error->redstone_ore_source_null = true;
    return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
}

static bool done(mc_world_mutation_error *error) {
    error->hook_class = error->hook_method = NULL;
    error->hook_bytecode = 0;
    error->redstone_ore_tick_bytecode = 28;
    return true;
}

bool mc_world_mutation_redstone_ore_tick(mc_world_runtime *world, const mc_block *block, int32_t x,
                                         int32_t y, int32_t z, mc_java_random *random,
                                         mc_world_mutation_error *error) {
    (void)random; /* The actual override never dereferences or draws from it. */
    if (error == NULL)
        return false;
    error->redstone_ore_tick_bytecode = 0;
    error->redstone_ore_source_null = false;
    if (block == NULL || block->classes == NULL || block->kind != MC_BLOCK_REDSTONE_ORE ||
        block->classes->options.side != MC_REGISTRY_CLIENT) {
        at(error, 0);
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Redstone Ore actual client tick receiver");
    }
    at(error, 1);
    const int32_t own_id = block->fields.bn;
    const mc_block *active = NULL;
    at(error, 4);
    if (mc_registry_block_static_get(block->classes, "aP", &active, &error->registry) !=
        MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &error->registry,
                                               "Redstone Ore tick live active Block");
    if (active == NULL)
        return null_receiver(world, error, 7, "Redstone Ore tick active bn receiver");
    if (own_id != active->fields.bn)
        return done(error);

    const mc_block *normal = NULL;
    at(error, 18);
    if (mc_registry_block_static_get(block->classes, "aO", &normal, &error->registry) !=
        MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, error, &error->registry,
                                               "Redstone Ore tick live inactive Block");
    if (normal == NULL)
        return null_receiver(world, error, 21, "Redstone Ore tick inactive bn receiver");
    const int32_t id = normal->fields.bn;
    at(error, 24);
    if (world == NULL)
        return null_receiver(world, error, 24, "Redstone Ore tick World setter receiver");
    mc_world_mutation_error *lower = malloc(sizeof(*lower));
    if (lower == NULL)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                      "Redstone Ore setter diagnostic allocation");
    mc_world_mutation_error_init(lower);
    bool ordinary;
    const mc_world_storage_status status =
        mc_world_runtime_block_set(world, x, y, z, id, &ordinary, lower);
    if (status != MC_WORLD_STORAGE_OK) {
        *error = *lower;
        if (error->cause_hook_class == NULL) {
            error->cause_hook_class = error->hook_class;
            error->cause_hook_method = error->hook_method;
            error->cause_hook_bytecode = error->hook_bytecode;
        }
        /* Keep a deeper Ore null witness beside this actual setter caller.
         * Other mutation/query/WorldAccess witnesses are copied unchanged. */
        if (!error->redstone_ore_source_null)
            error->redstone_ore_tick_bytecode = 24;
        error->hook_class = "bs";
        error->hook_method = "a(Lfd;IIILjava/util/Random;)V";
        error->hook_bytecode = 24;
    }
    free(lower);
    return status == MC_WORLD_STORAGE_OK ? done(error) : false;
}
