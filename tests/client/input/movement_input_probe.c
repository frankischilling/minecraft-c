#include "mc/client/input/movement_input.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define SCAN_NUMBERS sscanf_s
#else
#define SCAN_NUMBERS sscanf
#endif

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

int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    FILE *plan = NULL;
#ifdef _WIN32
    if (fopen_s(&plan, argv[1], "rb") != 0)
        return 2;
#else
    plan = fopen(argv[1], "rb");
    if (plan == NULL)
        return 2;
#endif
    int exit_code = 2;
    mc_game_settings *settings = NULL;
    mc_movement_input *input = NULL;
    const void *array_identity = NULL;
    size_t record = 0;
    char line[256], operation[24], kind[24];
    while (fgets(line, (int)sizeof(line), plan) != NULL) {
        const size_t operation_length = strcspn(line, " \r\n");
        if (strchr(line, '\n') == NULL || operation_length == 0 ||
            operation_length >= sizeof(operation))
            goto cleanup;
        memcpy(operation, line, operation_length);
        operation[operation_length] = '\0';
        const char *arguments = line + operation_length;
        if (*arguments == ' ')
            ++arguments;
        mc_movement_input_status status = MC_MOVEMENT_INPUT_OK;
        mc_game_settings_state options;
        int32_t index, source, code;
        int pressed, c, jump, sneak;
        if (strcmp(operation, "create") == 0) {
            const size_t kind_length = strcspn(arguments, "\r\n");
            if (kind_length == 0 || kind_length >= sizeof(kind))
                goto cleanup;
            memcpy(kind, arguments, kind_length);
            kind[kind_length] = '\0';
            if (mc_movement_input_retire(&input, NULL) != MC_MOVEMENT_INPUT_OK ||
                mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
                goto cleanup;
            if (strcmp(kind, "options") == 0) {
                if (mc_game_settings_create_defaults(NULL, &settings, NULL) != MC_GAME_SETTINGS_OK)
                    goto cleanup;
            } else if (strcmp(kind, "base") != 0 && strcmp(kind, "null") != 0)
                goto cleanup;
            status = strcmp(kind, "base") == 0
                         ? mc_movement_input_create(&input, NULL)
                         : mc_movement_input_create_from_options(settings, &input, NULL);
            mc_movement_input_state initial;
            if (status != MC_MOVEMENT_INPUT_OK || !mc_movement_input_read_state(input, &initial))
                goto cleanup;
            array_identity = initial.keys_identity;
        } else if (strcmp(operation, "fields") == 0) {
            uint32_t strafe, forward;
            if (SCAN_NUMBERS(arguments, "%" SCNx32 " %" SCNx32 " %d %d %d", &strafe, &forward, &c,
                             &jump, &sneak) != 5)
                goto cleanup;
            const mc_movement_input_fields fields = {.strafe = from_bits(strafe),
                                                     .forward = from_bits(forward),
                                                     .source_c = c != 0,
                                                     .jump = jump != 0,
                                                     .sneak = sneak != 0};
            status = mc_movement_input_set_fields(input, &fields, NULL);
        } else if (strcmp(operation, "event") == 0) {
            if (SCAN_NUMBERS(arguments, "%" SCNd32 " %d", &code, &pressed) != 2)
                goto cleanup;
            status = mc_movement_input_key(input, code, pressed != 0, NULL);
        } else if (strcmp(operation, "update") == 0)
            status = mc_movement_input_update(input, NULL, NULL);
        else if (strcmp(operation, "reset") == 0)
            status = mc_movement_input_reset(input, NULL);
        else if (strcmp(operation, "array_null") == 0) {
            if (mc_game_settings_set_binding_array(settings, NULL, 0, true, NULL) !=
                MC_GAME_SETTINGS_OK)
                goto cleanup;
        } else if (strcmp(operation, "code") == 0) {
            if (SCAN_NUMBERS(arguments, "%" SCNd32 " %" SCNd32, &index, &code) != 2 || index < 0 ||
                index >= (int32_t)MC_GAME_SETTINGS_NAMED_BINDINGS ||
                !mc_game_settings_read_state(settings, &options))
                goto cleanup;
            mc_game_settings_text name;
            int32_t previous;
            if (!mc_key_binding_read(options.named[index], &name, &previous) ||
                mc_game_settings_binding_set(settings, options.named[index], name, code, NULL) !=
                    MC_GAME_SETTINGS_OK)
                goto cleanup;
        } else if (strcmp(operation, "alias") == 0 || strcmp(operation, "null") == 0 ||
                   strcmp(operation, "replace") == 0) {
            if (SCAN_NUMBERS(arguments, "%" SCNd32, &index) != 1 || index < 0 ||
                index >= (int32_t)MC_GAME_SETTINGS_NAMED_BINDINGS ||
                !mc_game_settings_read_state(settings, &options))
                goto cleanup;
            mc_key_binding *binding = NULL;
            if (strcmp(operation, "alias") == 0) {
                if (SCAN_NUMBERS(arguments, "%" SCNd32 " %" SCNd32, &index, &source) != 2 ||
                    source < 0 || source >= (int32_t)MC_GAME_SETTINGS_NAMED_BINDINGS)
                    goto cleanup;
                binding = options.named[source];
            } else if (strcmp(operation, "replace") == 0) {
                const mc_game_settings_text name = {.is_null = false};
                if (SCAN_NUMBERS(arguments, "%" SCNd32 " %" SCNd32, &index, &code) != 2 ||
                    mc_game_settings_binding_create(settings, name, code, &binding, NULL) !=
                        MC_GAME_SETTINGS_OK)
                    goto cleanup;
            }
            if (mc_game_settings_set_named_binding(settings, (size_t)index, binding, NULL) !=
                MC_GAME_SETTINGS_OK)
                goto cleanup;
        } else
            goto cleanup;
        if (status != MC_MOVEMENT_INPUT_OK && status != MC_MOVEMENT_INPUT_SOURCE_EXCEPTION)
            goto cleanup;
        mc_movement_input_state state;
        if (!mc_movement_input_read_state(input, &state) || state.settings != settings ||
            state.keys_identity != array_identity)
            goto cleanup;
        unsigned int mask = 0;
        for (size_t i = 0; i < MC_MOVEMENT_INPUT_KEYS; ++i)
            if (state.keys[i])
                mask |= 1u << i;
        if (printf("%zu %s %s %08" PRIx32 " %08" PRIx32 " %d %d %d %d %zu %u\n", record++,
                   status == MC_MOVEMENT_INPUT_OK ? "OK" : "NPE",
                   state.kind == MC_MOVEMENT_INPUT_BASE ? "base" : "options",
                   bits(state.fields.strafe), bits(state.fields.forward), state.fields.source_c,
                   state.fields.jump, state.fields.sneak, state.settings != NULL, state.key_count,
                   mask) < 0)
            goto cleanup;
    }
    if (!ferror(plan) && !ferror(stdout) && record != 0)
        exit_code = 0;
cleanup:
    if (fclose(plan) != 0)
        exit_code = 2;
    (void)mc_movement_input_retire(&input, NULL);
    (void)mc_game_settings_retire(&settings, NULL);
    return exit_code;
}
