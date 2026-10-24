#include "mc/client/settings/game_settings.h"

#include <math.h>
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
#define CHECK(test)                                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(test)) {                                                                             \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test);                             \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const mc_game_settings_profile utf8_lf = {MC_JAVA_READER_UTF8, MC_JAVA_WRITER_LF};
static const char *root;

static bool path_for(char *out, size_t capacity, const char *directory, bool options) {
    const int count =
        snprintf(out, capacity, "%s/%s%s", root, directory, options ? "/options.txt" : "");
    return count >= 0 && (size_t)count < capacity;
}

static FILE *file_open(const char *path, bool writing) {
    FILE *file = NULL;
#ifdef _WIN32
    wchar_t native[4096];
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, native,
                                          (int)(sizeof(native) / sizeof(native[0])));
    if (count == 0)
        return NULL;
    if (_wfopen_s(&file, native, writing ? L"wb" : L"rb") != 0)
        return NULL;
#else
    file = fopen(path, writing ? "wb" : "rb");
#endif
    return file;
}

static bool input(const char *directory, const uint8_t *bytes, size_t count) {
    char path[4096];
    if (!path_for(path, sizeof(path), directory, true))
        return false;
    FILE *file = file_open(path, true);
    if (file == NULL)
        return false;
    const bool okay = fwrite(bytes, 1, count, file) == count && !ferror(file);
    return fclose(file) == 0 && okay;
}

static bool text_input(const char *directory, const char *text) {
    return input(directory, (const uint8_t *)text, strlen(text));
}

static bool output(const char *directory, uint8_t *bytes, size_t capacity, size_t *count) {
    char path[4096];
    if (!path_for(path, sizeof(path), directory, true))
        return false;
    FILE *file = file_open(path, false);
    if (file == NULL)
        return false;
    const size_t read = fread(bytes, 1, capacity, file);
    const bool okay = !ferror(file) && fgetc(file) == EOF && !ferror(file);
    const bool closed = fclose(file) == 0;
    if (!okay || !closed)
        return false;
    *count = read;
    return true;
}

static bool ascii(mc_game_settings_text text, const char *expected) {
    if (text.is_null || text.length != strlen(expected))
        return false;
    for (size_t i = 0; i < text.length; ++i)
        if (text.units[i] != (uint8_t)expected[i])
            return false;
    return true;
}

static bool code_is(mc_key_binding *binding, int32_t expected) {
    mc_game_settings_text name = {0};
    int32_t code = 0;
    return mc_key_binding_read(binding, &name, &code) && code == expected;
}

static mc_game_settings_status create(const char *directory,
                                      const mc_game_settings_profile *profile,
                                      mc_game_settings **settings,
                                      mc_game_settings_report *report) {
    char path[4096];
    if (!path_for(path, sizeof(path), directory, false))
        return MC_GAME_SETTINGS_INVALID_ARGUMENT;
    return mc_game_settings_create_file(NULL, path, profile, NULL, settings, report, NULL);
}

static bool defaults(void) {
    bool okay = false;
    mc_game_settings *first = NULL, *second = NULL;
    mc_game_settings_state state = {0}, other = {0};
    CHECK(mc_game_settings_create_defaults(&utf8_lf, &first, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_create_defaults(&utf8_lf, &second, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(first != second && mc_game_settings_read_state(first, &state) &&
          mc_game_settings_read_state(second, &other));
    CHECK(state.values.music_volume == 1.0f && state.values.sound_volume == 1.0f &&
          state.values.mouse_sensitivity == 0.5f);
    CHECK(!state.values.invert_mouse && state.values.render_distance == 0 &&
          state.values.view_bobbing && !state.values.anaglyph && !state.values.advanced_opengl);
    CHECK(state.values.performance_mode == 1 && state.values.fancy_graphics &&
          state.values.ambient_occlusion && state.values.difficulty == 2 &&
          state.values.gui_scale == 0);
    CHECK(!state.values.hide_gui && !state.values.third_person_view &&
          !state.values.show_debug_info && !state.values.source_D && !state.values.smooth_camera &&
          !state.values.debug_camera && state.values.source_G == 1.0f &&
          state.values.source_H == 1.0f);
    CHECK(ascii(state.values.texture_pack, "Default") && ascii(state.values.last_server, ""));
    CHECK(state.values.texture_pack.units == other.values.texture_pack.units &&
          state.values.last_server.units == other.values.last_server.units);
    const uint16_t *default_identity = state.values.texture_pack.units;
    CHECK(mc_game_settings_set_values(first, &state.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_read_state(first, &state) &&
          state.values.texture_pack.units == default_identity);
    CHECK(state.minecraft == NULL && state.options_path == NULL && state.file_identity == NULL);
    CHECK(!state.bindings_null && state.binding_count == 10u && state.bindings != other.bindings);
    static const size_t array_order[] = {0, 1, 2, 3, 4, 9, 6, 5, 7, 8};
    static const int32_t codes[] = {17, 30, 31, 32, 57, 18, 16, 20, 33, 42};
    static const char *const names[] = {"key.forward", "key.left",      "key.back", "key.right",
                                        "key.jump",    "key.inventory", "key.drop", "key.chat",
                                        "key.fog",     "key.sneak"};
    for (size_t i = 0; i < 10u; ++i) {
        mc_game_settings_text name = {0}, other_name = {0};
        int32_t code = 0, other_code = 0;
        CHECK(state.named[i] != other.named[i] && state.bindings[i] == state.named[array_order[i]]);
        CHECK(mc_key_binding_read(state.named[i], &name, &code) &&
              mc_key_binding_read(other.named[i], &other_name, &other_code));
        CHECK(ascii(name, names[i]) && code == codes[i] && name.units == other_name.units);
        for (size_t j = 0; j < i; ++j)
            CHECK(state.named[i] != state.named[j]);
    }
    mc_game_settings_report report = {0};
    CHECK(mc_game_settings_load(first, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.outer_exception_caught &&
          report.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_FILE && !report.source_opened &&
          !report.source_close_reached);
    CHECK(mc_game_settings_save(first, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_FILE &&
          !report.writer_state_present);
    CHECK(!mc_game_settings_read_state(NULL, &state) && !mc_key_binding_read(NULL, NULL, NULL));
    CHECK(mc_game_settings_set_named_binding(first, 10u, NULL, NULL) ==
          MC_GAME_SETTINGS_INVALID_ARGUMENT);
    CHECK(mc_game_settings_set_named_binding(first, 0u, other.named[0], NULL) ==
          MC_GAME_SETTINGS_INVALID_ARGUMENT);
    CHECK(mc_game_settings_set_binding_array(first, other.bindings, 10u, false, NULL) ==
          MC_GAME_SETTINGS_INVALID_ARGUMENT);
    CHECK(mc_game_settings_set_binding_array(first, NULL, 1025u, false, NULL) !=
          MC_GAME_SETTINGS_OK);
    mc_game_settings *unchanged = first;
    const mc_game_settings_profile invalid = {(mc_java_reader_charset)99, MC_JAVA_WRITER_LF};
    CHECK(mc_game_settings_create_defaults(&invalid, &unchanged, NULL) ==
              MC_GAME_SETTINGS_INVALID_ARGUMENT &&
          unchanged == first);
    okay = true;
cleanup:
    if (mc_game_settings_retire(&first, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_retire(&second, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

static bool ordinary(void) {
    bool okay = false;
    mc_game_settings *settings = NULL, *fresh = NULL;
    mc_game_settings_report report = {0};
    mc_game_settings_state state = {0}, original = {0};
    CHECK(create("ordinary", &utf8_lf, &settings, &report) == MC_GAME_SETTINGS_OK &&
          report.file_absent);
    CHECK(mc_game_settings_read_state(settings, &original) && original.file_identity != NULL);
    static const char authored[] =
        "music:0.25\r\nsound:0.75\nmouseSensitivity:0.125\rinvertYMouse:true\n"
        "viewDistance:-2147483648\nguiScale:+2147483647\nbobView:false\nanaglyph3d:true\n"
        "advancedOpengl:true\nfpsLimit:-8\ndifficulty:99\nfancyGraphics:false\nao:false\n"
        "skin:AuthoredPack\nlastServer:host:25565\nkey_key.forward:32\nkey_key.left:31\n"
        "key_key.back:30\nkey_key.right:17\nkey_key.jump:0\nkey_key.sneak:-42\n"
        "key_key.drop:18\nkey_key.inventory:16\nkey_key.chat:33\nkey_key.fog:20";
    CHECK(text_input("ordinary", authored));
    CHECK(mc_game_settings_load(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.lines_read == 25u && report.bad_options == 0u && report.source_close_reached &&
          !report.native_additional_close && !report.outer_exception_caught);
    CHECK(mc_game_settings_read_state(settings, &state));
    CHECK(state.file_identity == original.file_identity && state.bindings == original.bindings);
    CHECK(state.values.music_volume == .25f && state.values.sound_volume == .75f &&
          state.values.mouse_sensitivity == .125f && state.values.invert_mouse);
    CHECK(state.values.render_distance == INT32_MIN && state.values.gui_scale == INT32_MAX &&
          state.values.performance_mode == -8 && state.values.difficulty == 99);
    CHECK(!state.values.view_bobbing && state.values.anaglyph && state.values.advanced_opengl &&
          !state.values.fancy_graphics && !state.values.ambient_occlusion);
    CHECK(ascii(state.values.texture_pack, "AuthoredPack") &&
          ascii(state.values.last_server, "host"));
    static const int32_t expected_codes[] = {32, 31, 30, 17, 0, 16, 18, 33, 20, -42};
    for (size_t i = 0; i < 10u; ++i)
        CHECK(state.named[i] == original.named[i] && code_is(state.named[i], expected_codes[i]));
    CHECK(mc_game_settings_save(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.println_attempts == 25u && report.source_close_reached &&
          report.writer_state_present && report.writer_state.encoder_closed &&
          !report.writer_state.print_writer_out_present && !report.writer_state.trouble &&
          !report.native_additional_close);
    static const char expected[] =
        "music:0.25\nsound:0.75\ninvertYMouse:true\nmouseSensitivity:0.125\n"
        "viewDistance:-2147483648\nguiScale:2147483647\nbobView:false\nanaglyph3d:true\n"
        "advancedOpengl:true\nfpsLimit:-8\ndifficulty:99\nfancyGraphics:false\nao:false\n"
        "skin:AuthoredPack\nlastServer:host\nkey_key.forward:32\nkey_key.left:31\n"
        "key_key.back:30\nkey_key.right:17\nkey_key.jump:0\nkey_key.sneak:-42\n"
        "key_key.drop:18\nkey_key.inventory:16\nkey_key.chat:33\nkey_key.fog:20\n";
    uint8_t bytes[4096];
    size_t count = 0;
    CHECK(output("ordinary", bytes, sizeof(bytes), &count) && count == sizeof(expected) - 1u &&
          memcmp(bytes, expected, count) == 0);
    CHECK(create("ordinary", &utf8_lf, &fresh, &report) == MC_GAME_SETTINGS_OK &&
          report.lines_read == 25u && report.bad_options == 0u);
    mc_game_settings_state reloaded = {0};
    CHECK(mc_game_settings_read_state(fresh, &reloaded) &&
          reloaded.file_identity != state.file_identity && reloaded.bindings != state.bindings &&
          reloaded.values.gui_scale == INT32_MAX && reloaded.values.render_distance == INT32_MIN);
    okay = true;
cleanup:
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_retire(&fresh, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

typedef struct diagnostic_log {
    mc_game_settings *settings;
    size_t count, bad, failed;
    bool reentry_rejected, retire_rejected, inspected, fail;
    mc_game_settings_source_exception last;
    uint16_t last_line[128];
    size_t length;
} diagnostic_log;

static mc_game_settings_status diagnostic(void *context, const mc_game_settings_diagnostic *event,
                                          mc_game_settings_error *error) {
    diagnostic_log *log = context;
    ++log->count;
    log->last = event->exception;
    if (event->kind == MC_GAME_SETTINGS_BAD_OPTION) {
        ++log->bad;
        if (strcmp(event->message, "Skipping bad option: ") != 0 || event->line.is_null ||
            event->line.length > sizeof(log->last_line) / sizeof(log->last_line[0]))
            return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
        log->length = event->line.length;
        if (log->length != 0)
            memcpy(log->last_line, event->line.units, log->length * sizeof(uint16_t));
    } else {
        ++log->failed;
    }
    if (log->settings != NULL) {
        mc_game_settings_state state = {0};
        log->inspected = mc_game_settings_read_state(log->settings, &state);
        log->reentry_rejected =
            mc_game_settings_save(log->settings, NULL, NULL, NULL) == MC_GAME_SETTINGS_BUSY;
        mc_game_settings *alias = log->settings;
        log->retire_rejected = mc_game_settings_retire(&alias, NULL) == MC_GAME_SETTINGS_BUSY &&
                               alias == log->settings;
    }
    if (log->fail) {
        if (error != NULL)
            *error = (mc_game_settings_error){MC_GAME_SETTINGS_DIAGNOSTIC_FAILED, 71,
                                              "authored diagnostic refusal"};
        return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
    }
    return MC_GAME_SETTINGS_OK;
}

static bool lexical_and_partial(void) {
    bool okay = false;
    mc_game_settings *settings = NULL, *fresh = NULL;
    mc_game_settings_report report = {0};
    CHECK(create("lexical", &utf8_lf, &settings, &report) == MC_GAME_SETTINGS_OK);
    diagnostic_log log = {.settings = settings};
    const mc_game_settings_diagnostics observer = {&log, diagnostic};
    CHECK(text_input(
        "lexical",
        "music:0.25\nmusic:bad\nsound:true\nmouseSensitivity:false\n"
        "invertYMouse:True\nbobView:FALSE\nanaglyph3d:1\nadvancedOpengl: true\n"
        "fancyGraphics:unknown\nao:false\nviewDistance:2147483648\nguiScale: 2\n"
        "fpsLimit:-2147483648\ndifficulty:+2147483647\n"
        "skin:Before\nskin::extra\nskin:\nlastServer:before\nlastServer:\n"
        "lastServer:host:port\nmusic:\nsound::extra\n:\n::\n\nunknown\n# no comment\n"
        "music :0.8\nMUSIC:0.9\nkey_forward:9\nkey_key.forward:44\nkey_key.forward:wrong\n"));
    CHECK(mc_game_settings_load(settings, &observer, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.lines_read == 32u && report.bad_options == 9u && !report.outer_exception_caught &&
          report.source_close_reached);
    CHECK(log.bad == 9u && log.failed == 0u && log.reentry_rejected && log.retire_rejected &&
          log.inspected);
    mc_game_settings_state state = {0};
    CHECK(mc_game_settings_read_state(settings, &state));
    CHECK(state.values.music_volume == .25f && state.values.sound_volume == 1.0f &&
          state.values.mouse_sensitivity == 0.0f && !state.values.invert_mouse &&
          !state.values.view_bobbing && !state.values.anaglyph && !state.values.advanced_opengl &&
          !state.values.fancy_graphics && !state.values.ambient_occlusion);
    CHECK(state.values.render_distance == 0 && state.values.gui_scale == 0 &&
          state.values.performance_mode == INT32_MIN && state.values.difficulty == INT32_MAX);
    CHECK(ascii(state.values.texture_pack, "") && ascii(state.values.last_server, "host") &&
          code_is(state.named[0], 44));
    const uint16_t *old_server = state.values.last_server.units;
    CHECK(mc_game_settings_save(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(create("lexical", &utf8_lf, &fresh, &report) == MC_GAME_SETTINGS_OK &&
          report.bad_options == 1u);
    mc_game_settings_state other = {0};
    CHECK(mc_game_settings_read_state(fresh, &other) &&
          ascii(other.values.texture_pack, "Default"));
    CHECK(text_input("lexical", "music:-0.0\nsound:NaN\nmouseSensitivity:Infinity\n"));
    CHECK(mc_game_settings_load(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_read_state(settings, &state) && signbit(state.values.music_volume) &&
          isnan(state.values.sound_volume) && isinf(state.values.mouse_sensitivity));
    CHECK(text_input("lexical",
                     "music:-Infinity\nsound:0x1.0p-149f\nmouseSensitivity:1e999\n"
                     "viewDistance:\xd9\xa1\nguiScale:\xef\xbc\x92\nfpsLimit:\xe0\xa5\xa8\n"
                     "key_key.forward:+\xd9\xa1\xd9\xa7\nlastServer:new\n"));
    CHECK(mc_game_settings_load(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.bad_options == 0u);
    CHECK(mc_game_settings_read_state(settings, &state) && isinf(state.values.music_volume) &&
          signbit(state.values.music_volume) && isinf(state.values.mouse_sensitivity));
    uint32_t sound_bits = 0;
    memcpy(&sound_bits, &state.values.sound_volume, sizeof(sound_bits));
    CHECK(sound_bits == 1u && state.values.render_distance == 1 && state.values.gui_scale == 2 &&
          state.values.performance_mode == 2 && code_is(state.named[0], 17));
    CHECK(old_server[0] == 'h' && old_server[3] == 't'); /* Old String view remains live. */
    CHECK(text_input("lexical", "music:0.5\nsound:bad\nmouseSensitivity:0.75\n"));
    log.fail = true;
    mc_game_settings_error error = {0};
    CHECK(mc_game_settings_load(settings, &observer, &report, &error) ==
              MC_GAME_SETTINGS_DIAGNOSTIC_FAILED &&
          report.lines_read == 2u && report.bad_options == 1u && !report.source_close_reached &&
          report.native_additional_close && !report.native_cleanup_failed &&
          error.system_error == 71);
    CHECK(mc_game_settings_read_state(settings, &state) && state.values.music_volume == .5f &&
          isinf(state.values.mouse_sensitivity));
    okay = true;
cleanup:
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_retire(&fresh, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

static bool aliases(void) {
    bool okay = false;
    mc_game_settings *settings = NULL;
    mc_game_settings_report report = {0};
    mc_game_settings_state original = {0}, state = {0};
    CHECK(create("aliases", &utf8_lf, &settings, &report) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_read_state(settings, &original));
    mc_key_binding *duplicate = NULL;
    mc_game_settings_text forward_name = {0};
    int32_t forward_code = 0;
    CHECK(mc_key_binding_read(original.named[0], &forward_name, &forward_code));
    CHECK(mc_game_settings_binding_create(settings, forward_name, 99, &duplicate, NULL) ==
          MC_GAME_SETTINGS_OK);
    mc_key_binding *array[] = {original.named[0], duplicate, original.named[0], NULL,
                               original.named[1]};
    CHECK(mc_game_settings_set_binding_array(settings, array, 5u, false, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_named_binding(settings, 0, original.named[1], NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(text_input("aliases", "key_key.forward:51\nmusic:0.25\nkey_key.left:52\n"));
    CHECK(mc_game_settings_load(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.bad_options == 3u);
    CHECK(code_is(original.named[0], 51) && code_is(duplicate, 51) &&
          code_is(original.named[1], 30));
    CHECK(mc_game_settings_read_state(settings, &state) && state.named[0] == original.named[1] &&
          state.values.music_volume == .25f && state.bindings != original.bindings &&
          original.bindings[0] == original.named[0]);
    CHECK(mc_game_settings_save(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_BINDING &&
          report.println_attempts == 18u && !report.source_close_reached &&
          report.native_additional_close && report.writer_state_present &&
          !report.writer_state.source_close_reached && !report.writer_state.encoder_closed);
    uint8_t bytes[4096];
    size_t count = 99;
    CHECK(output("aliases", bytes, sizeof(bytes), &count) &&
          count == 0u); /* No invented finally flush. */
    CHECK(mc_game_settings_set_binding_array(settings, NULL, 0, true, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(text_input("aliases", "music:0.75\nunknown\n"));
    CHECK(mc_game_settings_load(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.bad_options == 2u);
    CHECK(mc_game_settings_read_state(settings, &state) && state.values.music_volume == .75f);
    CHECK(mc_game_settings_save(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.println_attempts == 15u &&
          report.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_BINDING_ARRAY);
    CHECK(mc_game_settings_set_binding_array(settings, NULL, 0, false, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_read_state(settings, &state) && !state.bindings_null &&
          state.bindings != NULL);
    CHECK(mc_game_settings_save(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.println_attempts == 15u && !report.outer_exception_caught &&
          report.source_close_reached);
    const mc_game_settings_text null_name = {.is_null = true};
    CHECK(mc_game_settings_binding_set(settings, duplicate, null_name, INT32_MIN, NULL) ==
          MC_GAME_SETTINGS_OK);
    array[0] = duplicate;
    array[1] = duplicate;
    CHECK(mc_game_settings_set_binding_array(settings, array, 2u, false, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(text_input("aliases", "key_null:64\n"));
    CHECK(mc_game_settings_load(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.bad_options == 0u && code_is(duplicate, 64));
    CHECK(mc_game_settings_save(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.println_attempts == 17u);
    CHECK(output("aliases", bytes, sizeof(bytes) - 1u, &count));
    bytes[count] = 0;
    CHECK(count >= 24u &&
          strcmp((const char *)bytes + count - 24u, "key_null:64\nkey_null:64\n") == 0);
    CHECK(mc_game_settings_read_state(settings, &state));
    state.values.texture_pack = null_name;
    state.values.last_server = null_name;
    CHECK(mc_game_settings_set_values(settings, &state.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_save(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(output("aliases", bytes, sizeof(bytes) - 1u, &count));
    bytes[count] = 0;
    CHECK(strstr((const char *)bytes, "skin:null\nlastServer:null\n") != NULL);
    okay = true;
cleanup:
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

static bool encoding_and_failures(void) {
    bool okay = false;
    mc_game_settings *settings = NULL, *directory = NULL, *missing_parent = NULL;
    uint16_t *large = NULL;
    mc_game_settings_report report = {0};
    CHECK(create("encoding", &utf8_lf, &settings, &report) == MC_GAME_SETTINGS_OK);
    /* A BOM is an actual leading key unit; malformed UTF8 is replaced. */
    static const uint8_t raw[] = {0xef, 0xbb, 0xbf, 'm', 'u', 's', 'i', 'c',  ':',  '0', '.',
                                  '5',  '\n', 's',  'k', 'i', 'n', ':', 0xff, '\n', 's', 'o',
                                  'u',  'n',  'd',  ':', '0', '.', '2', '5',  '\n'};
    CHECK(input("encoding", raw, sizeof(raw)));
    CHECK(mc_game_settings_load(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.bad_options == 0u);
    mc_game_settings_state state = {0};
    CHECK(mc_game_settings_read_state(settings, &state) && state.values.music_volume == 1.0f &&
          state.values.sound_volume == .25f && state.values.texture_pack.length == 1u &&
          state.values.texture_pack.units[0] == 0xfffdu);
    const uint16_t pack[] = {'C', 0xe9, 0x20ac, 0xd83d, 0xde80, 0xd800};
    state.values.texture_pack =
        (mc_game_settings_text){pack, sizeof(pack) / sizeof(pack[0]), false};
    state.values.last_server = (mc_game_settings_text){.is_null = true};
    state.values.hide_gui = true;
    state.values.third_person_view = true;
    state.values.show_debug_info = true;
    state.values.source_D = true;
    state.values.smooth_camera = true;
    state.values.debug_camera = true;
    state.values.source_G = -7.5f;
    state.values.source_H = 400.0f;
    CHECK(mc_game_settings_set_values(settings, &state.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_save(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          !report.writer_state.trouble);
    CHECK(mc_game_settings_read_state(settings, &state) && state.values.hide_gui &&
          state.values.third_person_view && state.values.show_debug_info && state.values.source_D &&
          state.values.smooth_camera && state.values.debug_camera &&
          state.values.source_G == -7.5f && state.values.source_H == 400.0f &&
          state.values.last_server.is_null);
    CHECK(create("utf8-output", &utf8_lf, &directory, &report) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_values(directory, &state.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_save(directory, NULL, &report, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_retire(&directory, NULL) == MC_GAME_SETTINGS_OK);
    /* Same literal UTF16 through a second real file and CP1252/CRLF profile. */
    const mc_game_settings_profile cp = {MC_JAVA_READER_WINDOWS1252, MC_JAVA_WRITER_CRLF};
    CHECK(create("cp1252", &cp, &directory, &report) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_values(directory, &state.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_save(directory, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.writer_state.charset == MC_JAVA_READER_WINDOWS1252);
    CHECK(mc_game_settings_retire(&directory, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(create("owned-directory", &utf8_lf, &directory, &report) == MC_GAME_SETTINGS_OK &&
          report.outer_exception_caught &&
          report.outer_exception == MC_GAME_SETTINGS_SOURCE_FILE_IO && !report.source_opened &&
          !report.source_close_reached && report.bytes_read == 0u && report.lines_read == 0u);
    CHECK(mc_game_settings_save(directory, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.outer_exception == MC_GAME_SETTINGS_SOURCE_FILE_IO && !report.source_opened);
    CHECK(create("absent-parent/child", &utf8_lf, &missing_parent, &report) ==
              MC_GAME_SETTINGS_OK &&
          report.file_absent);
    CHECK(mc_game_settings_save(missing_parent, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.outer_exception == MC_GAME_SETTINGS_SOURCE_FILE_IO && !report.source_opened);
    /* Real truncation and an emitted prefix survive a later source null-array
     * exception; retirement does not encode/flush the remainder. */
    large = malloc(20000u * sizeof(*large));
    CHECK(large != NULL);
    for (size_t i = 0; i < 20000u; ++i)
        large[i] = 'P';
    state.values.texture_pack = (mc_game_settings_text){large, 20000u, false};
    CHECK(mc_game_settings_set_values(settings, &state.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_binding_array(settings, NULL, 0, true, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_save(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK &&
          report.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_BINDING_ARRAY &&
          report.println_attempts == 15u && report.native_additional_close &&
          !report.source_close_reached);
    uint8_t bytes[24576];
    size_t count = 0;
    CHECK(output("encoding", bytes, sizeof(bytes), &count) && count == 16384u &&
          memcmp(bytes, "music:1.0\nsound:0.25\n", 21u) == 0 && bytes[count - 1u] == 'P');
    mc_game_settings_state before = {0};
    CHECK(mc_game_settings_read_state(settings, &before));
    mc_game_settings_values invalid = before.values;
    invalid.texture_pack = (mc_game_settings_text){NULL, 1u, false};
    CHECK(mc_game_settings_set_values(settings, &invalid, NULL) ==
          MC_GAME_SETTINGS_INVALID_ARGUMENT);
    CHECK(mc_game_settings_read_state(settings, &state) &&
          state.values.texture_pack.units == before.values.texture_pack.units);
    mc_key_binding *preserved = state.named[0];
    const mc_game_settings_text name = {.is_null = true};
    for (size_t i = 10; i < MC_GAME_SETTINGS_MAX_BINDING_OWNERS; ++i) {
        mc_key_binding *binding = NULL;
        CHECK(mc_game_settings_binding_create(settings, name, 0, &binding, NULL) ==
                  MC_GAME_SETTINGS_OK &&
              binding != NULL);
    }
    CHECK(mc_game_settings_binding_create(settings, name, 0, &preserved, NULL) ==
              MC_GAME_SETTINGS_LIMIT &&
          preserved == state.named[0]);
    okay = true;
cleanup:
    free(large);
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_retire(&directory, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_retire(&missing_parent, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

static bool limits_and_raw_strings(void) {
    bool okay = false;
    mc_game_settings *settings = NULL, *fresh = NULL;
    uint16_t *large = NULL;
    uint8_t *line = NULL;
    mc_game_settings_report report = {0};
    mc_game_settings_state state = {0};
    CHECK(create("raw-strings", &utf8_lf, &settings, &report) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_read_state(settings, &state));
    static const uint16_t pack[] = {'A', ':', 'B', '\n', 's', 'o', 'u', 'n',
                                    'd', ':', '0', '.',  '5', 0,   'Z'};
    state.values.texture_pack =
        (mc_game_settings_text){pack, sizeof(pack) / sizeof(pack[0]), false};
    state.values.last_server = (mc_game_settings_text){.is_null = true};
    CHECK(mc_game_settings_set_values(settings, &state.values, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_save(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(create("raw-strings", &utf8_lf, &fresh, &report) == MC_GAME_SETTINGS_OK &&
          report.bad_options == 1u && report.lines_read == 26u);
    CHECK(mc_game_settings_read_state(fresh, &state) && ascii(state.values.texture_pack, "A") &&
          state.values.sound_volume == 1.0f && ascii(state.values.last_server, "null"));
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(create("limits", &utf8_lf, &settings, &report) == MC_GAME_SETTINGS_OK);
    const size_t prefix = sizeof("music:0.5\n") - 1u;
    const size_t length = prefix + MC_GAME_SETTINGS_MAX_TEXT_UNITS + 1u;
    line = malloc(length);
    CHECK(line != NULL);
    memcpy(line, "music:0.5\n", prefix);
    memset(line + prefix, 'X', length - prefix);
    CHECK(input("limits", line, length));
    CHECK(mc_game_settings_load(settings, NULL, &report, NULL) == MC_GAME_SETTINGS_LIMIT &&
          report.lines_read == 2u && !report.outer_exception_caught &&
          !report.source_close_reached && report.native_additional_close &&
          !report.native_cleanup_failed);
    CHECK(mc_game_settings_read_state(settings, &state) && state.values.music_volume == .5f);
    const mc_game_settings_text previous = state.values.texture_pack;
    state.values.texture_pack =
        (mc_game_settings_text){pack, MC_GAME_SETTINGS_MAX_TEXT_UNITS + 1u, false};
    CHECK(mc_game_settings_set_values(settings, &state.values, NULL) == MC_GAME_SETTINGS_LIMIT);
    CHECK(mc_game_settings_read_state(settings, &state) &&
          state.values.texture_pack.units == previous.units);
    large = malloc(MC_GAME_SETTINGS_MAX_TEXT_UNITS * sizeof(*large));
    CHECK(large != NULL);
    for (size_t i = 0; i < MC_GAME_SETTINGS_MAX_TEXT_UNITS; ++i)
        large[i] = 'P';
    bool limited = false;
    for (size_t i = 0; i < 9u; ++i) {
        CHECK(mc_game_settings_read_state(settings, &state));
        const uint16_t *before = state.values.texture_pack.units;
        state.values.texture_pack =
            (mc_game_settings_text){large, MC_GAME_SETTINGS_MAX_TEXT_UNITS, false};
        const mc_game_settings_status status =
            mc_game_settings_set_values(settings, &state.values, NULL);
        CHECK(status == MC_GAME_SETTINGS_OK || status == MC_GAME_SETTINGS_LIMIT);
        if (status == MC_GAME_SETTINGS_LIMIT) {
            limited = true;
            CHECK(mc_game_settings_read_state(settings, &state) &&
                  state.values.texture_pack.units == before);
            break;
        }
    }
    CHECK(limited); /* Cumulative retained-string budget, not an artificial allocator. */
    okay = true;
cleanup:
    free(line);
    free(large);
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK ||
        mc_game_settings_retire(&fresh, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

typedef struct binding_action_log {
    mc_game_settings *settings;
    mc_key_binding *target;
    int32_t expected_code;
    size_t calls;
    bool inspected, action_busy, raw_busy, save_busy, load_busy, retire_busy;
    bool event_valid, fail;
} binding_action_log;

static mc_game_settings_status binding_action_diagnostic(void *context,
                                                         const mc_game_settings_diagnostic *event,
                                                         mc_game_settings_error *error) {
    binding_action_log *log = context;
    ++log->calls;
    log->event_valid = event->kind == MC_GAME_SETTINGS_SAVE_FAILED &&
                       strcmp(event->message, "Failed to save options") == 0 && event->line.is_null;
    mc_game_settings_state state = {0};
    mc_game_settings_text name = {0};
    int32_t code = 0;
    log->inspected = mc_game_settings_read_state(log->settings, &state) &&
                     mc_key_binding_read(log->target, &name, &code) && code == log->expected_code;
    mc_game_settings_binding_action_report nested = {.source_pc = 123,
                                                     .uncaught_exception =
                                                         MC_GAME_SETTINGS_BINDING_ACTION_INDEX,
                                                     .binding_assigned = true,
                                                     .save_reached = true,
                                                     .save = {.lines_read = 99}};
    log->action_busy = mc_game_settings_set_binding_code(log->settings, 0, 0, NULL, &nested,
                                                         NULL) == MC_GAME_SETTINGS_BUSY &&
                       nested.source_pc == 123 &&
                       nested.uncaught_exception == MC_GAME_SETTINGS_BINDING_ACTION_INDEX &&
                       nested.binding_assigned && nested.save_reached &&
                       nested.save.lines_read == 99;
    log->raw_busy = mc_game_settings_binding_set(log->settings, log->target, name, 0, NULL) ==
                    MC_GAME_SETTINGS_BUSY;
    log->save_busy =
        mc_game_settings_save(log->settings, NULL, NULL, NULL) == MC_GAME_SETTINGS_BUSY;
    log->load_busy =
        mc_game_settings_load(log->settings, NULL, NULL, NULL) == MC_GAME_SETTINGS_BUSY;
    mc_game_settings *alias = log->settings;
    log->retire_busy =
        mc_game_settings_retire(&alias, NULL) == MC_GAME_SETTINGS_BUSY && alias == log->settings;
    if (log->fail) {
        if (error != NULL)
            *error = (mc_game_settings_error){MC_GAME_SETTINGS_DIAGNOSTIC_FAILED, 73,
                                              "authored binding action diagnostic refusal"};
        return MC_GAME_SETTINGS_DIAGNOSTIC_FAILED;
    }
    return MC_GAME_SETTINGS_OK;
}

static bool binding_actions_noarg(void) {
    bool okay = false;
    mc_game_settings *settings = NULL;
    _Static_assert(MC_GAME_SETTINGS_DIAGNOSTIC_FAILED == 7 &&
                       MC_GAME_SETTINGS_SOURCE_EXCEPTION == 8,
                   "GameSettings status ABI is append-only");
    CHECK(mc_game_settings_create_defaults(&utf8_lf, &settings, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state state = {0};
    CHECK(mc_game_settings_read_state(settings, &state));
    binding_action_log log = {
        .settings = settings, .target = state.named[0], .expected_code = INT32_MIN};
    const mc_game_settings_diagnostics diagnostics = {&log, binding_action_diagnostic};
    mc_game_settings_binding_action_report report;
    mc_game_settings_error error;
    CHECK(mc_game_settings_set_binding_code(settings, 0, INT32_MIN, &diagnostics, &report,
                                            &error) == MC_GAME_SETTINGS_OK);
    CHECK(report.source_pc == 14 &&
          report.uncaught_exception == MC_GAME_SETTINGS_BINDING_ACTION_NONE &&
          report.binding_assigned && report.save_reached && report.save.outer_exception_caught &&
          report.save.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_FILE &&
          !report.save.source_opened && report.save.println_attempts == 0 &&
          !report.save.source_close_reached && !report.save.native_additional_close &&
          error.status == MC_GAME_SETTINGS_OK && error.operation == NULL);
    CHECK(code_is(state.named[0], INT32_MIN) && code_is(state.bindings[0], INT32_MIN));
    CHECK(log.calls == 1 && log.event_valid && log.inspected && log.action_busy && log.raw_busy &&
          log.save_busy && log.load_busy && log.retire_busy);
    log.expected_code = 42;
    log.fail = true;
    CHECK(mc_game_settings_set_binding_code(settings, 0, 42, &diagnostics, &report, &error) ==
          MC_GAME_SETTINGS_DIAGNOSTIC_FAILED);
    CHECK(report.source_pc == 11 &&
          report.uncaught_exception == MC_GAME_SETTINGS_BINDING_ACTION_NONE &&
          report.binding_assigned && report.save_reached && report.save.outer_exception_caught &&
          report.save.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_FILE &&
          code_is(state.named[0], 42) && error.system_error == 73 &&
          strcmp(error.operation, "authored binding action diagnostic refusal") == 0);
    CHECK(log.calls == 2 && log.inspected && log.action_busy && log.raw_busy && log.save_busy &&
          log.load_busy && log.retire_busy);
    CHECK(mc_game_settings_set_binding_code(settings, 0, 42, NULL, NULL, NULL) ==
              MC_GAME_SETTINGS_OK &&
          code_is(state.named[0], 42));
    mc_game_settings_text name = {0};
    int32_t code = 0;
    CHECK(mc_key_binding_read(state.named[0], &name, &code));
    CHECK(mc_game_settings_binding_set(settings, state.named[0], name, 43, NULL) ==
              MC_GAME_SETTINGS_OK &&
          code_is(state.named[0], 43));
    report = (mc_game_settings_binding_action_report){.source_pc = 123, .save_reached = true};
    CHECK(mc_game_settings_set_binding_code(NULL, 0, 1, NULL, &report, &error) ==
              MC_GAME_SETTINGS_INVALID_ARGUMENT &&
          report.source_pc == 123 && report.save_reached);
    okay = true;
cleanup:
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

static bool binding_actions_files(void) {
    bool okay = false;
    mc_game_settings *settings = NULL, *fresh = NULL, *directory = NULL, *missing = NULL;
    uint16_t *large = NULL;
    uint8_t bytes[32768], saved[4096];
    size_t count = 0, saved_count = 0;
    mc_game_settings_report load = {0};
    CHECK(text_input("aliases", "authored-unknown-input\n"));
    CHECK(create("aliases", &utf8_lf, &settings, &load) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state state = {0};
    CHECK(mc_game_settings_read_state(settings, &state));
    mc_key_binding *forward = state.named[0];
    mc_key_binding *duplicates[] = {forward, forward};
    CHECK(mc_game_settings_set_binding_array(settings, duplicates, 2, false, NULL) ==
          MC_GAME_SETTINGS_OK);
    mc_game_settings_binding_action_report report;
    CHECK(mc_game_settings_set_binding_code(settings, 1, INT32_MAX, NULL, &report, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(report.source_pc == 14 &&
          report.uncaught_exception == MC_GAME_SETTINGS_BINDING_ACTION_NONE &&
          report.binding_assigned && report.save_reached && report.save.println_attempts == 17 &&
          report.save.source_opened && report.save.source_close_reached &&
          !report.save.outer_exception_caught && !report.save.native_additional_close);
    CHECK(code_is(forward, INT32_MAX) && code_is(state.named[1], 30) &&
          code_is(state.bindings[0], INT32_MAX));
    CHECK(mc_game_settings_read_state(settings, &state) && state.bindings[0] == forward &&
          state.bindings[1] == forward && state.named[0] == forward);
    static const char suffix[] = "key_key.forward:2147483647\nkey_key.forward:2147483647\n";
    CHECK(output("aliases", saved, sizeof(saved), &saved_count) &&
          saved_count >= sizeof(suffix) - 1u &&
          memcmp(saved + saved_count - (sizeof(suffix) - 1u), suffix, sizeof(suffix) - 1u) == 0);
    CHECK(text_input("aliases", "different-authored-disk-input\n"));
    CHECK(mc_game_settings_set_binding_code(settings, 0, INT32_MAX, NULL, &report, NULL) ==
              MC_GAME_SETTINGS_OK &&
          report.save_reached && report.save.source_close_reached);
    CHECK(output("aliases", bytes, sizeof(bytes), &count) && count == saved_count &&
          memcmp(bytes, saved, count) == 0); /* Unchanged code still really saves. */
    CHECK(create("aliases", &utf8_lf, &fresh, &load) == MC_GAME_SETTINGS_OK);
    mc_game_settings_state reloaded = {0};
    CHECK(mc_game_settings_read_state(fresh, &reloaded) && code_is(reloaded.named[0], INT32_MAX) &&
          code_is(reloaded.named[1], 30));
    CHECK(mc_game_settings_retire(&fresh, NULL) == MC_GAME_SETTINGS_OK);
    mc_game_settings_text name = {0};
    int32_t code = 0;
    CHECK(mc_key_binding_read(forward, &name, &code));
    CHECK(mc_game_settings_binding_set(settings, forward, (mc_game_settings_text){NULL, 0, true},
                                       code, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_binding_code(settings, 1, INT32_MIN, NULL, &report, NULL) ==
              MC_GAME_SETTINGS_OK &&
          report.save.source_close_reached);
    static const char null_suffix[] = "key_null:-2147483648\nkey_null:-2147483648\n";
    CHECK(output("aliases", bytes, sizeof(bytes), &count) && count >= sizeof(null_suffix) - 1u &&
          memcmp(bytes + count - (sizeof(null_suffix) - 1u), null_suffix,
                 sizeof(null_suffix) - 1u) == 0);
    CHECK(mc_game_settings_binding_set(settings, forward, name, INT32_MIN, NULL) ==
          MC_GAME_SETTINGS_OK);
    large = malloc(9000u * sizeof(*large));
    CHECK(large != NULL);
    for (size_t i = 0; i < 9000u; ++i)
        large[i] = 'P';
    mc_game_settings_values values = state.values;
    values.texture_pack = (mc_game_settings_text){large, 9000, false};
    CHECK(mc_game_settings_set_values(settings, &values, NULL) == MC_GAME_SETTINGS_OK);
    mc_key_binding *partial[] = {forward, forward, NULL};
    CHECK(mc_game_settings_set_binding_array(settings, partial, 3, false, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_binding_code(settings, 1, -123, NULL, &report, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(report.source_pc == 14 &&
          report.uncaught_exception == MC_GAME_SETTINGS_BINDING_ACTION_NONE &&
          report.binding_assigned && report.save_reached && report.save.outer_exception_caught &&
          report.save.outer_exception == MC_GAME_SETTINGS_SOURCE_NULL_BINDING &&
          report.save.println_attempts == 17 && !report.save.source_close_reached &&
          report.save.native_additional_close && !report.save.native_cleanup_failed &&
          code_is(forward, -123));
    CHECK(output("aliases", bytes, sizeof(bytes), &count) && count == 8192u &&
          memcmp(bytes, "music:1.0\nsound:1.0\n", sizeof("music:1.0\nsound:1.0\n") - 1u) == 0 &&
          bytes[count - 1u] == 'P');
    CHECK(create("owned-directory", &utf8_lf, &directory, &load) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_binding_code(directory, 0, 99, NULL, &report, NULL) ==
              MC_GAME_SETTINGS_OK &&
          report.binding_assigned && report.save_reached && report.source_pc == 14 &&
          report.save.outer_exception_caught &&
          report.save.outer_exception == MC_GAME_SETTINGS_SOURCE_FILE_IO &&
          !report.save.source_opened);
    CHECK(mc_game_settings_read_state(directory, &reloaded) && code_is(reloaded.named[0], 99));
    CHECK(create("absent-parent/child", &utf8_lf, &missing, &load) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_binding_code(missing, 0, -1, NULL, &report, NULL) ==
              MC_GAME_SETTINGS_OK &&
          report.binding_assigned && report.save_reached && report.source_pc == 14 &&
          report.save.outer_exception_caught &&
          report.save.outer_exception == MC_GAME_SETTINGS_SOURCE_FILE_IO &&
          !report.save.source_opened);
    CHECK(mc_game_settings_read_state(missing, &reloaded) && code_is(reloaded.named[0], -1));
    /* Failures before save must leave the existing file and code untouched. */
    static const char sentinel[] = "sentinel-no-save\n";
    CHECK(text_input("aliases", sentinel));
    binding_action_log log = {.settings = settings, .target = forward, .expected_code = -123};
    const mc_game_settings_diagnostics diagnostics = {&log, binding_action_diagnostic};
    mc_game_settings_error error;
    CHECK(mc_game_settings_set_binding_code(settings, 2, 7, &diagnostics, &report, &error) ==
          MC_GAME_SETTINGS_SOURCE_EXCEPTION);
    CHECK(report.source_pc == 7 &&
          report.uncaught_exception == MC_GAME_SETTINGS_BINDING_ACTION_NULL_BINDING &&
          !report.binding_assigned && !report.save_reached && report.save.println_attempts == 0 &&
          error.status == MC_GAME_SETTINGS_SOURCE_EXCEPTION);
    CHECK(mc_game_settings_set_binding_array(settings, NULL, 0, true, NULL) == MC_GAME_SETTINGS_OK);
    const int32_t invalid[] = {INT32_MIN, 0, INT32_MAX};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(mc_game_settings_set_binding_code(settings, invalid[i], 7, &diagnostics, &report,
                                                NULL) == MC_GAME_SETTINGS_SOURCE_EXCEPTION);
        CHECK(report.source_pc == 5 &&
              report.uncaught_exception == MC_GAME_SETTINGS_BINDING_ACTION_NULL_ARRAY &&
              !report.binding_assigned && !report.save_reached);
    }
    CHECK(mc_game_settings_set_binding_array(settings, NULL, 0, false, NULL) ==
          MC_GAME_SETTINGS_OK);
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        CHECK(mc_game_settings_set_binding_code(settings, invalid[i], 7, &diagnostics, &report,
                                                NULL) == MC_GAME_SETTINGS_SOURCE_EXCEPTION);
        CHECK(report.source_pc == 5 &&
              report.uncaught_exception == MC_GAME_SETTINGS_BINDING_ACTION_INDEX &&
              !report.binding_assigned && !report.save_reached);
    }
    mc_key_binding *null_first[] = {NULL, forward};
    CHECK(mc_game_settings_set_binding_array(settings, null_first, 2, false, NULL) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_set_binding_code(settings, -1, 7, &diagnostics, &report, NULL) ==
              MC_GAME_SETTINGS_SOURCE_EXCEPTION &&
          report.source_pc == 5 &&
          report.uncaught_exception == MC_GAME_SETTINGS_BINDING_ACTION_INDEX);
    CHECK(mc_game_settings_set_binding_code(settings, 0, 7, &diagnostics, &report, NULL) ==
              MC_GAME_SETTINGS_SOURCE_EXCEPTION &&
          report.source_pc == 7 &&
          report.uncaught_exception == MC_GAME_SETTINGS_BINDING_ACTION_NULL_BINDING);
    CHECK(log.calls == 0 && code_is(forward, -123));
    CHECK(output("aliases", bytes, sizeof(bytes), &count) && count == sizeof(sentinel) - 1u &&
          memcmp(bytes, sentinel, count) == 0);
    okay = true;
cleanup:
    free(large);
    if (mc_game_settings_retire(&settings, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    if (mc_game_settings_retire(&fresh, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    if (mc_game_settings_retire(&directory, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    if (mc_game_settings_retire(&missing, NULL) != MC_GAME_SETTINGS_OK)
        okay = false;
    return okay;
}

static int test_body(int argc, char **argv) {
    if (argc != 2)
        return EXIT_FAILURE;
    root = argv[1];
    if (!defaults() || !ordinary() || !lexical_and_partial() || !aliases() ||
        !encoding_and_failures() || !limits_and_raw_strings() || !binding_actions_noarg() ||
        !binding_actions_files())
        return EXIT_FAILURE;
    if (printf("PASS: %u GameSettings persistence interface checks\n", checks) < 0 ||
        fflush(stdout) != 0)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **wide) {
    if (argc != 2)
        return EXIT_FAILURE;
    const int size =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[1], -1, NULL, 0, NULL, NULL);
    if (size <= 0 || size > 4096)
        return EXIT_FAILURE;
    char *path = malloc((size_t)size);
    if (path == NULL)
        return EXIT_FAILURE;
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide[1], -1, path, size, NULL, NULL) !=
        size) {
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
