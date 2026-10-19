#include "mc/block/registry/runtime.h"

#include "registry_internal.h"

#include <stdint.h>

static mc_registry_status result(mc_registry_error *error, mc_registry_status status,
                                 const char *message)
{
    if (error != NULL)
        *error = (mc_registry_error){.status = status,
                                     .message = message,
                                     .initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_class = NULL,
                                     .constructor_pc = -1,
                                     .profile_status = MC_JAVA_RANDOM_PROFILE_OK,
                                     .active_initializer_class = NULL,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return status;
}

static mc_registry_status quiescent(mc_registry_classes *classes, mc_registry_error *error)
{
    if (classes == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "registry owner");
    if (classes->entered != 0)
        return result(error, MC_REGISTRY_BUSY, "registry initialization entered");
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_classes_retain(mc_registry_classes *classes,
                                              mc_registry_side expected_side,
                                              mc_java_random_profile *expected_profile,
                                              mc_registry_error *error)
{
    mc_registry_status status = quiescent(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if ((expected_side != MC_REGISTRY_CLIENT && expected_side != MC_REGISTRY_SERVER) ||
        expected_side != classes->options.side || expected_profile == NULL ||
        expected_profile != classes->options.random_profile)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "registry binding dependencies");
    if (classes->runtime_borrows == SIZE_MAX)
        return result(error, MC_REGISTRY_LIMIT, "registry binding count");
    ++classes->runtime_borrows;
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_classes_release(mc_registry_classes *classes,
                                               mc_registry_error *error)
{
    mc_registry_status status = quiescent(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (classes->runtime_borrows == 0)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "registry binding count is zero");
    --classes->runtime_borrows;
    return result(error, MC_REGISTRY_OK, NULL);
}

static mc_registry_status enter_blocks(mc_registry_classes *classes, mc_registry_error *error)
{
    mc_registry_status status = quiescent(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (classes->blocks_phase == MC_REGISTRY_INITIALIZING)
        return result(error, MC_REGISTRY_BUSY, "registry initialization entered");
    ++classes->entered;
    return MC_REGISTRY_OK;
}

static mc_registry_status leave_blocks(mc_registry_classes *classes, mc_registry_status status,
                                       mc_registry_error *error)
{
    --classes->entered;
    if (status != MC_REGISTRY_OK) {
        if (error != NULL)
            *error = classes->last_error;
        return status;
    }
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_block_static_get(mc_registry_classes *classes, const char *name,
                                                const mc_block **out, mc_registry_error *error)
{
    if (name == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime static arguments");
    mc_registry_status status = enter_blocks(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_block *block = NULL;
    status = mc_registry_block_static_load(classes, name, &block);
    status = leave_blocks(classes, status, error);
    if (status == MC_REGISTRY_OK)
        *out = block;
    return status;
}

mc_registry_status mc_registry_block_get(mc_registry_classes *classes, int32_t id,
                                         const mc_block **out, mc_registry_error *error)
{
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime array output");
    mc_registry_status status = enter_blocks(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    const mc_block *block = NULL;
    status = mc_registry_blocks_active_use(classes);
    if (status == MC_REGISTRY_OK) {
        mc_block **array = classes->blocks.m;
        if (array == NULL)
            status = result(&classes->last_error, MC_REGISTRY_NULL_REFERENCE,
                            "Block runtime m array is null");
        else if (id < 0 || id >= (int32_t)MC_REGISTRY_BLOCK_SLOTS)
            status = result(&classes->last_error, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                            "Block runtime m index");
        else
            block = array[id];
    }
    status = leave_blocks(classes, status, error);
    if (status == MC_REGISTRY_OK)
        *out = block;
    return status;
}

mc_registry_status mc_registry_block_opaque_get(mc_registry_classes *classes, int32_t id, bool *out,
                                                mc_registry_error *error)
{
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime opacity output");
    mc_registry_status status = enter_blocks(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    bool opaque = false;
    status = mc_registry_blocks_active_use(classes);
    if (status == MC_REGISTRY_OK) {
        const bool *array = classes->blocks.o;
        if (array == NULL)
            status = result(&classes->last_error, MC_REGISTRY_NULL_REFERENCE,
                            "Block runtime o array is null");
        else if (id < 0 || id >= (int32_t)MC_REGISTRY_BLOCK_SLOTS)
            status = result(&classes->last_error, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                            "Block runtime o index");
        else
            opaque = array[id];
    }
    status = leave_blocks(classes, status, error);
    if (status == MC_REGISTRY_OK)
        *out = opaque;
    return status;
}

mc_registry_status mc_registry_material_get(mc_registry_classes *classes, mc_material_id id,
                                            mc_material **out, mc_registry_error *error)
{
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Material runtime output");
    mc_registry_status status = quiescent(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if ((unsigned int)id >= (unsigned int)MC_MATERIAL_COUNT)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Material runtime selector");
    if (classes->options.materials == NULL)
        return result(error, MC_REGISTRY_NULL_REFERENCE, "Material runtime owner is null");
    mc_material *material = mc_materials_get(classes->options.materials, id);
    *out = material;
    return result(error, MC_REGISTRY_OK, NULL);
}
