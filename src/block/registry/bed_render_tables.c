#include "bed_render_tables_internal.h"
#include "registry_internal.h"

#include <limits.h>

static mc_registry_status result(mc_registry_error *out, mc_registry_status status,
                                 const char *operation) {
    if (out != NULL)
        *out = (mc_registry_error){.status = status,
                                   .message = operation,
                                   .initializer_pc = -1,
                                   .block_id = -1,
                                   .constructor_pc = -1,
                                   .active_initializer_pc = -1,
                                   .item_argument = -1};
    return status;
}

static mc_registry_status body(mc_registry_classes *c) {
    static const int32_t a[4] = {3, 4, 2, 5}, b[4] = {2, 3, 0, 1};
    static const int32_t rows[4][6] = {
        {1, 0, 3, 2, 5, 4}, {1, 0, 5, 4, 2, 3}, {1, 0, 2, 3, 4, 5}, {1, 0, 4, 5, 3, 2}};
    int32_t *array;
    mc_registry_initializer_stage(c, 1);
    array = mc_registry_allocate(c, sizeof a);
    if (array == NULL)
        return c->last_error.status;
    for (size_t i = 0; i < 4; ++i) {
        mc_registry_initializer_stage(c, 6 + (int32_t)i * 4);
        array[i] = a[i];
    }
    mc_registry_initializer_stage(c, 19);
    c->bed_render_a = array;
    mc_registry_initializer_stage(c, 23);
    array = mc_registry_allocate(c, sizeof b);
    if (array == NULL)
        return c->last_error.status;
    for (size_t i = 0; i < 4; ++i) {
        mc_registry_initializer_stage(c, 28 + (int32_t)i * 4);
        array[i] = b[i];
    }
    mc_registry_initializer_stage(c, 41);
    c->bed_render_b = array;
    mc_registry_initializer_stage(c, 45);
    int32_t **matrix = mc_registry_allocate(c, 4u * sizeof(*matrix));
    if (matrix == NULL)
        return c->last_error.status;
    for (size_t row = 0; row < 4; ++row) {
        mc_registry_initializer_stage(c, 52 + (int32_t)row * 31);
        array = mc_registry_allocate(c, sizeof rows[row]);
        if (array == NULL)
            return c->last_error.status;
        for (size_t i = 0; i < 6; ++i) {
            mc_registry_initializer_stage(c, 57 + (int32_t)row * 31 + (int32_t)i * 4);
            array[i] = rows[row][i];
        }
        mc_registry_initializer_stage(c, 78 + (int32_t)row * 31);
        matrix[row] = array;
    }
    mc_registry_initializer_stage(c, 172);
    c->bed_render_c = matrix;
    mc_registry_initializer_stage(c, 175);
    return MC_REGISTRY_OK;
}

static mc_registry_status active_use(mc_registry_classes *c, mc_registry_error *out) {
    if (c == NULL || c->options.side != MC_REGISTRY_CLIENT)
        return result(out, MC_REGISTRY_INVALID_ARGUMENT, "JJ actual client Registry owner");
    if (c->entered == UINT_MAX)
        return result(out, MC_REGISTRY_LIMIT, "JJ initializer flow count");
    mc_registry_class_state *state = &c->bed_render_state;
    if (state->phase == MC_REGISTRY_FAILED) {
        c->last_error = state->failure;
        c->last_error.status = MC_REGISTRY_FAILED_INITIALIZATION;
        if (out != NULL)
            *out = c->last_error;
        return c->last_error.status;
    }
    if (state->phase == MC_REGISTRY_READY || state->phase == MC_REGISTRY_INITIALIZING)
        return result(out, MC_REGISTRY_OK, NULL);
    ++c->entered;
    state->phase = MC_REGISTRY_INITIALIZING;
    state->pc = 0;
    mc_registry_initializer frame;
    mc_registry_initializer_enter(c, &frame, "jj", NULL, &state->pc);
    const mc_registry_status status = body(c);
    if (status == MC_REGISTRY_OK)
        state->phase = MC_REGISTRY_READY;
    else {
        bool enclosing_blocks = false;
        for (const mc_registry_initializer *f = frame.parent; f != NULL; f = f->parent)
            if (f->pc == &c->block_initializer_pc)
                enclosing_blocks = true;
        if (!enclosing_blocks)
            c->last_error.initializer_pc = -1;
        state->phase = MC_REGISTRY_FAILED;
        state->failure = c->last_error;
    }
    mc_registry_initializer_leave(c, &frame);
    --c->entered;
    if (status != MC_REGISTRY_OK) {
        if (out != NULL)
            *out = c->last_error;
        return status;
    }
    return result(out, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_bed_render_array_get(mc_registry_classes *c, bool second,
                                                    mc_registry_bed_render_array *out,
                                                    mc_registry_error *error) {
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "JJ array output");
    const mc_registry_status status = active_use(c, error);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = (mc_registry_bed_render_array){c, second ? c->bed_render_b : c->bed_render_a};
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_bed_render_matrix_get(mc_registry_classes *c,
                                                     mc_registry_bed_render_matrix *out,
                                                     mc_registry_error *error) {
    if (out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "JJ matrix output");
    const mc_registry_status status = active_use(c, error);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = (mc_registry_bed_render_matrix){c, c->bed_render_c};
    return result(error, MC_REGISTRY_OK, NULL);
}

static mc_registry_status element(mc_registry_classes *c, const void *identity, size_t width,
                                  int32_t index, mc_registry_error *error) {
    if (identity == NULL)
        return result(error, MC_REGISTRY_NULL_REFERENCE, "JJ captured array is null");
    size_t bytes;
    if (!mc_registry_permanent_extent(c, identity, &bytes) || bytes % width != 0)
        return result(error, MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY, "JJ captured array ownership");
    if (index < 0 || (size_t)index >= bytes / width)
        return result(error, MC_REGISTRY_INDEX_OUT_OF_RANGE, "JJ captured array index");
    return MC_REGISTRY_OK;
}

mc_registry_status mc_registry_bed_render_array_at(const mc_registry_bed_render_array *array,
                                                   int32_t index, int32_t *out,
                                                   mc_registry_error *error) {
    if (array == NULL || array->classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "JJ element arguments");
    const mc_registry_status status =
        element(array->classes, array->identity, sizeof(*array->identity), index, error);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = array->identity[index];
    return result(error, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_registry_bed_render_matrix_at(const mc_registry_bed_render_matrix *matrix,
                                                    int32_t index,
                                                    mc_registry_bed_render_array *out,
                                                    mc_registry_error *error) {
    if (matrix == NULL || matrix->classes == NULL || out == NULL)
        return result(error, MC_REGISTRY_INVALID_ARGUMENT, "JJ row arguments");
    const mc_registry_status status =
        element(matrix->classes, matrix->identity, sizeof(*matrix->identity), index, error);
    if (status != MC_REGISTRY_OK)
        return status;
    *out = (mc_registry_bed_render_array){matrix->classes, matrix->identity[index]};
    return result(error, MC_REGISTRY_OK, NULL);
}
