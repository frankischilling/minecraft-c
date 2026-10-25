#include "mc/client/input/movement_input.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;
#define CHECK(test)                                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(test)) {                                                                             \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test);                             \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static float from_bits(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static unsigned int keys(const mc_movement_input_state *state) {
    unsigned int mask = 0;
    for (size_t i = 0; i < MC_MOVEMENT_INPUT_KEYS; ++i)
        if (state->keys[i])
            mask |= 1u << i;
    return mask;
}

static bool fields_equal(mc_movement_input_fields a, mc_movement_input_fields b) {
    return bits(a.strafe) == bits(b.strafe) && bits(a.forward) == bits(b.forward) &&
           a.source_c == b.source_c && a.jump == b.jump && a.sneak == b.sneak;
}

static bool code_set(mc_game_settings *settings, size_t named, int32_t code) {
    mc_game_settings_state state;
    mc_game_settings_text name;
    int32_t previous;
    return mc_game_settings_read_state(settings, &state) &&
           mc_key_binding_read(state.named[named], &name, &previous) &&
           mc_game_settings_binding_set(settings, state.named[named], name, code, NULL) ==
               MC_GAME_SETTINGS_OK;
}

int main(void) {
    int exit_code = 1;
    mc_movement_input *base = NULL, *input = NULL, *other = NULL;
    mc_game_settings *settings = NULL;
    mc_movement_input_state state = {0}, previous = {0};
    mc_movement_input_error error = {0};
    mc_game_settings_state options;
    const mc_movement_input_fields custom = {.strafe = from_bits(UINT32_C(0x80000000)),
                                             .forward = from_bits(UINT32_C(0x7fc12345)),
                                             .source_c = true,
                                             .jump = true,
                                             .sneak = true};
    CHECK(mc_movement_input_create(NULL, &error) == MC_MOVEMENT_INPUT_INVALID_ARGUMENT);
    CHECK(mc_movement_input_create(&base, &error) == MC_MOVEMENT_INPUT_OK);
    CHECK(error.status == MC_MOVEMENT_INPUT_OK && error.named_index == -1);
    CHECK(mc_movement_input_read_state(base, &state));
    CHECK(state.kind == MC_MOVEMENT_INPUT_BASE && state.settings == NULL &&
          state.keys_identity == NULL && state.key_count == 0 && keys(&state) == 0);
    CHECK(bits(state.fields.strafe) == 0 && bits(state.fields.forward) == 0 &&
          !state.fields.source_c && !state.fields.jump && !state.fields.sneak);
    CHECK(mc_movement_input_set_fields(base, &custom, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_key(base, INT32_MIN, true, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_update(base, NULL, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_reset(base, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(base, &state) && fields_equal(state.fields, custom));
    CHECK(!mc_movement_input_read_state(NULL, &state));
    CHECK(fields_equal(state.fields, custom));
    CHECK(!mc_movement_input_read_state(base, NULL));
    CHECK(mc_movement_input_set_fields(base, NULL, &error) == MC_MOVEMENT_INPUT_INVALID_ARGUMENT);
    CHECK(mc_movement_input_key(NULL, 0, false, NULL) == MC_MOVEMENT_INPUT_INVALID_ARGUMENT);
    CHECK(mc_movement_input_update(NULL, NULL, NULL) == MC_MOVEMENT_INPUT_INVALID_ARGUMENT);
    CHECK(mc_movement_input_reset(NULL, NULL) == MC_MOVEMENT_INPUT_INVALID_ARGUMENT);
    CHECK(mc_movement_input_set_fields(NULL, &custom, NULL) == MC_MOVEMENT_INPUT_INVALID_ARGUMENT);
    CHECK(mc_movement_input_retire(NULL, NULL) == MC_MOVEMENT_INPUT_INVALID_ARGUMENT);

    CHECK(mc_movement_input_create_from_options(NULL, NULL, NULL) ==
          MC_MOVEMENT_INPUT_INVALID_ARGUMENT);
    CHECK(mc_movement_input_create_from_options(NULL, &input, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_set_fields(input, &custom, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &previous));
    CHECK(previous.settings == NULL && previous.keys_identity != NULL &&
          previous.key_count == MC_MOVEMENT_INPUT_KEYS);
    CHECK(mc_movement_input_key(input, 17, true, &error) == MC_MOVEMENT_INPUT_SOURCE_EXCEPTION);
    CHECK(error.exception == MC_MOVEMENT_INPUT_NULL_SETTINGS && error.bytecode == 7 &&
          error.named_index == 0);
    CHECK(mc_movement_input_read_state(input, &state) && fields_equal(state.fields, custom) &&
          keys(&state) == 0 && state.keys_identity == previous.keys_identity);
    CHECK(mc_movement_input_reset(input, &error) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && fields_equal(state.fields, custom));
    CHECK(mc_movement_input_update(input, NULL, &error) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && bits(state.fields.strafe) == 0 &&
          bits(state.fields.forward) == 0 && state.fields.source_c && !state.fields.jump &&
          !state.fields.sneak);
    CHECK(mc_movement_input_retire(&input, NULL) == MC_MOVEMENT_INPUT_OK && input == NULL);

    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_read_state(settings, &options));
    CHECK(mc_movement_input_create_from_options(settings, &input, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_create_from_options(settings, &other, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && state.settings == settings);
    CHECK(mc_movement_input_read_state(other, &previous) &&
          previous.keys_identity != state.keys_identity);
    static const int32_t codes[] = {17, 31, 30, 32, 57, 42};
    for (unsigned int mask = 0; mask < 64; ++mask) {
        CHECK(mc_movement_input_reset(input, NULL) == MC_MOVEMENT_INPUT_OK);
        CHECK(mc_movement_input_set_fields(input, &custom, NULL) == MC_MOVEMENT_INPUT_OK);
        for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
            CHECK(mc_movement_input_key(input, codes[i], (mask & (1u << i)) != 0, NULL) ==
                  MC_MOVEMENT_INPUT_OK);
        CHECK(mc_movement_input_read_state(input, &state) && keys(&state) == mask &&
              fields_equal(state.fields, custom));
        CHECK(mc_movement_input_update(input, NULL, NULL) == MC_MOVEMENT_INPUT_OK);
        CHECK(mc_movement_input_read_state(input, &state));
        const int forward = ((mask & 1u) != 0 ? 1 : 0) - ((mask & 2u) != 0 ? 1 : 0);
        const int strafe = ((mask & 4u) != 0 ? 1 : 0) - ((mask & 8u) != 0 ? 1 : 0);
        const uint32_t unit = (mask & 32u) != 0 ? UINT32_C(0x3e99999a) : UINT32_C(0x3f800000);
        CHECK(bits(state.fields.forward) ==
              (forward == 0 ? 0 : unit | (forward < 0 ? UINT32_C(0x80000000) : 0)));
        CHECK(bits(state.fields.strafe) ==
              (strafe == 0 ? 0 : unit | (strafe < 0 ? UINT32_C(0x80000000) : 0)));
        CHECK(state.fields.jump == ((mask & 16u) != 0) &&
              state.fields.sneak == ((mask & 32u) != 0) && state.fields.source_c);
        previous = state;
        CHECK(mc_movement_input_reset(input, NULL) == MC_MOVEMENT_INPUT_OK);
        CHECK(mc_movement_input_read_state(input, &state) && keys(&state) == 0 &&
              fields_equal(state.fields, previous.fields));
        CHECK(mc_movement_input_read_state(other, &state) && keys(&state) == 0 &&
              bits(state.fields.strafe) == 0 && bits(state.fields.forward) == 0);
    }

    /* Named fields are live and distinct from w; event rebinds never clear an
     * earlier held cell unless that cell is actually selected by this event. */
    CHECK(mc_game_settings_set_binding_array(settings, NULL, 0, true, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_movement_input_key(input, 17, true, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(code_set(settings, 0, INT32_MIN));
    CHECK(mc_movement_input_key(input, 17, false, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && keys(&state) == 1);
    CHECK(mc_movement_input_key(input, INT32_MIN, false, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && keys(&state) == 0);
    CHECK(code_set(settings, 0, INT32_MAX));
    CHECK(mc_movement_input_key(input, INT32_MAX, true, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && keys(&state) == 1);
    CHECK(mc_movement_input_reset(input, NULL) == MC_MOVEMENT_INPUT_OK);

    static const size_t named[] = {0, 2, 1, 3, 4, 9};
    for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); ++i)
        CHECK(code_set(settings, named[i], -7));
    CHECK(mc_movement_input_key(input, -7, true, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && keys(&state) == 32);
    CHECK(mc_movement_input_update(input, NULL, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && state.fields.sneak &&
          bits(state.fields.forward) == 0 && bits(state.fields.strafe) == 0);
    CHECK(mc_movement_input_key(input, -7, false, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && keys(&state) == 0);

    /* Failure at every named field occurs after earlier comparisons but before
     * the final key store. No partial match is committed. */
    for (size_t i = 0; i < sizeof(named) / sizeof(named[0]); ++i) {
        CHECK(mc_game_settings_set_named_binding(settings, named[i], NULL, NULL) ==
              MC_GAME_SETTINGS_OK);
        CHECK(mc_movement_input_read_state(input, &previous));
        CHECK(mc_movement_input_key(input, -7, true, &error) == MC_MOVEMENT_INPUT_SOURCE_EXCEPTION);
        CHECK(error.exception == MC_MOVEMENT_INPUT_NULL_BINDING &&
              error.bytecode == 10u + (unsigned int)i * 16u &&
              error.named_index == (int32_t)named[i]);
        CHECK(mc_movement_input_read_state(input, &state) && keys(&state) == keys(&previous) &&
              fields_equal(state.fields, previous.fields));
        CHECK(mc_movement_input_update(input, NULL, NULL) == MC_MOVEMENT_INPUT_OK);
        CHECK(mc_movement_input_reset(input, NULL) == MC_MOVEMENT_INPUT_OK);
        CHECK(mc_game_settings_set_named_binding(settings, named[i], options.named[named[i]],
                                                 NULL) == MC_GAME_SETTINGS_OK);
    }

    CHECK(code_set(settings, 0, 91));
    CHECK(mc_game_settings_set_named_binding(settings, 1, options.named[0], NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_movement_input_key(input, 91, true, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && keys(&state) == 4);
    mc_key_binding *replacement = NULL;
    const mc_game_settings_text empty = {.units = NULL, .length = 0, .is_null = false};
    CHECK(mc_game_settings_binding_create(settings, empty, 92, &replacement, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_named_binding(settings, 1, replacement, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_movement_input_key(input, 92, false, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && keys(&state) == 0);
    CHECK(mc_movement_input_key(input, 91, true, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_movement_input_read_state(input, &state) && keys(&state) == 1);

    CHECK(mc_movement_input_retire(&other, NULL) == MC_MOVEMENT_INPUT_OK && other == NULL);
    CHECK(mc_movement_input_retire(&input, NULL) == MC_MOVEMENT_INPUT_OK && input == NULL);
    CHECK(mc_movement_input_retire(&input, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_game_settings_read_state(settings, &options) && options.bindings_null &&
          options.named[1] == replacement);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK && settings == NULL);
    CHECK(mc_movement_input_retire(&base, NULL) == MC_MOVEMENT_INPUT_OK && base == NULL);
    printf("MovementInput: %u checks passed\n", checks);
    exit_code = 0;
cleanup:
    (void)mc_movement_input_retire(&other, NULL);
    (void)mc_movement_input_retire(&input, NULL);
    (void)mc_movement_input_retire(&base, NULL);
    (void)mc_game_settings_retire(&settings, NULL);
    return exit_code;
}
