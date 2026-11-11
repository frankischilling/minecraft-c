#include "mutation_internal.h"
#include "world_execution_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../collision/blocks_internal.h"
#include "mc/core/java/java.h"

#include <stdlib.h>

static void bed_at(mc_world_mutation_error *e, unsigned int pc) {
    e->hook_class = "ve";
    e->hook_method = "b(Lfd;IIII)V";
    e->hook_bytecode = pc;
}

static bool bed_nested(mc_world_runtime *world, mc_world_mutation_error *e, unsigned int pc,
                       const mc_world_mutation_error *child) {
    if (e != child)
        *e = *child;
    if (e->cause_hook_class == NULL) {
        e->cause_hook_class = e->hook_class;
        e->cause_hook_method = e->hook_method;
        e->cause_hook_bytecode = e->hook_bytecode;
    }
    bed_at(e, pc);
    if (world != NULL)
        world->last_error = e->cause;
    return false;
}

static bool bed_value(mc_world_runtime *world, mc_world_mutation_error *e, unsigned int pc,
                      int32_t x, int32_t y, int32_t z, bool metadata, int32_t *out,
                      mc_world_mutation_execution_call *owned) {
    bed_at(e, pc);
    if (world == NULL) {
        e->bed_source_null = true;
        return mc_world_mutation_fail(world, e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                      "Bed neighbor actual World null receiver");
    }
    if (owned != NULL) {
        const mc_entity_tick_execution_site site = {"ve", "b", "(Lfd;IIII)V", (int)pc};
        return mc_world_mutation_execution_get(owned, world, x, y, z, metadata, out, e, site);
    }
    mc_world_collision_error *child = malloc(sizeof(*child));
    if (child == NULL)
        return mc_world_mutation_fail(world, e, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                      "Bed neighbor reached value diagnostic");
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
        e->status = status;
        e->cause = child->nearby.query.cause;
        e->operation = e->cause.operation;
        world->last_error = e->cause;
    }
    free(child);
    return status == MC_WORLD_STORAGE_OK;
}

static bool bed_direction(mc_world_runtime *world, const mc_block *block,
                          mc_world_mutation_error *e, int32_t direction, int32_t axis,
                          unsigned int get_pc, unsigned int row_pc, unsigned int value_pc,
                          int32_t *out) {
    mc_registry_bed_table_view table;
    mc_registry_bed_row_view row;
    bed_at(e, get_pc);
    mc_registry_status status = mc_registry_bed_table_get(block->classes, &table, &e->registry);
    if (status == MC_REGISTRY_OK) {
        bed_at(e, row_pc);
        status = mc_registry_bed_table_at(&table, direction, &row, &e->registry);
    }
    if (status == MC_REGISTRY_OK) {
        bed_at(e, value_pc);
        status = mc_registry_bed_row_at(&row, axis, out, &e->registry);
    }
    if (status != MC_REGISTRY_OK)
        return mc_world_mutation_registry_fail(world, e, &e->registry,
                                               "Bed neighbor captured facing array");
    return true;
}

static bool bed_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x, int32_t y,
                         int32_t z, int32_t changed_id, mc_world_mutation_error *e,
                         mc_world_mutation_execution_call *owned) {
    (void)changed_id;
    int32_t metadata;
    if (!bed_value(world, e, 5, x, y, z, true, &metadata, owned))
        return false;
    const int32_t direction = (int32_t)((uint32_t)metadata & 3u);
    const bool head = ((uint32_t)metadata & 8u) != 0;
    int32_t dx, dz;
    if (!bed_direction(world, block, e, direction, 0, head ? 27u : 73u, head ? 32u : 78u,
                       head ? 34u : 80u, &dx))
        return false;
    const int32_t px = head ? mc_java_i32_sub(x, dx) : mc_java_i32_add(x, dx);
    if (!bed_direction(world, block, e, direction, 1, head ? 39u : 85u, head ? 44u : 90u,
                       head ? 46u : 92u, &dz))
        return false;
    const int32_t pz = head ? mc_java_i32_sub(z, dz) : mc_java_i32_add(z, dz);
    int32_t partner;
    if (!bed_value(world, e, head ? 48u : 94u, px, y, pz, false, &partner, owned))
        return false;
    bed_at(e, head ? 52u : 98u);
    if (partner != block->fields.bn) {
        const unsigned int setter_pc = head ? 64u : 110u;
        bed_at(e, setter_pc);
        if (owned != NULL) {
            const mc_entity_tick_execution_site site = {"ve", "b", "(Lfd;IIII)V", (int)setter_pc};
            if (!mc_world_mutation_execution_remove(owned, world, x, y, z, e, site)) {
                if (e->status != MC_WORLD_STORAGE_OK)
                    return bed_nested(world, e, setter_pc, e);
                bed_at(e, setter_pc);
                return false;
            }
        } else {
            mc_world_mutation_error *child = malloc(sizeof(*child));
            if (child == NULL)
                return mc_world_mutation_fail(world, e, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                              "Bed neighbor notifying child diagnostic");
            mc_world_mutation_error_init(child);
            bool ignored;
            const mc_world_storage_status status =
                mc_world_runtime_block_set(world, x, y, z, 0, &ignored, child);
            if (status != MC_WORLD_STORAGE_OK)
                (void)bed_nested(world, e, setter_pc, child);
            free(child);
            if (status != MC_WORLD_STORAGE_OK)
                return false;
        }
        if (!head) {
            bed_at(e, 115);
            if (!world->fields.B) {
                bed_at(e, 129);
                if (owned != NULL) {
                    const mc_entity_tick_execution_site site = {"ve", "b", "(Lfd;IIII)V", 129};
                    if (!mc_world_mutation_execution_drop(owned, world, block, x, y, z, metadata, e,
                                                          site))
                        return false;
                } else {
                    if (!mc_world_mutation_drop(world, block, x, y, z, metadata, e))
                        return bed_nested(world, e, 129, e);
                }
            }
        }
    }
    e->hook_class = e->hook_method = NULL;
    e->hook_bytecode = 0;
    return true;
}

bool mc_world_mutation_bed_neighbor(mc_world_runtime *world, const mc_block *block, int32_t x,
                                    int32_t y, int32_t z, int32_t changed_id,
                                    mc_world_mutation_error *e) {
    return bed_neighbor(world, block, x, y, z, changed_id, e, NULL);
}

bool mc_world_mutation_bed_neighbor_in_execution(mc_world_mutation_execution_call *owned,
                                                 mc_world_runtime *world, const mc_block *block,
                                                 int32_t x, int32_t y, int32_t z,
                                                 int32_t changed_id, mc_world_mutation_error *e) {
    return owned != NULL && bed_neighbor(world, block, x, y, z, changed_id, e, owned);
}
