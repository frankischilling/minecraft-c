#include "mc/client/settings/game_option.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static unsigned int checks;
static const char *root;
static const mc_game_settings_profile profile = {MC_JAVA_READER_UTF8, MC_JAVA_WRITER_LF};

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static float from_bits(uint32_t raw) {
    float result;
    memcpy(&result, &raw, sizeof(result));
    return result;
}

static uint32_t bits(float value) {
    uint32_t result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static bool path_for(char *out, size_t capacity, const char *name) {
    const int count = snprintf(out, capacity, "%s/%s/options.txt", root, name);
    return count >= 0 && (size_t)count < capacity;
}

static FILE *open_file(const char *path, bool writing) {
    FILE *file = NULL;
#ifdef _WIN32
    wchar_t native[4096];
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, native,
                            (int)(sizeof(native) / sizeof(native[0]))) == 0)
        return NULL;
    if (_wfopen_s(&file, native, writing ? L"wb" : L"rb") != 0)
        return NULL;
#else
    file = fopen(path, writing ? "wb" : "rb");
#endif
    return file;
}

static bool write_input(const char *name, const char *text) {
    char path[4096];
    if (!path_for(path, sizeof(path), name))
        return false;
    FILE *file = open_file(path, true);
    if (file == NULL)
        return false;
    const size_t count = strlen(text);
    const bool written = fwrite(text, 1, count, file) == count && !ferror(file);
    const bool closed = fclose(file) == 0;
    return written && closed;
}

static bool read_output(const char *name, uint8_t *out, size_t capacity, size_t *count) {
    char path[4096];
    if (!path_for(path, sizeof(path), name))
        return false;
    FILE *file = open_file(path, false);
    if (file == NULL)
        return false;
    const size_t actual = fread(out, 1, capacity, file);
    const bool complete = !ferror(file) && fgetc(file) == EOF && !ferror(file);
    const bool closed = fclose(file) == 0;
    if (!complete || !closed)
        return false;
    *count = actual;
    return true;
}

static mc_game_settings_status create_file(const char *name, void *minecraft,
                                           mc_game_settings **out) {
    char path[4096];
    const int count = snprintf(path, sizeof(path), "%s/%s", root, name);
    if (count < 0 || (size_t)count >= sizeof(path))
        return MC_GAME_SETTINGS_INVALID_ARGUMENT;
    return mc_game_settings_create_file(minecraft, path, &profile, NULL, out, NULL, NULL);
}

static bool same_text(mc_game_settings_text a, mc_game_settings_text b) {
    return a.units == b.units && a.length == b.length && a.is_null == b.is_null;
}

/* Every scalar/String role is checked, including the unsaved flags. Expected
 * arithmetic cases below are literals, not calls to the production helpers. */
static bool same_values(const mc_game_settings_values *a, const mc_game_settings_values *b) {
    return bits(a->music_volume) == bits(b->music_volume) &&
           bits(a->sound_volume) == bits(b->sound_volume) &&
           bits(a->mouse_sensitivity) == bits(b->mouse_sensitivity) &&
           a->invert_mouse == b->invert_mouse && a->render_distance == b->render_distance &&
           a->view_bobbing == b->view_bobbing && a->anaglyph == b->anaglyph &&
           a->advanced_opengl == b->advanced_opengl && a->performance_mode == b->performance_mode &&
           a->fancy_graphics == b->fancy_graphics && a->ambient_occlusion == b->ambient_occlusion &&
           same_text(a->texture_pack, b->texture_pack) && a->difficulty == b->difficulty &&
           a->hide_gui == b->hide_gui && a->third_person_view == b->third_person_view &&
           a->show_debug_info == b->show_debug_info && same_text(a->last_server, b->last_server) &&
           a->source_D == b->source_D && a->smooth_camera == b->smooth_camera &&
           a->debug_camera == b->debug_camera && bits(a->source_G) == bits(b->source_G) &&
           bits(a->source_H) == bits(b->source_H) && a->gui_scale == b->gui_scale;
}

static bool same_owners(const mc_game_settings_state *before, const mc_game_settings_state *after) {
    if (before->minecraft != after->minecraft || before->options_path != after->options_path ||
        before->file_identity != after->file_identity || before->bindings != after->bindings ||
        before->binding_count != after->binding_count ||
        before->bindings_null != after->bindings_null)
        return false;
    for (size_t i = 0; i < MC_GAME_SETTINGS_NAMED_BINDINGS; ++i)
        if (before->named[i] != after->named[i])
            return false;
    for (size_t i = 0; i < before->binding_count && !before->bindings_null; ++i) {
        if (before->bindings[i] != after->bindings[i])
            return false;
    }
    return true;
}

static bool default_bindings_unchanged(const mc_game_settings_state *state) {
    static const int32_t codes[] = {17, 30, 31, 32, 57, 18, 16, 20, 33, 42};
    static const char *const names[] = {"key.forward", "key.left",      "key.back", "key.right",
                                        "key.jump",    "key.inventory", "key.drop", "key.chat",
                                        "key.fog",     "key.sneak"};
    static const size_t order[] = {0, 1, 2, 3, 4, 9, 6, 5, 7, 8};
    if (state->bindings_null || state->binding_count != 10)
        return false;
    for (size_t i = 0; i < MC_GAME_SETTINGS_NAMED_BINDINGS; ++i) {
        mc_game_settings_text name = {0};
        int32_t code = 0;
        if (!mc_key_binding_read(state->named[i], &name, &code) || code != codes[i] ||
            name.is_null || name.length != strlen(names[i]))
            return false;
        for (size_t j = 0; j < name.length; ++j)
            if (name.units[j] != (uint8_t)names[i][j])
                return false;
        if (state->bindings[i] != state->named[order[i]])
            return false;
    }
    return true;
}

static bool seed_values(mc_game_settings *settings, mc_game_settings_state *out) {
    if (!mc_game_settings_read_state(settings, out))
        return false;
    static const uint16_t shared[] = {'A', 0, 'B', 0xd800};
    mc_game_settings_values values = out->values;
    values.music_volume = 0.75f;
    values.sound_volume = 0.125f;
    values.mouse_sensitivity = 0.375f;
    values.invert_mouse = true;
    values.render_distance = 7;
    values.view_bobbing = false;
    values.anaglyph = false;
    values.advanced_opengl = true;
    values.performance_mode = -5;
    values.fancy_graphics = false;
    values.ambient_occlusion = false;
    values.texture_pack = (mc_game_settings_text){shared, 4, false};
    values.last_server = values.texture_pack;
    values.difficulty = -6;
    values.hide_gui = values.third_person_view = values.show_debug_info = true;
    values.source_D = values.smooth_camera = values.debug_camera = true;
    values.source_G = 1.25f;
    values.source_H = -2.0f;
    values.gui_scale = 6;
    return mc_game_settings_set_values(settings, &values, NULL) == MC_GAME_SETTINGS_OK &&
           mc_game_settings_read_state(settings, out) &&
           out->values.texture_pack.units == out->values.last_server.units &&
           out->values.texture_pack.units != shared &&
           memcmp(out->values.texture_pack.units, shared, sizeof(shared)) == 0;
}

static bool float_one(int32_t id, uint32_t raw) {
    bool okay = false;
    mc_game_settings *settings = NULL;
    mc_game_settings_state before = {0}, after = {0};
    CHECK(mc_game_settings_create_defaults(&profile, &settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(seed_values(settings, &before));
    mc_game_settings_values expected = before.values;
    const float value = from_bits(raw);
    const mc_game_option *option = mc_game_option_by_id(id);
    if (id == MC_GAME_OPTION_MUSIC)
        memcpy(&expected.music_volume, &value, sizeof(value));
    else if (id == MC_GAME_OPTION_SOUND)
        memcpy(&expected.sound_volume, &value, sizeof(value));
    else if (id == MC_GAME_OPTION_SENSITIVITY)
        memcpy(&expected.mouse_sensitivity, &value, sizeof(value));
    mc_game_settings_option_action_report report = {0};
    mc_game_settings_error error = {0};
    const mc_game_settings_status status =
        mc_game_settings_set_option_float(settings, option, value, NULL, &report, &error);
    if (id == MC_GAME_OPTION_MUSIC || id == MC_GAME_OPTION_SOUND) {
        CHECK(status == MC_GAME_SETTINGS_SOURCE_EXCEPTION &&
              report.source_pc == (id == MC_GAME_OPTION_MUSIC ? 16 : 38) && report.value_assigned &&
              report.effect_dependency == MC_GAME_SETTINGS_OPTION_EFFECT_SOUND_MANAGER &&
              report.uncaught_exception == MC_GAME_SETTINGS_OPTION_ACTION_NULL_MINECRAFT);
    } else {
        CHECK(status == MC_GAME_SETTINGS_OK && report.source_pc == 56 &&
              report.value_assigned == (id == MC_GAME_OPTION_SENSITIVITY) &&
              report.effect_dependency == MC_GAME_SETTINGS_OPTION_EFFECT_NONE &&
              report.uncaught_exception == MC_GAME_SETTINGS_OPTION_ACTION_NONE);
    }
    CHECK(!report.save_reached && !report.save.source_opened && report.save.println_attempts == 0);
    CHECK(error.status == status && mc_game_settings_read_state(settings, &after) &&
          same_values(&expected, &after.values) && same_owners(&before, &after));
    CHECK(default_bindings_unchanged(&after));
    CHECK(memcmp(before.values.texture_pack.units, (uint16_t[]){'A', 0, 'B', 0xd800},
                 4u * sizeof(uint16_t)) == 0);
    okay = true;
cleanup:
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

static bool float_domain(void) {
    for (int32_t id = 0; id < MC_GAME_OPTION_COUNT; ++id)
        if (!float_one(id, UINT32_C(0x7fc12345)))
            return false;
    if (!float_one(-1, UINT32_C(0x7f800000)))
        return false;
    static const uint32_t raw[] = {0,
                                   UINT32_C(0x80000000),
                                   UINT32_C(0x00000001),
                                   UINT32_C(0x80000001),
                                   UINT32_C(0x7f800000),
                                   UINT32_C(0xff800000),
                                   UINT32_C(0x7fc12345),
                                   UINT32_C(0xc1000000),
                                   UINT32_C(0x41000000)};
    const int32_t ids[] = {MC_GAME_OPTION_MUSIC, MC_GAME_OPTION_SOUND, MC_GAME_OPTION_SENSITIVITY};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
        for (size_t j = 0; j < sizeof(raw) / sizeof(raw[0]); ++j)
            if (!float_one(ids[i], raw[j]))
                return false;
    return true;
}

static bool discrete_domain(void) {
    bool okay = false;
    mc_game_settings *settings = NULL;
    for (int32_t id = -1; id < MC_GAME_OPTION_COUNT; ++id) {
        mc_game_settings_state before = {0}, after = {0};
        CHECK(mc_game_settings_create_defaults(&profile, &settings, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(seed_values(settings, &before));
        mc_game_settings_values expected = before.values;
        bool assigned = true;
        int32_t effect_pc = 0;
        mc_game_settings_option_effect effect = MC_GAME_SETTINGS_OPTION_EFFECT_NONE;
        switch (id) {
            case MC_GAME_OPTION_INVERT_MOUSE:
                expected.invert_mouse = false;
                break;
            case MC_GAME_OPTION_RENDER_DISTANCE:
                expected.render_distance = 0;
                break;
            case MC_GAME_OPTION_VIEW_BOBBING:
                expected.view_bobbing = true;
                break;
            case MC_GAME_OPTION_GUI_SCALE:
                expected.gui_scale = 3;
                break;
            case MC_GAME_OPTION_FRAMERATE_LIMIT:
                expected.performance_mode = 0;
                break;
            case MC_GAME_OPTION_DIFFICULTY:
                expected.difficulty = 3;
                break;
            case MC_GAME_OPTION_ADVANCED_OPENGL:
                expected.advanced_opengl = false;
                effect_pc = 111;
                effect = MC_GAME_SETTINGS_OPTION_EFFECT_RENDER_GLOBAL;
                break;
            case MC_GAME_OPTION_ANAGLYPH:
                expected.anaglyph = true;
                effect_pc = 144;
                effect = MC_GAME_SETTINGS_OPTION_EFFECT_RENDER_ENGINE;
                break;
            case MC_GAME_OPTION_GRAPHICS:
                expected.fancy_graphics = true;
                effect_pc = 217;
                effect = MC_GAME_SETTINGS_OPTION_EFFECT_RENDER_GLOBAL;
                break;
            case MC_GAME_OPTION_AMBIENT_OCCLUSION:
                expected.ambient_occlusion = true;
                effect_pc = 250;
                effect = MC_GAME_SETTINGS_OPTION_EFFECT_RENDER_GLOBAL;
                break;
            default:
                assigned = false;
                break;
        }
        mc_game_settings_option_action_report report = {0};
        const mc_game_settings_status status =
            mc_game_settings_set_option(settings, mc_game_option_by_id(id), 5, NULL, &report, NULL);
        CHECK(report.value_assigned == assigned && report.effect_dependency == effect);
        if (effect_pc != 0) {
            CHECK(status == MC_GAME_SETTINGS_SOURCE_EXCEPTION && report.source_pc == effect_pc &&
                  report.uncaught_exception == MC_GAME_SETTINGS_OPTION_ACTION_NULL_MINECRAFT &&
                  !report.save_reached && !report.save.source_opened);
        } else {
            CHECK(status == MC_GAME_SETTINGS_OK && report.source_pc == 260 && report.save_reached &&
                  report.uncaught_exception == MC_GAME_SETTINGS_OPTION_ACTION_NONE &&
                  report.save.outer_exception_caught &&
                  report.save.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_FILE &&
                  !report.save.source_opened && report.save.println_attempts == 0);
        }
        CHECK(mc_game_settings_read_state(settings, &after) &&
              same_values(&expected, &after.values) && same_owners(&before, &after));
        CHECK(default_bindings_unchanged(&after));
        CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    }
    okay = true;
cleanup:
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

static bool wrapping(void) {
    bool okay = false;
    mc_game_settings *settings = NULL;

    static const struct {
        int32_t old, step, expected;
    } masked[] = {{INT32_MAX, 1, 0}, {INT32_MIN, -1, 3}, {-9, -6, 1}, {4, INT32_MIN, 0}};

    static const struct {
        int32_t old, step, expected;
    } remainder[] = {{INT32_MAX, 1, -2}, {INT32_MAX, INT32_MAX, 1},
                     {INT32_MIN, -1, 0}, {-5, 0, -2},
                     {-4, 0, -1},        {-3, 0, 0},
                     {-2, 0, 1},         {0, -4, -1},
                     {INT32_MIN, 0, -2}, {1, 0, 1}};

    CHECK(mc_game_settings_create_defaults(&profile, &settings, NULL) == MC_GAME_SETTINGS_OK);
    for (size_t which = 0; which < 3; ++which)
        for (size_t i = 0; i < sizeof(masked) / sizeof(masked[0]); ++i) {
            mc_game_settings_state before = {0}, after = {0};
            CHECK(mc_game_settings_read_state(settings, &before));
            mc_game_settings_values expected = before.values;
            if (which == 0)
                expected.render_distance = masked[i].old;
            else if (which == 1)
                expected.gui_scale = masked[i].old;
            else
                expected.difficulty = masked[i].old;
            CHECK(mc_game_settings_set_values(settings, &expected, NULL) == MC_GAME_SETTINGS_OK);
            CHECK(mc_game_settings_read_state(settings, &before));
            expected = before.values;
            if (which == 0)
                expected.render_distance = masked[i].expected;
            else if (which == 1)
                expected.gui_scale = masked[i].expected;
            else
                expected.difficulty = masked[i].expected;
            const int32_t id = which == 0   ? MC_GAME_OPTION_RENDER_DISTANCE
                               : which == 1 ? MC_GAME_OPTION_GUI_SCALE
                                            : MC_GAME_OPTION_DIFFICULTY;
            mc_game_settings_option_action_report report = {0};
            CHECK(mc_game_settings_set_option(settings, mc_game_option_by_id(id), masked[i].step,
                                              NULL, &report, NULL) == MC_GAME_SETTINGS_OK);
            CHECK(report.value_assigned && report.save_reached && report.source_pc == 260);
            CHECK(mc_game_settings_read_state(settings, &after) &&
                  same_values(&expected, &after.values) && same_owners(&before, &after));
            CHECK(default_bindings_unchanged(&after));
        }
    for (size_t i = 0; i < sizeof(remainder) / sizeof(remainder[0]); ++i) {
        mc_game_settings_state before = {0}, after = {0};
        CHECK(mc_game_settings_read_state(settings, &before));
        mc_game_settings_values expected = before.values;
        expected.performance_mode = remainder[i].old;
        CHECK(mc_game_settings_set_values(settings, &expected, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(mc_game_settings_read_state(settings, &before));
        expected = before.values;
        expected.performance_mode = remainder[i].expected;
        CHECK(mc_game_settings_set_option(
                  settings, mc_game_option_by_id(MC_GAME_OPTION_FRAMERATE_LIMIT), remainder[i].step,
                  NULL, NULL, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(mc_game_settings_read_state(settings, &after) &&
              same_values(&expected, &after.values) && same_owners(&before, &after));
        CHECK(default_bindings_unchanged(&after));
    }
    okay = true;
cleanup:
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

static bool opaque_effects_and_no_save(void) {
    bool okay = false;
    int opaque_receiver = 7;
    mc_game_settings *settings = NULL;
    uint8_t before_bytes[4096], after_bytes[4096];
    size_t before_count = 0, after_count = 0;
    CHECK(create_file("no-save", &opaque_receiver, &settings) == MC_GAME_SETTINGS_OK);
    CHECK(read_output("no-save", before_bytes, sizeof(before_bytes), &before_count));
    for (int32_t id = -1; id < MC_GAME_OPTION_COUNT; ++id) {
        mc_game_settings_state before = {0}, after = {0};
        CHECK(seed_values(settings, &before));
        mc_game_settings_values expected = before.values;
        const float value = from_bits(UINT32_C(0x80000000));
        if (id == MC_GAME_OPTION_MUSIC)
            expected.music_volume = value;
        else if (id == MC_GAME_OPTION_SOUND)
            expected.sound_volume = value;
        else if (id == MC_GAME_OPTION_SENSITIVITY)
            expected.mouse_sensitivity = value;
        mc_game_settings_option_action_report report = {0};
        mc_game_settings_error cause = {0};
        const mc_game_settings_status status = mc_game_settings_set_option_float(
            settings, mc_game_option_by_id(id), value, NULL, &report, &cause);
        const bool audio = id == MC_GAME_OPTION_MUSIC || id == MC_GAME_OPTION_SOUND;
        CHECK(status == (audio ? MC_GAME_SETTINGS_UNIMPLEMENTED_DEPENDENCY : MC_GAME_SETTINGS_OK));
        CHECK(!report.save_reached &&
              report.uncaught_exception == MC_GAME_SETTINGS_OPTION_ACTION_NONE &&
              report.source_pc == (audio ? (id == MC_GAME_OPTION_MUSIC ? 16 : 38) : 56) &&
              report.value_assigned == (audio || id == MC_GAME_OPTION_SENSITIVITY) &&
              report.effect_dependency == (audio ? MC_GAME_SETTINGS_OPTION_EFFECT_SOUND_MANAGER
                                                 : MC_GAME_SETTINGS_OPTION_EFFECT_NONE) &&
              cause.status == status);
        CHECK(mc_game_settings_read_state(settings, &after) &&
              same_values(&expected, &after.values) && same_owners(&before, &after));
        CHECK(default_bindings_unchanged(&after));
    }
    const int32_t ids[] = {MC_GAME_OPTION_ADVANCED_OPENGL, MC_GAME_OPTION_ANAGLYPH,
                           MC_GAME_OPTION_GRAPHICS, MC_GAME_OPTION_AMBIENT_OCCLUSION};
    const int32_t pcs[] = {111, 144, 217, 250};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        mc_game_settings_state before = {0}, after = {0};
        CHECK(seed_values(settings, &before));
        mc_game_settings_values expected = before.values;
        if (i == 0)
            expected.advanced_opengl = false;
        else if (i == 1)
            expected.anaglyph = true;
        else if (i == 2)
            expected.fancy_graphics = true;
        else
            expected.ambient_occlusion = true;
        mc_game_settings_option_action_report report = {0};
        mc_game_settings_error error = {0};
        CHECK(mc_game_settings_set_option(settings, mc_game_option_by_id(ids[i]), INT32_MIN, NULL,
                                          &report,
                                          &error) == MC_GAME_SETTINGS_UNIMPLEMENTED_DEPENDENCY);
        CHECK(report.source_pc == pcs[i] && report.value_assigned && !report.save_reached &&
              report.uncaught_exception == MC_GAME_SETTINGS_OPTION_ACTION_NONE &&
              report.effect_dependency == (i == 1 ? MC_GAME_SETTINGS_OPTION_EFFECT_RENDER_ENGINE
                                                  : MC_GAME_SETTINGS_OPTION_EFFECT_RENDER_GLOBAL) &&
              error.status == MC_GAME_SETTINGS_UNIMPLEMENTED_DEPENDENCY);
        CHECK(mc_game_settings_read_state(settings, &after) &&
              same_values(&expected, &after.values) && same_owners(&before, &after));
        CHECK(default_bindings_unchanged(&after));
    }
    CHECK(read_output("no-save", after_bytes, sizeof(after_bytes), &after_count) &&
          after_count == before_count && memcmp(before_bytes, after_bytes, before_count) == 0);
    okay = true;
cleanup:
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

static bool real_saves_and_aliases(void) {
    bool okay = false;
    mc_game_settings *settings = NULL, *fresh = NULL, *aliases = NULL;
    uint8_t original[4096], bytes[4096];
    size_t original_count = 0, count = 0;
    CHECK(create_file("saved", NULL, &settings) == MC_GAME_SETTINGS_OK);
    const int32_t ids[] = {MC_GAME_OPTION_INVERT_MOUSE,    MC_GAME_OPTION_VIEW_BOBBING,
                           MC_GAME_OPTION_RENDER_DISTANCE, MC_GAME_OPTION_GUI_SCALE,
                           MC_GAME_OPTION_DIFFICULTY,      MC_GAME_OPTION_FRAMERATE_LIMIT};
    const int32_t steps[] = {0, INT32_MIN, INT32_MAX, 6, -7, -6};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i) {
        mc_game_settings_option_action_report report = {0};
        CHECK(mc_game_settings_set_option(settings, mc_game_option_by_id(ids[i]), steps[i], NULL,
                                          &report, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(report.value_assigned && report.save_reached && report.source_pc == 260 &&
              report.save.source_opened && report.save.source_close_reached &&
              !report.save.outer_exception_caught && report.save.println_attempts == 25);
    }
    CHECK(mc_game_settings_set_option_float(
              settings, mc_game_option_by_id(MC_GAME_OPTION_SENSITIVITY),
              from_bits(UINT32_C(0x80000000)), NULL, NULL, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_option(settings, mc_game_option_by_id(MC_GAME_OPTION_MUSIC), 0, NULL,
                                      NULL, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(read_output("saved", original, sizeof(original), &original_count));
    CHECK(write_input("saved", "authored-replacement-before-step-zero\n"));
    mc_game_settings_option_action_report report = {0};
    CHECK(mc_game_settings_set_option(settings, mc_game_option_by_id(MC_GAME_OPTION_GUI_SCALE), 0,
                                      NULL, &report, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(report.value_assigned && report.save_reached && report.save.source_close_reached);
    CHECK(read_output("saved", bytes, sizeof(bytes), &count) && count == original_count &&
          memcmp(bytes, original, count) == 0);
    CHECK(write_input("saved", "authored-replacement-before-null-option\n"));
    CHECK(mc_game_settings_set_option(settings, NULL, 0, NULL, &report, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(!report.value_assigned && report.save_reached && report.source_pc == 260 &&
          report.save.source_close_reached && report.save.println_attempts == 25);
    CHECK(read_output("saved", bytes, sizeof(bytes), &count) && count == original_count &&
          memcmp(bytes, original, count) == 0);
    CHECK(write_input("saved", "authored-replacement-before-same-binding-code\n"));
    mc_game_settings_binding_action_report binding_report = {0};
    CHECK(mc_game_settings_set_binding_code(settings, 0, 17, NULL, &binding_report, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(binding_report.binding_assigned && binding_report.save_reached &&
          binding_report.source_pc == 14 && binding_report.save.source_close_reached);
    CHECK(read_output("saved", bytes, sizeof(bytes), &count) && count == original_count &&
          memcmp(bytes, original, count) == 0);
    CHECK(create_file("saved", NULL, &fresh) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state loaded = {0};
    CHECK(mc_game_settings_read_state(fresh, &loaded) && loaded.values.invert_mouse &&
          !loaded.values.view_bobbing && loaded.values.render_distance == 3 &&
          loaded.values.gui_scale == 2 && loaded.values.difficulty == 3 &&
          loaded.values.performance_mode == -2 &&
          bits(loaded.values.mouse_sensitivity) == UINT32_C(0x80000000));

    CHECK(create_file("aliases", NULL, &aliases) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state before = {0}, after = {0};
    CHECK(mc_game_settings_read_state(aliases, &before));
    mc_key_binding *forward = before.named[0], *old_left = before.named[1];
    mc_key_binding *const *old_array = before.bindings;
    mc_key_binding *duplicates[] = {forward, forward};
    CHECK(mc_game_settings_set_binding_array(aliases, duplicates, 2, false, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_named_binding(aliases, 1, NULL, NULL) == MC_GAME_SETTINGS_OK);
    const uint16_t name[] = {'k', 'e', 'y', '.', 'a', 'l', 'i', 'a', 's', '-', 0x00e9};
    CHECK(mc_game_settings_binding_set(
              aliases, forward,
              (mc_game_settings_text){name, sizeof(name) / sizeof(name[0]), false}, INT32_MIN,
              NULL) == MC_GAME_SETTINGS_OK);
    uint16_t pack[] = {'P', 'a', 'c', 'k', '-', 0x00e9};
    mc_game_settings_values values = before.values;
    values.texture_pack = (mc_game_settings_text){pack, sizeof(pack) / sizeof(pack[0]), false};
    values.last_server = values.texture_pack;
    CHECK(mc_game_settings_set_values(aliases, &values, NULL) == MC_GAME_SETTINGS_OK);
    pack[0] = 'X';
    CHECK(mc_game_settings_read_state(aliases, &before) &&
          before.values.texture_pack.units == before.values.last_server.units &&
          before.values.texture_pack.units[0] == 'P');
    CHECK(mc_game_settings_set_option(aliases, mc_game_option_by_id(MC_GAME_OPTION_SENSITIVITY), 0,
                                      NULL, &report, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(!report.value_assigned && report.save_reached && report.save.println_attempts == 17 &&
          report.save.source_close_reached && !report.save.outer_exception_caught);
    CHECK(mc_game_settings_read_state(aliases, &after) &&
          same_values(&before.values, &after.values) && same_owners(&before, &after) &&
          after.named[0] == forward && after.named[1] == NULL && after.bindings[0] == forward &&
          after.bindings[1] == forward && old_array[1] == old_left);
    mc_game_settings_text retained_name = {0};
    int32_t retained_code = 0;
    CHECK(mc_key_binding_read(forward, &retained_name, &retained_code) &&
          retained_code == INT32_MIN && retained_name.length == sizeof(name) / sizeof(name[0]) &&
          memcmp(retained_name.units, name, sizeof(name)) == 0);
    CHECK(mc_key_binding_read(old_left, &retained_name, &retained_code) && retained_code == 30);
    okay = true;
cleanup:
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    if (mc_game_settings_retire(&fresh, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    if (mc_game_settings_retire(&aliases, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

typedef struct diagnostic_log {
    mc_game_settings *settings;
    size_t calls;
    bool expected_invert, inspected, busy, fail;
} diagnostic_log;

static mc_game_settings_status diagnostic(void *context, const mc_game_settings_diagnostic *event,
                                          mc_game_settings_error *error) {
    diagnostic_log *log = context;
    ++log->calls;
    mc_game_settings_state state = {0};
    mc_game_settings_text current_name = {0};
    int32_t current_code = 0;
    log->inspected = event->kind == MC_GAME_SETTINGS_SAVE_FAILED && event->line.is_null &&
                     strcmp(event->message, "Failed to save options") == 0 &&
                     mc_game_settings_read_state(log->settings, &state) &&
                     state.values.invert_mouse == log->expected_invert &&
                     mc_key_binding_read(state.named[0], &current_name, &current_code);
    mc_game_settings_option_action_report nested = {
        .source_pc = 987, .save_reached = true, .value_assigned = true, .save = {.lines_read = 42}};
    log->busy =
        mc_game_settings_set_option_float(log->settings,
                                          mc_game_option_by_id(MC_GAME_OPTION_SENSITIVITY), 0, NULL,
                                          &nested, NULL) == MC_GAME_SETTINGS_BUSY &&
        nested.source_pc == 987 && nested.save.lines_read == 42 &&
        mc_game_settings_set_option(log->settings, NULL, 0, NULL, &nested, NULL) ==
            MC_GAME_SETTINGS_BUSY &&
        nested.source_pc == 987 && nested.save_reached &&
        mc_game_settings_set_binding_code(log->settings, 0, 1, NULL, NULL, NULL) ==
            MC_GAME_SETTINGS_BUSY &&
        mc_game_settings_binding_set(log->settings, state.named[0], current_name, current_code,
                                     NULL) == MC_GAME_SETTINGS_BUSY &&
        mc_game_settings_set_values(log->settings, &state.values, NULL) == MC_GAME_SETTINGS_BUSY &&
        mc_game_settings_save(log->settings, NULL, NULL, NULL) == MC_GAME_SETTINGS_BUSY &&
        mc_game_settings_load(log->settings, NULL, NULL, NULL) == MC_GAME_SETTINGS_BUSY;
    mc_game_settings *alias = log->settings;
    log->busy = log->busy && mc_game_settings_retire(&alias, NULL) == MC_GAME_SETTINGS_BUSY &&
                alias == log->settings;
    if (log->fail) {
        if (error != NULL)
            *error = (mc_game_settings_error){MC_GAME_SETTINGS_DIAGNOSTIC_FAILED, 73,
                                              "authored option action diagnostic refusal"};
        return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
    }
    return MC_GAME_SETTINGS_OK;
}

static bool caught_failures_and_admission(void) {
    bool okay = false;
    mc_game_settings *settings = NULL, *directory = NULL, *missing = NULL, *late = NULL,
                     *null_array = NULL;
    uint16_t *large = NULL;
    uint8_t bytes[16384];
    size_t count = 0;
    _Static_assert(MC_GAME_SETTINGS_DIAGNOSTIC_FAILED == 7 &&
                       MC_GAME_SETTINGS_SOURCE_EXCEPTION == 8 &&
                       MC_GAME_SETTINGS_UNIMPLEMENTED_DEPENDENCY == 9,
                   "GameSettings status ABI remains append-only");
    CHECK(mc_game_settings_create_defaults(&profile, &settings, NULL) == MC_GAME_SETTINGS_OK);
    diagnostic_log log = {.settings = settings, .expected_invert = true};
    const mc_game_settings_diagnostics sink = {&log, diagnostic};
    mc_game_settings_option_action_report report = {0};
    CHECK(mc_game_settings_set_option(settings, mc_game_option_by_id(MC_GAME_OPTION_INVERT_MOUSE),
                                      0, &sink, &report, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(report.value_assigned && report.save_reached && report.source_pc == 260 &&
          report.save.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_FILE && log.calls == 1 &&
          log.inspected && log.busy);
    log.fail = true;
    log.expected_invert = false;
    mc_game_settings_error error = {0};
    CHECK(mc_game_settings_set_option(settings, mc_game_option_by_id(MC_GAME_OPTION_INVERT_MOUSE),
                                      1, &sink, &report,
                                      &error) == MC_GAME_SETTINGS_DIAGNOSTIC_FAILED);
    CHECK(report.value_assigned && report.save_reached && report.source_pc == 257 &&
          log.calls == 2 && log.inspected && log.busy && error.system_error == 73);
    CHECK(mc_game_settings_set_option_float(settings, NULL, 1, &sink, &report, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(report.source_pc == 56 && !report.value_assigned && !report.save_reached &&
          log.calls == 2);
    CHECK(mc_game_settings_set_option_float(settings, mc_game_option_by_id(MC_GAME_OPTION_MUSIC),
                                            0.25f, &sink, &report,
                                            NULL) == MC_GAME_SETTINGS_SOURCE_EXCEPTION);
    CHECK(report.source_pc == 16 && report.value_assigned && !report.save_reached &&
          log.calls == 2);
    CHECK(mc_game_settings_set_option(settings, mc_game_option_by_id(MC_GAME_OPTION_ANAGLYPH), 0,
                                      &sink, &report, NULL) == MC_GAME_SETTINGS_SOURCE_EXCEPTION);
    CHECK(report.source_pc == 144 && report.value_assigned && !report.save_reached &&
          log.calls == 2);

    union {
        void *pointer;
        uint64_t integer;
        long double scalar;
    } foreign = {0};

    report = (mc_game_settings_option_action_report){.source_pc = 987, .save_reached = true};
    CHECK(mc_game_settings_set_option(NULL, NULL, 0, NULL, &report, NULL) ==
              MC_GAME_SETTINGS_INVALID_ARGUMENT &&
          report.source_pc == 987 && report.save_reached);
    CHECK(mc_game_settings_set_option_float(NULL, NULL, 0, NULL, &report, NULL) ==
              MC_GAME_SETTINGS_INVALID_ARGUMENT &&
          report.source_pc == 987 && report.save_reached);
    CHECK(mc_game_settings_set_option_float(settings, (const mc_game_option *)&foreign, 0, NULL,
                                            &report, NULL) == MC_GAME_SETTINGS_INVALID_ARGUMENT &&
          report.source_pc == 987 && report.save_reached);
    CHECK(mc_game_settings_set_option(settings, (const mc_game_option *)&foreign, 0, NULL, &report,
                                      NULL) == MC_GAME_SETTINGS_INVALID_ARGUMENT &&
          report.source_pc == 987 && report.save_reached);

    CHECK(create_file("io-directory", NULL, &directory) == MC_GAME_SETTINGS_OK);
    CHECK(create_file("absent-parent/child", NULL, &missing) == MC_GAME_SETTINGS_OK);
    mc_game_settings *io[] = {directory, missing};
    for (size_t i = 0; i < sizeof(io) / sizeof(io[0]); ++i) {
        CHECK(mc_game_settings_set_option(io[i], mc_game_option_by_id(MC_GAME_OPTION_INVERT_MOUSE),
                                          0, NULL, &report, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(report.source_pc == 260 && report.value_assigned && report.save_reached &&
              report.save.outer_exception_caught &&
              report.save.outer_exception == MC_GAME_SETTINGS_SOURCE_FILE_IO &&
              !report.save.source_opened);
        mc_game_settings_state current = {0};
        CHECK(mc_game_settings_read_state(io[i], &current) && current.values.invert_mouse);
    }
    CHECK(create_file("late", NULL, &late) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state state = {0};
    CHECK(mc_game_settings_read_state(late, &state));
    large = malloc(9000u * sizeof(*large));
    CHECK(large != NULL);
    for (size_t i = 0; i < 9000u; ++i)
        large[i] = 'P';
    mc_game_settings_values values = state.values;
    values.texture_pack = (mc_game_settings_text){large, 9000, false};
    CHECK(mc_game_settings_set_values(late, &values, NULL) == MC_GAME_SETTINGS_OK);
    mc_key_binding *partial[] = {state.named[0], NULL};
    CHECK(mc_game_settings_set_binding_array(late, partial, 2, false, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_option(late, mc_game_option_by_id(MC_GAME_OPTION_INVERT_MOUSE), 0,
                                      NULL, &report, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(report.source_pc == 260 && report.value_assigned && report.save_reached &&
          report.save.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_BINDING &&
          report.save.println_attempts == 16 && !report.save.source_close_reached &&
          report.save.native_additional_close && !report.save.native_cleanup_failed);
    CHECK(read_output("late", bytes, sizeof(bytes), &count) && count == 8192 &&
          bytes[count - 1] == 'P');
    CHECK(mc_game_settings_read_state(late, &state) && state.values.invert_mouse);
    CHECK(create_file("null-array", NULL, &null_array) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_binding_array(null_array, NULL, 0, true, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_option(null_array, NULL, 0, NULL, &report, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(report.source_pc == 260 && !report.value_assigned && report.save_reached &&
          report.save.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_BINDING_ARRAY &&
          report.save.println_attempts == 15 && !report.save.source_close_reached &&
          report.save.native_additional_close);
    CHECK(read_output("null-array", bytes, sizeof(bytes), &count) && count == 0);
    okay = true;
cleanup:
    free(large);
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    if (mc_game_settings_retire(&directory, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    if (mc_game_settings_retire(&missing, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    if (mc_game_settings_retire(&late, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    if (mc_game_settings_retire(&null_array, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

static int test_body(int argc, char **argv) {
    if (argc != 2)
        return EXIT_FAILURE;
    root = argv[1];
    if (!float_domain() || !discrete_domain() || !wrapping() || !opaque_effects_and_no_save() ||
        !real_saves_and_aliases() || !caught_failures_and_admission())
        return EXIT_FAILURE;
    if (printf("PASS: %u GameSettings option action interface checks\n", checks) < 0 ||
        fflush(stdout) != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **wide) {
    if (argc != 2)
        return EXIT_FAILURE;
    const int count =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[1], -1, NULL, 0, NULL, NULL);
    if (count <= 0 || count > 4096)
        return EXIT_FAILURE;
    char *path = malloc((size_t)count);
    if (path == NULL)
        return EXIT_FAILURE;
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[1], -1, path, count, NULL, NULL) !=
        count) {
        free(path);
        return EXIT_FAILURE;
    }
    char *args[] = {NULL, path};
    const int status = test_body(argc, args);
    free(path);
    return status;
}
#else
int main(int argc, char **argv) {
    return test_body(argc, argv);
}
#endif
