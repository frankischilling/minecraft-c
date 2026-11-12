#include "mc/client/settings/game_option.h"

#include <limits.h>
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

/* Literal source metadata and API constant facts; no source execution oracle. */
static const mc_game_option_info expected_options[] = {
    {MC_GAME_OPTION_MUSIC, "MUSIC", "options.music", true, false},
    {MC_GAME_OPTION_SOUND, "SOUND", "options.sound", true, false},
    {MC_GAME_OPTION_INVERT_MOUSE, "INVERT_MOUSE", "options.invertMouse", false, true},
    {MC_GAME_OPTION_SENSITIVITY, "SENSITIVITY", "options.sensitivity", true, false},
    {MC_GAME_OPTION_RENDER_DISTANCE, "RENDER_DISTANCE", "options.renderDistance", false, false},
    {MC_GAME_OPTION_VIEW_BOBBING, "VIEW_BOBBING", "options.viewBobbing", false, true},
    {MC_GAME_OPTION_ANAGLYPH, "ANAGLYPH", "options.anaglyph", false, true},
    {MC_GAME_OPTION_ADVANCED_OPENGL, "ADVANCED_OPENGL", "options.advancedOpengl", false, true},
    {MC_GAME_OPTION_FRAMERATE_LIMIT, "FRAMERATE_LIMIT", "options.framerateLimit", false, false},
    {MC_GAME_OPTION_DIFFICULTY, "DIFFICULTY", "options.difficulty", false, false},
    {MC_GAME_OPTION_GRAPHICS, "GRAPHICS", "options.graphics", false, false},
    {MC_GAME_OPTION_AMBIENT_OCCLUSION, "AMBIENT_OCCLUSION", "options.ao", false, true},
    {MC_GAME_OPTION_GUI_SCALE, "GUI_SCALE", "options.guiScale", false, false}};

typedef struct key_fact {
    int32_t code;
    const char *name;
} key_fact;

static const key_fact key_facts[] = {{0, "NONE"},
                                     {1, "ESCAPE"},
                                     {2, "1"},
                                     {3, "2"},
                                     {4, "3"},
                                     {5, "4"},
                                     {6, "5"},
                                     {7, "6"},
                                     {8, "7"},
                                     {9, "8"},
                                     {10, "9"},
                                     {11, "0"},
                                     {12, "MINUS"},
                                     {13, "EQUALS"},
                                     {14, "BACK"},
                                     {15, "TAB"},
                                     {16, "Q"},
                                     {17, "W"},
                                     {18, "E"},
                                     {19, "R"},
                                     {20, "T"},
                                     {21, "Y"},
                                     {22, "U"},
                                     {23, "I"},
                                     {24, "O"},
                                     {25, "P"},
                                     {26, "LBRACKET"},
                                     {27, "RBRACKET"},
                                     {28, "RETURN"},
                                     {29, "LCONTROL"},
                                     {30, "A"},
                                     {31, "S"},
                                     {32, "D"},
                                     {33, "F"},
                                     {34, "G"},
                                     {35, "H"},
                                     {36, "J"},
                                     {37, "K"},
                                     {38, "L"},
                                     {39, "SEMICOLON"},
                                     {40, "APOSTROPHE"},
                                     {41, "GRAVE"},
                                     {42, "LSHIFT"},
                                     {43, "BACKSLASH"},
                                     {44, "Z"},
                                     {45, "X"},
                                     {46, "C"},
                                     {47, "V"},
                                     {48, "B"},
                                     {49, "N"},
                                     {50, "M"},
                                     {51, "COMMA"},
                                     {52, "PERIOD"},
                                     {53, "SLASH"},
                                     {54, "RSHIFT"},
                                     {55, "MULTIPLY"},
                                     {56, "LMENU"},
                                     {57, "SPACE"},
                                     {58, "CAPITAL"},
                                     {59, "F1"},
                                     {60, "F2"},
                                     {61, "F3"},
                                     {62, "F4"},
                                     {63, "F5"},
                                     {64, "F6"},
                                     {65, "F7"},
                                     {66, "F8"},
                                     {67, "F9"},
                                     {68, "F10"},
                                     {69, "NUMLOCK"},
                                     {70, "SCROLL"},
                                     {71, "NUMPAD7"},
                                     {72, "NUMPAD8"},
                                     {73, "NUMPAD9"},
                                     {74, "SUBTRACT"},
                                     {75, "NUMPAD4"},
                                     {76, "NUMPAD5"},
                                     {77, "NUMPAD6"},
                                     {78, "ADD"},
                                     {79, "NUMPAD1"},
                                     {80, "NUMPAD2"},
                                     {81, "NUMPAD3"},
                                     {82, "NUMPAD0"},
                                     {83, "DECIMAL"},
                                     {87, "F11"},
                                     {88, "F12"},
                                     {100, "F13"},
                                     {101, "F14"},
                                     {102, "F15"},
                                     {103, "F16"},
                                     {104, "F17"},
                                     {105, "F18"},
                                     {112, "KANA"},
                                     {113, "F19"},
                                     {121, "CONVERT"},
                                     {123, "NOCONVERT"},
                                     {125, "YEN"},
                                     {141, "NUMPADEQUALS"},
                                     {144, "CIRCUMFLEX"},
                                     {145, "AT"},
                                     {146, "COLON"},
                                     {147, "UNDERLINE"},
                                     {148, "KANJI"},
                                     {149, "STOP"},
                                     {150, "AX"},
                                     {151, "UNLABELED"},
                                     {156, "NUMPADENTER"},
                                     {157, "RCONTROL"},
                                     {167, "SECTION"},
                                     {179, "NUMPADCOMMA"},
                                     {181, "DIVIDE"},
                                     {183, "SYSRQ"},
                                     {184, "RMENU"},
                                     {196, "FUNCTION"},
                                     {197, "PAUSE"},
                                     {199, "HOME"},
                                     {200, "UP"},
                                     {201, "PRIOR"},
                                     {203, "LEFT"},
                                     {205, "RIGHT"},
                                     {207, "END"},
                                     {208, "DOWN"},
                                     {209, "NEXT"},
                                     {210, "INSERT"},
                                     {211, "DELETE"},
                                     {218, "CLEAR"},
                                     {219, "LMETA"},
                                     {220, "RMETA"},
                                     {221, "APPS"},
                                     {222, "POWER"},
                                     {223, "SLEEP"}};

static int metadata(void) {
    int result = 1;
    mc_game_option_info info;
    const mc_game_option *seen[MC_GAME_OPTION_COUNT];
    for (size_t i = 0; i < COUNT(expected_options); ++i) {
        seen[i] = mc_game_option_by_id((int32_t)i);
        CHECK(seen[i] != NULL && mc_game_option_read(seen[i], &info));
        CHECK(info.id == expected_options[i].id && info.id == (mc_game_option_id)i);
        CHECK(strcmp(info.name, expected_options[i].name) == 0 &&
              strcmp(info.translation_key, expected_options[i].translation_key) == 0);
        CHECK(info.is_float == expected_options[i].is_float &&
              info.is_boolean == expected_options[i].is_boolean);
        CHECK(mc_game_option_by_id((int32_t)i) == seen[i]);
        for (size_t j = 0; j < i; ++j)
            CHECK(seen[j] != seen[i]);
    }
    const int32_t invalid[] = {-1, 13, INT32_MIN, INT32_MAX};
    for (size_t i = 0; i < COUNT(invalid); ++i)
        CHECK(mc_game_option_by_id(invalid[i]) == NULL);
    info = expected_options[0];
    CHECK(!mc_game_option_read(NULL, &info) && info.id == MC_GAME_OPTION_MUSIC && info.is_float);

    union {
        void *pointer;
        uint64_t word;
        long double scalar;
    } foreign = {0};

    CHECK(!mc_game_option_read((const mc_game_option *)&foreign, &info));
    CHECK(!mc_game_option_read(seen[0], NULL));

    static const struct {
        mc_game_option_id id;
        size_t count;
        const char *keys[4];
    } tables[] = {{MC_GAME_OPTION_RENDER_DISTANCE,
                   4,
                   {"options.renderDistance.far", "options.renderDistance.normal",
                    "options.renderDistance.short", "options.renderDistance.tiny"}},
                  {MC_GAME_OPTION_DIFFICULTY,
                   4,
                   {"options.difficulty.peaceful", "options.difficulty.easy",
                    "options.difficulty.normal", "options.difficulty.hard"}},
                  {MC_GAME_OPTION_GUI_SCALE,
                   4,
                   {"options.guiScale.auto", "options.guiScale.small", "options.guiScale.normal",
                    "options.guiScale.large"}},
                  {MC_GAME_OPTION_FRAMERATE_LIMIT,
                   3,
                   {"performance.max", "performance.balanced", "performance.powersaver", NULL}}};

    const char *text = "unchanged";
    for (size_t i = 0; i < COUNT(tables); ++i) {
        const mc_game_option *option = mc_game_option_by_id((int32_t)tables[i].id);
        for (size_t j = 0; j < tables[i].count; ++j) {
            CHECK(mc_game_option_value_key(option, (int32_t)j, &text) == MC_GAME_OPTION_OK);
            CHECK(strcmp(text, tables[i].keys[j]) == 0);
        }
        const char *prior = text;
        CHECK(mc_game_option_value_key(option, -1, &text) ==
                  MC_GAME_OPTION_SOURCE_INDEX_OUT_OF_BOUNDS &&
              text == prior);
        CHECK(mc_game_option_value_key(option, (int32_t)tables[i].count, &text) ==
                  MC_GAME_OPTION_SOURCE_INDEX_OUT_OF_BOUNDS &&
              text == prior);
        CHECK(mc_game_option_value_key(option, INT32_MAX, &text) ==
                  MC_GAME_OPTION_SOURCE_INDEX_OUT_OF_BOUNDS &&
              text == prior);
    }
    const char *prior = text;
    CHECK(mc_game_option_value_key(mc_game_option_by_id(0), 0, &text) ==
              MC_GAME_OPTION_INVALID_ARGUMENT &&
          text == prior);
    CHECK(mc_game_option_value_key(NULL, 0, &text) == MC_GAME_OPTION_SOURCE_NULL_OPTION &&
          text == prior);
    CHECK(mc_game_option_value_key(mc_game_option_by_id(4), 0, NULL) ==
          MC_GAME_OPTION_INVALID_ARGUMENT);
    result = 0;
cleanup:
    return result;
}

static int queries(void) {
    int result = 1;
    mc_game_settings *settings = NULL;
    CHECK(mc_game_settings_create_defaults(NULL, &settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state state;
    CHECK(mc_game_settings_read_state(settings, &state));
    mc_game_settings_values values = state.values;
    values.music_volume = from_bits(UINT32_C(0x80000000));
    values.sound_volume = from_bits(UINT32_C(0x7fc01234));
    values.mouse_sensitivity = -2.5f;
    values.fancy_graphics = true;
    CHECK(mc_game_settings_set_values(settings, &values, NULL) == MC_GAME_SETTINGS_OK);
    const uint32_t expected[] = {UINT32_C(0x80000000),
                                 UINT32_C(0x7fc01234),
                                 0,
                                 UINT32_C(0xc0200000),
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0,
                                 0};
    for (size_t i = 0; i < COUNT(expected); ++i) {
        float value = 17.0f;
        CHECK(mc_game_settings_option_float(settings, mc_game_option_by_id((int32_t)i), &value) ==
              MC_GAME_OPTION_OK);
        CHECK(bits(value) == expected[i]);
    }
    float value = -1.0f;
    CHECK(mc_game_settings_option_float(settings, NULL, &value) == MC_GAME_OPTION_OK &&
          bits(value) == 0);
    const mc_game_option_id boolean_ids[] = {
        MC_GAME_OPTION_INVERT_MOUSE, MC_GAME_OPTION_VIEW_BOBBING, MC_GAME_OPTION_ANAGLYPH,
        MC_GAME_OPTION_ADVANCED_OPENGL, MC_GAME_OPTION_AMBIENT_OCCLUSION};
    for (size_t active = 0; active < COUNT(boolean_ids); ++active) {
        values.invert_mouse = active == 0;
        values.view_bobbing = active == 1;
        values.anaglyph = active == 2;
        values.advanced_opengl = active == 3;
        values.ambient_occlusion = active == 4;
        CHECK(mc_game_settings_set_values(settings, &values, NULL) == MC_GAME_SETTINGS_OK);
        for (int32_t id = 0; id < (int32_t)MC_GAME_OPTION_COUNT; ++id) {
            bool actual = true;
            CHECK(mc_game_settings_option_boolean(settings, mc_game_option_by_id(id), &actual) ==
                  MC_GAME_OPTION_OK);
            CHECK(actual == (id == (int32_t)boolean_ids[active]));
        }
    }
    bool boolean = true;
    CHECK(mc_game_settings_option_boolean(settings, NULL, &boolean) ==
              MC_GAME_OPTION_SOURCE_NULL_OPTION &&
          boolean);
    CHECK(mc_game_settings_option_float(NULL, mc_game_option_by_id(0), &value) ==
              MC_GAME_OPTION_INVALID_ARGUMENT &&
          bits(value) == 0);
    CHECK(mc_game_settings_option_boolean(NULL, mc_game_option_by_id(0), &boolean) ==
              MC_GAME_OPTION_INVALID_ARGUMENT &&
          boolean);
    CHECK(mc_game_settings_option_float(settings, mc_game_option_by_id(0), NULL) ==
          MC_GAME_OPTION_INVALID_ARGUMENT);
    CHECK(mc_game_settings_option_boolean(settings, mc_game_option_by_id(0), NULL) ==
          MC_GAME_OPTION_INVALID_ARGUMENT);
    result = 0;
cleanup:
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        result = 1;
    return result;
}

static int key_names(void) {
    int result = 1;
    size_t populated = 0, holes = 0;
    for (int32_t code = 0; code < 256; ++code) {
        const char *expected = NULL;
        for (size_t i = 0; i < COUNT(key_facts); ++i)
            if (key_facts[i].code == code)
                expected = key_facts[i].name;
        const char *actual = "unchanged";
        CHECK(mc_game_key_name(code, &actual) == MC_GAME_OPTION_OK);
        if (expected == NULL) {
            CHECK(actual == NULL);
            ++holes;
        } else {
            CHECK(actual != NULL && strcmp(actual, expected) == 0);
            ++populated;
        }
    }
    CHECK(populated == 131 && holes == 125);
    const int32_t invalid[] = {-1, 256, INT32_MIN, INT32_MAX};
    const char *canary = "unchanged", *text = canary;
    for (size_t i = 0; i < COUNT(invalid); ++i)
        CHECK(mc_game_key_name(invalid[i], &text) == MC_GAME_OPTION_SOURCE_INDEX_OUT_OF_BOUNDS &&
              text == canary);
    CHECK(mc_game_key_name(0, NULL) == MC_GAME_OPTION_INVALID_ARGUMENT);
    result = 0;
cleanup:
    return result;
}

int main(void) {
    if (metadata() != 0 || queries() != 0 || key_names() != 0)
        return 1;
    printf("PASS: %u GameOption metadata/query checks\n", checks);
    return 0;
}
