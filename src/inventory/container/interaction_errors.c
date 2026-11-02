#include "interaction_internal.h"

#include <string.h>

void mc_container_interaction_error_init(mc_container_interaction_error *e) {
    if (e == NULL)
        return;
    memset(e, 0, sizeof(*e));
    e->native_policy = true;
    e->site.pc = e->child.pc = e->dependency.pc = -1;
    e->registry.initializer_pc = e->registry.constructor_pc = e->registry.active_initializer_pc =
        e->registry.block_id = e->registry.item_argument = -1;
}

bool inv_interaction_fail(mc_container_interaction_error *e, mc_inventory_status status,
                          const char *operation) {
    e->status = status;
    e->operation = operation;
    e->native_policy = !e->source_exception && !e->source_error;
    return false;
}

bool inv_interaction_null(mc_container_interaction_error *e, const char *operation) {
    e->source_exception = true;
    e->exception_class = "java/lang/NullPointerException";
    return inv_interaction_fail(e, MC_INVENTORY_NULL_REFERENCE, operation);
}

bool inv_interaction_inventory(mc_container_interaction_error *e, mc_inventory_status status) {
    if (status == MC_INVENTORY_OK)
        return true;
    if (e->inventory.frame_count != 0) {
        e->dependency = e->inventory.frames[0];
        if (status == MC_INVENTORY_NULL_REFERENCE || status == MC_INVENTORY_INDEX_OUT_OF_BOUNDS) {
            e->source_exception = true;
            e->exception_class = status == MC_INVENTORY_NULL_REFERENCE
                                     ? "java/lang/NullPointerException"
                                     : "java/lang/ArrayIndexOutOfBoundsException";
        }
    }
    return inv_interaction_fail(e, status, e->inventory.operation);
}

bool inv_interaction_stack(mc_container_interaction_error *e, mc_item_stack_status status,
                           const char *operation) {
    e->stack = status;
    return status == MC_ITEM_STACK_OK ||
           inv_interaction_fail(e,
                                status == MC_ITEM_STACK_LIMIT ? MC_INVENTORY_LIMIT
                                : status == MC_ITEM_STACK_ALLOCATION_FAILED
                                    ? MC_INVENTORY_ALLOCATION_FAILED
                                    : MC_INVENTORY_STACK_FAILED,
                                operation);
}

mc_inventory_status mc_container_retain(mc_container *c, mc_inventory_error *error) {
    mc_inventory_error e = {0};
    if (c == NULL)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else if (c->interaction_holds == SIZE_MAX) {
        e.status = MC_INVENTORY_LIMIT;
        e.operation = "Container native hold bound";
    } else
        ++c->interaction_holds;
    return inv_finish(error, &e);
}

mc_inventory_status mc_container_release(mc_container *c, mc_inventory_error *error) {
    mc_inventory_error e = {0};
    if (c == NULL || c->interaction_holds == 0)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else
        --c->interaction_holds;
    return inv_finish(error, &e);
}

mc_inventory_status mc_container_window_id(const mc_container *c, int32_t *out,
                                           mc_inventory_error *error) {
    mc_inventory_error e = {0};
    if (c == NULL || out == NULL)
        e.status = MC_INVENTORY_INVALID_ARGUMENT;
    else
        *out = c->window_id;
    return inv_finish(error, &e);
}

mc_inventory_status mc_container_source_slots_size(mc_container *c, int32_t *out,
                                                   mc_container_interaction_error *error) {
    mc_container_interaction_error e;
    mc_container_interaction_error_init(&e);
    if (out == NULL)
        (void)inv_interaction_fail(&e, MC_INVENTORY_INVALID_ARGUMENT, "Slot List size output");
    else if (c == NULL || c->slots == NULL)
        (void)inv_interaction_null(&e, "Container source null Slot List size receiver");
    else if (c->slots->size > INT32_MAX)
        (void)inv_interaction_fail(&e, MC_INVENTORY_LIMIT, "Container native Slot List extent");
    else
        *out = (int32_t)c->slots->size;
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_inventory_status mc_container_source_slot(mc_container *c, int32_t index,
                                             mc_inventory_slot **out,
                                             mc_container_interaction_error *error) {
    mc_container_interaction_error e;
    mc_container_interaction_error_init(&e);
    if (out == NULL)
        (void)inv_interaction_fail(&e, MC_INVENTORY_INVALID_ARGUMENT, "Slot List get output");
    else if (c == NULL || c->slots == NULL)
        (void)inv_interaction_null(&e, "Container source null Slot List get receiver");
    else if (index < 0 || (size_t)index >= c->slots->size) {
        e.source_exception = true;
        e.exception_class = "java/lang/IndexOutOfBoundsException";
        (void)inv_interaction_fail(&e, MC_INVENTORY_INDEX_OUT_OF_BOUNDS,
                                   "Container source Slot List signed index");
    } else if (c->slots->values == NULL)
        (void)inv_interaction_fail(&e, MC_INVENTORY_INVALID_ARGUMENT,
                                   "Container native Slot List storage");
    else
        *out = c->slots->values[index];
    if (error != NULL)
        *error = e;
    return e.status;
}

typedef enum interaction_throwable {
    INTERACTION_NO_THROWABLE,
    INTERACTION_SOURCE_EXCEPTION,
    INTERACTION_SOURCE_ERROR
} interaction_throwable;

static const char *registry_source_exception(mc_registry_status status) {
    switch (status) {
        case MC_REGISTRY_NULL_REFERENCE:
            return "java/lang/NullPointerException";
        case MC_REGISTRY_INDEX_OUT_OF_RANGE:
            return "java/lang/ArrayIndexOutOfBoundsException";
        case MC_REGISTRY_ILLEGAL_ARGUMENT:
        case MC_REGISTRY_DUPLICATE_BLOCK:
        case MC_REGISTRY_DUPLICATE_STATISTIC:
            return "java/lang/IllegalArgumentException";
        default:
            return NULL;
    }
}

static bool same_registry_cause(const mc_registry_error *a, const mc_registry_error *b,
                                bool cached) {
    return (cached ? a->status == MC_REGISTRY_FAILED_INITIALIZATION : a->status == b->status) &&
           a->message == b->message && a->initializer_pc == b->initializer_pc &&
           a->block_id == b->block_id && a->constructor_class == b->constructor_class &&
           a->constructor_pc == b->constructor_pc && a->profile_status == b->profile_status &&
           a->active_initializer_class == b->active_initializer_class &&
           a->active_initializer_pc == b->active_initializer_pc &&
           a->item_argument == b->item_argument &&
           a->format_error.status == b->format_error.status &&
           a->format_error.offset == b->format_error.offset &&
           a->format_error.length == b->format_error.length &&
           a->format_error.value == b->format_error.value &&
           a->format_error.flags == b->format_error.flags &&
           a->format_error.conversion == b->format_error.conversion &&
           a->format_error.date_time == b->format_error.date_time;
}

/* JL active use is also reached by IZ's successful use wrapper. These are
 * actual independent initializer latches on the same Registry, including the
 * source translation/recipe dependencies they can reach. Inspect their exact
 * immutable causes; a native failed-bootstrap status alone proves no Error. */
static const mc_registry_error *other_initializer_cause(mc_registry_classes *classes,
                                                        const mc_registry_error *error, bool cached,
                                                        bool exception_only) {
    const mc_registry_class_state *states[] = {&classes->statistics_state,
                                               &classes->statistic_key_state,
                                               &classes->statistic_base_state,
                                               &classes->translation_state,
                                               &classes->translation_wrapper_state,
                                               &classes->achievements_state,
                                               &classes->crafting_state,
                                               &classes->smelting_state};
    for (size_t i = 0; i < sizeof states / sizeof *states; ++i)
        if (states[i]->phase == MC_REGISTRY_FAILED &&
            same_registry_cause(error, &states[i]->failure, cached) &&
            (!exception_only || registry_source_exception(states[i]->failure.status) != NULL))
            return &states[i]->failure;
    return NULL;
}

static void classify_registry(mc_registry_classes *classes, const mc_registry_error *error,
                              interaction_throwable *kind, const char **name) {
    *kind = INTERACTION_NO_THROWABLE;
    *name = NULL;
    const bool cached = error->status == MC_REGISTRY_FAILED_INITIALIZATION;
    const mc_registry_error *first = NULL;
    if (classes != NULL) {
        if (classes->blocks_phase == MC_REGISTRY_FAILED &&
            classes->block_states[MC_BLOCK_BASE].phase == MC_REGISTRY_FAILED &&
            same_registry_cause(&classes->block_failure,
                                &classes->block_states[MC_BLOCK_BASE].failure, false) &&
            same_registry_cause(error, &classes->block_failure, cached))
            first = &classes->block_failure;
        if (first == NULL) {
            for (size_t i = 0; i < (size_t)MC_BLOCK_KIND_COUNT; ++i)
                if (classes->block_states[i].phase == MC_REGISTRY_FAILED &&
                    same_registry_cause(error, &classes->block_states[i].failure, cached)) {
                    first = &classes->block_states[i].failure;
                    break;
                }
        }
        if (first == NULL) {
            for (size_t i = 0; i < (size_t)MC_ITEM_KIND_COUNT; ++i)
                if (classes->item_states[i].phase == MC_REGISTRY_FAILED &&
                    same_registry_cause(error, &classes->item_states[i].failure, cached)) {
                    first = &classes->item_states[i].failure;
                    break;
                }
        }
        if (first == NULL)
            first = other_initializer_cause(classes, error, cached, false);
    }
    if (first != NULL && registry_source_exception(first->status) != NULL) {
        *kind = INTERACTION_SOURCE_ERROR;
        *name = cached ? "java/lang/NoClassDefFoundError" : "java/lang/ExceptionInInitializerError";
    } else if (first != NULL && first->status == MC_REGISTRY_FAILED_INITIALIZATION &&
               classes != NULL) {
        /* A genuinely reached already erroneous dependency propagates its
         * Error unchanged. Match its actual immutable cause rather than
         * inferring a Throwable from FAILED_INITIALIZATION alone. */
        for (size_t i = 0; i < (size_t)MC_BLOCK_KIND_COUNT; ++i)
            if (classes->block_states[i].phase == MC_REGISTRY_FAILED &&
                same_registry_cause(first, &classes->block_states[i].failure, true) &&
                registry_source_exception(classes->block_states[i].failure.status) != NULL) {
                *kind = INTERACTION_SOURCE_ERROR;
                *name = "java/lang/NoClassDefFoundError";
                return;
            }
        for (size_t i = 0; i < (size_t)MC_ITEM_KIND_COUNT; ++i)
            if (classes->item_states[i].phase == MC_REGISTRY_FAILED &&
                same_registry_cause(first, &classes->item_states[i].failure, true) &&
                registry_source_exception(classes->item_states[i].failure.status) != NULL) {
                *kind = INTERACTION_SOURCE_ERROR;
                *name = "java/lang/NoClassDefFoundError";
                return;
            }
        if (other_initializer_cause(classes, first, true, true) != NULL) {
            *kind = INTERACTION_SOURCE_ERROR;
            *name = "java/lang/NoClassDefFoundError";
            return;
        }
    } else if (!cached && error->active_initializer_class == NULL) {
        *name = registry_source_exception(error->status);
        if (*name != NULL)
            *kind = INTERACTION_SOURCE_EXCEPTION;
    }
}

bool inv_interaction_registry(mc_container_interaction_error *e, mc_registry_classes *classes,
                              mc_registry_status status) {
    if (status == MC_REGISTRY_OK)
        return true;
    interaction_throwable kind;
    classify_registry(classes, &e->registry, &kind, &e->exception_class);
    e->source_exception = kind == INTERACTION_SOURCE_EXCEPTION;
    e->source_error = kind == INTERACTION_SOURCE_ERROR;
    if (e->registry.active_initializer_class != NULL)
        e->dependency = (mc_inventory_site){e->registry.active_initializer_class, "<clinit>", "()V",
                                            e->registry.active_initializer_pc};
    return inv_interaction_fail(e,
                                status == MC_REGISTRY_LIMIT ? MC_INVENTORY_LIMIT
                                : status == MC_REGISTRY_ALLOCATION_FAILED
                                    ? MC_INVENTORY_ALLOCATION_FAILED
                                : status == MC_REGISTRY_BUSY ? MC_INVENTORY_BUSY
                                                             : MC_INVENTORY_REGISTRY_FAILED,
                                e->registry.message);
}
