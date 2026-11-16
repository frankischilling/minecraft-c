#include "furnace_internal.h"

#include "../../../../block/registry/registry_internal.h"
#include "../../../../block/registry/runtime_internal.h"
#include "../../block_queries_internal.h"
#include "../../collision/blocks_internal.h"
#include "../mutation_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

#include <stdlib.h>

static void at(mc_world_mutation_error *e, const char *method, unsigned int pc) {
    e->hook_class = "tc";
    e->hook_method = method;
    e->hook_bytecode = pc;
}

static bool nested(mc_world_mutation_error *e, const char *method, unsigned int pc) {
    if (e->cause_hook_class == NULL) {
        e->cause_hook_class = e->hook_class;
        e->cause_hook_method = e->hook_method;
        e->cause_hook_bytecode = e->hook_bytecode;
    }
    at(e, method, pc);
    return false;
}

static bool value(mc_world_runtime *world, int32_t x, int32_t y, int32_t z, bool metadata,
                  int32_t *out, mc_world_mutation_error *e, mc_world_mutation_execution_call *owned,
                  const char *method, unsigned int pc) {
    at(e, method, pc);
    if (owned != NULL)
        return mc_world_mutation_execution_get(
            owned, world, x, y, z, metadata, out, e,
            (mc_entity_tick_execution_site){"tc", method, "(Lfd;III)V", (int)pc});
    if (world == NULL) {
        e->furnace_source_null = true;
        return mc_world_mutation_fail(world, e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Furnace actual null World value receiver");
    }
    mc_world_collision_error *child = malloc(sizeof(*child));
    if (child == NULL)
        return mc_world_mutation_fail(world, e, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                      "Furnace reached value diagnostic allocation");
    *child = mc_world_collision_empty_error();
    const mc_world_storage_status status =
        mc_world_collision_block_value_held(world, x, y, z, metadata, out, child);
    if (status != MC_WORLD_STORAGE_OK) {
        e->nearby = child->nearby;
        e->query = child->nearby.query;
        e->population = child->nearby.query.population;
        e->entity = child->nearby.entity;
        e->tile_entity = child->nearby.tile_entity;
        e->batch = child->nearby.batch;
        e->load_entity = child->nearby.load_entity;
        e->activation_bytecode = child->nearby.activation_bytecode;
        e->activation_world_bytecode = child->nearby.activation_world_bytecode;
        e->load_bytecode = child->nearby.load_bytecode;
        e->load_builder_bytecode = child->nearby.load_builder_bytecode;
        e->registry = child->nearby.query.registry;
        (void)mc_world_mutation_fail(world, e, status, child->nearby.query.operation);
        e->cause = child->nearby.query.cause;
    }
    free(child);
    return status == MC_WORLD_STORAGE_OK;
}

static bool opaque(mc_world_runtime *world, mc_registry_classes *registry, int32_t id,
                   unsigned int static_pc, unsigned int element_pc, bool *out,
                   mc_world_mutation_error *e) {
    mc_registry_block_o_view array;
    at(e, "h(Lfd;III)V", static_pc);
    if (mc_registry_block_o_get(registry, &array, &e->registry) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, e, &e->registry,
                                               "Furnace reached Block.o active use");
    at(e, "h(Lfd;III)V", element_pc);
    if (mc_registry_block_o_at(&array, id, out, &e->registry) != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, e, &e->registry,
                                               "Furnace captured Block.o element");
    return true;
}

bool mc_world_mutation_furnace_orient(mc_world_runtime *world, const mc_block *block, int32_t x,
                                      int32_t y, int32_t z, mc_world_mutation_error *e,
                                      mc_world_mutation_execution_call *owned) {
    at(e, "h(Lfd;III)V", 1);
    if (world == NULL) {
        e->furnace_source_null = true;
        return mc_world_mutation_fail(world, e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Furnace private orientation null World receiver");
    }
    if (world->fields.B)
        goto done;
    int32_t north, south, west, east;
    if (!value(world, x, y, mc_java_i32_sub(z, 1), false, &north, e, owned, "h", 15) ||
        !value(world, x, y, mc_java_i32_add(z, 1), false, &south, e, owned, "h", 27) ||
        !value(world, mc_java_i32_sub(x, 1), y, z, false, &west, e, owned, "h", 39) ||
        !value(world, mc_java_i32_add(x, 1), y, z, false, &east, e, owned, "h", 51))
        return nested(e, "c(Lfd;III)V", 15);
    mc_registry_classes *registry =
        owned == NULL ? block->classes : mc_world_mutation_execution_registry(owned);
    int32_t metadata = 3;
    bool a, b = false;
    /* Every GETSTATIC is fresh, including the short-circuited second operand. */
    if (!opaque(world, registry, north, 59, 64, &a, e) ||
        (a && !opaque(world, registry, south, 68, 73, &b, e)))
        return nested(e, "c(Lfd;III)V", 15);
    if (a && !b)
        metadata = 3;
    if (!opaque(world, registry, south, 80, 85, &a, e) ||
        (a && !opaque(world, registry, north, 89, 94, &b, e)))
        return nested(e, "c(Lfd;III)V", 15);
    if (a && !b)
        metadata = 2;
    if (!opaque(world, registry, west, 101, 106, &a, e) ||
        (a && !opaque(world, registry, east, 110, 115, &b, e)))
        return nested(e, "c(Lfd;III)V", 15);
    if (a && !b)
        metadata = 5;
    if (!opaque(world, registry, east, 122, 127, &a, e) ||
        (a && !opaque(world, registry, west, 131, 136, &b, e)))
        return nested(e, "c(Lfd;III)V", 15);
    if (a && !b)
        metadata = 4;
    at(e, "h(Lfd;III)V", 150);
    if (owned != NULL) {
        if (!mc_world_mutation_execution_metadata(
                owned, world, x, y, z, metadata, e,
                (mc_entity_tick_execution_site){"tc", "h", "(Lfd;III)V", 150}))
            return nested(e, "c(Lfd;III)V", 15);
    } else if (mc_world_runtime_metadata_set(world, x, y, z, metadata, e) != MC_WORLD_STORAGE_OK)
        return nested(e, "c(Lfd;III)V", 15);
done:
    e->hook_class = e->hook_method = NULL;
    e->hook_bytecode = 0;
    return true;
}

bool mc_world_mutation_furnace_switch(mc_registry_classes *registry, mc_world_runtime *world,
                                      bool burning, int32_t x, int32_t y, int32_t z,
                                      mc_world_mutation_error *e) {
    /* The INVOKESTATIC's declaring class initializes before its body begins. */
    if (registry == NULL)
        return mc_world_mutation_fail(world, e, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                      "Furnace switch defining Registry namespace");
    if (mc_registry_block_class_initialize(registry, MC_BLOCK_FURNACE) != MC_REGISTRY_OK) {
        e->registry = registry->last_error;
        return mc_world_mutation_registry_fail(world, e, &e->registry,
                                               "Furnace switch actual TC active use");
    }
    int32_t metadata;
    if (!value(world, x, y, z, true, &metadata, e, NULL, "a(ZLfd;III)V", 5))
        return false;
    at(e, "a(ZLfd;III)V", 15);
    mc_tile_entity *tile = NULL;
    const mc_world_storage_status found =
        mc_live_tile_entities_get(world, x, y, z, &tile, &e->tile_entity);
    if (found != MC_WORLD_STORAGE_OK)
        return mc_world_mutation_fail(world, e, found, e->tile_entity.operation);
    bool ok = false;
    at(e, "a(ZLfd;III)V", 21);
    registry->tc_c = true;
    const mc_block *replacement = NULL;
    at(e, "a(ZLfd;III)V", burning ? 33u : 51u);
    if (mc_registry_block_static_get(registry, burning ? "aD" : "aC", &replacement, &e->registry) !=
        MC_REGISTRY_OK) {
        (void)mc_world_mutation_registry_fail(world, e, &e->registry,
                                              "Furnace switch actual Block active use");
        goto leave;
    }
    at(e, "a(ZLfd;III)V", burning ? 36u : 54u);
    if (replacement == NULL) {
        (void)mc_world_mutation_fail(world, e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "Furnace switch null static Block id receiver");
        goto leave;
    }
    bool ignored;
    if (mc_world_runtime_block_set(world, x, y, z, replacement->fields.bn, &ignored, e) !=
        MC_WORLD_STORAGE_OK) {
        (void)nested(e, "a(ZLfd;III)V", burning ? 39u : 57u);
        goto leave;
    }
    registry->tc_c = false; /* Actual successful source prefix at62 only. */
    if (mc_world_runtime_metadata_set(world, x, y, z, metadata, e) != MC_WORLD_STORAGE_OK) {
        (void)nested(e, "a(ZLfd;III)V", 72);
        goto leave;
    }
    at(e, "a(ZLfd;III)V", 77);
    const mc_tile_entity_status validated = mc_tile_entity_set_invalid(tile, false);
    if (validated != MC_TILE_ENTITY_OK) {
        e->furnace_source_null = tile == NULL;
        e->tile_entity.tile_entity.status = validated;
        (void)mc_world_mutation_fail(world, e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "Furnace saved Tile validation receiver");
        goto leave;
    }
    at(e, "a(ZLfd;III)V", 87);
    const mc_world_storage_status installed =
        mc_live_tile_entities_set(world, x, y, z, tile, &e->tile_entity);
    if (installed != MC_WORLD_STORAGE_OK) {
        (void)mc_world_mutation_fail(world, e, installed, e->tile_entity.operation);
        goto leave;
    }
    e->hook_class = e->hook_method = NULL;
    e->hook_bytecode = 0;
    ok = true;
leave:
    mc_tile_entity_release(tile);
    return ok;
}
