#include "piston_internal.h"

#include "../../../block/registry/registry_internal.h"
#include "../../../block/registry/runtime_internal.h"
#include "../block_queries_internal.h"
#include "../collision/blocks_internal.h"
#include "../construction_internal.h"
#include "../power/power_internal.h"
#include "../tile_entities/piston_list_internal.h"
#include "../tile_entities/piston_math_internal.h"
#include "mc/block/material/material.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/tile_entities/piston.h"
#include "mc/world/runtime/tile_entities/tile_entities.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef enum piston_method {
    P_ADDED,
    P_NEIGHBOR,
    P_UPDATE,
    P_POWER,
    P_CAN_PUSH,
    P_SCAN,
    P_MOVE,
    P_EVENT,
    P_WORLD_EVENT,
    P_FACTORY,
    P_HEAD_REMOVED,
    P_HEAD_NEIGHBOR
} piston_method;

typedef union piston_scratch {
    mc_world_mutation_error mutation;
    mc_world_collision_error value;
    mc_world_power_error power;
    mc_live_tile_entity_error tile;
} piston_scratch;

typedef struct piston_frame {
    mc_world_runtime_classes *code;
    mc_registry_classes *registry;
    mc_world_runtime *world;
    mc_block *block;
    mc_world_mutation_error *error;
    piston_scratch **scratch;
    piston_method method;
} piston_frame;

static void piston_at(const piston_frame *f, unsigned int pc) {
    static const char *const methods[] = {"c(Lfd;III)V",  "b(Lfd;IIII)V",  "h(Lfd;III)V",
                                          "f(Lfd;IIII)Z", "a(ILfd;IIIZ)Z", "h(Lfd;IIII)Z",
                                          "i(Lfd;IIII)Z", "a(Lfd;IIIII)V", "d(IIIII)V",
                                          "a(IIIZZ)Low;", "b(Lfd;III)V",   "b(Lfd;IIII)V"};
    mc_world_mutation_error *e = f->error;
    e->hook_class = f->method == P_WORLD_EVENT    ? "fd"
                    : f->method == P_FACTORY      ? "ut"
                    : f->method >= P_HEAD_REMOVED ? "h"
                                                  : "jq";
    e->hook_method = methods[f->method];
    e->hook_bytecode = pc;
    unsigned int *const sites[] = {
        &e->piston.added,   &e->piston.neighbor,     &e->piston.update,
        &e->piston.power,   &e->piston.can_push,     &e->piston.scan,
        &e->piston.move,    &e->piston.event,        &e->piston.world_event,
        &e->piston.factory, &e->piston.head_removed, &e->piston.head_neighbor};
    *sites[f->method] = pc;
}

static bool piston_fail(const piston_frame *f, unsigned int pc, mc_world_storage_status status,
                        const char *operation) {
    piston_at(f, pc);
    return mc_world_mutation_fail(f->world, f->error, status, operation);
}

static bool piston_null(const piston_frame *f, unsigned int pc, const char *operation) {
    f->error->piston_source_null = true;
    return piston_fail(f, pc, MC_WORLD_STORAGE_UNSUPPORTED_STATE, operation);
}

static bool piston_done(const piston_frame *f) {
    f->error->hook_class = f->error->hook_method = NULL;
    f->error->hook_bytecode = 0;
    f->error->piston = (mc_world_piston_sites){0};
    return true;
}

static bool piston_nested(const piston_frame *f, unsigned int pc,
                          const mc_world_mutation_error *child) {
    const mc_world_piston_sites saved = f->error->piston;
    if (f->error != child)
        *f->error = *child;
#define KEEP(name)                                                                                 \
    if (saved.name != 0)                                                                           \
    f->error->piston.name = saved.name
    KEEP(added);
    KEEP(neighbor);
    KEEP(update);
    KEEP(power);
    KEEP(can_push);
    KEEP(scan);
    KEEP(move);
    KEEP(event);
    KEEP(world_event);
    KEEP(factory);
    KEEP(head_removed);
    KEEP(head_neighbor);
#undef KEEP
    if (f->error->cause_hook_class == NULL) {
        f->error->cause_hook_class = f->error->hook_class;
        f->error->cause_hook_method = f->error->hook_method;
        f->error->cause_hook_bytecode = f->error->hook_bytecode;
    }
    piston_at(f, pc);
    return false;
}

static piston_scratch *piston_workspace(const piston_frame *f, unsigned int pc) {
    piston_at(f, pc);
    if (*f->scratch == NULL)
        *f->scratch = malloc(sizeof(**f->scratch));
    if (*f->scratch == NULL)
        (void)piston_fail(f, pc, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                          "Piston reached child diagnostic allocation");
    return *f->scratch;
}

static bool piston_storage(const piston_frame *f, unsigned int pc, mc_world_storage_error cause) {
    piston_at(f, pc);
    f->error->status = cause.status;
    f->error->operation = cause.operation;
    f->error->cause = cause;
    if (f->world != NULL)
        f->world->last_error = cause;
    return false;
}

static bool piston_registry(const piston_frame *f, unsigned int pc, const char *operation) {
    piston_at(f, pc);
    return mc_world_mutation_registry_fail(f->world, f->error, &f->error->registry, operation);
}

static void piston_access(mc_world_mutation_error *e, const mc_world_nearby_error *child) {
    e->nearby = *child;
    e->query = child->query;
    e->population = child->query.population;
    e->entity = child->entity;
    e->tile_entity = child->tile_entity;
    e->batch = child->batch;
    e->load_entity = child->load_entity;
    e->activation_bytecode = child->activation_bytecode;
    e->activation_world_bytecode = child->activation_world_bytecode;
    e->load_bytecode = child->load_bytecode;
    e->load_builder_bytecode = child->load_builder_bytecode;
    e->provider_bytecode = child->query.provider_bytecode;
    e->registry = child->query.registry;
}

static bool piston_value(const piston_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                         bool metadata, int32_t *out) {
    if (f->world == NULL)
        return piston_null(f, pc, "Piston actual null World value receiver");
    piston_scratch *s = piston_workspace(f, pc);
    if (s == NULL)
        return false;
    mc_world_collision_error_init(&s->value);
    if (mc_world_collision_block_value_held(f->world, x, y, z, metadata, out, &s->value) ==
        MC_WORLD_STORAGE_OK)
        return true;
    piston_access(f->error, &s->value.nearby);
    return piston_storage(f, pc, s->value.nearby.query.cause);
}

static bool piston_static(const piston_frame *f, const char *name, unsigned int static_pc,
                          unsigned int field_pc, const mc_block **out) {
    piston_at(f, static_pc);
    if (mc_registry_block_static_get(f->registry, name, out, &f->error->registry) != MC_REGISTRY_OK)
        return piston_registry(f, static_pc, "Piston actual Block static active use");
    if (*out == NULL)
        return piston_null(f, field_pc, "Piston actual null static Block field receiver");
    return true;
}

static bool piston_element(const piston_frame *f, int32_t id, unsigned int static_pc,
                           unsigned int index_pc, unsigned int receiver_pc, const mc_block **out) {
    mc_registry_block_m_view array;
    piston_at(f, static_pc);
    if (mc_registry_block_m_get(f->registry, &array, &f->error->registry) != MC_REGISTRY_OK)
        return piston_registry(f, static_pc, "Piston actual Block.m active use");
    piston_at(f, index_pc);
    if (mc_registry_block_m_at(&array, id, out, &f->error->registry) != MC_REGISTRY_OK)
        return piston_registry(f, index_pc, "Piston captured Block.m element");
    if (*out == NULL && receiver_pc != UINT_MAX)
        return piston_null(f, receiver_pc, "Piston actual null Block invocation receiver");
    return true;
}

static bool piston_direction(const piston_frame *f, mc_registry_direction_field field,
                             int32_t facing, unsigned int static_pc, unsigned int index_pc,
                             int32_t *out) {
    mc_registry_direction_view array;
    piston_at(f, static_pc);
    if (mc_registry_direction_get(f->registry, field, &array, &f->error->registry) !=
        MC_REGISTRY_OK)
        return piston_registry(f, static_pc, "Piston actual Direction active use");
    piston_at(f, index_pc);
    if (mc_registry_direction_at(&array, facing, out, &f->error->registry) != MC_REGISTRY_OK)
        return piston_registry(f, index_pc, "Piston captured Direction element");
    return true;
}

static bool piston_step(const piston_frame *f, int32_t facing, int32_t *x, int32_t *y, int32_t *z,
                        unsigned int x_pc, unsigned int y_pc, unsigned int z_pc, bool subtract) {
    static const mc_registry_direction_field fields[] = {
        MC_REGISTRY_DIRECTION_B, MC_REGISTRY_DIRECTION_C, MC_REGISTRY_DIRECTION_D};
    int32_t *const coordinates[] = {x, y, z};
    const unsigned int pcs[] = {x_pc, y_pc, z_pc};
    for (size_t i = 0; i < 3; ++i) {
        int32_t component;
        if (!piston_direction(f, fields[i], facing, pcs[i], pcs[i] + 5u, &component))
            return false;
        *coordinates[i] = subtract ? mc_java_i32_sub(*coordinates[i], component)
                                   : mc_java_i32_add(*coordinates[i], component);
    }
    return true;
}

static bool piston_mobility(const piston_frame *f, const mc_block *block, unsigned int pc,
                            int32_t *out) {
    piston_at(f, pc);
    if (block == NULL)
        return piston_null(f, pc, "Piston actual null Block.h invocation receiver");
    /* Virtual h()I overrides return before the inherited Material read. */
    switch (block->kind) {
        case MC_BLOCK_PRESSURE_PLATE: /* BV.h */
        case MC_BLOCK_DOOR:           /* LE.h */
        case MC_BLOCK_BED:            /* VE.h */
            *out = 1;
            return true;
        case MC_BLOCK_RAIL:          /* PC.h */
        case MC_BLOCK_DETECTOR_RAIL: /* PH inherits PC.h. */
        case MC_BLOCK_ICE:           /* NK.h */
            *out = 0;
            return true;
        default:
            break;
    }
    if (block->fields.bA == NULL) {
        f->error->cause_hook_class = "uu";
        f->error->cause_hook_method = "h()I";
        f->error->cause_hook_bytecode = 4;
        return piston_null(f, pc, "Piston actual null Material.j invocation receiver");
    }
    *out = mc_material_j(block->fields.bA); /* Complete UU.h -> actual LN.j. */
    return true;
}

static bool piston_tile(const piston_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                        mc_tile_entity **out) {
    if (f->world == NULL)
        return piston_null(f, pc, "Piston actual null World TileEntity receiver");
    piston_scratch *s = piston_workspace(f, pc);
    if (s == NULL)
        return false;
    const mc_world_storage_status status =
        mc_live_tile_entities_get(f->world, x, y, z, out, &s->tile);
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    f->error->tile_entity = s->tile;
    f->error->registry = s->tile.registry;
    f->error->entity = s->tile.entity;
    f->error->publication = s->tile.publication;
    f->error->batch = s->tile.batch;
    f->error->load_entity = s->tile.load_entity;
    f->error->activation_bytecode = s->tile.activation_bytecode;
    f->error->activation_world_bytecode = s->tile.activation_world_bytecode;
    f->error->load_bytecode = s->tile.load_bytecode;
    f->error->load_builder_bytecode = s->tile.load_builder_bytecode;
    f->error->provider_bytecode = s->tile.provider_bytecode;
    return piston_storage(f, pc, s->tile.cause);
}

static bool piston_power(const piston_frame *caller, int32_t x, int32_t y, int32_t z,
                         int32_t facing, bool *out) {
    piston_frame f = *caller;
    f.method = P_POWER;
    static const int32_t sides[] = {0, 1, 2, 3, 5, 4, 0, 1, 2, 3, 4, 5};
    static const unsigned int pcs[] = {13, 35, 57, 79, 101, 123, 137, 153, 171, 189, 207, 225};
    const int32_t xs[] = {x, x, x, x, mc_java_i32_add(x, 1), mc_java_i32_sub(x, 1),
                          x, x, x, x, mc_java_i32_sub(x, 1), mc_java_i32_add(x, 1)};
    const int32_t ys[] = {mc_java_i32_sub(y, 1),
                          mc_java_i32_add(y, 1),
                          y,
                          y,
                          y,
                          y,
                          y,
                          mc_java_i32_add(y, 2),
                          mc_java_i32_add(y, 1),
                          mc_java_i32_add(y, 1),
                          mc_java_i32_add(y, 1),
                          mc_java_i32_add(y, 1)};
    const int32_t zs[] = {z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1), z, z,
                          z, z, mc_java_i32_sub(z, 1), mc_java_i32_add(z, 1), z, z};
    for (size_t i = 0; i < sizeof(sides) / sizeof(sides[0]); ++i) {
        if (i < 6 && facing == sides[i])
            continue;
        if (f.world == NULL)
            return piston_null(&f, pcs[i], "Piston power actual null World.k receiver");
        piston_scratch *s = piston_workspace(&f, pcs[i]);
        if (s == NULL)
            return false;
        bool powered;
        if (mc_world_power_at_held(f.world, xs[i], ys[i], zs[i], sides[i], &powered, &s->power) !=
            MC_WORLD_STORAGE_OK) {
            f.error->power = s->power;
            return piston_storage(&f, pcs[i], s->power.cause);
        }
        if (powered) {
            *out = true;
            return true;
        }
    }
    *out = false;
    return true;
}

static bool piston_can_push(const piston_frame *caller, int32_t id, int32_t x, int32_t y, int32_t z,
                            bool destroy, bool *out) {
    piston_frame f = *caller;
    f.method = P_CAN_PUSH;
    const mc_block *block;
    if (!piston_static(&f, "aq", 1, 4, &block))
        return false;
    if (id == block->fields.bn) {
        *out = false;
        return true;
    }
    if (!piston_static(&f, "aa", 13, 16, &block))
        return false;
    bool base = id == block->fields.bn;
    if (!base) {
        if (!piston_static(&f, "W", 23, 26, &block))
            return false;
        base = id == block->fields.bn;
    }
    if (base) {
        int32_t metadata;
        if (!piston_value(&f, 37, x, y, z, true, &metadata))
            return false;
        if ((metadata & 8) != 0) {
            *out = false;
            return true;
        }
    } else {
        if (!piston_element(&f, id, 48, 52, 53, &block))
            return false;
        uint32_t hardness;
        memcpy(&hardness, &block->fields.bo, sizeof(hardness));
        if (hardness == UINT32_C(0xbf800000)) {
            *out = false;
            return true;
        }
        int32_t mobility;
        if (!piston_element(&f, id, 64, 68, 69, &block) ||
            !piston_mobility(&f, block, 69, &mobility))
            return false;
        if (mobility == 2) {
            *out = false;
            return true;
        }
        if (!destroy) {
            if (!piston_element(&f, id, 83, 87, 88, &block) ||
                !piston_mobility(&f, block, 88, &mobility))
                return false;
            if (mobility == 1) {
                *out = false;
                return true;
            }
        }
    }
    mc_tile_entity *tile = NULL;
    if (!piston_tile(&f, 102, x, y, z, &tile))
        return false;
    *out = tile == NULL;
    mc_tile_entity_release(tile);
    return true;
}

static bool piston_scan(const piston_frame *caller, int32_t x, int32_t y, int32_t z, int32_t facing,
                        bool *out) {
    piston_frame f = *caller;
    f.method = P_SCAN;
    if (!piston_step(&f, facing, &x, &y, &z, 1, 11, 21, false))
        return false;
    for (int32_t count = 0; count < 13; ++count) {
        if (y <= 0 || y >= 127) {
            *out = false;
            return true;
        }
        int32_t id;
        if (!piston_value(&f, 61, x, y, z, false, &id))
            return false;
        if (id == 0) {
            *out = true;
            return true;
        }
        bool pushable;
        if (!piston_can_push(&f, id, x, y, z, true, &pushable))
            return piston_nested(&f, 84, f.error);
        if (!pushable) {
            *out = false;
            return true;
        }
        const mc_block *block;
        int32_t mobility;
        if (!piston_element(&f, id, 92, 97, 98, &block) ||
            !piston_mobility(&f, block, 98, &mobility))
            return false;
        if (mobility == 1) {
            *out = true;
            return true;
        }
        if (count == 12) {
            *out = false;
            return true;
        }
        if (!piston_step(&f, facing, &x, &y, &z, 119, 130, 141, false))
            return false;
    }
    *out = true;
    return true;
}

static bool piston_set(const piston_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                       int32_t id, bool with_metadata, int32_t metadata, bool notify) {
    if (f->world == NULL)
        return piston_null(f, pc, "Piston actual null World setter receiver");
    piston_scratch *s = piston_workspace(f, pc);
    if (s == NULL)
        return false;
    bool ignored;
    if (mc_world_runtime_piston_set(f->code, f->world, x, y, z, id, with_metadata, metadata, notify,
                                    &ignored, &s->mutation) != MC_WORLD_STORAGE_OK)
        return piston_nested(f, pc, &s->mutation);
    return true; /* Actual pop discards the ordinary Boolean. */
}

static bool piston_metadata(const piston_frame *f, unsigned int pc, int32_t x, int32_t y, int32_t z,
                            int32_t metadata, bool notify) {
    if (f->world == NULL)
        return piston_null(f, pc, "Piston actual null World metadata setter receiver");
    piston_scratch *s = piston_workspace(f, pc);
    if (s == NULL)
        return false;
    if (notify) {
        if (mc_world_runtime_piston_metadata(f->code, f->world, x, y, z, metadata, &s->mutation) !=
            MC_WORLD_STORAGE_OK)
            return piston_nested(f, pc, &s->mutation);
    } else {
        bool ignored;
        mc_world_storage_error cause = {0};
        if (mc_world_runtime_metadata_set_quiet(f->world, x, y, z, metadata, &ignored, &cause) !=
            MC_WORLD_STORAGE_OK)
            return piston_storage(f, pc, cause);
    }
    return true;
}

static bool piston_drop(const piston_frame *f, unsigned int pc, const mc_block *block, int32_t x,
                        int32_t y, int32_t z, int32_t metadata) {
    if (block == NULL)
        return piston_null(f, pc, "Piston actual null Block drop invocation receiver");
    piston_scratch *s = piston_workspace(f, pc);
    if (s == NULL)
        return false;
    mc_world_mutation_error_init(&s->mutation);
    if (!mc_world_mutation_drop(f->world, block, x, y, z, metadata, &s->mutation))
        return piston_nested(f, pc, &s->mutation);
    return true;
}

static bool piston_construct(const piston_frame *caller, int32_t id, int32_t data, int32_t facing,
                             bool extending, bool render_head, mc_tile_entity **out) {
    piston_frame f = *caller;
    f.method = P_FACTORY;
    piston_at(&f, 0);
    if (f.code == NULL)
        return piston_fail(&f, 0, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Piston UT factory requires explicit defining UK classes");
    if (f.code->side != MC_WORLD_INFO_CLIENT)
        return piston_fail(&f, 0, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "Piston client UT factory defining namespace side");
    mc_tile_entity *tile = NULL;
    mc_tile_entity_status status = mc_tile_entity_allocate_in_classes_detailed(
        f.code, MC_TILE_ENTITY_PISTON, &tile, &f.error->tile_entity.tile_entity);
    if (status == MC_TILE_ENTITY_OK) {
        piston_at(&f, 10);
        status = mc_tile_entity_piston_construct(tile, id, data, facing, extending, render_head,
                                                 &f.error->tile_entity.tile_entity);
    }
    if (status != MC_TILE_ENTITY_OK) {
        mc_tile_entity_release(tile);
        return piston_fail(&f, f.error->piston.factory,
                           status == MC_TILE_ENTITY_LIMIT ? MC_WORLD_STORAGE_LIMIT
                           : status == MC_TILE_ENTITY_ALLOCATION_FAILED
                               ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                               : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Piston actual UT new/UK constructor");
    }
    *out = tile;
    return true;
}

static bool piston_install(const piston_frame *f, unsigned int factory_pc, unsigned int invoke_pc,
                           int32_t x, int32_t y, int32_t z, int32_t id, int32_t data,
                           int32_t facing, bool extending, bool render_head) {
    mc_tile_entity *tile = NULL;
    if (!piston_construct(f, id, data, facing, extending, render_head, &tile))
        return piston_nested(f, factory_pc, f->error);
    if (f->world == NULL) {
        mc_tile_entity_release(tile);
        return piston_null(f, invoke_pc,
                           "Piston actual null World TileEntity installation receiver");
    }
    piston_scratch *s = piston_workspace(f, invoke_pc);
    if (s == NULL) {
        mc_tile_entity_release(tile);
        return false;
    }
    const mc_world_storage_status status =
        mc_live_tile_entities_set(f->world, x, y, z, tile, &s->tile);
    mc_tile_entity_release(tile); /* Only the factory-local occurrence. */
    if (status == MC_WORLD_STORAGE_OK)
        return true;
    f->error->tile_entity = s->tile;
    f->error->registry = s->tile.registry;
    f->error->entity = s->tile.entity;
    f->error->publication = s->tile.publication;
    f->error->batch = s->tile.batch;
    f->error->load_entity = s->tile.load_entity;
    f->error->activation_bytecode = s->tile.activation_bytecode;
    f->error->activation_world_bytecode = s->tile.activation_world_bytecode;
    f->error->load_bytecode = s->tile.load_bytecode;
    f->error->load_builder_bytecode = s->tile.load_builder_bytecode;
    f->error->provider_bytecode = s->tile.provider_bytecode;
    return piston_storage(f, invoke_pc, s->tile.cause);
}

static bool piston_finalize(const piston_frame *f, unsigned int pc, mc_tile_entity *tile) {
    piston_scratch *s = piston_workspace(f, pc);
    if (s == NULL)
        return false;
    if (mc_tile_entity_piston_finalize(tile, &s->mutation) != MC_WORLD_STORAGE_OK)
        return piston_nested(f, pc, &s->mutation);
    return true;
}

static bool piston_move(const piston_frame *caller, int32_t x, int32_t y, int32_t z, int32_t facing,
                        bool *out) {
    piston_frame f = *caller;
    f.method = P_MOVE;
    int32_t end_x = x, end_y = y, end_z = z;
    if (!piston_step(&f, facing, &end_x, &end_y, &end_z, 1, 11, 21, false))
        return false;
    for (int32_t count = 0; count < 13; ++count) {
        if (end_y <= 0 || end_y >= 127) {
            *out = false;
            return true;
        }
        int32_t id;
        if (!piston_value(&f, 62, end_x, end_y, end_z, false, &id))
            return false;
        if (id == 0)
            break;
        bool pushable;
        if (!piston_can_push(&f, id, end_x, end_y, end_z, true, &pushable))
            return piston_nested(&f, 85, f.error);
        if (!pushable) {
            *out = false;
            return true;
        }
        const mc_block *block;
        int32_t mobility;
        if (!piston_element(&f, id, 93, 98, 99, &block) ||
            !piston_mobility(&f, block, 99, &mobility))
            return false;
        if (mobility == 1) {
            int32_t metadata;
            /* Capture the drop receiver before the later World metadata read. */
            if (!piston_element(&f, id, 106, 111, UINT_MAX, &block) ||
                !piston_value(&f, 126, end_x, end_y, end_z, true, &metadata) ||
                !piston_drop(&f, 129, block, end_x, end_y, end_z, metadata) ||
                !piston_set(&f, 140, end_x, end_y, end_z, 0, false, 0, true))
                return false;
            break;
        }
        if (count == 12) {
            *out = false;
            return true;
        }
        if (!piston_step(&f, facing, &end_x, &end_y, &end_z, 158, 169, 180, false))
            return false;
    }
    while (end_x != x || end_y != y || end_z != z) {
        int32_t from_x = end_x, from_y = end_y, from_z = end_z;
        if (!piston_step(&f, facing, &from_x, &from_y, &from_z, 216, 227, 238, true))
            return false;
        int32_t id, metadata;
        if (!piston_value(&f, 254, from_x, from_y, from_z, false, &id) ||
            !piston_value(&f, 266, from_x, from_y, from_z, true, &metadata))
            return false;
        const mc_block *moving;
        if (id == f.block->fields.bn && from_x == x && from_y == y && from_z == z) {
            if (!piston_static(&f, "ad", 306, 309, &moving))
                return false;
            const int32_t moving_data = facing | (f.block->subtype.jq.a ? 8 : 0);
            if (!piston_set(&f, 328, end_x, end_y, end_z, moving->fields.bn, true, moving_data,
                            false))
                return false;
            const mc_block *head;
            if (!piston_static(&f, "ab", 339, 342, &head))
                return false;
            const int32_t head_data = facing | (f.block->subtype.jq.a ? 8 : 0);
            if (!piston_install(&f, 365, 368, end_x, end_y, end_z, head->fields.bn, head_data,
                                facing, true, false))
                return false;
        } else {
            if (!piston_static(&f, "ad", 381, 384, &moving) ||
                !piston_set(&f, 389, end_x, end_y, end_z, moving->fields.bn, true, metadata,
                            false) ||
                !piston_install(&f, 408, 411, end_x, end_y, end_z, id, metadata, facing, true,
                                false))
                return false;
        }
        end_x = from_x;
        end_y = from_y;
        end_z = from_z;
    }
    *out = true;
    return true;
}

static bool piston_sound(const piston_frame *f, int32_t x, int32_t y, int32_t z, bool extending) {
    const unsigned int random_pc = extending ? 66u : 599u;
    const mc_registry_string *name = NULL;
    const unsigned int literal_pc = extending ? 61u : 594u;
    piston_at(f, literal_pc);
    if (mc_registry_runtime_literal_ascii(f->registry,
                                          extending ? "tile.piston.out" : "tile.piston.in", &name,
                                          &f->error->registry) != MC_REGISTRY_OK)
        return piston_registry(f, literal_pc, "Piston reached sound literal");
    if (f->world == NULL)
        return piston_null(f, random_pc, "Piston actual null World Random field receiver");
    if (f->world->fields.r == NULL)
        return piston_null(f, extending ? 69u : 602u,
                           "Piston actual null Random.nextFloat receiver");
    const float draw = mc_java_random_next_float(f->world->fields.r);
    if (!mc_piston_math_admitted())
        return piston_fail(f, extending ? 74u : 607u, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Piston reached sound strict floating environment");
    const float pitch =
        mc_piston_float_add(mc_piston_float_multiply(draw, extending ? 0.25f : 0.15f), 0.6f);
    piston_scratch *s = piston_workspace(f, extending ? 78u : 611u);
    if (s == NULL)
        return false;
    mc_world_mutation_error_init(&s->mutation);
    if (!mc_world_mutation_sound(f->world, (double)x + 0.5, (double)y + 0.5, (double)z + 0.5, name,
                                 0.5f, pitch, &s->mutation))
        return piston_nested(f, extending ? 78u : 611u, &s->mutation);
    return true;
}

static bool piston_instance(const piston_frame *f, unsigned int pc, mc_tile_entity *tile,
                            bool *out) {
    if (tile == NULL) {
        *out = false;
        return true;
    }
    mc_tile_entity_snapshot state;
    const mc_tile_entity_status status = mc_tile_entity_get_snapshot(tile, &state);
    if (status != MC_TILE_ENTITY_OK) {
        f->error->tile_entity.tile_entity.status = status;
        return piston_fail(f, pc, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Piston reached TileEntity instance identity");
    }
    if (state.kind != MC_TILE_ENTITY_PISTON) {
        *out = false;
        return true;
    }
    if (f->code == NULL)
        return piston_fail(f, pc, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Piston UK instanceof requires explicit defining classes");
    mc_world_runtime_classes *actual = mc_tile_entity_piston_code_classes_held(tile);
    if (actual == NULL)
        return piston_fail(f, pc, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "Piston actual UK instance native phase");
    *out = actual == f->code;
    return true;
}

static bool piston_offset(const piston_frame *f, int32_t facing, int32_t *x, int32_t *y, int32_t *z,
                          unsigned int x_pc, unsigned int y_pc, unsigned int z_pc, int32_t scale) {
    const unsigned int pcs[] = {x_pc, y_pc, z_pc};
    static const mc_registry_direction_field fields[] = {
        MC_REGISTRY_DIRECTION_B, MC_REGISTRY_DIRECTION_C, MC_REGISTRY_DIRECTION_D};
    int32_t *const coordinates[] = {x, y, z};
    for (size_t i = 0; i < 3; ++i) {
        int32_t value;
        if (!piston_direction(f, fields[i], facing, pcs[i], pcs[i] + 5u, &value))
            return false;
        *coordinates[i] = mc_java_i32_add(*coordinates[i], mc_java_i32_mul(value, scale));
    }
    return true;
}

static bool piston_tile_fields(const piston_frame *f, unsigned int pc, mc_tile_entity *tile,
                               mc_tile_entity_snapshot *out) {
    const mc_tile_entity_status status = mc_tile_entity_get_snapshot(tile, out);
    if (status == MC_TILE_ENTITY_OK)
        return true;
    f->error->tile_entity.tile_entity.status = status;
    return piston_fail(f, pc, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Piston reached UK getter native receiver");
}

static bool piston_event(const piston_frame *caller, int32_t x, int32_t y, int32_t z, int32_t event,
                         int32_t facing) {
    piston_frame f = *caller;
    f.method = P_EVENT;
    piston_at(&f, 2);
    f.block->subtype.jq.b = true;
    if (event == 0) {
        bool moved;
        if (!piston_move(&f, x, y, z, facing, &moved))
            return piston_nested(&f, 22, f.error);
        if (moved && (!piston_metadata(&f, 38, x, y, z, facing | 8, true) ||
                      !piston_sound(&f, x, y, z, true)))
            return false;
    } else if (event == 1) {
        int32_t front_x = x, front_y = y, front_z = z;
        if (!piston_step(&f, facing, &front_x, &front_y, &front_z, 92, 102, 112, false))
            return false;
        mc_tile_entity *front = NULL;
        if (!piston_tile(&f, 116, front_x, front_y, front_z, &front))
            return false;
        bool piston;
        if (!piston_instance(&f, 128, front, &piston)) {
            mc_tile_entity_release(front);
            return false;
        }
        const bool finalized = !piston || piston_finalize(&f, 139, front);
        mc_tile_entity_release(front);
        if (!finalized)
            return false;
        const mc_block *moving;
        if (!piston_static(&f, "ad", 147, 150, &moving) ||
            !piston_set(&f, 155, x, y, z, moving->fields.bn, true, facing, false))
            return false;
        const int32_t carried_id = f.block->fields.bn; /* Actual field165. */
        if (!piston_install(&f, 174, 177, x, y, z, carried_id, facing, facing, false, true))
            return false;
        if (f.block->subtype.jq.a) { /* Fresh actual sticky flag180. */
            int32_t pull_x = x, pull_y = y, pull_z = z;
            if (!piston_offset(&f, facing, &pull_x, &pull_y, &pull_z, 188, 200, 214, 2))
                return false;
            int32_t id, data;
            if (!piston_value(&f, 231, pull_x, pull_y, pull_z, false, &id) ||
                !piston_value(&f, 243, pull_x, pull_y, pull_z, true, &data))
                return false;
            bool already_pulled = false;
            if (!piston_static(&f, "ad", 253, 256, &moving))
                return false;
            if (id == moving->fields.bn) {
                mc_tile_entity *pull = NULL;
                if (!piston_tile(&f, 269, pull_x, pull_y, pull_z, &pull))
                    return false;
                if (!piston_instance(&f, 281, pull, &piston)) {
                    mc_tile_entity_release(pull);
                    return false;
                }
                bool ok = true;
                if (piston) {
                    mc_tile_entity_snapshot state;
                    ok = piston_tile_fields(&f, 296, pull, &state);
                    if (ok && state.piston_facing == facing) {
                        ok = piston_tile_fields(&f, 306, pull, &state);
                        if (ok && state.piston_extending) {
                            ok = piston_finalize(&f, 314, pull) &&
                                 piston_tile_fields(&f, 319, pull, &state);
                            if (ok) {
                                id = state.piston_block_id;
                                ok = piston_tile_fields(&f, 326, pull, &state);
                                if (ok) {
                                    data = state.piston_block_data;
                                    already_pulled = true;
                                }
                            }
                        }
                    }
                }
                mc_tile_entity_release(pull);
                if (!ok)
                    return false;
            }
            bool can_pull = false;
            if (!already_pulled && id > 0) {
                if (!piston_can_push(&f, id, pull_x, pull_y, pull_z, false, &can_pull))
                    return piston_nested(&f, 354, f.error);
                if (can_pull) {
                    const mc_block *block;
                    int32_t mobility;
                    if (!piston_element(&f, id, 360, 365, 366, &block) ||
                        !piston_mobility(&f, block, 366, &mobility))
                        return false;
                    if (mobility != 0) {
                        if (!piston_static(&f, "aa", 374, 377, &block))
                            return false;
                        can_pull = id == block->fields.bn;
                        if (!can_pull) {
                            if (!piston_static(&f, "W", 385, 388, &block))
                                return false;
                            can_pull = id == block->fields.bn;
                        }
                    }
                }
            }
            if (can_pull && id > 0) {
                f.block->subtype.jq.b = false; /* Source396, no finally repair. */
                if (!piston_set(&f, 407, pull_x, pull_y, pull_z, 0, false, 0, true))
                    return false;
                f.block->subtype.jq.b = true; /* Source413. */
                if (!piston_step(&f, facing, &x, &y, &z, 417, 427, 437, false) ||
                    !piston_static(&f, "ad", 450, 453, &moving) ||
                    !piston_set(&f, 458, x, y, z, moving->fields.bn, true, data, false) ||
                    !piston_install(&f, 475, 478, x, y, z, id, data, facing, false, false))
                    return false;
            } else if (!already_pulled) {
                f.block->subtype.jq.b = false; /* Source491. */
                front_x = x;
                front_y = y;
                front_z = z;
                if (!piston_step(&f, facing, &front_x, &front_y, &front_z, 496, 506, 516, false) ||
                    !piston_set(&f, 521, front_x, front_y, front_z, 0, false, 0, true))
                    return false;
                f.block->subtype.jq.b = true; /* Source527. */
            }
        } else {
            f.block->subtype.jq.b = false; /* Source535. */
            front_x = x;
            front_y = y;
            front_z = z;
            if (!piston_step(&f, facing, &front_x, &front_y, &front_z, 540, 550, 560, false) ||
                !piston_set(&f, 565, front_x, front_y, front_z, 0, false, 0, true))
                return false;
            f.block->subtype.jq.b = true; /* Source571. */
        }
        if (!piston_sound(&f, x, y, z, false))
            return false;
    }
    f.block->subtype.jq.b = false; /* Source616, only reached normally. */
    return true;
}

bool mc_world_mutation_piston_world_event(mc_world_runtime_classes *code,
                                          mc_registry_classes *registry, mc_world_runtime *world,
                                          int32_t x, int32_t y, int32_t z, int32_t event,
                                          int32_t data, mc_world_mutation_error *error) {
    piston_scratch *scratch = NULL;
    piston_frame f = {code, registry, world, NULL, error, &scratch, P_WORLD_EVENT};
    bool ok = false;
    int32_t id;
    if (!piston_value(&f, 4, x, y, z, false, &id))
        goto done;
    if (id <= 0) {
        ok = true;
        goto done;
    }
    const mc_block *block;
    if (!piston_element(&f, id, 14, 19, 28, &block))
        goto done;
    f.block = (mc_block *)block;
    if (block->kind == MC_BLOCK_PISTON_BASE) {
        f.registry = block->classes;
        if (!piston_event(&f, x, y, z, event, data)) {
            (void)piston_nested(&f, 28, error);
            goto done;
        }
    } else if (block->kind == MC_BLOCK_NOTE_BLOCK) {
        (void)piston_fail(&f, 28, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World block event reached unimplemented NoteBlock body");
        goto done;
    } /* Other registered Blocks inherit the actual empty UU.a body. */
    ok = true;
done:
    free(scratch);
    return ok;
}

static bool piston_update(const piston_frame *caller, int32_t x, int32_t y, int32_t z) {
    piston_frame f = *caller;
    f.method = P_UPDATE;
    int32_t metadata;
    if (!piston_value(&f, 5, x, y, z, true, &metadata))
        return false;
    const int32_t facing = metadata & 7;
    bool powered;
    if (!piston_power(&f, x, y, z, facing, &powered))
        return piston_nested(&f, 25, f.error);
    if (metadata == 7)
        return true; /* Source32 compares the complete raw metadata. */
    if (powered && (metadata & 8) == 0) {
        bool possible;
        if (!piston_scan(&f, x, y, z, facing, &possible))
            return piston_nested(&f, 58, f.error);
        if (possible) {
            if (!piston_metadata(&f, 74, x, y, z, facing | 8, false))
                return false;
            if (!mc_world_mutation_piston_world_event(f.code, f.registry, f.world, x, y, z, 0,
                                                      facing, f.error))
                return piston_nested(&f, 86, f.error);
        }
    } else if (!powered && (metadata & 8) != 0) {
        if (!piston_metadata(&f, 112, x, y, z, facing, false))
            return false;
        if (!mc_world_mutation_piston_world_event(f.code, f.registry, f.world, x, y, z, 1, facing,
                                                  f.error))
            return piston_nested(&f, 124, f.error);
    }
    return true;
}

static bool piston_base_hook(mc_world_runtime_classes *code, mc_world_runtime *world,
                             const mc_block *block, int32_t x, int32_t y, int32_t z,
                             mc_world_mutation_error *error, bool added) {
    piston_scratch *scratch = NULL;
    piston_frame f = {code,
                      block->classes,
                      world,
                      (mc_block *)block,
                      error,
                      &scratch,
                      added ? P_ADDED : P_NEIGHBOR};
    bool ok = false;
    if (world == NULL) {
        (void)piston_null(&f, 1, "Piston hook actual null World remote field receiver");
        goto done;
    }
    if (world->fields.B || (!added && block->subtype.jq.b)) {
        ok = true;
        goto done;
    }
    if (added) {
        mc_tile_entity *tile = NULL;
        if (!piston_tile(&f, 12, x, y, z, &tile))
            goto done;
        const bool absent = tile == NULL;
        mc_tile_entity_release(tile);
        if (!absent) {
            ok = true;
            goto done;
        }
    }
    if (!piston_update(&f, x, y, z)) {
        (void)piston_nested(&f, added ? 24u : 20u, error);
        goto done;
    }
    ok = true;
done:
    free(scratch);
    return ok ? piston_done(&f) : false;
}

bool mc_world_mutation_piston_base_added(mc_world_runtime_classes *code, mc_world_runtime *world,
                                         const mc_block *block, int32_t x, int32_t y, int32_t z,
                                         mc_world_mutation_error *error) {
    return piston_base_hook(code, world, block, x, y, z, error, true);
}

bool mc_world_mutation_piston_base_neighbor(mc_world_runtime_classes *code, mc_world_runtime *world,
                                            const mc_block *block, int32_t x, int32_t y, int32_t z,
                                            int32_t source_id, mc_world_mutation_error *error) {
    (void)source_id;
    return piston_base_hook(code, world, block, x, y, z, error, false);
}

bool mc_world_mutation_piston_head_removed(mc_world_runtime_classes *code, mc_world_runtime *world,
                                           const mc_block *block, int32_t x, int32_t y, int32_t z,
                                           mc_world_mutation_error *error) {
    piston_scratch *scratch = NULL;
    piston_frame f = {code,  block->classes, world,         (mc_block *)block,
                      error, &scratch,       P_HEAD_REMOVED};
    bool ok = false;
    /* Actual invokespecial UU.b6 is the complete empty parent body. */
    int32_t metadata, opposite;
    if (!piston_value(&f, 14, x, y, z, true, &metadata) ||
        !piston_direction(&f, MC_REGISTRY_DIRECTION_A, metadata & 7, 19, 27, &opposite) ||
        !piston_step(&f, opposite, &x, &y, &z, 31, 41, 51, false))
        goto done;
    int32_t id;
    if (!piston_value(&f, 64, x, y, z, false, &id))
        goto done;
    const mc_block *base;
    if (!piston_static(&f, "aa", 71, 74, &base))
        goto done;
    bool matching = id == base->fields.bn;
    if (!matching) {
        if (!piston_static(&f, "W", 82, 85, &base))
            goto done;
        matching = id == base->fields.bn;
    }
    if (matching) {
        if (!piston_value(&f, 96, x, y, z, true, &metadata))
            goto done;
        if ((metadata & 8) != 0) {
            /* The actual captured array receiver gets the drop, rather than
             * substituting a remembered base identity from the static tests. */
            if (!piston_element(&f, id, 109, 114, 122, &base) ||
                !piston_drop(&f, 122, base, x, y, z, metadata) ||
                !piston_set(&f, 131, x, y, z, 0, false, 0, true))
                goto done;
        }
    }
    ok = true;
done:
    free(scratch);
    return ok ? piston_done(&f) : false;
}

bool mc_world_mutation_piston_head_neighbor(mc_world_runtime_classes *code, mc_world_runtime *world,
                                            const mc_block *block, int32_t x, int32_t y, int32_t z,
                                            int32_t source_id, mc_world_mutation_error *error) {
    piston_scratch *scratch = NULL;
    piston_frame f = {code,  block->classes, world,          (mc_block *)block,
                      error, &scratch,       P_HEAD_NEIGHBOR};
    bool ok = false;
    int32_t metadata;
    if (!piston_value(&f, 5, x, y, z, true, &metadata))
        goto done;
    const int32_t facing = metadata & 7;
    int32_t base_x = x, base_y = y, base_z = z;
    if (!piston_step(&f, facing, &base_x, &base_y, &base_z, 15, 25, 35, true))
        goto done;
    int32_t id;
    if (!piston_value(&f, 39, base_x, base_y, base_z, false, &id))
        goto done;
    const mc_block *base;
    if (!piston_static(&f, "aa", 46, 49, &base))
        goto done;
    bool matching = id == base->fields.bn;
    if (!matching) {
        if (!piston_static(&f, "W", 57, 60, &base))
            goto done;
        matching = id == base->fields.bn;
    }
    if (!matching) {
        if (!piston_set(&f, 72, x, y, z, 0, false, 0, true))
            goto done;
    } else {
        if (!piston_element(&f, id, 79, 84, UINT_MAX, &base))
            goto done;
        /* Re-evaluate each Direction after the captured Block receiver. */
        base_x = x;
        base_y = y;
        base_z = z;
        if (!piston_step(&f, facing, &base_x, &base_y, &base_z, 87, 97, 107, true))
            goto done;
        if (base == NULL) {
            (void)piston_null(&f, 113, "Piston head actual null neighbor invocation receiver");
            goto done;
        }
        piston_scratch *s = piston_workspace(&f, 113);
        if (s == NULL)
            goto done;
        mc_world_mutation_error_init(&s->mutation);
        if (!mc_world_mutation_block_neighbor_with_code(code, world, base, base_x, base_y, base_z,
                                                        source_id, &s->mutation)) {
            (void)piston_nested(&f, 113, &s->mutation);
            goto done;
        }
    }
    ok = true;
done:
    free(scratch);
    return ok ? piston_done(&f) : false;
}
