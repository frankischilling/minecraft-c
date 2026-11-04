#include "plant_substrate_internal.h"

#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"

#include <string.h>

static void stage(mc_world_plant_substrate_error *error, const mc_block *receiver,
                  const char *client, const char *server, unsigned int pc) {
    const bool is_client = receiver->classes->options.side == MC_REGISTRY_CLIENT;
    error->class_name = is_client ? client : server;
    error->method = is_client ? "d" : "c";
    error->pc = pc;
}

static bool static_id(const mc_block *receiver, const char *name, unsigned int load_pc,
                      unsigned int field_pc, int32_t *out, mc_world_plant_substrate_error *error) {
    const mc_block *block = NULL;
    error->pc = load_pc;
    error->operation = "Plant predicate Block static active use";
    if (mc_registry_block_static_get(receiver->classes, name, &block, &error->registry) !=
        MC_REGISTRY_OK)
        return false;
    error->pc = field_pc;
    error->operation = "Plant predicate Block static receiver";
    if (block == NULL) {
        error->registry.status = MC_REGISTRY_NULL_REFERENCE;
        error->registry.message = "Plant predicate Block static receiver";
        return false;
    }
    *out = block->fields.bn;
    return true;
}

mc_registry_status mc_world_plant_substrate(const mc_block *receiver, int32_t id, bool *out,
                                            mc_world_plant_substrate_error *error) {
    if (error == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    memset(error, 0, sizeof(*error));
    error->registry = (mc_registry_error){.initializer_pc = -1,
                                          .block_id = -1,
                                          .constructor_pc = -1,
                                          .active_initializer_pc = -1,
                                          .item_argument = -1};
    if (receiver == NULL || receiver->classes == NULL || out == NULL) {
        error->registry.status = MC_REGISTRY_INVALID_ARGUMENT;
        error->registry.message = "Entered plant substrate arguments";
        error->operation = error->registry.message;
        return error->registry.status;
    }
    int32_t candidate;
    switch (receiver->kind) {
        case MC_BLOCK_MUSHROOM: {
            stage(error, receiver, "tl", "md", 0);
            mc_registry_block_o_view array;
            error->operation = "Mushroom substrate Block.o active use";
            if (mc_registry_block_o_get(receiver->classes, &array, &error->registry) !=
                MC_REGISTRY_OK)
                return error->registry.status;
            error->pc = 4;
            error->operation = "Mushroom substrate Block.o element";
            if (mc_registry_block_o_at(&array, id, out, &error->registry) != MC_REGISTRY_OK)
                return error->registry.status;
            error->pc = 5;
            return MC_REGISTRY_OK;
        }
        case MC_BLOCK_DEAD_BUSH:
        case MC_BLOCK_CROPS:
            stage(error, receiver, receiver->kind == MC_BLOCK_CROPS ? "ni" : "jb",
                  receiver->kind == MC_BLOCK_CROPS ? "ij" : "fz", 0);
            if (!static_id(receiver, receiver->kind == MC_BLOCK_CROPS ? "aB" : "F", 1, 4,
                           &candidate, error))
                return error->registry.status;
            *out = id == candidate;
            error->pc = 15;
            return MC_REGISTRY_OK;
        case MC_BLOCK_FLOWER:
        case MC_BLOCK_SAPLING:
        case MC_BLOCK_TALL_GRASS:
            stage(error, receiver, "wb", "nw", 0);
            if (!static_id(receiver, "v", 1, 4, &candidate, error))
                return error->registry.status;
            if (id == candidate) {
                *out = true;
                break;
            }
            if (!static_id(receiver, "w", 11, 14, &candidate, error))
                return error->registry.status;
            if (id == candidate) {
                *out = true;
                break;
            }
            if (!static_id(receiver, "aB", 21, 24, &candidate, error))
                return error->registry.status;
            *out = id == candidate;
            break;
        default:
            error->registry.status = MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY;
            error->registry.message = "Plant substrate receiver kind";
            error->operation = error->registry.message;
            return error->registry.status;
    }
    error->pc = 35;
    return MC_REGISTRY_OK;
}
