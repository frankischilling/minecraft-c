#include "mc/client/settings/game_option.h"

#include <string.h>

struct mc_game_option {
    mc_game_option_info info;
};

static const mc_game_option options[MC_GAME_OPTION_COUNT] = {
    {{MC_GAME_OPTION_MUSIC, "MUSIC", "options.music", true, false}},
    {{MC_GAME_OPTION_SOUND, "SOUND", "options.sound", true, false}},
    {{MC_GAME_OPTION_INVERT_MOUSE, "INVERT_MOUSE", "options.invertMouse", false, true}},
    {{MC_GAME_OPTION_SENSITIVITY, "SENSITIVITY", "options.sensitivity", true, false}},
    {{MC_GAME_OPTION_RENDER_DISTANCE, "RENDER_DISTANCE", "options.renderDistance", false, false}},
    {{MC_GAME_OPTION_VIEW_BOBBING, "VIEW_BOBBING", "options.viewBobbing", false, true}},
    {{MC_GAME_OPTION_ANAGLYPH, "ANAGLYPH", "options.anaglyph", false, true}},
    {{MC_GAME_OPTION_ADVANCED_OPENGL, "ADVANCED_OPENGL", "options.advancedOpengl", false, true}},
    {{MC_GAME_OPTION_FRAMERATE_LIMIT, "FRAMERATE_LIMIT", "options.framerateLimit", false, false}},
    {{MC_GAME_OPTION_DIFFICULTY, "DIFFICULTY", "options.difficulty", false, false}},
    {{MC_GAME_OPTION_GRAPHICS, "GRAPHICS", "options.graphics", false, false}},
    {{MC_GAME_OPTION_AMBIENT_OCCLUSION, "AMBIENT_OCCLUSION", "options.ao", false, true}},
    {{MC_GAME_OPTION_GUI_SCALE, "GUI_SCALE", "options.guiScale", false, false}}};

/* Source keys/facts, not translated resource contents. These are the four
 * static tables initialized by kv.<clinit> at PCs24/51/78/100. */
static const char *const render_distance_keys[] = {
    "options.renderDistance.far", "options.renderDistance.normal", "options.renderDistance.short",
    "options.renderDistance.tiny"};
static const char *const difficulty_keys[] = {
    "options.difficulty.peaceful", "options.difficulty.easy", "options.difficulty.normal",
    "options.difficulty.hard"};
static const char *const gui_scale_keys[] = {"options.guiScale.auto", "options.guiScale.small",
                                             "options.guiScale.normal", "options.guiScale.large"};
static const char *const performance_keys[] = {"performance.max", "performance.balanced",
                                               "performance.powersaver"};

/* KEY_ API ConstantValue/name facts from retained LWJGL2.9.0 class bytes.
 * KEY_LWIN/KEY_RWIN are excluded by its endsWith("WIN") predicate. */
static const char *const key_names[256] = {[0] = "NONE",
                                           [1] = "ESCAPE",
                                           [2] = "1",
                                           [3] = "2",
                                           [4] = "3",
                                           [5] = "4",
                                           [6] = "5",
                                           [7] = "6",
                                           [8] = "7",
                                           [9] = "8",
                                           [10] = "9",
                                           [11] = "0",
                                           [12] = "MINUS",
                                           [13] = "EQUALS",
                                           [14] = "BACK",
                                           [15] = "TAB",
                                           [16] = "Q",
                                           [17] = "W",
                                           [18] = "E",
                                           [19] = "R",
                                           [20] = "T",
                                           [21] = "Y",
                                           [22] = "U",
                                           [23] = "I",
                                           [24] = "O",
                                           [25] = "P",
                                           [26] = "LBRACKET",
                                           [27] = "RBRACKET",
                                           [28] = "RETURN",
                                           [29] = "LCONTROL",
                                           [30] = "A",
                                           [31] = "S",
                                           [32] = "D",
                                           [33] = "F",
                                           [34] = "G",
                                           [35] = "H",
                                           [36] = "J",
                                           [37] = "K",
                                           [38] = "L",
                                           [39] = "SEMICOLON",
                                           [40] = "APOSTROPHE",
                                           [41] = "GRAVE",
                                           [42] = "LSHIFT",
                                           [43] = "BACKSLASH",
                                           [44] = "Z",
                                           [45] = "X",
                                           [46] = "C",
                                           [47] = "V",
                                           [48] = "B",
                                           [49] = "N",
                                           [50] = "M",
                                           [51] = "COMMA",
                                           [52] = "PERIOD",
                                           [53] = "SLASH",
                                           [54] = "RSHIFT",
                                           [55] = "MULTIPLY",
                                           [56] = "LMENU",
                                           [57] = "SPACE",
                                           [58] = "CAPITAL",
                                           [59] = "F1",
                                           [60] = "F2",
                                           [61] = "F3",
                                           [62] = "F4",
                                           [63] = "F5",
                                           [64] = "F6",
                                           [65] = "F7",
                                           [66] = "F8",
                                           [67] = "F9",
                                           [68] = "F10",
                                           [69] = "NUMLOCK",
                                           [70] = "SCROLL",
                                           [71] = "NUMPAD7",
                                           [72] = "NUMPAD8",
                                           [73] = "NUMPAD9",
                                           [74] = "SUBTRACT",
                                           [75] = "NUMPAD4",
                                           [76] = "NUMPAD5",
                                           [77] = "NUMPAD6",
                                           [78] = "ADD",
                                           [79] = "NUMPAD1",
                                           [80] = "NUMPAD2",
                                           [81] = "NUMPAD3",
                                           [82] = "NUMPAD0",
                                           [83] = "DECIMAL",
                                           [87] = "F11",
                                           [88] = "F12",
                                           [100] = "F13",
                                           [101] = "F14",
                                           [102] = "F15",
                                           [103] = "F16",
                                           [104] = "F17",
                                           [105] = "F18",
                                           [112] = "KANA",
                                           [113] = "F19",
                                           [121] = "CONVERT",
                                           [123] = "NOCONVERT",
                                           [125] = "YEN",
                                           [141] = "NUMPADEQUALS",
                                           [144] = "CIRCUMFLEX",
                                           [145] = "AT",
                                           [146] = "COLON",
                                           [147] = "UNDERLINE",
                                           [148] = "KANJI",
                                           [149] = "STOP",
                                           [150] = "AX",
                                           [151] = "UNLABELED",
                                           [156] = "NUMPADENTER",
                                           [157] = "RCONTROL",
                                           [167] = "SECTION",
                                           [179] = "NUMPADCOMMA",
                                           [181] = "DIVIDE",
                                           [183] = "SYSRQ",
                                           [184] = "RMENU",
                                           [196] = "FUNCTION",
                                           [197] = "PAUSE",
                                           [199] = "HOME",
                                           [200] = "UP",
                                           [201] = "PRIOR",
                                           [203] = "LEFT",
                                           [205] = "RIGHT",
                                           [207] = "END",
                                           [208] = "DOWN",
                                           [209] = "NEXT",
                                           [210] = "INSERT",
                                           [211] = "DELETE",
                                           [218] = "CLEAR",
                                           [219] = "LMETA",
                                           [220] = "RMETA",
                                           [221] = "APPS",
                                           [222] = "POWER",
                                           [223] = "SLEEP"};

const mc_game_option *mc_game_option_by_id(int32_t id) {
    if (id < 0 || id >= (int32_t)MC_GAME_OPTION_COUNT)
        return NULL;
    return &options[(size_t)id];
}

bool mc_game_option_read(const mc_game_option *option, mc_game_option_info *out) {
    if (out == NULL)
        return false;
    for (size_t i = 0; i < (size_t)MC_GAME_OPTION_COUNT; ++i) {
        if (option == &options[i]) {
            *out = options[i].info;
            return true;
        }
    }
    return false;
}

mc_game_option_status mc_game_settings_option_float(const mc_game_settings *settings,
                                                    const mc_game_option *option, float *out) {
    mc_game_option_info info = {0};
    mc_game_settings_state state;
    if (out == NULL || (option != NULL && !mc_game_option_read(option, &info)) ||
        !mc_game_settings_read_state(settings, &state))
        return MC_GAME_OPTION_INVALID_ARGUMENT;
    const float *value = NULL;
    if (option != NULL) {
        switch (info.id) {
            case MC_GAME_OPTION_MUSIC:
                value = &state.values.music_volume;
                break;
            case MC_GAME_OPTION_SOUND:
                value = &state.values.sound_volume;
                break;
            case MC_GAME_OPTION_SENSITIVITY:
                value = &state.values.mouse_sensitivity;
                break;
            default:
                break;
        }
    }
    if (value != NULL)
        memcpy(out, value, sizeof(*out));
    else
        *out = 0.0f;
    return MC_GAME_OPTION_OK;
}

mc_game_option_status mc_game_settings_option_boolean(const mc_game_settings *settings,
                                                      const mc_game_option *option, bool *out) {
    mc_game_option_info info = {0};
    mc_game_settings_state state;
    if (settings == NULL || out == NULL || (option != NULL && !mc_game_option_read(option, &info)))
        return MC_GAME_OPTION_INVALID_ARGUMENT;
    if (option == NULL)
        return MC_GAME_OPTION_SOURCE_NULL_OPTION;
    if (!mc_game_settings_read_state(settings, &state))
        return MC_GAME_OPTION_INVALID_ARGUMENT;
    bool value = false;
    switch (info.id) {
        case MC_GAME_OPTION_INVERT_MOUSE:
            value = state.values.invert_mouse;
            break;
        case MC_GAME_OPTION_VIEW_BOBBING:
            value = state.values.view_bobbing;
            break;
        case MC_GAME_OPTION_ANAGLYPH:
            value = state.values.anaglyph;
            break;
        case MC_GAME_OPTION_ADVANCED_OPENGL:
            value = state.values.advanced_opengl;
            break;
        case MC_GAME_OPTION_AMBIENT_OCCLUSION:
            value = state.values.ambient_occlusion;
            break;
        default:
            break;
    }
    *out = value;
    return MC_GAME_OPTION_OK;
}

mc_game_option_status mc_game_option_value_key(const mc_game_option *option, int32_t index,
                                               const char **out) {
    mc_game_option_info info = {0};
    if (out == NULL || (option != NULL && !mc_game_option_read(option, &info)))
        return MC_GAME_OPTION_INVALID_ARGUMENT;
    if (option == NULL)
        return MC_GAME_OPTION_SOURCE_NULL_OPTION;
    const char *const *keys = NULL;
    size_t length = 0;
    switch (info.id) {
        case MC_GAME_OPTION_RENDER_DISTANCE:
            keys = render_distance_keys;
            length = sizeof(render_distance_keys) / sizeof(render_distance_keys[0]);
            break;
        case MC_GAME_OPTION_DIFFICULTY:
            keys = difficulty_keys;
            length = sizeof(difficulty_keys) / sizeof(difficulty_keys[0]);
            break;
        case MC_GAME_OPTION_GUI_SCALE:
            keys = gui_scale_keys;
            length = sizeof(gui_scale_keys) / sizeof(gui_scale_keys[0]);
            break;
        case MC_GAME_OPTION_FRAMERATE_LIMIT:
            keys = performance_keys;
            length = sizeof(performance_keys) / sizeof(performance_keys[0]);
            break;
        default:
            return MC_GAME_OPTION_INVALID_ARGUMENT;
    }
    if (index < 0 || (size_t)index >= length)
        return MC_GAME_OPTION_SOURCE_INDEX_OUT_OF_BOUNDS;
    *out = keys[(size_t)index];
    return MC_GAME_OPTION_OK;
}

mc_game_option_status mc_game_key_name(int32_t code, const char **out) {
    if (out == NULL)
        return MC_GAME_OPTION_INVALID_ARGUMENT;
    if (code < 0 || code >= 256)
        return MC_GAME_OPTION_SOURCE_INDEX_OUT_OF_BOUNDS;
    *out = key_names[(size_t)code];
    return MC_GAME_OPTION_OK;
}
