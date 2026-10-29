/* Included once at the end of canonical game_settings.c. Uses its actual
 * private storage, admit/result and active guard, never a second layout. */
#include "source_input.h"

#include <xmmintrin.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Settings source input arithmetic requires the qualified x86-64 profile"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Settings source input arithmetic requires strict floating transformations"
#endif

typedef enum source_input_toggle_field {
    SOURCE_INPUT_HIDE_GUI,
    SOURCE_INPUT_DEBUG,
    SOURCE_INPUT_THIRD_PERSON,
    SOURCE_INPUT_SMOOTH_CAMERA
} source_input_toggle_field;

static mc_game_settings_status source_input_toggle(mc_game_settings *settings,
                                                   source_input_toggle_field field,
                                                   mc_game_settings_error *error) {
    const mc_game_settings_status status = admit(settings, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    settings->active = true;
    bool *slot = field == SOURCE_INPUT_HIDE_GUI       ? &settings->state.values.hide_gui
                 : field == SOURCE_INPUT_DEBUG        ? &settings->state.values.show_debug_info
                 : field == SOURCE_INPUT_THIRD_PERSON ? &settings->state.values.third_person_view
                                                      : &settings->state.values.smooth_camera;
    *slot = !*slot;
    settings->active = false;
    return result(error, MC_GAME_SETTINGS_OK, 0, NULL);
}

mc_game_settings_status
mc_game_settings_source_input_toggle_hide_gui(mc_game_settings *settings,
                                              mc_game_settings_error *error) {
    return source_input_toggle(settings, SOURCE_INPUT_HIDE_GUI, error);
}

mc_game_settings_status mc_game_settings_source_input_toggle_debug(mc_game_settings *settings,
                                                                   mc_game_settings_error *error) {
    return source_input_toggle(settings, SOURCE_INPUT_DEBUG, error);
}

mc_game_settings_status
mc_game_settings_source_input_toggle_third_person(mc_game_settings *settings,
                                                  mc_game_settings_error *error) {
    return source_input_toggle(settings, SOURCE_INPUT_THIRD_PERSON, error);
}

mc_game_settings_status
mc_game_settings_source_input_toggle_smooth_camera(mc_game_settings *settings,
                                                   mc_game_settings_error *error) {
    return source_input_toggle(settings, SOURCE_INPUT_SMOOTH_CAMERA, error);
}

mc_game_settings_status
mc_game_settings_source_input_wheel(mc_game_settings *settings, int32_t wheel,
                                    mc_game_settings_source_input_wheel_report *out,
                                    mc_game_settings_error *error) {
    const mc_game_settings_status status = admit(settings, error);
    if (status != MC_GAME_SETTINGS_OK)
        return status;
    settings->active = true;
    mc_game_settings_source_input_wheel_report report = {0};
    report.source_pc = 368;
    mc_game_settings_status returned = MC_GAME_SETTINGS_OK;
    if (wheel != 0) {
        report.D_read = true;
        report.D = settings->state.values.source_D; /* k386, after actual Inventory call379. */
        report.source_pc = 386;
        if (report.D) {
            report.normalized_wheel = wheel > 0 ? 1 : -1;
            report.G_read = true;
            memcpy(&report.before_G, &settings->state.values.source_G, sizeof(report.before_G));
            memcpy(&report.after_G, &report.before_G, sizeof(report.after_G));
            report.source_pc = 413;
            if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
                returned = result(error, MC_GAME_SETTINGS_STATE_ERROR, 0,
                                  "GameSettings source input native floating environment");
            } else {
                const __m128 converted = _mm_cvtsi32_ss(_mm_setzero_ps(), report.normalized_wheel);
                const __m128 product = _mm_mul_ss(converted, _mm_set_ss(0.25f));
                const float value = _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(report.before_G), product));
                memcpy(&settings->state.values.source_G, &value, sizeof(value)); /* k418 */
                memcpy(&report.after_G, &value, sizeof(value));
                report.G_stored = true;
                report.source_pc = 418;
            }
        }
    }
    if (out != NULL)
        *out = report;
    settings->active = false;
    if (returned != MC_GAME_SETTINGS_OK)
        return returned;
    return result(error, MC_GAME_SETTINGS_OK, 0, NULL);
}
