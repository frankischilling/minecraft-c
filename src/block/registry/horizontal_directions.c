#include "horizontal_directions_internal.h"
#include "registry_internal.h"

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

/* Complete paired jj/ge <clinit>. Each newarray/anewarray is one actual arena
 * allocation. The outer c reference is not published until all four aastore
 * operations complete. Private provenance handles retain any failure prefix. */
static mc_registry_status initialize_body(mc_registry_classes *classes) {
    int32_t *array;
#define ARRAY(storage, length, pc)                                                                 \
    do {                                                                                           \
        mc_registry_initializer_stage(classes, pc);                                                \
        array = mc_registry_allocate(classes, (length) * sizeof(*array));                          \
        if (array == NULL)                                                                         \
            return classes->last_error.status;                                                     \
        (storage) = array;                                                                         \
    } while (0)
#define ELEMENT(index, value, pc)                                                                  \
    do {                                                                                           \
        mc_registry_initializer_stage(classes, pc);                                                \
        array[index] = value;                                                                      \
    } while (0)
#define ROW(index, pc)                                                                             \
    do {                                                                                           \
        mc_registry_initializer_stage(classes, pc);                                                \
        classes->horizontal_c_storage[index] = array;                                              \
    } while (0)
    ARRAY(classes->horizontal_storage[0], 4u, 1);
    ELEMENT(0, 3, 6);
    ELEMENT(1, 4, 10);
    ELEMENT(2, 2, 14);
    ELEMENT(3, 5, 18);
    mc_registry_initializer_stage(classes, 19);
    classes->horizontal_a = array;
    ARRAY(classes->horizontal_storage[1], 4u, 23);
    ELEMENT(0, 2, 28);
    ELEMENT(1, 3, 32);
    ELEMENT(2, 0, 36);
    ELEMENT(3, 1, 40);
    mc_registry_initializer_stage(classes, 41);
    classes->horizontal_b = array;
    mc_registry_initializer_stage(classes, 45);
    classes->horizontal_c_storage =
        mc_registry_allocate(classes, 4u * sizeof(*classes->horizontal_c_storage));
    if (classes->horizontal_c_storage == NULL)
        return classes->last_error.status;
    ARRAY(classes->horizontal_rows[0], 6u, 52);
    ELEMENT(0, 1, 57);
    ELEMENT(1, 0, 61);
    ELEMENT(2, 3, 65);
    ELEMENT(3, 2, 69);
    ELEMENT(4, 5, 73);
    ELEMENT(5, 4, 77);
    ROW(0, 78);
    ARRAY(classes->horizontal_rows[1], 6u, 83);
    ELEMENT(0, 1, 88);
    ELEMENT(1, 0, 92);
    ELEMENT(2, 5, 96);
    ELEMENT(3, 4, 100);
    ELEMENT(4, 2, 104);
    ELEMENT(5, 3, 108);
    ROW(1, 109);
    ARRAY(classes->horizontal_rows[2], 6u, 114);
    ELEMENT(0, 1, 119);
    ELEMENT(1, 0, 123);
    ELEMENT(2, 2, 127);
    ELEMENT(3, 3, 131);
    ELEMENT(4, 4, 135);
    ELEMENT(5, 5, 139);
    ROW(2, 140);
    ARRAY(classes->horizontal_rows[3], 6u, 145);
    ELEMENT(0, 1, 150);
    ELEMENT(1, 0, 154);
    ELEMENT(2, 4, 158);
    ELEMENT(3, 5, 162);
    ELEMENT(4, 3, 166);
    ELEMENT(5, 2, 170);
    ROW(3, 171);
    mc_registry_initializer_stage(classes, 172);
    classes->horizontal_c = classes->horizontal_c_storage;
#undef ROW
#undef ELEMENT
#undef ARRAY
    mc_registry_initializer_stage(classes, 175);
    return MC_REGISTRY_OK;
}

static mc_registry_status active_use(mc_registry_classes *classes) {
    mc_registry_class_state *const state = &classes->horizontal_state;
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
    mc_registry_initializer_enter(classes, &frame, "jj", "ge", &state->pc);
    const mc_registry_status status = initialize_body(classes);
    if (status == MC_REGISTRY_OK)
        state->phase = MC_REGISTRY_READY;
    else {
        /* Keep a live enclosing Block position, but do not attribute an old
         * completed/failed Block driver to this independent initializer. */
        if (!enclosing_blocks(classes))
            classes->last_error.initializer_pc = -1;
        state->phase = MC_REGISTRY_FAILED;
        state->failure = classes->last_error;
    }
    mc_registry_initializer_leave(classes, &frame);
    return status;
}

mc_registry_status mc_registry_horizontal_b_get(mc_registry_classes *classes,
                                                mc_registry_horizontal_view *out,
                                                mc_registry_error *error) {
    if (classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Horizontal capture arguments");
    if (classes->entered == UINT_MAX)
        return result(error, MC_REGISTRY_LIMIT, "Horizontal initializer flow count");
    ++classes->entered;
    const mc_registry_status status = active_use(classes);
    --classes->entered;
    if (status != MC_REGISTRY_OK) {
        if (error != NULL)
            *error = classes->last_error;
        return status;
    }
    const int32_t *const identity = classes->horizontal_b;
    *out = (mc_registry_horizontal_view){classes, identity, identity == NULL ? 0u : 4u};
    return result(error, MC_REGISTRY_OK, NULL);
}

static mc_registry_status element_failure(mc_registry_classes *classes, mc_registry_error *error,
                                          mc_registry_status status, const char *message) {
    classes->last_error = fresh(status, message);
    if (error != NULL)
        *error = classes->last_error;
    return status;
}

mc_registry_status mc_registry_horizontal_at(const mc_registry_horizontal_view *array,
                                             int32_t index, int32_t *out,
                                             mc_registry_error *error) {
    if (array == NULL || array->classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Horizontal element arguments");
    if (array->identity == NULL)
        return element_failure(array->classes, error, MC_REGISTRY_NULL_REFERENCE,
                               "Horizontal captured array is null");
    if (array->length != 4)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "Horizontal captured native extent");
    if (array->identity != array->classes->horizontal_storage[0] &&
        array->identity != array->classes->horizontal_storage[1])
        return element_failure(array->classes, error, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY,
                               "Horizontal captured array owner");
    if (index < 0 || (size_t)index >= array->length)
        return element_failure(array->classes, error, MC_REGISTRY_INDEX_OUT_OF_RANGE,
                               "Horizontal captured index");
    *out = array->identity[index];
    return result(error, MC_REGISTRY_OK, NULL);
}
