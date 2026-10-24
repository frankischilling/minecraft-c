#include "labels.h"

#include "mc/core/java/java.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128,
               "GameSettings labels require IEEE binary32");

static mc_game_settings_label_error clean_error(void) {
    return (mc_game_settings_label_error){.settings_pc = -1, .dependency_pc = -1};
}

static mc_game_settings_label_status finish(mc_game_settings_label_error *detail,
                                            mc_game_settings_label_error *error,
                                            mc_game_settings_label_status status) {
    detail->status = status;
    if (error != NULL)
        *error = status == MC_GAME_SETTINGS_LABEL_OK ? clean_error() : *detail;
    return status;
}

static mc_game_settings_label_status source_failure(mc_game_settings_label_error *detail,
                                                    mc_game_settings_label_failure failure,
                                                    int32_t settings_pc, int32_t dependency_pc) {
    detail->source_failure = failure;
    detail->settings_pc = settings_pc;
    detail->dependency_pc = dependency_pc;
    return MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION;
}

static mc_game_settings_label_status assets_failure(mc_game_settings_label_error *detail,
                                                    int32_t settings_pc) {
    detail->settings_pc = settings_pc;
    return MC_GAME_SETTINGS_LABEL_ASSETS_FAILED;
}

static mc_game_settings_label_status copy_join(mc_font_text prefix, bool separator,
                                               mc_font_text suffix, bool is_null,
                                               mc_game_settings_label *out) {
    const size_t middle = separator ? 2u : 0u;
    if ((prefix.units == NULL && prefix.length != 0) ||
        (suffix.units == NULL && suffix.length != 0) ||
        (is_null && (prefix.length != 0 || suffix.length != 0 || separator)))
        return MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT;
    if (prefix.length > MC_GAME_SETTINGS_LABEL_MAX_UNITS ||
        suffix.length > MC_GAME_SETTINGS_LABEL_MAX_UNITS - prefix.length ||
        middle > MC_GAME_SETTINGS_LABEL_MAX_UNITS - prefix.length - suffix.length)
        return MC_GAME_SETTINGS_LABEL_LIMIT;
    const size_t length = prefix.length + middle + suffix.length;
    uint16_t *units = NULL;
    if (length != 0) {
        if (length > SIZE_MAX / sizeof(*units))
            return MC_GAME_SETTINGS_LABEL_LIMIT;
        units = malloc(length * sizeof(*units));
        if (units == NULL)
            return MC_GAME_SETTINGS_LABEL_ALLOCATION_FAILED;
        if (prefix.length != 0)
            memcpy(units, prefix.units, prefix.length * sizeof(*units));
        if (separator) {
            units[prefix.length] = ':';
            units[prefix.length + 1u] = ' ';
        }
        if (suffix.length != 0)
            memcpy(units + prefix.length + middle, suffix.units, suffix.length * sizeof(*units));
    }
    *out = (mc_game_settings_label){units, length, is_null};
    return MC_GAME_SETTINGS_LABEL_OK;
}

static mc_game_settings_label_status lookup_ascii(mc_client_assets *assets, const char *key,
                                                  bool wrapper, mc_font_text *out,
                                                  mc_game_settings_label_error *detail,
                                                  int32_t settings_pc) {
    uint16_t units[64];
    const size_t length = strlen(key); /* Only owned static option/key literals. */
    if (length > sizeof(units) / sizeof(units[0]))
        return MC_GAME_SETTINGS_LABEL_LIMIT;
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)key[i];
    const mc_font_text text = {units, length};
    const mc_client_assets_status status =
        wrapper ? mc_client_assets_label_text_wrapper(assets, text, false, out, &detail->assets)
                : mc_client_assets_label_text(assets, text, false, out, &detail->assets);
    return status == MC_CLIENT_ASSETS_OK ? MC_GAME_SETTINGS_LABEL_OK
                                         : assets_failure(detail, settings_pc);
}

static mc_game_settings_label_status selected_binding(const mc_game_settings *settings,
                                                      int32_t index, bool physical,
                                                      mc_game_settings_text *name, int32_t *code,
                                                      mc_game_settings_label_error *detail) {
    mc_game_settings_state state;
    if (!mc_game_settings_read_state(settings, &state))
        return MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT;
    const int32_t array_pc = physical ? 5 : 10;
    const int32_t binding_pc = physical ? 6 : 11;
    if (state.bindings_null)
        return source_failure(detail, MC_GAME_SETTINGS_LABEL_NULL_BINDING_ARRAY, array_pc, -1);
    if (index < 0 || (size_t)index >= state.binding_count)
        return source_failure(detail, MC_GAME_SETTINGS_LABEL_BINDING_INDEX, array_pc, -1);
    const mc_key_binding *binding = state.bindings[(size_t)index];
    if (binding == NULL)
        return source_failure(detail, MC_GAME_SETTINGS_LABEL_NULL_BINDING, binding_pc, -1);
    if (!mc_key_binding_read(binding, name, code))
        return MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT;
    return MC_GAME_SETTINGS_LABEL_OK;
}

mc_game_settings_label_status mc_game_settings_binding_label(const mc_game_settings *settings,
                                                             mc_client_assets *assets,
                                                             int32_t index,
                                                             mc_game_settings_label *out,
                                                             mc_game_settings_label_error *error) {
    mc_game_settings_label_error detail = clean_error();
    if (settings == NULL || assets == NULL || out == NULL)
        return finish(&detail, error, MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT);
    if (mc_client_assets_initialize_translation(assets, &detail.assets) != MC_CLIENT_ASSETS_OK)
        return finish(&detail, error, assets_failure(&detail, 0));
    mc_game_settings_text name = {0};
    int32_t code = 0;
    mc_game_settings_label_status status =
        selected_binding(settings, index, false, &name, &code, &detail);
    if (status != MC_GAME_SETTINGS_LABEL_OK)
        return finish(&detail, error, status);
    mc_font_text value = {0};
    const mc_client_assets_status translated = mc_client_assets_label_text(
        assets, (mc_font_text){name.units, name.length}, name.is_null, &value, &detail.assets);
    if (translated == MC_CLIENT_ASSETS_NULL_REFERENCE)
        status = source_failure(&detail, MC_GAME_SETTINGS_LABEL_NULL_NAME, 14, 6);
    else if (translated != MC_CLIENT_ASSETS_OK)
        status = assets_failure(&detail, 14);
    else
        status = copy_join((mc_font_text){0}, false, value, false, out);
    return finish(&detail, error, status);
}

mc_game_settings_label_status mc_game_settings_key_label(const mc_game_settings *settings,
                                                         int32_t index, mc_game_settings_label *out,
                                                         mc_game_settings_label_error *error) {
    mc_game_settings_label_error detail = clean_error();
    if (settings == NULL || out == NULL)
        return finish(&detail, error, MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT);
    mc_game_settings_text name = {0};
    int32_t code = 0;
    mc_game_settings_label_status status =
        selected_binding(settings, index, true, &name, &code, &detail);
    if (status != MC_GAME_SETTINGS_LABEL_OK)
        return finish(&detail, error, status);
    const char *key = NULL;
    if (mc_game_key_name(code, &key) != MC_GAME_OPTION_OK)
        return finish(&detail, error,
                      source_failure(&detail, MC_GAME_SETTINGS_LABEL_KEY_INDEX, 9, 4));
    if (key == NULL)
        return finish(&detail, error,
                      copy_join((mc_font_text){0}, false, (mc_font_text){0}, true, out));
    uint16_t units[32];
    const size_t length = strlen(key);
    if (length > sizeof(units) / sizeof(units[0]))
        return finish(&detail, error, MC_GAME_SETTINGS_LABEL_LIMIT);
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)key[i];
    return finish(&detail, error,
                  copy_join((mc_font_text){0}, false, (mc_font_text){units, length}, false, out));
}

static mc_font_text percentage(float value, float scale, uint16_t units[13]) {
    /* Source fmul rounds to binary32 before f2i. No native FP mode change. */
    volatile float product = value * scale;
    const int32_t integer = mc_java_float_to_i32(product);
    uint32_t magnitude = integer < 0 ? 0u - (uint32_t)integer : (uint32_t)integer;
    uint16_t reverse[10];
    size_t digits = 0, length = 0;
    do {
        reverse[digits++] = (uint16_t)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude != 0);
    if (integer < 0)
        units[length++] = '-';
    while (digits != 0)
        units[length++] = reverse[--digits];
    units[length++] = '%';
    return (mc_font_text){units, length};
}

mc_game_settings_label_status mc_game_settings_option_label(const mc_game_settings *settings,
                                                            mc_client_assets *assets,
                                                            const mc_game_option *option,
                                                            mc_game_settings_label *out,
                                                            mc_game_settings_label_error *error) {
    mc_game_settings_label_error detail = clean_error();
    mc_game_option_info info = {0};
    if (settings == NULL || assets == NULL || out == NULL ||
        (option != NULL && !mc_game_option_read(option, &info)))
        return finish(&detail, error, MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT);
    if (mc_client_assets_initialize_translation(assets, &detail.assets) != MC_CLIENT_ASSETS_OK)
        return finish(&detail, error, assets_failure(&detail, 0));
    if (option == NULL)
        return finish(&detail, error,
                      source_failure(&detail, MC_GAME_SETTINGS_LABEL_NULL_OPTION, 13, -1));
    mc_font_text prefix = {0}, suffix = {0};
    mc_game_settings_label_status status =
        lookup_ascii(assets, info.translation_key, false, &prefix, &detail, 16);
    if (status != MC_GAME_SETTINGS_LABEL_OK)
        return finish(&detail, error, status);
    uint16_t numeric[13];
    if (info.is_float) {
        float value = 0.0f;
        if (mc_game_settings_option_float(settings, option, &value) != MC_GAME_OPTION_OK)
            return finish(&detail, error, MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT);
        if (info.id == MC_GAME_OPTION_SENSITIVITY && value == 0.0f)
            status = lookup_ascii(assets, "options.sensitivity.min", false, &suffix, &detail, 73);
        else if (info.id == MC_GAME_OPTION_SENSITIVITY && value == 1.0f)
            status = lookup_ascii(assets, "options.sensitivity.max", false, &suffix, &detail, 104);
        else if (info.id != MC_GAME_OPTION_SENSITIVITY && value == 0.0f)
            status = lookup_ascii(assets, "options.off", false, &suffix, &detail, 164);
        else
            suffix =
                percentage(value, info.id == MC_GAME_OPTION_SENSITIVITY ? 200.0f : 100.0f, numeric);
    } else if (info.is_boolean) {
        bool value = false;
        if (mc_game_settings_option_boolean(settings, option, &value) != MC_GAME_OPTION_OK)
            return finish(&detail, error, MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT);
        status = lookup_ascii(assets, value ? "options.on" : "options.off", false, &suffix, &detail,
                              value ? 236 : 260);
    } else {
        mc_game_settings_state state;
        if (!mc_game_settings_read_state(settings, &state))
            return finish(&detail, error, MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT);
        if (info.id == MC_GAME_OPTION_GRAPHICS)
            status = lookup_ascii(assets,
                                  state.values.fancy_graphics ? "options.graphics.fancy"
                                                              : "options.graphics.fast",
                                  false, &suffix, &detail, state.values.fancy_graphics ? 445 : 469);
        else {
            int32_t index = 0, source_pc = 0, lookup_pc = 0;
            switch (info.id) {
                case MC_GAME_OPTION_RENDER_DISTANCE:
                    index = state.values.render_distance;
                    source_pc = 296;
                    lookup_pc = 297;
                    break;
                case MC_GAME_OPTION_DIFFICULTY:
                    index = state.values.difficulty;
                    source_pc = 333;
                    lookup_pc = 334;
                    break;
                case MC_GAME_OPTION_GUI_SCALE:
                    index = state.values.gui_scale;
                    source_pc = 370;
                    lookup_pc = 371;
                    break;
                case MC_GAME_OPTION_FRAMERATE_LIMIT:
                    index = state.values.performance_mode;
                    source_pc = 406;
                    lookup_pc = 407;
                    break;
                default:
                    return finish(&detail, error, MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT);
            }
            const char *key = NULL;
            if (mc_game_option_value_key(option, index, &key) != MC_GAME_OPTION_OK)
                return finish(
                    &detail, error,
                    source_failure(&detail, MC_GAME_SETTINGS_LABEL_VALUE_INDEX, source_pc, -1));
            status = lookup_ascii(assets, key, info.id == MC_GAME_OPTION_FRAMERATE_LIMIT, &suffix,
                                  &detail, lookup_pc);
        }
    }
    if (status == MC_GAME_SETTINGS_LABEL_OK)
        status = copy_join(prefix, true, suffix, false, out);
    return finish(&detail, error, status);
}

void mc_game_settings_label_free(mc_game_settings_label *label) {
    if (label != NULL) {
        free(label->units);
        *label = (mc_game_settings_label){0};
    }
}
