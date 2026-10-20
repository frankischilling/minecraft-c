#include "mc/block/registry/runtime.h"

#include "registry_internal.h"
#include "runtime_internal.h"

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

static mc_registry_status quiescent(const mc_registry_classes *classes, mc_registry_error *error)
{
    if (classes == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "registry owner");
    if (classes->entered != 0)
        return result(error, MC_REGISTRY_BUSY, "registry initialization entered");
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_runtime_literal_ascii(mc_registry_classes *classes, const char *text,
                                                     const mc_registry_string **out,
                                                     mc_registry_error *error)
{
    if (classes == NULL || text == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "runtime literal arguments");
    const mc_registry_string *literal = NULL;
    const mc_registry_status status = mc_registry_literal_ascii(classes, text, &literal);
    if (status != MC_REGISTRY_OK) {
        const char *message = classes->last_error.message;
        mc_registry_error cause;
        (void)result(&cause, status, message);
        classes->last_error = cause;
        if (error != NULL)
            *error = cause;
        return status;
    }
    *out = literal;
    return result(error, MC_REGISTRY_OK, NULL);
}

bool mc_registry_runtime_borrow_release(mc_registry_classes *classes)
{
    if (classes == NULL || classes->runtime_borrows == 0)
        return false;
    --classes->runtime_borrows;
    return true;
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

mc_registry_status mc_registry_runtime_borrow_release_admit(const mc_registry_classes *classes,
                                                            mc_registry_error *error)
{
    mc_registry_status status = quiescent(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (classes->runtime_borrows == 0)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "registry binding count is zero");
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_classes_release(mc_registry_classes *classes,
                                               mc_registry_error *error)
{
    const mc_registry_status status = mc_registry_runtime_borrow_release_admit(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    --classes->runtime_borrows;
    return status;
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

mc_registry_status mc_registry_block_q_get(mc_registry_classes *classes,
                                           mc_registry_block_q_view *out, mc_registry_error *error)
{
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime q output");
    mc_registry_status status = enter_blocks(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    const int32_t *identity = NULL;
    status = mc_registry_blocks_active_use(classes);
    if (status == MC_REGISTRY_OK)
        identity = classes->blocks.q;
    status = leave_blocks(classes, status, error);
    if (status == MC_REGISTRY_OK)
        *out =
            (mc_registry_block_q_view){.classes = classes,
                                       .identity = identity,
                                       .length = identity == NULL ? 0u : MC_REGISTRY_BLOCK_SLOTS};
    return status;
}

mc_registry_status mc_registry_block_q_at(const mc_registry_block_q_view *array, int32_t index,
                                          int32_t *out, mc_registry_error *error)
{
    if (array == NULL || array->classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime q element arguments");
    mc_registry_status status;
    if (array->identity == NULL) {
        status = result(&array->classes->last_error, MC_REGISTRY_NULL_REFERENCE,
                        "Block runtime captured q array is null");
    } else {
        if (array->length > MC_REGISTRY_BLOCK_SLOTS)
            return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime q native extent");
        if (index < 0 || (size_t)index >= array->length) {
            status = result(&array->classes->last_error, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                            "Block runtime captured q index");
        } else {
            *out = array->identity[index];
            return result(error, MC_REGISTRY_OK, NULL);
        }
    }
    if (error != NULL)
        *error = array->classes->last_error;
    return status;
}

mc_registry_status mc_registry_block_m_get(mc_registry_classes *classes,
                                           mc_registry_block_m_view *out, mc_registry_error *error)
{
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime m output");
    mc_registry_status status = enter_blocks(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_block *const *identity = NULL;
    status = mc_registry_blocks_active_use(classes);
    if (status == MC_REGISTRY_OK)
        identity = classes->blocks.m;
    status = leave_blocks(classes, status, error);
    if (status == MC_REGISTRY_OK)
        *out = (mc_registry_block_m_view){classes, identity,
                                          identity == NULL ? 0u : MC_REGISTRY_BLOCK_SLOTS};
    return status;
}

mc_registry_status mc_registry_block_m_at(const mc_registry_block_m_view *array, int32_t index,
                                          const mc_block **out, mc_registry_error *error)
{
    if (array == NULL || array->classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime m element arguments");
    mc_registry_status status;
    if (array->identity == NULL)
        status = result(&array->classes->last_error, MC_REGISTRY_NULL_REFERENCE,
                        "Block runtime captured m array is null");
    else if (array->length > MC_REGISTRY_BLOCK_SLOTS)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime m native extent");
    else if (index < 0 || (size_t)index >= array->length)
        status = result(&array->classes->last_error, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                        "Block runtime captured m index");
    else {
        *out = array->identity[index];
        return result(error, MC_REGISTRY_OK, NULL);
    }
    if (error != NULL)
        *error = array->classes->last_error;
    return status;
}

mc_registry_status mc_registry_block_o_get(mc_registry_classes *classes,
                                           mc_registry_block_o_view *out, mc_registry_error *error)
{
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime o output");
    mc_registry_status status = enter_blocks(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    const bool *identity = NULL;
    status = mc_registry_blocks_active_use(classes);
    if (status == MC_REGISTRY_OK)
        identity = classes->blocks.o;
    status = leave_blocks(classes, status, error);
    if (status == MC_REGISTRY_OK)
        *out = (mc_registry_block_o_view){classes, identity,
                                          identity == NULL ? 0u : MC_REGISTRY_BLOCK_SLOTS};
    return status;
}

mc_registry_status mc_registry_block_o_at(const mc_registry_block_o_view *array, int32_t index,
                                          bool *out, mc_registry_error *error)
{
    if (array == NULL || array->classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime o element arguments");
    mc_registry_status status;
    if (array->identity == NULL)
        status = result(&array->classes->last_error, MC_REGISTRY_NULL_REFERENCE,
                        "Block runtime captured o array is null");
    else if (array->length > MC_REGISTRY_BLOCK_SLOTS)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime o native extent");
    else if (index < 0 || (size_t)index >= array->length)
        status = result(&array->classes->last_error, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                        "Block runtime captured o index");
    else {
        *out = array->identity[index];
        return result(error, MC_REGISTRY_OK, NULL);
    }
    if (error != NULL)
        *error = array->classes->last_error;
    return status;
}

mc_registry_status mc_registry_block_p_get(mc_registry_classes *classes,
                                           mc_registry_block_p_view *out, mc_registry_error *error)
{
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime p output");
    mc_registry_status status = enter_blocks(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    const bool *identity = NULL;
    status = mc_registry_blocks_active_use(classes);
    if (status == MC_REGISTRY_OK)
        identity = classes->blocks.p;
    status = leave_blocks(classes, status, error);
    if (status == MC_REGISTRY_OK)
        *out = (mc_registry_block_p_view){classes, identity,
                                          identity == NULL ? 0u : MC_REGISTRY_BLOCK_SLOTS};
    return status;
}

mc_registry_status mc_registry_block_p_at(const mc_registry_block_p_view *array, int32_t index,
                                          bool *out, mc_registry_error *error)
{
    if (array == NULL || array->classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime p element arguments");
    mc_registry_status status;
    if (array->identity == NULL)
        status = result(&array->classes->last_error, MC_REGISTRY_NULL_REFERENCE,
                        "Block runtime captured p array is null");
    else if (array->length > MC_REGISTRY_BLOCK_SLOTS)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime p native extent");
    else if (index < 0 || (size_t)index >= array->length)
        status = result(&array->classes->last_error, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                        "Block runtime captured p index");
    else {
        *out = array->identity[index];
        return result(error, MC_REGISTRY_OK, NULL);
    }
    if (error != NULL)
        *error = array->classes->last_error;
    return status;
}

mc_registry_status mc_registry_block_t_get(mc_registry_classes *classes,
                                           mc_registry_block_t_view *out, mc_registry_error *error)
{
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime t output");
    mc_registry_status status = enter_blocks(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    const bool *identity = NULL;
    status = mc_registry_blocks_active_use(classes);
    if (status == MC_REGISTRY_OK)
        identity = classes->blocks.t;
    status = leave_blocks(classes, status, error);
    if (status == MC_REGISTRY_OK)
        *out = (mc_registry_block_t_view){classes, identity,
                                          identity == NULL ? 0u : MC_REGISTRY_BLOCK_SLOTS};
    return status;
}

mc_registry_status mc_registry_block_t_at(const mc_registry_block_t_view *array, int32_t index,
                                          bool *out, mc_registry_error *error)
{
    if (array == NULL || array->classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime t element arguments");
    mc_registry_status status;
    if (array->identity == NULL)
        status = result(&array->classes->last_error, MC_REGISTRY_NULL_REFERENCE,
                        "Block runtime captured t array is null");
    else if (array->length > MC_REGISTRY_BLOCK_SLOTS)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Block runtime t native extent");
    else if (index < 0 || (size_t)index >= array->length)
        status = result(&array->classes->last_error, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                        "Block runtime captured t index");
    else {
        *out = array->identity[index];
        return result(error, MC_REGISTRY_OK, NULL);
    }
    if (error != NULL)
        *error = array->classes->last_error;
    return status;
}
