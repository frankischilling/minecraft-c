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

static int test_body(int argc, char **argv) {
    if (argc != 2)
        return EXIT_FAILURE;
    root = argv[1];
    if (!defaults() || !ordinary() || !lexical_and_partial() || !aliases() ||
        !encoding_and_failures() || !limits_and_raw_strings())
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
