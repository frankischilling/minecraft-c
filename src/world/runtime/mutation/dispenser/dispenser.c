#include "dispenser_internal.h"

#include "../../../../block/registry/registry_internal.h"
#include "../../../../block/registry/runtime_internal.h"
#include "../../collision/blocks_internal.h"
#include "../mutation_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"

#include <stdlib.h>

static void at(mc_world_mutation_error *error, const char *method, unsigned int pc) {
    error->hook_class = "xq";
    error->hook_method = method;
    error->hook_bytecode = pc;
}

static bool nested(mc_world_mutation_error *error) {
    if (error->cause_hook_class == NULL) {
        error->cause_hook_class = error->hook_class;
        error->cause_hook_method = error->hook_method;
        error->cause_hook_bytecode = error->hook_bytecode;
    }
    at(error, "c(Lfd;III)V", 15);
    return false;
}

static bool value(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, int32_t *out,
                  mc_world_mutation_error *error, mc_world_mutation_execution_call *owned,
                  unsigned int pc) {
    at(error, "h(Lfd;III)V", pc);
    if (owned != NULL)
        return mc_world_mutation_execution_get(
            owned, world, x, y, z, false, out, error,
            (mc_entity_tick_execution_site){"xq", "h", "(Lfd;III)V", (int)pc});
    mc_world_collision_error *child = malloc(sizeof(*child));
    if (child == NULL)
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                      "Dispenser reached value diagnostic allocation");
    *child = mc_world_collision_empty_error();
    const mc_world_storage_status status =
        mc_world_collision_block_value_held(world, x, y, z, false, out, child);
    if (status != MC_WORLD_STORAGE_OK) {
        error->nearby = child->nearby;
        error->query = child->nearby.query;
        error->population = child->nearby.query.population;
        error->entity = child->nearby.entity;
        error->tile_entity = child->nearby.tile_entity;
        error->batch = child->nearby.batch;
        error->load_entity = child->nearby.load_entity;
        error->activation_bytecode = child->nearby.activation_bytecode;
        error->activation_world_bytecode = child->nearby.activation_world_bytecode;
        error->load_bytecode = child->nearby.load_bytecode;
        error->load_builder_bytecode = child->nearby.load_builder_bytecode;
        error->registry = child->nearby.query.registry;
        error->registry_failure = (mc_world_registry_failure){0};
        (void)mc_world_mutation_fail(world, error, status, child->nearby.query.operation);
        error->cause = child->nearby.query.cause;
    }
    free(child);
    return status == MC_WORLD_STORAGE_OK;
}

static bool opaque(mc_world_runtime *world, mc_registry_classes *registry, int32_t id,
                   unsigned int static_pc, unsigned int element_pc, bool *out,
                   mc_world_mutation_error *error) {
    mc_registry_block_o_view array;
    at(error, "h(Lfd;III)V", static_pc);
    if (mc_registry_block_o_get(registry, &array, &error->registry) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail_in(world, registry, error, &error->registry,
                                                  "Dispenser reached Block.o active use");
    at(error, "h(Lfd;III)V", element_pc);
    if (mc_registry_block_o_at(&array, id, out, &error->registry) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail_in(world, registry, error, &error->registry,
                                                  "Dispenser captured Block.o element");
    return true;
}

bool mc_world_mutation_dispenser_orient(mc_world_runtime *world, const mc_block *block, int32_t x,
                                        int32_t y, int32_t z, mc_world_mutation_error *error,
                                        mc_world_mutation_execution_call *owned) {
    at(error, "h(Lfd;III)V", 1);
    if (world == NULL) {
        error->dispenser_source_null = true;
        return mc_world_mutation_fail(world, error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Dispenser private orientation null World receiver");
    }
    if (world->fields.B)
        goto done;

    /* All four acquiring reads finish before the first GETSTATIC/array access. */
    int32_t north, south, west, east;
    if (!value(world, x, y, mc_java_i32_sub(z, 1), &north, error, owned, 15) ||
        !value(world, x, y, mc_java_i32_add(z, 1), &south, error, owned, 27) ||
        !value(world, mc_java_i32_sub(x, 1), y, z, &west, error, owned, 39) ||
        !value(world, mc_java_i32_add(x, 1), y, z, &east, error, owned, 51))
        return nested(error);

    mc_registry_classes *registry =
        owned == NULL ? block->classes : mc_world_mutation_execution_registry(owned);
    int32_t metadata = 3;
    bool first, second = false;
    /* Each reached GETSTATIC is fresh. The second operand is short-circuited;
     * the four independent conditions let X override an earlier Z choice. */
    if (!opaque(world, registry, north, 59, 64, &first, error) ||
        (first && !opaque(world, registry, south, 68, 73, &second, error)))
        return nested(error);
    if (first && !second)
        metadata = 3;
    if (!opaque(world, registry, south, 80, 85, &first, error) ||
        (first && !opaque(world, registry, north, 89, 94, &second, error)))
        return nested(error);
    if (first && !second)
        metadata = 2;
    if (!opaque(world, registry, west, 101, 106, &first, error) ||
        (first && !opaque(world, registry, east, 110, 115, &second, error)))
        return nested(error);
    if (first && !second)
        metadata = 5;
    if (!opaque(world, registry, east, 122, 127, &first, error) ||
        (first && !opaque(world, registry, west, 131, 136, &second, error)))
        return nested(error);
    if (first && !second)
        metadata = 4;

    at(error, "h(Lfd;III)V", 150);
    const bool written =
        owned != NULL
            ? mc_world_mutation_execution_metadata(
                  owned, world, x, y, z, metadata, error,
                  (mc_entity_tick_execution_site){"xq", "h", "(Lfd;III)V", 150})
            : mc_world_runtime_metadata_set(world, x, y, z, metadata, error) == MC_WORLD_STORAGE_OK;
    if (!written) {
        /* Preserve an actual deeper Block failure; a fresh metadata projection
         * otherwise needs its reached XQ.h150 caller beside XQ.c15. */
        if (error->hook_class == NULL && error->cause_hook_class == NULL)
            at(error, "h(Lfd;III)V", 150);
        return nested(error);
    }
done:
    error->hook_class = error->hook_method = NULL;
    error->hook_bytecode = 0;
    return true;
}
