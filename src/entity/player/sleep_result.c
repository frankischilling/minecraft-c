#include "../classes/classes_internal.h"
#include "sleep_internal.h"
#include <string.h>

static mc_player_sleep_result_status publish(mc_player_sleep_result_error *out,
                                             const mc_player_sleep_result_error *error) {
    if (out != NULL)
        *out = *error;
    return error->status;
}

static mc_player_sleep_result **cell(mc_player_sleep_result_class *state, char field) {
    switch (field) {
        case 'a':
            return &state->a;
        case 'b':
            return &state->b;
        case 'c':
            return &state->c;
        case 'd':
            return &state->d;
        case 'e':
            return &state->e;
        default:
            return NULL;
    }
}

static mc_player_sleep_result_status failed(mc_entity_classes *classes, unsigned int pc,
                                            const char *operation,
                                            mc_player_sleep_result_error *error) {
    mc_player_sleep_result_error *first = &classes->sleep_result_failure;
    first->status = first->native.status == MC_ENTITY_LIMIT
                        ? MC_PLAYER_SLEEP_RESULT_LIMIT
                        : MC_PLAYER_SLEEP_RESULT_ALLOCATION_FAILED;
    first->operation = operation;
    first->site = (mc_player_site){"cw", "<clinit>", "()V", pc};
    first->native_policy = true;
    classes->sleep_result_phase = MC_ENTITY_CLASS_FAILED;
    if (classes->sleep_results != NULL)
        classes->sleep_results->last_pc = pc;
    *error = *first;
    return error->status;
}

static mc_player_sleep_result_status ensure(mc_entity_classes *classes,
                                            mc_player_sleep_result_error *error) {
    if (classes->sleep_result_phase == MC_ENTITY_CLASS_READY)
        return MC_PLAYER_SLEEP_RESULT_OK;
    if (classes->sleep_result_phase == MC_ENTITY_CLASS_FAILED) {
        *error = classes->sleep_result_failure;
        error->status = MC_PLAYER_SLEEP_RESULT_CLASS_FAILED;
        return error->status;
    }
    if (classes->sleep_result_phase != MC_ENTITY_CLASS_COLD) {
        *error = (mc_player_sleep_result_error){.status = MC_PLAYER_SLEEP_RESULT_BUSY,
                                                .operation = "CW native initialization entered",
                                                .native_policy = true};
        return error->status;
    }
    classes->sleep_result_phase = MC_ENTITY_CLASS_INITIALIZING;
    mc_player_sleep_result_class *state = mc_entity_class_memory_allocate(
        classes, sizeof(*state), &classes->sleep_result_failure.native, "CW native class state", 0);
    if (state == NULL)
        return failed(classes, 0, "CW native class state", error);
    memset(state, 0, sizeof(*state));
    classes->sleep_results = state;
    static const char *const names[] = {"OK", "NOT_POSSIBLE_HERE", "NOT_POSSIBLE_NOW",
                                        "TOO_FAR_AWAY", "OTHER_PROBLEM"};
    for (size_t index = 0; index < 5; ++index) {
        const unsigned int pc = (unsigned int)index * 13;
        state->last_pc = pc;
        mc_player_sleep_result *result = mc_entity_class_memory_allocate(
            classes, sizeof(*result), &classes->sleep_result_failure.native, "CW actual NEW",
            (uint16_t)pc);
        if (result == NULL)
            return failed(classes, pc, "CW actual NEW", error);
        memset(result, 0, sizeof(*result));
        result->classes = classes;
        state->candidates[index] = result;
        const size_t length = strlen(names[index]);
        state->names[index].length = length;
        for (size_t unit = 0; unit < length; ++unit)
            state->names[index].units[unit] = (uint8_t)names[index][unit];
        result->phase = MC_ENTITY_CLASS_INITIALIZING;
        /* Actual CW constructor3 enters Enum(String,int): name then ordinal. */
        result->name = &state->names[index];
        result->ordinal = (int32_t)index;
        result->phase = MC_ENTITY_CLASS_READY;
        *cell(state, (char)('a' + index)) = result;
        state->last_pc = state->last_store_pc = pc + 10;
    }
    state->last_pc = 66;
    state->values = mc_entity_class_memory_allocate(classes, sizeof(*state->values),
                                                    &classes->sleep_result_failure.native,
                                                    "CW actual values array", 66);
    if (state->values == NULL)
        return failed(classes, 66, "CW actual values array", error);
    state->values->length = 5;
    for (size_t index = 0; index < 5; ++index)
        state->values->elements[index] = *cell(state, (char)('a' + index));
    state->last_store_pc = 99;
    state->last_pc = 102;
    classes->sleep_result_phase = MC_ENTITY_CLASS_READY;
    return MC_PLAYER_SLEEP_RESULT_OK;
}

mc_player_sleep_result_status
mc_player_sleep_result_static_get(mc_entity_classes *classes, const char *field,
                                  const mc_player_sleep_result **out,
                                  mc_player_sleep_result_error *out_error) {
    mc_player_sleep_result_error error = {0};
    if (classes == NULL || classes->source != MC_ENTITY_CLIENT || field == NULL || field[0] < 'a' ||
        field[0] > 'e' || field[1] != '\0' || out == NULL) {
        error =
            (mc_player_sleep_result_error){.status = MC_PLAYER_SLEEP_RESULT_ARGUMENT,
                                           .operation = "CW native client namespace/static/output",
                                           .native_policy = true};
    } else if (ensure(classes, &error) == MC_PLAYER_SLEEP_RESULT_OK) {
        *out = *cell(classes->sleep_results, field[0]);
    }
    return publish(out_error, &error);
}

bool mc_player_sleep_result_inspect(const mc_player_sleep_result *value,
                                    mc_player_sleep_result_view *out) {
    if (value == NULL || out == NULL)
        return false;
    *out = (mc_player_sleep_result_view){value->classes,
                                         value->phase,
                                         value->name,
                                         value->name == NULL ? NULL : value->name->units,
                                         value->name == NULL ? 0 : value->name->length,
                                         value->ordinal};
    return true;
}

bool mc_player_sleep_result_classes_inspect(const mc_entity_classes *classes,
                                            mc_player_sleep_result_class_view *out) {
    if (classes == NULL || out == NULL || classes->source != MC_ENTITY_CLIENT)
        return false;
    *out = (mc_player_sleep_result_class_view){.phase = classes->sleep_result_phase,
                                               .failure = &classes->sleep_result_failure};
    mc_player_sleep_result_class *state = classes->sleep_results;
    if (state != NULL) {
        out->a = state->a;
        out->b = state->b;
        out->c = state->c;
        out->d = state->d;
        out->e = state->e;
        out->last_pc = state->last_pc;
        out->last_store_pc = state->last_store_pc;
        for (size_t index = 0; index < 5; ++index)
            out->candidates[index] = state->candidates[index];
        if (state->values != NULL) {
            out->values_identity = state->values;
            out->values_length = state->values->length;
            for (size_t index = 0; index < 5; ++index)
                out->values[index] = state->values->elements[index];
        }
    }
    return true;
}

void mc_player_sleep_result_dispose(mc_entity_classes *classes) {
    mc_player_sleep_result_class *state = classes->sleep_results;
    if (state == NULL)
        return;
    mc_entity_class_memory_retire(classes, state->values, sizeof(*state->values));
    for (size_t index = 0; index < 5; ++index)
        mc_entity_class_memory_retire(classes, state->candidates[index],
                                      sizeof(*state->candidates[index]));
    mc_entity_class_memory_retire(classes, state, sizeof(*state));
}
