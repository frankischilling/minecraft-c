#include "internal.h"

#include <string.h>

static const char *kind_class(const mc_entity_classes *classes) {
    return classes->source == MC_ENTITY_CLIENT ? "jg" : "gd";
}

static mc_entity_hit_status kind_failure(mc_entity_classes *classes, uint16_t pc,
                                         const char *operation) {
    mc_entity_hit_kind_error *failure = &classes->hit_kind_failure;
    failure->status = MC_ENTITY_HIT_NATIVE_FAILED;
    failure->operation = operation;
    failure->native_policy = true;
    failure->site =
        (mc_entity_geometry_site){true, true, kind_class(classes), "<clinit>", "()V", pc};
    classes->hit_kind_phase = MC_ENTITY_CLASS_FAILED;
    return failure->status;
}

mc_entity_hit_status mc_entity_hit_kind_ensure(mc_entity_classes *classes,
                                               mc_entity_hit_kind_error *error) {
    memset(error, 0, sizeof(*error));
    if (classes->hit_kind_phase == MC_ENTITY_CLASS_READY)
        return MC_ENTITY_HIT_OK;
    if (classes->hit_kind_phase == MC_ENTITY_CLASS_FAILED) {
        *error = classes->hit_kind_failure;
        return MC_ENTITY_HIT_CLASS_FAILED;
    }
    if (classes->hit_kind_phase != MC_ENTITY_CLASS_COLD) {
        error->status = MC_ENTITY_HIT_NATIVE_FAILED;
        error->native_policy = true;
        error->operation = "Hit kind recursive native initialization";
        error->native.status = MC_ENTITY_STATE_ERROR;
        return error->status;
    }
    classes->hit_kind_phase = MC_ENTITY_CLASS_INITIALIZING;
    mc_entity_hit_kind_state *state =
        mc_entity_class_memory_allocate(classes, sizeof(*state), &classes->hit_kind_failure.native,
                                        "Hit kind native class state", 0);
    if (state == NULL) {
        kind_failure(classes, 0, "Hit kind native class state");
        *error = classes->hit_kind_failure;
        return error->status;
    }
    memset(state, 0, sizeof(*state));
    classes->hit_kinds = state;
    state->names[0] = (mc_entity_hit_name){{'T', 'I', 'L', 'E'}, 4};
    state->names[1] = (mc_entity_hit_name){{'E', 'N', 'T', 'I', 'T', 'Y'}, 6};
    for (size_t index = 0; index < 2; ++index) {
        const uint16_t pc = index == 0 ? 0 : 13;
        mc_entity_hit_kind *kind = mc_entity_class_memory_allocate(
            classes, sizeof(*kind), &classes->hit_kind_failure.native, "Hit kind NEW", pc);
        if (kind == NULL) {
            kind_failure(classes, pc, "Hit kind NEW");
            *error = classes->hit_kind_failure;
            return error->status;
        }
        memset(kind, 0, sizeof(*kind));
        kind->classes = classes;
        state->candidates[index] = kind;
        kind->phase = MC_ENTITY_HIT_CONSTRUCTING;
        /* Actual Enum constructor name then ordinal stores beneath JG/GD ctor3.
         * These literals are canonical class-owned UTF16 objects, not labels. */
        kind->name = &state->names[index];
        kind->ordinal = (int32_t)index;
        kind->phase = MC_ENTITY_HIT_READY;
        if (index == 0)
            state->a = kind; /* putstatic10 */
        else
            state->b = kind; /* putstatic23 */
    }
    state->pending_values = mc_entity_class_memory_allocate(classes, sizeof(*state->pending_values),
                                                            &classes->hit_kind_failure.native,
                                                            "Hit kind values array", 27);
    if (state->pending_values == NULL) {
        kind_failure(classes, 27, "Hit kind values array");
        *error = classes->hit_kind_failure;
        return error->status;
    }
    memset(state->pending_values, 0, sizeof(*state->pending_values));
    state->pending_values->length = 2;
    state->pending_values->elements[0] = state->a; /* aastore35 */
    state->pending_values->elements[1] = state->b; /* aastore41 */
    state->values = state->pending_values;         /* putstatic42 */
    classes->hit_kind_phase = MC_ENTITY_CLASS_READY;
    return MC_ENTITY_HIT_OK;
}

mc_entity_hit_status mc_entity_hit_kind_class_inspect(const mc_entity_classes *classes,
                                                      mc_entity_hit_kind_class_view *out) {
    if (classes == NULL || out == NULL)
        return MC_ENTITY_HIT_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->phase = classes->hit_kind_phase;
    out->failure =
        classes->hit_kind_phase == MC_ENTITY_CLASS_FAILED ? &classes->hit_kind_failure : NULL;
    const mc_entity_hit_kind_state *state = classes->hit_kinds;
    if (state != NULL) {
        out->a = state->a;
        out->b = state->b;
        out->candidates[0] = state->candidates[0];
        out->candidates[1] = state->candidates[1];
        out->values_identity = state->values;
        if (state->values != NULL) {
            out->values_length = state->values->length;
            out->values[0] = state->values->elements[0];
            out->values[1] = state->values->elements[1];
        }
    }
    return MC_ENTITY_HIT_OK;
}

mc_entity_hit_status mc_entity_hit_kind_inspect(const mc_entity_hit_kind *kind,
                                                mc_entity_hit_kind_view *out) {
    if (kind == NULL || out == NULL)
        return MC_ENTITY_HIT_INVALID_ARGUMENT;
    *out = (mc_entity_hit_kind_view){kind->classes,
                                     kind->phase,
                                     kind->name,
                                     kind->name == NULL ? NULL : kind->name->units,
                                     kind->name == NULL ? 0 : kind->name->length,
                                     kind->ordinal};
    return MC_ENTITY_HIT_OK;
}

void mc_entity_hit_kind_dispose(mc_entity_classes *classes) {
    mc_entity_hit_kind_state *state = classes->hit_kinds;
    if (state != NULL) {
        mc_entity_class_memory_retire(classes, state->pending_values,
                                      sizeof(*state->pending_values));
        for (size_t index = 0; index < 2; ++index)
            mc_entity_class_memory_retire(classes, state->candidates[index],
                                          sizeof(*state->candidates[index]));
        mc_entity_class_memory_retire(classes, state, sizeof(*state));
    }
}
