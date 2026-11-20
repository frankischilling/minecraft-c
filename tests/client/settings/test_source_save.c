#include "mc/client/settings/construction.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "typed Settings save check%u line%d: %s\n", checks, __LINE__,          \
                    #condition);                                                                   \
            failed = 1;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const char suffix[] =
    "invertYMouse:false\nmouseSensitivity:0.5\nviewDistance:0\nguiScale:0\n"
    "bobView:true\nanaglyph3d:false\nadvancedOpengl:false\nfpsLimit:1\n"
    "difficulty:2\nfancyGraphics:true\nao:true\n";
static const char bindings[] =
    "lastServer:null\nkey_key.forward:17\nkey_key.left:30\nkey_key.back:31\n"
    "key_key.right:32\nkey_key.jump:57\nkey_key.sneak:42\nkey_key.drop:16\n"
    "key_key.inventory:18\nkey_key.chat:20\nkey_key.fog:33\n";

int main(int argc, char **argv) {
    int failed = 0;
    mc_java_files *files = NULL;
    mc_java_file *parent = NULL, *held_options = NULL;
    mc_java_file_stream *input = NULL;
    mc_world_info_name *name = NULL;
    mc_game_settings_classes *classes = NULL;
    mc_game_settings *settings = NULL, *reloaded = NULL;
    uint16_t *units = NULL;
    mc_game_settings_source_error *source_error = calloc(1, sizeof(*source_error));
    mc_java_file_error file_error = {0};
    mc_game_settings_error error = {0};
    CHECK(source_error != NULL && argc == 2);
    const size_t path_length = strlen(argv[1]);
    CHECK(path_length > 0 && path_length < MC_GAME_SETTINGS_MAX_PATH_BYTES);
    units = calloc(path_length, sizeof(*units));
    CHECK(units != NULL);
    for (size_t index = 0; index < path_length; ++index) {
        CHECK((unsigned char)argv[1][index] < 128);
        units[index] = (unsigned char)argv[1][index];
    }
    CHECK(mc_java_files_create(NULL, &files, &file_error) == MC_JAVA_FILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){units, path_length}, 8u * 1048576u, &name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_java_file_allocate(files, &parent, &file_error) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_string(parent, name, &file_error) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(name);
    name = NULL;
    const mc_game_settings_profile profile = {MC_JAVA_READER_UTF8, MC_JAVA_WRITER_LF};
    CHECK(mc_game_settings_classes_create(files, &profile, NULL, &classes, source_error) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_source_allocate(classes, &profile, &settings, source_error) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_source_construct(settings, NULL, parent, NULL, source_error) ==
          MC_GAME_SETTINGS_OK);
    mc_game_settings_source_state source;
    mc_game_settings_state state;
    CHECK(mc_game_settings_source_inspect(settings, &source));
    CHECK(mc_game_settings_read_state(settings, &state));
    CHECK(source.N != NULL && source.N != parent && state.file_identity == source.N &&
          state.options_path == NULL && source.load_report->file_absent);
    CHECK(mc_java_file_retain(source.N, &file_error) == MC_JAVA_FILE_OK);
    held_options = source.N;

    for (unsigned int round = 0; round < 2; ++round) {
        mc_game_settings_values values = state.values;
        values.music_volume = round == 0 ? 0.0f : 0.5f;
        values.sound_volume = round == 0 ? 0.0f : 0.75f;
        values.last_server = (mc_game_settings_text){.is_null = true};
        static const uint16_t short_pack[] = {'A'};
        if (round == 1)
            values.texture_pack = (mc_game_settings_text){short_pack, 1, false};
        CHECK(mc_game_settings_set_values(settings, &values, &error) == MC_GAME_SETTINGS_OK);
        mc_game_settings_report report = {0};
        const mc_game_settings_status saved =
            mc_game_settings_save(settings, NULL, &report, &error);
        if (saved != MC_GAME_SETTINGS_OK)
            fprintf(stderr, "Typed save result=%d operation=%s\n", (int)saved,
                    error.operation == NULL ? "<null>" : error.operation);
        CHECK(saved == MC_GAME_SETTINGS_OK);
        CHECK(report.source_opened && report.source_close_reached &&
              report.println_attempts == 25 && !report.outer_exception_caught &&
              !report.native_cleanup_failed && report.writer_state_present &&
              report.writer_state.encoder_closed && !report.writer_state.trouble);
        CHECK(mc_game_settings_source_inspect(settings, &source));
        CHECK(source.N == held_options && source.phase == MC_GAME_SETTINGS_SOURCE_READY);
        CHECK(mc_game_settings_read_state(settings, &state));
        CHECK(state.file_identity == held_options && state.options_path == NULL);
        char expected[1024];
        const int count =
            snprintf(expected, sizeof(expected), "%s%s%s%s",
                     round == 0 ? "music:0.0\nsound:0.0\n" : "music:0.5\nsound:0.75\n", suffix,
                     round == 0 ? "skin:Default\n" : "skin:A\n", bindings);
        CHECK(count > 0 && (size_t)count < sizeof(expected));
        CHECK(mc_java_file_stream_allocate(files, &input, &file_error) == MC_JAVA_FILE_OK);
        CHECK(mc_java_file_stream_construct(input, held_options, &file_error) == MC_JAVA_FILE_OK);
        uint8_t actual[1024];
        size_t used = 0;
        bool eof = false;
        while (!eof && used < sizeof(actual)) {
            mc_java_file_read read;
            CHECK(mc_java_file_stream_read(input, actual + used, sizeof(actual) - used, &read,
                                           &file_error) == MC_JAVA_FILE_OK);
            CHECK(read.count != 0 || read.eof);
            used += read.count;
            eof = read.eof;
        }
        CHECK(eof && used == (size_t)count && memcmp(actual, expected, used) == 0);
        CHECK(mc_java_file_stream_source_close(input, &file_error) == MC_JAVA_FILE_OK);
        CHECK(mc_java_file_stream_release(&input, &file_error) == MC_JAVA_FILE_OK);
    }

    CHECK(mc_game_settings_source_allocate(classes, &profile, &reloaded, source_error) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_source_construct(reloaded, NULL, parent, NULL, source_error) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_read_state(reloaded, &state));
    CHECK(state.values.music_volume == 0.5f && state.values.sound_volume == 0.75f &&
          !state.values.texture_pack.is_null && state.values.texture_pack.length == 1 &&
          state.values.texture_pack.units[0] == 'A');

cleanup:
    if (mc_java_file_stream_release(&input, &file_error) != MC_JAVA_FILE_OK)
        failed = 1;
    if (mc_game_settings_retire(&reloaded, &error) != MC_GAME_SETTINGS_OK)
        failed = 1;
    if (mc_game_settings_retire(&settings, &error) != MC_GAME_SETTINGS_OK)
        failed = 1;
    if (mc_java_file_release(&held_options, &file_error) != MC_JAVA_FILE_OK)
        failed = 1;
    if (source_error != NULL &&
        mc_game_settings_classes_release(&classes, source_error) != MC_GAME_SETTINGS_OK)
        failed = 1;
    if (mc_java_file_release(&parent, &file_error) != MC_JAVA_FILE_OK)
        failed = 1;
    if (mc_java_files_release(&files, &file_error) != MC_JAVA_FILE_OK)
        failed = 1;
    mc_world_info_name_release(name);
    free(units);
    free(source_error);
    if (!failed)
        printf("Typed Settings save controls: %u checks\n", checks);
    return failed;
}
