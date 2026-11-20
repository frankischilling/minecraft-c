#include "registry_internal.h"
#include "runtime_internal.h"

#include <limits.h>

static mc_registry_error fresh(mc_registry_status status, const char *message) {
    return (mc_registry_error){.status = status,
                               .message = message,
                               .initializer_pc = -1,
                               .block_id = -1,
                               .constructor_pc = -1,
                               .active_initializer_pc = -1,
                               .item_argument = -1};
}

static mc_registry_status result(mc_registry_error *error, mc_registry_status status,
                                 const char *message) {
    if (error != NULL)
        *error = fresh(status, message);
    return status;
}

static bool enclosing_blocks(const mc_registry_classes *classes) {
    for (const mc_registry_initializer *frame = classes->initializer; frame != NULL;
         frame = frame->parent)
        if (frame->pc == &classes->block_initializer_pc)
            return true;
    return false;
}

/* No parent game class is initialized by wj/od. Object's empty game-facing
 * dependency has no native allocation/callback here. Each array is one actual
 * retained six-int allocation; native arena records are charged separately. */
static mc_registry_status initialize_body(mc_registry_classes *classes) {
    int32_t *array;
#define ARRAY(index, pc)                                                                           \
    do {                                                                                           \
        mc_registry_initializer_stage(classes, pc);                                                \
        array = mc_registry_allocate(classes, 6u * sizeof(*array));                                \
        if (array == NULL)                                                                         \
            return classes->last_error.status;                                                     \
        classes->direction_storage[index] = array;                                                 \
    } while (0)
#define ELEMENT(index, value, pc)                                                                  \
    do {                                                                                           \
        mc_registry_initializer_stage(classes, pc);                                                \
        array[index] = value;                                                                      \
    } while (0)
#define PUBLISH(field, pc)                                                                         \
    do {                                                                                           \
        mc_registry_initializer_stage(classes, pc);                                                \
        classes->direction_##field = array;                                                        \
    } while (0)
    ARRAY(0, 2);
    ELEMENT(0, 1, 7);
    ELEMENT(1, 0, 11);
    ELEMENT(2, 3, 15);
    ELEMENT(3, 2, 19);
    ELEMENT(4, 5, 23);
    ELEMENT(5, 4, 27);
    PUBLISH(a, 28);
    ARRAY(1, 33);
    ELEMENT(0, 0, 38);
    ELEMENT(1, 0, 42);
    ELEMENT(2, 0, 46);
    ELEMENT(3, 0, 50);
    ELEMENT(4, -1, 54);
    ELEMENT(5, 1, 58);
    PUBLISH(b, 59);
    ARRAY(2, 64);
    ELEMENT(0, -1, 69);
    ELEMENT(1, 1, 73);
    ELEMENT(2, 0, 77);
    ELEMENT(3, 0, 81);
    ELEMENT(4, 0, 85);
    ELEMENT(5, 0, 89);
    PUBLISH(c, 90);
    ARRAY(3, 95);
    ELEMENT(0, 0, 100);
    ELEMENT(1, 0, 104);
    ELEMENT(2, -1, 108);
    ELEMENT(3, 1, 112);
    ELEMENT(4, 0, 116);
    ELEMENT(5, 0, 120);
    PUBLISH(d, 121);
#undef PUBLISH
#undef ELEMENT
#undef ARRAY
    mc_registry_initializer_stage(classes, 124);
    return MC_REGISTRY_OK;
}

static mc_registry_status active_use(mc_registry_classes *classes) {
    mc_registry_class_state *const state = &classes->direction_state;
    if (state->phase == MC_REGISTRY_READY || state->phase == MC_REGISTRY_INITIALIZING)
        return MC_REGISTRY_OK;
    if (state->phase == MC_REGISTRY_FAILED) {
        classes->last_error = state->failure;
        classes->last_error.status = MC_REGISTRY_FAILED_INITIALIZATION;
        return MC_REGISTRY_FAILED_INITIALIZATION;
    }
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = 0;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(classes, &frame, "wj", "od", &state->pc);
    const mc_registry_status status = initialize_body(classes);
    if (status == MC_REGISTRY_OK)
        state->phase = MC_REGISTRY_READY;
    else {
        /* mc_registry_allocate reports the real active wj/od frame. An old
         * completed Block driver is not an enclosing source initializer. */
        if (!enclosing_blocks(classes))
            classes->last_error.initializer_pc = -1;
        state->phase = MC_REGISTRY_FAILED;
        state->failure = classes->last_error;
    }
    mc_registry_initializer_leave(classes, &frame);
    return status;
}

mc_registry_status mc_registry_direction_get(mc_registry_classes *classes,
                                             mc_registry_direction_field field,
                                             mc_registry_direction_view *out,
                                             mc_registry_error *error) {
    if (classes == NULL || out == NULL ||
        (unsigned int)field > (unsigned int)MC_REGISTRY_DIRECTION_D)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Direction capture arguments");
    if (classes->entered == UINT_MAX)
        return result(error, MC_REGISTRY_LIMIT, "Direction initializer flow count");
    ++classes->entered;
    const mc_registry_status status = active_use(classes);
    --classes->entered;
    if (status != MC_REGISTRY_OK) {
        if (error != NULL)
            *error = classes->last_error;
        return status;
    }
    const int32_t *identity = NULL;
    switch (field) {
        case MC_REGISTRY_DIRECTION_A:
            identity = classes->direction_a;
            break;
        case MC_REGISTRY_DIRECTION_B:
            identity = classes->direction_b;
            break;
        case MC_REGISTRY_DIRECTION_C:
            identity = classes->direction_c;
            break;
        case MC_REGISTRY_DIRECTION_D:
            identity = classes->direction_d;
            break;
    }
    *out = (mc_registry_direction_view){classes, identity, identity == NULL ? 0u : 6u};
    return result(error, MC_REGISTRY_OK, NULL);
}

static mc_registry_status element_failure(mc_registry_classes *classes, mc_registry_error *error,
                                          mc_registry_status status, const char *message) {
    classes->last_error = fresh(status, message);
    if (error != NULL)
        *error = classes->last_error;
    return status;
}

mc_registry_status mc_registry_direction_at(const mc_registry_direction_view *array, int32_t index,
                                            int32_t *out, mc_registry_error *error) {
    if (array == NULL || array->classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Direction element arguments");
    if (array->identity == NULL)
        return element_failure(array->classes, error, MC_REGISTRY_NULL_REFERENCE,
                               "Direction captured array is null");
    if (array->length != 6)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Direction captured native extent");
    bool known = false;
    for (size_t i = 0; i < 4; ++i)
        if (array->identity == array->classes->direction_storage[i])
            known = true;
    if (!known)
        return element_failure(array->classes, error, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                               "Direction captured array owner");
    if (index < 0 || (size_t)index >= array->length)
        return element_failure(array->classes, error, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                               "Direction captured index");
    *out = array->identity[index];
    return result(error, MC_REGISTRY_OK, NULL);
}
