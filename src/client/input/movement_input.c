#include "mc/client/input/movement_input.h"

#include <float.h>
#include <stdlib.h>

_Static_assert(FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && DBL_MANT_DIG == 53 &&
                   DBL_MAX_EXP == 1024,
               "MovementInput requires IEEE binary32/binary64");
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "MovementInput requires strict floating evaluation"
#endif

struct mc_movement_input {
    mc_movement_input_kind kind;
    mc_movement_input_fields fields;
    bool *keys;
    mc_game_settings *settings;
};

static mc_movement_input_status result(mc_movement_input_error *error,
                                       mc_movement_input_status status,
                                       mc_movement_input_exception exception, const char *operation,
                                       unsigned int bytecode, int32_t named_index) {
    if (error != NULL)
        *error = (mc_movement_input_error){.status = status,
                                           .exception = exception,
                                           .operation = operation,
                                           .bytecode = bytecode,
                                           .named_index = named_index};
    return status;
}

static mc_movement_input_status okay(mc_movement_input_error *error) {
    return result(error, MC_MOVEMENT_INPUT_OK, MC_MOVEMENT_INPUT_EXCEPTION_NONE, NULL, 0, -1);
}

static mc_movement_input_status invalid(mc_movement_input_error *error, const char *operation) {
    return result(error, MC_MOVEMENT_INPUT_INVALID_ARGUMENT, MC_MOVEMENT_INPUT_EXCEPTION_NONE,
                  operation, 0, -1);
}

static mc_movement_input_status create(mc_movement_input_kind kind, mc_game_settings *settings,
                                       mc_movement_input **out, mc_movement_input_error *error) {
    if (out == NULL)
        return invalid(error, "MovementInput constructor output");
    mc_movement_input *input = malloc(sizeof(*input));
    if (input == NULL)
        return result(error, MC_MOVEMENT_INPUT_ALLOCATION_FAILED, MC_MOVEMENT_INPUT_EXCEPTION_NONE,
                      "MovementInput allocation", 0, -1);
    *input = (mc_movement_input){.kind = kind};
    if (kind == MC_MOVEMENT_INPUT_FROM_OPTIONS) {
        input->keys = calloc(MC_MOVEMENT_INPUT_KEYS, sizeof(*input->keys));
        if (input->keys == NULL) {
            free(input);
            return result(error, MC_MOVEMENT_INPUT_ALLOCATION_FAILED,
                          MC_MOVEMENT_INPUT_EXCEPTION_NONE,
                          "MovementInputFromOptions key array allocation", 7, -1);
        }
        input->settings = settings; /* lr.<init> putfield14 follows newarray7. */
    }
    *out = input;
    return okay(error);
}

mc_movement_input_status mc_movement_input_create(mc_movement_input **out,
                                                  mc_movement_input_error *error) {
    return create(MC_MOVEMENT_INPUT_BASE, NULL, out, error);
}

mc_movement_input_status mc_movement_input_create_from_options(mc_game_settings *settings,
                                                               mc_movement_input **out,
                                                               mc_movement_input_error *error) {
    return create(MC_MOVEMENT_INPUT_FROM_OPTIONS, settings, out, error);
}

bool mc_movement_input_read_state(const mc_movement_input *input, mc_movement_input_state *out) {
    if (input == NULL || out == NULL)
        return false;
    mc_movement_input_state state = {.kind = input->kind,
                                     .fields = input->fields,
                                     .settings = input->settings,
                                     .keys_identity = input->keys};
    if (input->kind == MC_MOVEMENT_INPUT_FROM_OPTIONS) {
        state.key_count = MC_MOVEMENT_INPUT_KEYS;
        for (size_t i = 0; i < MC_MOVEMENT_INPUT_KEYS; ++i)
            state.keys[i] = input->keys[i];
    }
    *out = state;
    return true;
}

mc_movement_input_status mc_movement_input_set_fields(mc_movement_input *input,
                                                      const mc_movement_input_fields *fields,
                                                      mc_movement_input_error *error) {
    if (input == NULL || fields == NULL)
        return invalid(error, "MovementInput field assignment arguments");
    input->fields = *fields;
    return okay(error);
}

mc_movement_input_status mc_movement_input_key(mc_movement_input *input, int32_t code, bool pressed,
                                               mc_movement_input_error *error) {
    if (input == NULL)
        return invalid(error, "MovementInput key receiver");
    if (input->kind == MC_MOVEMENT_INPUT_BASE)
        return okay(error);                           /* Actual uo.a(IZ)V return0. */
    static const size_t named[] = {0, 2, 1, 3, 4, 9}; /* kv.m,o,n,p,q,v */
    int selected = -1;
    for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); ++i) {
        mc_game_settings_state settings;
        if (!mc_game_settings_read_state(input->settings, &settings))
            return result(error, MC_MOVEMENT_INPUT_SOURCE_EXCEPTION,
                          MC_MOVEMENT_INPUT_NULL_SETTINGS, "lr.a(IZ)V settings field read",
                          7u + (unsigned int)i * 16u, (int32_t)named[i]);
        mc_game_settings_text name;
        int32_t binding_code;
        if (!mc_key_binding_read(settings.named[named[i]], &name, &binding_code))
            return result(error, MC_MOVEMENT_INPUT_SOURCE_EXCEPTION, MC_MOVEMENT_INPUT_NULL_BINDING,
                          "lr.a(IZ)V binding code read", 10u + (unsigned int)i * 16u,
                          (int32_t)named[i]);
        if (code == binding_code)
            selected = (int)i;
    }
    if (selected >= 0)
        input->keys[selected] = pressed; /* The one actual bastore108. */
    return okay(error);
}

mc_movement_input_status mc_movement_input_reset(mc_movement_input *input,
                                                 mc_movement_input_error *error) {
    if (input == NULL)
        return invalid(error, "MovementInput reset receiver");
    if (input->kind == MC_MOVEMENT_INPUT_FROM_OPTIONS)
        for (size_t i = 0; i < MC_MOVEMENT_INPUT_KEYS; ++i)
            input->keys[i] = false;
    return okay(error);
}

mc_movement_input_status mc_movement_input_update(mc_movement_input *input,
                                                  struct mc_entity *player,
                                                  mc_movement_input_error *error) {
    (void)player; /* Both actual a(Lgs;)V bodies leave the argument unread. */
    if (input == NULL)
        return invalid(error, "MovementInput update receiver");
    if (input->kind == MC_MOVEMENT_INPUT_BASE)
        return okay(error);
    input->fields.strafe = 0.0f;
    input->fields.forward = 0.0f;
    if (input->keys[0])
        input->fields.forward += 1.0f;
    if (input->keys[1])
        input->fields.forward -= 1.0f;
    if (input->keys[2])
        input->fields.strafe += 1.0f;
    if (input->keys[3])
        input->fields.strafe -= 1.0f;
    input->fields.jump = input->keys[4];
    input->fields.sneak = input->keys[5];
    if (input->fields.sneak) {
        input->fields.strafe = (float)((double)input->fields.strafe * 0.3);
        input->fields.forward = (float)((double)input->fields.forward * 0.3);
    }
    return okay(error);
}

mc_movement_input_status mc_movement_input_retire(mc_movement_input **input,
                                                  mc_movement_input_error *error) {
    if (input == NULL)
        return invalid(error, "MovementInput retirement pointer");
    if (*input != NULL) {
        free((*input)->keys);
        free(*input);
        *input = NULL;
    }
    return okay(error);
}
