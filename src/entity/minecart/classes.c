#include "../classes/classes_internal.h"
#include "../classes/memory_internal.h"
#include "classes_internal.h"

/* yl/pr.<clinit>: the rows describe only literal instructions. Each source
 * allocation below still creates a distinct, owned array in source order. */
typedef struct initializer_endpoint {
    uint16_t allocation, stores[3], publication;
    int32_t values[3];
} initializer_endpoint;

typedef struct initializer_row {
    uint16_t allocation, publication;
    initializer_endpoint endpoints[2];
} initializer_row;

static const initializer_row rows[10] = {
    {8, 47, {{14, {19, 23, 27}, 28, {0, 0, -1}}, {32, {37, 41, 45}, 46, {0, 0, 1}}}},
    {51, 90, {{57, {62, 66, 70}, 71, {-1, 0, 0}}, {75, {80, 84, 88}, 89, {1, 0, 0}}}},
    {94, 133, {{100, {105, 109, 113}, 114, {-1, -1, 0}}, {118, {123, 127, 131}, 132, {1, 0, 0}}}},
    {137, 176, {{143, {148, 152, 156}, 157, {-1, 0, 0}}, {161, {166, 170, 174}, 175, {1, -1, 0}}}},
    {180, 219, {{186, {191, 195, 199}, 200, {0, 0, -1}}, {204, {209, 213, 217}, 218, {0, -1, 1}}}},
    {223, 262, {{229, {234, 238, 242}, 243, {0, -1, -1}}, {247, {252, 256, 260}, 261, {0, 0, 1}}}},
    {267, 306, {{273, {278, 282, 286}, 287, {0, 0, 1}}, {291, {296, 300, 304}, 305, {1, 0, 0}}}},
    {311, 350, {{317, {322, 326, 330}, 331, {0, 0, 1}}, {335, {340, 344, 348}, 349, {-1, 0, 0}}}},
    {355, 394, {{361, {366, 370, 374}, 375, {0, 0, -1}}, {379, {384, 388, 392}, 393, {-1, 0, 0}}}},
    {399, 438, {{405, {410, 414, 418}, 419, {0, 0, -1}}, {423, {428, 432, 436}, 437, {1, 0, 0}}}}};

static mc_minecart_site site(const mc_entity_classes *classes, bool entered, uint16_t pc) {
    return (mc_minecart_site){.selected = true,
                              .entered = entered,
                              .declaring_class = classes->source == MC_ENTITY_CLIENT ? "yl" : "pr",
                              .method = "<clinit>()V",
                              .bytecode = pc};
}

static mc_entity_status latch(mc_entity_classes *classes, mc_entity_error *error, bool entered,
                              uint16_t pc) {
    error->minecart_initializer = site(classes, entered, pc);
    error->minecart_initializer_cause = error->minecart_initializer;
    classes->minecart_failure = *error;
    classes->minecart_phase = MC_ENTITY_CLASS_FAILED;
    return error->status;
}

static void *array_allocate(mc_entity_classes *classes, size_t header, size_t length,
                            size_t element, uint16_t pc, mc_entity_error *error) {
    if (length > (SIZE_MAX - header) / element) {
        mc_entity_error_set(error, MC_ENTITY_LIMIT, "minecart class initialization", pc);
        return NULL;
    }
    return mc_entity_class_memory_allocate(classes, header + length * element, error,
                                           "minecart class initialization", pc);
}

static void retire_row(mc_entity_classes *classes, mc_minecart_row_array *row) {
    if (row == NULL)
        return;
    for (size_t i = 0; i < row->length; ++i) {
        mc_minecart_int_array *endpoint = row->elements[i];
        if (endpoint != NULL)
            mc_entity_class_memory_retire(classes, endpoint,
                                          sizeof(*endpoint) +
                                              endpoint->length * sizeof(*endpoint->elements));
    }
    mc_entity_class_memory_retire(classes, row,
                                  sizeof(*row) + row->length * sizeof(*row->elements));
}

static void retire_shape(mc_entity_classes *classes, mc_minecart_shape_array *shape) {
    if (shape == NULL)
        return;
    for (size_t i = 0; i < shape->length; ++i)
        retire_row(classes, shape->elements[i]);
    mc_entity_class_memory_retire(classes, shape,
                                  sizeof(*shape) + shape->length * sizeof(*shape->elements));
}

mc_entity_status mc_minecart_class_initialize(mc_entity_classes *classes, mc_entity_error *error) {
    mc_entity_error local_error;
    if (error == NULL)
        error = &local_error;
    mc_entity_error_set(error, MC_ENTITY_OK, NULL, 0);
    if (classes == NULL)
        return mc_entity_error_set(error, MC_ENTITY_INVALID_ARGUMENT,
                                   "minecart class initialization", 0);
    if (classes->minecart_phase == MC_ENTITY_CLASS_READY)
        return MC_ENTITY_OK;
    if (classes->minecart_phase == MC_ENTITY_CLASS_FAILED) {
        *error = classes->minecart_failure;
        error->status = MC_ENTITY_CLASS_FAILED_STATUS;
        error->operation = "minecart class initialization";
        error->bytecode = 0;
        error->minecart_initializer_cause =
            classes->minecart_failure.minecart_initializer_cause.selected
                ? classes->minecart_failure.minecart_initializer_cause
                : classes->minecart_failure.minecart_initializer;
        error->minecart_initializer = site(classes, false, 0);
        return error->status;
    }
    if (classes->minecart_phase != MC_ENTITY_CLASS_COLD) {
        mc_entity_error_set(error, MC_ENTITY_STATE_ERROR, "minecart class initialization", 0);
        error->minecart_initializer = site(classes, false, 0);
        return error->status;
    }
    classes->minecart_phase = MC_ENTITY_CLASS_INITIALIZING;
    const mc_entity_status superclass = mc_entity_base_class_initialize(classes, error);
    if (superclass != MC_ENTITY_OK)
        return latch(classes, error, false, 0);

    uint16_t failure_pc = 2;
    mc_minecart_shape_array *shape =
        array_allocate(classes, sizeof(*shape), 10, sizeof(*shape->elements), failure_pc, error);
    mc_minecart_row_array *row = NULL;
    if (shape == NULL)
        return latch(classes, error, true, failure_pc);
    shape->length = 10;
    for (size_t i = 0; i < shape->length; ++i)
        shape->elements[i] = NULL;
    classes->minecart_bytecode = 2;
    for (size_t i = 0; i < 10; ++i) {
        const initializer_row *source = &rows[i];
        failure_pc = source->allocation;
        row = array_allocate(classes, sizeof(*row), 2, sizeof(*row->elements), failure_pc, error);
        if (row == NULL)
            goto failed;
        row->length = 2;
        row->elements[0] = NULL;
        row->elements[1] = NULL;
        classes->minecart_bytecode = source->allocation;
        for (size_t j = 0; j < 2; ++j) {
            const initializer_endpoint *endpoint_source = &source->endpoints[j];
            failure_pc = endpoint_source->allocation;
            mc_minecart_int_array *endpoint = array_allocate(
                classes, sizeof(*endpoint), 3, sizeof(*endpoint->elements), failure_pc, error);
            if (endpoint == NULL)
                goto failed;
            endpoint->length = 3;
            for (size_t k = 0; k < 3; ++k)
                endpoint->elements[k] = 0;
            classes->minecart_bytecode = endpoint_source->allocation;
            for (size_t k = 0; k < 3; ++k) {
                endpoint->elements[k] = endpoint_source->values[k];
                classes->minecart_bytecode = endpoint_source->stores[k];
            }
            row->elements[j] = endpoint;
            classes->minecart_bytecode = endpoint_source->publication;
        }
        shape->elements[i] = row;
        row = NULL;
        classes->minecart_bytecode = source->publication;
    }
    classes->minecart_j = shape;
    classes->minecart_bytecode = 439;
    classes->minecart_bytecode = 442;
    classes->minecart_phase = MC_ENTITY_CLASS_READY;
    return mc_entity_error_set(error, MC_ENTITY_OK, NULL, 0);

failed:
    /* Neither current row nor completed shape was published to j. */
    retire_row(classes, row);
    retire_shape(classes, shape);
    return latch(classes, error, true, failure_pc);
}

void mc_minecart_class_dispose(mc_entity_classes *classes) {
    retire_shape(classes, classes->minecart_j);
}

mc_entity_status mc_minecart_class_get_snapshot(const mc_entity_classes *classes,
                                                mc_minecart_class_snapshot *out) {
    if (classes == NULL || out == NULL)
        return MC_ENTITY_INVALID_ARGUMENT;
    mc_minecart_class_snapshot value = {.source = classes->source,
                                        .phase = classes->minecart_phase,
                                        .bytecode = classes->minecart_bytecode,
                                        .failure = classes->minecart_failure,
                                        .j = classes->minecart_j};
    const mc_minecart_shape_array *shape = classes->minecart_j;
    if (shape != NULL) {
        if (classes->minecart_phase != MC_ENTITY_CLASS_READY || shape->length != 10)
            return MC_ENTITY_STATE_ERROR;
        value.length = shape->length;
        value.native_array_count = 1;
        value.native_allocation_bytes = sizeof(*shape) + shape->length * sizeof(*shape->elements);
        for (size_t i = 0; i < 10; ++i) {
            const mc_minecart_row_array *row = shape->elements[i];
            if (row == NULL || row->length != 2)
                return MC_ENTITY_STATE_ERROR;
            value.rows[i] = row;
            value.row_lengths[i] = row->length;
            ++value.native_array_count;
            value.native_allocation_bytes += sizeof(*row) + row->length * sizeof(*row->elements);
            for (size_t j = 0; j < 2; ++j) {
                const mc_minecart_int_array *endpoint = row->elements[j];
                if (endpoint == NULL || endpoint->length != 3)
                    return MC_ENTITY_STATE_ERROR;
                value.endpoints[i][j] = endpoint;
                value.endpoint_lengths[i][j] = endpoint->length;
                ++value.native_array_count;
                value.native_allocation_bytes +=
                    sizeof(*endpoint) + endpoint->length * sizeof(*endpoint->elements);
                for (size_t k = 0; k < 3; ++k)
                    value.values[i][j][k] = endpoint->elements[k];
            }
        }
    } else if (classes->minecart_phase == MC_ENTITY_CLASS_READY)
        return MC_ENTITY_STATE_ERROR;
    *out = value;
    return MC_ENTITY_OK;
}
