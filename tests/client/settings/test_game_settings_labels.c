#include "labels.h"

#include <stdio.h>
#include <string.h>

#define COUNT(values) (sizeof(values) / sizeof((values)[0]))
static unsigned int checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* Authored Properties bytes, not original assets. Translation must consume the
 * real source streams and its second resource overwrites the first binding. */
static const char english[] =
    "options.music=M\noptions.sound=S\noptions.invertMouse=Invert\n"
    "options.sensitivity=Sense\noptions.sensitivity.min=LOW\n"
    "options.sensitivity.max=HIGH\noptions.off=OFF\noptions.on=ON\n"
    "options.renderDistance=Distance\noptions.renderDistance.far=Far\n"
    "options.renderDistance.normal=Normal\noptions.renderDistance.short=Short\n"
    "options.renderDistance.tiny=Tiny\noptions.viewBobbing=Bob\n"
    "options.anaglyph=Anaglyph\noptions.advancedOpengl=Advanced\n"
    "options.framerateLimit=Performance\nperformance.max=Max\n"
    "performance.balanced=Balanced\nperformance.powersaver=Saver\n"
    "options.difficulty=Difficulty\noptions.difficulty.peaceful=Peaceful\n"
    "options.difficulty.easy=Easy\noptions.difficulty.normal=Normal\n"
    "options.difficulty.hard=Hard\noptions.graphics=Graphics\n"
    "options.graphics.fancy=Fancy\noptions.graphics.fast=Fast\n"
    "options.ao=AO\noptions.guiScale=Scale\noptions.guiScale.auto=Auto\n"
    "options.guiScale.small=Small\noptions.guiScale.normal=Normal\n"
    "options.guiScale.large=Large\nkey.forward=first\nalias.key=ALIAS\n"
    "\\u00e9\\u0000\\ud800=FOUND\n";
static const char statistics[] = "key.forward=FORWARD\n";
static const char malformed[] = "bad=\\u12xz\n";

typedef struct stream {
    const uint8_t *bytes;
    size_t length, position;
} stream;

typedef struct source {
    mc_registry_resources api;
    stream streams[2];
    const char *opened[2];
    size_t opens, closes;
    unsigned int failure; /* 1: absent first resource; 2: malformed Properties. */
    bool invalid;
} source;

typedef struct fixture {
    source source;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_registry_classes *classes;
    mc_client_assets *assets;
    mc_game_settings *settings;
} fixture;

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    stream *input = context;
    if (out == NULL || count == NULL || capacity == 0)
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    size_t take = input->length - input->position;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, input->bytes + input->position, take);
    input->position += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    source *resources = context;
    if (name == NULL || out == NULL || resources->opens == COUNT(resources->streams)) {
        resources->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    const size_t index = resources->opens++;
    resources->opened[index] = name;
    if (resources->failure == 1u)
        return MC_REGISTRY_NULL_REFERENCE;
    const bool first = strcmp(name, "/lang/en_US.lang") == 0;
    const bool second = strcmp(name, "/lang/stats_US.lang") == 0;
    if ((!first && !second) || first != (index == 0)) {
        resources->invalid = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    const char *bytes = first ? english : statistics;
    if (first && resources->failure == 2u)
        bytes = malformed;
    resources->streams[index] = (stream){(const uint8_t *)bytes, strlen(bytes), 0};
    *out = (mc_java_properties_input){&resources->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    source *resources = context;
    (void)input;
    ++resources->closes;
    resources->invalid = true; /* nh constructor has no reached close. */
    return MC_REGISTRY_OK;
}

static bool create(fixture *f, unsigned int failure) {
    f->source.failure = failure;
    f->source.api = (mc_registry_resources){&f->source, open_resource, close_resource, NULL};
    if (!mc_materials_create(&f->materials) ||
        mc_java_random_profile_create(1024u * 1024u, &f->profile) != MC_JAVA_RANDOM_PROFILE_OK)
        return false;
    const mc_registry_options options = {.side = MC_REGISTRY_CLIENT,
                                         .random_profile = f->profile,
                                         .materials = f->materials,
                                         .resources = &f->source.api};
    return mc_registry_classes_allocate(&options, &f->classes, NULL) == MC_REGISTRY_OK &&
           mc_client_assets_create(&f->source.api, f->classes, &f->assets) == MC_CLIENT_ASSETS_OK &&
           mc_game_settings_create_defaults(NULL, &f->settings, NULL) == MC_GAME_SETTINGS_OK;
}

static bool destroy(fixture *f) {
    if (mc_game_settings_retire(&f->settings, NULL) != MC_GAME_SETTINGS_OK)
        return false;
    mc_client_assets_free(f->assets);
    f->assets = NULL;
    if (mc_registry_classes_free(f->classes, NULL) != MC_REGISTRY_OK)
        return false;
    f->classes = NULL;
    mc_materials_destroy(f->materials);
    f->materials = NULL;
    mc_java_random_profile_free(f->profile);
    f->profile = NULL;
    return true;
}

static bool phases(fixture *f, mc_registry_phase singleton, mc_registry_phase wrapper) {
    mc_registry_translation_view view;
    return mc_registry_classes_inspect_translation(f->classes, &view, NULL) == MC_REGISTRY_OK &&
           view.singleton_phase == singleton && view.wrapper_phase == wrapper;
}

static bool text_is(mc_game_settings_label text, const uint16_t *units, size_t length) {
    return !text.is_null && text.length == length &&
           (length == 0 ||
            (text.units != NULL && memcmp(text.units, units, length * sizeof(*units)) == 0));
}

static bool ascii_is(mc_game_settings_label text, const char *expected) {
    const size_t length = strlen(expected);
    if (text.is_null || text.length != length || (length != 0 && text.units == NULL))
        return false;
    for (size_t i = 0; i < length; ++i)
        if (text.units[i] != (unsigned char)expected[i])
            return false;
    return true;
}

static bool option_is(fixture *f, mc_game_option_id id, const char *expected) {
    mc_game_settings_label text = {0};
    mc_game_settings_label_error error;
    const mc_game_settings_label_status status = mc_game_settings_option_label(
        f->settings, f->assets, mc_game_option_by_id((int32_t)id), &text, &error);
    const bool result = status == MC_GAME_SETTINGS_LABEL_OK && ascii_is(text, expected) &&
                        error.status == MC_GAME_SETTINGS_LABEL_OK &&
                        error.source_failure == MC_GAME_SETTINGS_LABEL_SOURCE_NONE &&
                        error.settings_pc == -1 && error.dependency_pc == -1;
    mc_game_settings_label_free(&text);
    return result;
}

static int source_order(void) {
    int result = 1;
    fixture f = {0};
    CHECK(create(&f, 0));
    CHECK(f.source.opens == 0 && phases(&f, MC_REGISTRY_COLD, MC_REGISTRY_COLD));
    uint16_t safe[] = {'s', 0, 0xd800};
    mc_game_settings_label text = {safe, COUNT(safe), false};
    mc_game_settings_label_error error;
    CHECK(mc_game_settings_option_label(f.settings, f.assets, NULL, &text, &error) ==
          MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION);
    CHECK(text.units == safe && text.length == COUNT(safe) && !text.is_null);
    CHECK(error.source_failure == MC_GAME_SETTINGS_LABEL_NULL_OPTION && error.settings_pc == 13);
    CHECK(phases(&f, MC_REGISTRY_READY, MC_REGISTRY_COLD));
    CHECK(f.source.opens == 2 && strcmp(f.source.opened[0], "/lang/en_US.lang") == 0 &&
          strcmp(f.source.opened[1], "/lang/stats_US.lang") == 0 && f.source.closes == 0);
    mc_game_settings_state state;
    CHECK(mc_game_settings_read_state(f.settings, &state));
    mc_game_settings_values values = state.values;
    values.performance_mode = 3;
    CHECK(mc_game_settings_set_values(f.settings, &values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_option_label(f.settings, f.assets,
                                        mc_game_option_by_id(MC_GAME_OPTION_FRAMERATE_LIMIT), &text,
                                        &error) == MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION);
    CHECK(error.source_failure == MC_GAME_SETTINGS_LABEL_VALUE_INDEX && error.settings_pc == 406 &&
          text.units == safe && phases(&f, MC_REGISTRY_READY, MC_REGISTRY_COLD));
    /* Populate the suffix cache through the direct path before the wrapper use. */
    static const uint16_t performance_key[] = {'p', 'e', 'r', 'f', 'o', 'r', 'm', 'a', 'n', 'c',
                                               'e', '.', 'b', 'a', 'l', 'a', 'n', 'c', 'e', 'd'};
    mc_font_text cached;
    CHECK(mc_client_assets_label_text(f.assets,
                                      (mc_font_text){performance_key, COUNT(performance_key)},
                                      false, &cached, NULL) == MC_CLIENT_ASSETS_OK);
    CHECK(phases(&f, MC_REGISTRY_READY, MC_REGISTRY_COLD));
    values.performance_mode = 1;
    CHECK(mc_game_settings_set_values(f.settings, &values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(option_is(&f, MC_GAME_OPTION_FRAMERATE_LIMIT, "Performance: Balanced"));
    CHECK(phases(&f, MC_REGISTRY_READY, MC_REGISTRY_READY) && f.source.opens == 2);
    result = 0;
cleanup:
    if (!destroy(&f) || f.source.invalid)
        result = 1;
    return result;
}

static float from_bits(uint32_t value) {
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static int option_labels(void) {
    int result = 1;
    fixture f = {0};
    CHECK(create(&f, 0));
    static const char *const defaults[] = {"M: 100%",
                                           "S: 100%",
                                           "Invert: OFF",
                                           "Sense: 100%",
                                           "Distance: Far",
                                           "Bob: ON",
                                           "Anaglyph: OFF",
                                           "Advanced: OFF",
                                           "Performance: Balanced",
                                           "Difficulty: Normal",
                                           "Graphics: Fancy",
                                           "AO: ON",
                                           "Scale: Auto"};
    for (size_t i = 0; i < COUNT(defaults); ++i)
        CHECK(option_is(&f, (mc_game_option_id)i, defaults[i]));
    mc_game_settings_state state;
    CHECK(mc_game_settings_read_state(f.settings, &state));
    mc_game_settings_values values = state.values;

    static const struct {
        mc_game_option_id id;
        uint32_t bits;
        const char *label;
    } floats[] = {{MC_GAME_OPTION_MUSIC, UINT32_C(0x00000000), "M: OFF"},
                  {MC_GAME_OPTION_MUSIC, UINT32_C(0x80000000), "M: OFF"},
                  {MC_GAME_OPTION_MUSIC, UINT32_C(0x3e947ae1), "M: 29%"},
                  {MC_GAME_OPTION_MUSIC, UINT32_C(0x3f000000), "M: 50%"},
                  {MC_GAME_OPTION_MUSIC, UINT32_C(0x3f7fffff), "M: 99%"},
                  {MC_GAME_OPTION_MUSIC, UINT32_C(0x3fc00000), "M: 150%"},
                  {MC_GAME_OPTION_MUSIC, UINT32_C(0xbe800000), "M: -25%"},
                  {MC_GAME_OPTION_MUSIC, UINT32_C(0x7fc01234), "M: 0%"},
                  {MC_GAME_OPTION_MUSIC, UINT32_C(0x7f800000), "M: 2147483647%"},
                  {MC_GAME_OPTION_MUSIC, UINT32_C(0xff800000), "M: -2147483648%"},
                  {MC_GAME_OPTION_SOUND, UINT32_C(0x00000000), "S: OFF"},
                  {MC_GAME_OPTION_SOUND, UINT32_C(0xbf000000), "S: -50%"},
                  {MC_GAME_OPTION_SOUND, UINT32_C(0x7f7fffff), "S: 2147483647%"},
                  {MC_GAME_OPTION_SENSITIVITY, UINT32_C(0x00000000), "Sense: LOW"},
                  {MC_GAME_OPTION_SENSITIVITY, UINT32_C(0x80000000), "Sense: LOW"},
                  {MC_GAME_OPTION_SENSITIVITY, UINT32_C(0x3f800000), "Sense: HIGH"},
                  {MC_GAME_OPTION_SENSITIVITY, UINT32_C(0x3e147ae1), "Sense: 29%"},
                  {MC_GAME_OPTION_SENSITIVITY, UINT32_C(0x3f7fffff), "Sense: 199%"},
                  {MC_GAME_OPTION_SENSITIVITY, UINT32_C(0x3f800001), "Sense: 200%"},
                  {MC_GAME_OPTION_SENSITIVITY, UINT32_C(0xbe800000), "Sense: -50%"},
                  {MC_GAME_OPTION_SENSITIVITY, UINT32_C(0x3fc00000), "Sense: 300%"},
                  {MC_GAME_OPTION_SENSITIVITY, UINT32_C(0x7fc01234), "Sense: 0%"},
                  {MC_GAME_OPTION_SENSITIVITY, UINT32_C(0x7f800000), "Sense: 2147483647%"},
                  {MC_GAME_OPTION_SENSITIVITY, UINT32_C(0xff800000), "Sense: -2147483648%"}};

    for (size_t i = 0; i < COUNT(floats); ++i) {
        const float value = from_bits(floats[i].bits);
        if (floats[i].id == MC_GAME_OPTION_MUSIC)
            values.music_volume = value;
        else if (floats[i].id == MC_GAME_OPTION_SOUND)
            values.sound_volume = value;
        else
            values.mouse_sensitivity = value;
        CHECK(mc_game_settings_set_values(f.settings, &values, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(option_is(&f, floats[i].id, floats[i].label));
    }

    static const struct {
        mc_game_option_id id;
        const char *labels[4];
        size_t count;
        int32_t pc;
    } tables[] = {
        {MC_GAME_OPTION_RENDER_DISTANCE,
         {"Distance: Far", "Distance: Normal", "Distance: Short", "Distance: Tiny"},
         4,
         296},
        {MC_GAME_OPTION_DIFFICULTY,
         {"Difficulty: Peaceful", "Difficulty: Easy", "Difficulty: Normal", "Difficulty: Hard"},
         4,
         333},
        {MC_GAME_OPTION_GUI_SCALE,
         {"Scale: Auto", "Scale: Small", "Scale: Normal", "Scale: Large"},
         4,
         370},
        {MC_GAME_OPTION_FRAMERATE_LIMIT,
         {"Performance: Max", "Performance: Balanced", "Performance: Saver", NULL},
         3,
         406}};

    uint16_t canary[] = {'x'};
    for (size_t i = 0; i < COUNT(tables); ++i) {
        for (int32_t index = -1; index <= (int32_t)tables[i].count; ++index) {
            switch (tables[i].id) {
                case MC_GAME_OPTION_RENDER_DISTANCE:
                    values.render_distance = index;
                    break;
                case MC_GAME_OPTION_DIFFICULTY:
                    values.difficulty = index;
                    break;
                case MC_GAME_OPTION_GUI_SCALE:
                    values.gui_scale = index;
                    break;
                case MC_GAME_OPTION_FRAMERATE_LIMIT:
                    values.performance_mode = index;
                    break;
                default:
                    goto cleanup;
            }
            CHECK(mc_game_settings_set_values(f.settings, &values, NULL) == MC_GAME_SETTINGS_OK);
            if (index >= 0 && (size_t)index < tables[i].count)
                CHECK(option_is(&f, tables[i].id, tables[i].labels[(size_t)index]));
            else {
                mc_game_settings_label text = {canary, COUNT(canary), false};
                mc_game_settings_label_error error;
                CHECK(mc_game_settings_option_label(
                          f.settings, f.assets, mc_game_option_by_id((int32_t)tables[i].id), &text,
                          &error) == MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION);
                CHECK(text.units == canary && text.length == COUNT(canary) &&
                      error.source_failure == MC_GAME_SETTINGS_LABEL_VALUE_INDEX &&
                      error.settings_pc == tables[i].pc);
                /* Unrelated invalid raw fields must not preflight another label. */
                CHECK(option_is(&f, MC_GAME_OPTION_GRAPHICS, "Graphics: Fancy"));
            }
        }
    }
    values.invert_mouse = true;
    values.view_bobbing = false;
    values.anaglyph = true;
    values.advanced_opengl = true;
    values.ambient_occlusion = false;
    values.fancy_graphics = false;
    CHECK(mc_game_settings_set_values(f.settings, &values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(option_is(&f, MC_GAME_OPTION_INVERT_MOUSE, "Invert: ON"));
    CHECK(option_is(&f, MC_GAME_OPTION_VIEW_BOBBING, "Bob: OFF"));
    CHECK(option_is(&f, MC_GAME_OPTION_ANAGLYPH, "Anaglyph: ON"));
    CHECK(option_is(&f, MC_GAME_OPTION_ADVANCED_OPENGL, "Advanced: ON"));
    CHECK(option_is(&f, MC_GAME_OPTION_AMBIENT_OCCLUSION, "AO: OFF"));
    CHECK(option_is(&f, MC_GAME_OPTION_GRAPHICS, "Graphics: Fast"));
    result = 0;
cleanup:
    if (!destroy(&f) || f.source.invalid || f.source.closes != 0)
        result = 1;
    return result;
}

static int bindings(void) {
    int result = 1;
    fixture f = {0};
    mc_game_settings_label text = {0};
    CHECK(create(&f, 0));
    mc_game_settings_state state;
    CHECK(mc_game_settings_read_state(f.settings, &state));
    static const uint16_t forward[] = {'k', 'e', 'y', '.', 'f', 'o', 'r', 'w', 'a', 'r', 'd'};
    for (int32_t code = 0; code < 256; ++code) {
        CHECK(mc_game_settings_binding_set(f.settings, state.named[0],
                                           (mc_game_settings_text){forward, COUNT(forward), false},
                                           code, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(mc_game_settings_key_label(f.settings, 0, &text, NULL) == MC_GAME_SETTINGS_LABEL_OK);
        const char *key = NULL;
        CHECK(mc_game_key_name(code, &key) == MC_GAME_OPTION_OK);
        CHECK(key == NULL ? text.is_null && text.units == NULL && text.length == 0
                          : ascii_is(text, key));
        mc_game_settings_label_free(&text);
    }
    CHECK(phases(&f, MC_REGISTRY_COLD, MC_REGISTRY_COLD) && f.source.opens == 0);
    const int32_t invalid[] = {-1, 256, INT32_MIN, INT32_MAX};
    uint16_t safe[] = {'s'};
    for (size_t i = 0; i < COUNT(invalid); ++i) {
        CHECK(mc_game_settings_binding_set(f.settings, state.named[0],
                                           (mc_game_settings_text){forward, COUNT(forward), false},
                                           invalid[i], NULL) == MC_GAME_SETTINGS_OK);
        mc_game_settings_label unchanged = {safe, COUNT(safe), false};
        mc_game_settings_label_error error;
        CHECK(mc_game_settings_key_label(f.settings, 0, &unchanged, &error) ==
              MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION);
        CHECK(unchanged.units == safe && error.source_failure == MC_GAME_SETTINGS_LABEL_KEY_INDEX &&
              error.settings_pc == 9 && error.dependency_pc == 4);
    }
    mc_key_binding *extra = NULL;
    static const uint16_t alias[] = {'a', 'l', 'i', 'a', 's', '.', 'k', 'e', 'y'};
    CHECK(mc_game_settings_binding_create(f.settings,
                                          (mc_game_settings_text){alias, COUNT(alias), false}, 2,
                                          &extra, NULL) == MC_GAME_SETTINGS_OK);
    mc_key_binding *array[] = {extra, extra, NULL};
    CHECK(mc_game_settings_set_binding_array(f.settings, array, COUNT(array), false, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_key_label(f.settings, 0, &text, NULL) == MC_GAME_SETTINGS_LABEL_OK &&
          ascii_is(text, "1"));
    mc_game_settings_label_free(&text);
    CHECK(mc_game_settings_binding_label(f.settings, f.assets, 1, &text, NULL) ==
              MC_GAME_SETTINGS_LABEL_OK &&
          ascii_is(text, "ALIAS"));
    mc_game_settings_label_free(&text);
    CHECK(mc_game_settings_binding_set(f.settings, extra,
                                       (mc_game_settings_text){forward, COUNT(forward), false}, 17,
                                       NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_binding_label(f.settings, f.assets, 0, &text, NULL) ==
              MC_GAME_SETTINGS_LABEL_OK &&
          ascii_is(text, "FORWARD"));
    mc_game_settings_label_free(&text);
    uint16_t unicode[] = {0x00e9, 0, 0xd800};
    CHECK(mc_game_settings_binding_set(f.settings, extra,
                                       (mc_game_settings_text){unicode, COUNT(unicode), false}, 17,
                                       NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_binding_label(f.settings, f.assets, 1, &text, NULL) ==
              MC_GAME_SETTINGS_LABEL_OK &&
          ascii_is(text, "FOUND"));
    mc_game_settings_label_free(&text);
    CHECK(mc_game_settings_binding_set(f.settings, extra, (mc_game_settings_text){NULL, 0, false},
                                       17, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_binding_label(f.settings, f.assets, 0, &text, NULL) ==
              MC_GAME_SETTINGS_LABEL_OK &&
          !text.is_null && text.length == 0);
    mc_game_settings_label_free(&text);
    CHECK(mc_game_settings_binding_set(f.settings, extra, (mc_game_settings_text){NULL, 0, true},
                                       17, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_label unchanged = {safe, COUNT(safe), false};
    mc_game_settings_label_error error;
    CHECK(mc_game_settings_binding_label(f.settings, f.assets, 0, &unchanged, &error) ==
          MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION);
    CHECK(unchanged.units == safe && error.source_failure == MC_GAME_SETTINGS_LABEL_NULL_NAME &&
          error.settings_pc == 14 && error.dependency_pc == 6);
    CHECK(mc_game_settings_key_label(f.settings, 0, &text, NULL) == MC_GAME_SETTINGS_LABEL_OK &&
          ascii_is(text, "W"));
    mc_game_settings_label_free(&text);
    CHECK(mc_game_settings_binding_label(f.settings, f.assets, 2, &unchanged, &error) ==
          MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION);
    CHECK(error.source_failure == MC_GAME_SETTINGS_LABEL_NULL_BINDING && error.settings_pc == 11);
    CHECK(mc_game_settings_key_label(f.settings, 2, &unchanged, &error) ==
          MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION);
    CHECK(error.source_failure == MC_GAME_SETTINGS_LABEL_NULL_BINDING && error.settings_pc == 6);
    CHECK(mc_game_settings_binding_label(f.settings, f.assets, -1, &unchanged, &error) ==
          MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION);
    CHECK(error.source_failure == MC_GAME_SETTINGS_LABEL_BINDING_INDEX && error.settings_pc == 10);
    CHECK(mc_game_settings_set_binding_array(f.settings, NULL, 0, true, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_binding_label(f.settings, f.assets, INT32_MIN, &unchanged, &error) ==
          MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION);
    CHECK(error.source_failure == MC_GAME_SETTINGS_LABEL_NULL_BINDING_ARRAY &&
          error.settings_pc == 10);
    CHECK(mc_game_settings_set_binding_array(f.settings, NULL, 0, false, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_key_label(f.settings, 0, &unchanged, &error) ==
          MC_GAME_SETTINGS_LABEL_SOURCE_EXCEPTION);
    CHECK(error.source_failure == MC_GAME_SETTINGS_LABEL_BINDING_INDEX && error.settings_pc == 5);
    /* A result survives both original input mutation and all owner retirement. */
    uint16_t absent[] = {0x00e9, 0, 0xd800, 'x'};
    const uint16_t expected[] = {0x00e9, 0, 0xd800, 'x'};
    CHECK(mc_game_settings_binding_set(f.settings, extra,
                                       (mc_game_settings_text){absent, COUNT(absent), false}, 17,
                                       NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_binding_array(f.settings, &extra, 1, false, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_binding_label(f.settings, f.assets, 0, &text, NULL) ==
          MC_GAME_SETTINGS_LABEL_OK);
    absent[0] = 'z';
    CHECK(text_is(text, expected, COUNT(expected)));
    CHECK(phases(&f, MC_REGISTRY_READY, MC_REGISTRY_COLD));
    CHECK(destroy(&f));
    CHECK(text_is(text, expected, COUNT(expected)));
    result = 0;
cleanup:
    mc_game_settings_label_free(&text);
    if (!destroy(&f) || f.source.invalid || f.source.closes != 0)
        result = 1;
    return result;
}

static int failure_prefixes(void) {
    int result = 1;
    fixture f = {0};
    for (unsigned int fault = 1; fault <= 2; ++fault) {
        CHECK(create(&f, fault));
        CHECK(mc_game_settings_set_binding_array(f.settings, NULL, 0, true, NULL) ==
              MC_GAME_SETTINGS_OK);
        uint16_t safe[] = {'s'};
        mc_game_settings_label text = {safe, COUNT(safe), false};
        mc_game_settings_label_error error;
        CHECK(mc_game_settings_binding_label(f.settings, f.assets, INT32_MIN, &text, &error) ==
              MC_GAME_SETTINGS_LABEL_ASSETS_FAILED);
        CHECK(error.source_failure == MC_GAME_SETTINGS_LABEL_SOURCE_NONE &&
              error.settings_pc == 0 && text.units == safe && text.length == COUNT(safe));
        CHECK(f.source.opens == 1 && f.source.closes == 0 &&
              phases(&f, MC_REGISTRY_FAILED, MC_REGISTRY_COLD));
        CHECK(mc_game_settings_option_label(f.settings, f.assets, NULL, &text, &error) ==
                  MC_GAME_SETTINGS_LABEL_ASSETS_FAILED &&
              f.source.opens == 1);
        CHECK(error.settings_pc == 0 &&
              error.assets.registry_error.status == MC_REGISTRY_FAILED_INITIALIZATION);
        CHECK(destroy(&f));
        f = (fixture){0};
    }
    CHECK(create(&f, 0));
    uint16_t safe[] = {'s'};
    mc_game_settings_label text = {safe, COUNT(safe), false};
    mc_game_settings_label_error error;
    CHECK(mc_game_settings_option_label(NULL, f.assets, mc_game_option_by_id(0), &text, &error) ==
              MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT &&
          text.units == safe);
    CHECK(mc_game_settings_binding_label(f.settings, NULL, 0, &text, &error) ==
              MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT &&
          f.source.opens == 0);
    CHECK(mc_game_settings_key_label(f.settings, 0, NULL, &error) ==
          MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT);

    union {
        void *pointer;
        uint64_t word;
        long double scalar;
    } foreign = {0};

    CHECK(mc_game_settings_option_label(f.settings, f.assets, (const mc_game_option *)&foreign,
                                        &text, &error) == MC_GAME_SETTINGS_LABEL_INVALID_ARGUMENT &&
          error.settings_pc == -1 && f.source.opens == 0);
    result = 0;
cleanup:
    if (!destroy(&f) || f.source.invalid)
        result = 1;
    return result;
}

int main(void) {
    if (source_order() != 0 || option_labels() != 0 || bindings() != 0 || failure_prefixes() != 0)
        return 1;
    printf("PASS: %u GameSettings label checks\n", checks);
    return 0;
}
