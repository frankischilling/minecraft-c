#include "mc/item/registry/runtime.h"

#include "../../block/registry/registry_internal.h"

#include <string.h>

static mc_registry_status result(mc_registry_error *error, mc_registry_status status,
                                 const char *message) {
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

static mc_registry_status quiescent(const mc_registry_classes *classes, mc_registry_error *error) {
    if (classes == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Item runtime registry owner");
    if (classes->entered != 0)
        return result(error, MC_REGISTRY_BUSY, "registry initialization entered");
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_item_base_icon(mc_registry_classes *classes, const mc_item *item,
                                              int32_t damage, int32_t *out,
                                              mc_registry_error *error) {
    (void)damage; /* Actual GM body reads no argument. */
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Item icon native output");
    if (item == NULL)
        return result(error, MC_REGISTRY_NULL_REFERENCE, "actual GM icon null receiver");
    const mc_registry_status status = quiescent(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (item->classes != classes)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Item icon actual Registry owner");
    if (item->kind != MC_ITEM_BASE)
        return result(error, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                      "actual Item icon override is not connected");
    const int32_t icon = item->fields.bh; /* Actual GM.a(I) getfield1. */
    *out = icon;
    return result(error, MC_REGISTRY_OK, NULL); /* Actual return4. */
}

static mc_registry_status enter_items(mc_registry_classes *classes, mc_registry_error *error) {
    const mc_registry_status status = quiescent(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (classes->item_states[MC_ITEM_BASE].phase == MC_REGISTRY_INITIALIZING)
        return result(error, MC_REGISTRY_BUSY, "registry initialization entered");
    ++classes->entered;
    return MC_REGISTRY_OK;
}

static mc_registry_status leave_items(mc_registry_classes *classes, mc_registry_status status,
                                      mc_registry_error *error) {
    --classes->entered;
    if (status != MC_REGISTRY_OK) {
        if (error != NULL)
            *error = classes->last_error;
        return status;
    }
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_item_static_get(mc_registry_classes *classes, const char *name,
                                               const mc_item **out, mc_registry_error *error) {
    if (name == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Item runtime static arguments");
    mc_registry_status status = enter_items(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    const mc_item *item = NULL;
    status = mc_registry_items_active_use(classes);
    if (status == MC_REGISTRY_OK) {
        bool found = false;
        for (size_t index = 0; index < 106; ++index)
            if (strcmp(mc_registry_item_static_name(index), name) == 0) {
                item = classes->items.named[index];
                found = true;
                break;
            }
        if (!found)
            status = result(&classes->last_error, MC_REGISTRY_INVALID_ARGUMENT,
                            "unknown Item runtime static name");
    }
    status = leave_items(classes, status, error);
    if (status == MC_REGISTRY_OK)
        *out = item;
    return status;
}

mc_registry_status mc_registry_item_c_get(mc_registry_classes *classes,
                                          const mc_registry_item_array **out,
                                          mc_registry_error *error) {
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Item runtime c output");
    mc_registry_status status = enter_items(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    const mc_registry_item_array *array = NULL;
    status = mc_registry_items_active_use(classes);
    if (status == MC_REGISTRY_OK)
        array = classes->items.c;
    status = leave_items(classes, status, error);
    if (status == MC_REGISTRY_OK)
        *out = array;
    return status;
}

mc_registry_status mc_registry_item_b_get(mc_registry_classes *classes, mc_java_random **out,
                                          mc_registry_error *error) {
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Item runtime b output");
    mc_registry_status status = enter_items(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    mc_java_random *random = NULL;
    status = mc_registry_items_active_use(classes);
    if (status == MC_REGISTRY_OK)
        random = classes->items.b;
    status = leave_items(classes, status, error);
    if (status == MC_REGISTRY_OK)
        *out = random;
    return status;
}

mc_registry_status mc_registry_item_c_at(mc_registry_classes *classes,
                                         const mc_registry_item_array *captured_array,
                                         int32_t index, const mc_item **out,
                                         mc_registry_error *error) {
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Item runtime c element output");
    mc_registry_status status = quiescent(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (captured_array == NULL)
        status = result(&classes->last_error, MC_REGISTRY_NULL_REFERENCE,
                        "Item runtime captured c array is null");
    else if (index < 0 || (size_t)index >= captured_array->length)
        status = result(&classes->last_error, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                        "Item runtime captured c index");
    else if (captured_array->elements == NULL)
        status = result(&classes->last_error, MC_REGISTRY_INVALID_ARGUMENT,
                        "Item runtime captured c native storage");
    else {
        *out = captured_array->elements[index];
        return result(error, MC_REGISTRY_OK, NULL);
    }
    if (error != NULL)
        *error = classes->last_error;
    return status;
}

/* These three ordinary operations deliberately do not enter Item active use.
 * The enclosing caller already evaluated its actual getstatic when reached. */
static mc_registry_status source_item(mc_registry_classes *classes, const mc_item *item,
                                      mc_registry_error *error) {
    const mc_registry_status status = quiescent(classes, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (item->classes != classes || (unsigned int)item->kind >= (unsigned int)MC_ITEM_KIND_COUNT)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Item source storage provenance");
    return MC_REGISTRY_OK;
}

static bool food_kind(mc_item_kind kind) {
    return kind == MC_ITEM_FOOD || kind == MC_ITEM_SOUP || kind == MC_ITEM_COOKIE;
}

mc_registry_status mc_registry_item_source_id(mc_registry_classes *classes, const mc_item *item,
                                              int32_t *out, mc_registry_error *error) {
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Item source id output");
    if (item == NULL)
        return result(error, MC_REGISTRY_NULL_REFERENCE, "actual GM.bf null receiver");
    const mc_registry_status status = source_item(classes, item, error);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = item->fields.bf;
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_item_food_type(mc_registry_classes *classes, const mc_item *item,
                                              bool *out, mc_registry_error *error) {
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Item instanceof Food output");
    if (item == NULL) {
        *out = false;
        return result(error, MC_REGISTRY_OK, NULL);
    }
    const mc_registry_status status = source_item(classes, item, error);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = food_kind(item->kind);
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_item_food_wolf_preferred(mc_registry_classes *classes,
                                                        const mc_item *item, bool *out,
                                                        mc_registry_error *error) {
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Food.m output");
    if (item == NULL)
        return result(error, MC_REGISTRY_NULL_REFERENCE, "actual Food.m null receiver");
    const mc_registry_status status = source_item(classes, item, error);
    if (status != MC_REGISTRY_OK)
        return status;
    if (!food_kind(item->kind))
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Food.m actual Food storage");
    *out = item->yw.bk;                         /* Actual YW.m getfield1, inherited by BZ and QK. */
    return result(error, MC_REGISTRY_OK, NULL); /* ireturn4. */
}
