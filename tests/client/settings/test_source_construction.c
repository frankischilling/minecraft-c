#include "mc/client/settings/construction.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Authored source contracts, UNRUN. NULL Minecraft is a genuine supported kv
 * argument; no fabricated App, replacement class or successful child exists. */
static unsigned int checks;
#define CHECK(test)                                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(test)) {                                                                             \
            fprintf(stderr, "source Settings check%u line%d: %s\n", checks, __LINE__, #test);      \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static int cpu(void) {
    mc_java_files *files = NULL;
    mc_game_settings_source_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_java_files_create(NULL, &files, &error->file) == MC_JAVA_FILE_OK);
    static const int32_t allocation_pc[] = {1, 28, 55, 82};
    static const unsigned int published_pc[] = {0, 24, 51, 78};
    for (size_t permitted = 0; permitted < 4; ++permitted) {
        mc_game_settings_class_options options = mc_game_settings_class_default_options();
        options.maximum_arrays = permitted;
        mc_game_settings_classes *classes = NULL;
        CHECK(mc_game_settings_classes_create(files, NULL, &options, &classes, error) ==
              MC_GAME_SETTINGS_OK);
        mc_game_settings *existing = NULL;
        CHECK(mc_game_settings_create_defaults(NULL, &existing, NULL) == MC_GAME_SETTINGS_OK);
        mc_game_settings *out = existing; /* Actual owned output preimage. */
        CHECK(mc_game_settings_source_allocate(classes, NULL, &out, error) ==
              MC_GAME_SETTINGS_LIMIT);
        CHECK(out == existing && error->site.pc == allocation_pc[permitted]);
        mc_game_settings_class_state state;
        CHECK(mc_game_settings_classes_inspect(classes, &state));
        CHECK(state.phase == MC_GAME_SETTINGS_CLASS_PHASE_ERRONEOUS && state.arrays == permitted &&
              state.last_store_pc == published_pc[permitted]);
        CHECK(mc_game_settings_classes_active_use(classes, error) ==
              MC_GAME_SETTINGS_CLASS_ERRONEOUS);
        CHECK(error->site.pc == allocation_pc[permitted]);
        CHECK(state.failure->native.status == MC_GAME_SETTINGS_LIMIT);
        CHECK(mc_game_settings_retire(&existing, NULL) == MC_GAME_SETTINGS_OK);
        CHECK(mc_game_settings_classes_release(&classes, error) == MC_GAME_SETTINGS_OK);
    }
    mc_game_settings_classes *classes = NULL;
    CHECK(mc_game_settings_classes_create(files, NULL, NULL, &classes, error) ==
          MC_GAME_SETTINGS_OK);
    mc_game_settings_class_state cold;
    CHECK(mc_game_settings_classes_inspect(classes, &cold));
    CHECK(cold.phase == MC_GAME_SETTINGS_CLASS_COLD && cold.J == NULL && cold.K == NULL &&
          cold.L == NULL && cold.M == NULL && !cold.stdout_captured);
    mc_game_settings *settings = NULL;
    CHECK(mc_game_settings_source_allocate(classes, NULL, &settings, error) == MC_GAME_SETTINGS_OK);
    mc_game_settings_source_state raw;
    mc_game_settings_state values;
    CHECK(mc_game_settings_source_inspect(settings, &raw));
    CHECK(raw.phase == MC_GAME_SETTINGS_SOURCE_RAW && raw.N == NULL && raw.pending_file == NULL &&
          raw.pending_stream == NULL && raw.source_stores == 0);
    CHECK(mc_game_settings_read_state(settings, &values));
    CHECK(values.values.texture_pack.is_null && values.values.last_server.is_null &&
          values.bindings_null && values.minecraft == NULL && values.file_identity == NULL);
    CHECK(values.values.music_volume == 0.0f && values.values.sound_volume == 0.0f &&
          values.values.mouse_sensitivity == 0.0f);
    for (size_t index = 0; index < MC_GAME_SETTINGS_NAMED_BINDINGS; ++index)
        CHECK(values.named[index] == NULL);
    mc_game_settings_class_state hot;
    CHECK(mc_game_settings_classes_inspect(classes, &hot));
    CHECK(hot.phase == MC_GAME_SETTINGS_CLASS_READY && hot.arrays == 4 && hot.last_store_pc == 100);
    CHECK(hot.J != hot.K && hot.J != hot.L && hot.J != hot.M && hot.K != hot.L && hot.K != hot.M &&
          hot.L != hot.M);
    CHECK(hot.J[0] != NULL && hot.K[0] != NULL && hot.L[0] != NULL && hot.M[0] != NULL &&
          hot.options_literal == NULL && !hot.stdout_captured);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_classes_release(&classes, error) == MC_GAME_SETTINGS_OK);
    CHECK(mc_java_files_release(&files, &error->file) == MC_JAVA_FILE_OK);

    mc_java_file_options file_options = mc_java_file_default_options();
    file_options.max_path_units = 10; /* Real native File limit; options.txt has11 units. */
    CHECK(mc_java_files_create(&file_options, &files, &error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_game_settings_classes_create(files, NULL, NULL, &classes, error) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_source_allocate(classes, NULL, &settings, error) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_source_construct(settings, NULL, NULL, NULL, error) ==
          MC_GAME_SETTINGS_FILE_ERROR);
    CHECK(error->site.pc == 368 && error->file.status == MC_JAVA_FILE_LIMIT);
    mc_game_settings_source_state failed;
    CHECK(mc_game_settings_source_inspect(settings, &failed));
    CHECK(failed.phase == MC_GAME_SETTINGS_SOURCE_FAILED && failed.N == NULL &&
          failed.pending_file != NULL && failed.pending_stream == NULL &&
          failed.source_stores == 35 && failed.last_store_pc == 357);
    CHECK(mc_game_settings_read_state(settings, &values));
    CHECK(values.values.music_volume == 1.0f && values.values.sound_volume == 1.0f &&
          values.values.mouse_sensitivity == 0.5f && values.binding_count == 10 &&
          !values.bindings_null && values.bindings[5] == values.named[9] &&
          values.bindings[7] == values.named[5]);
    CHECK(values.values.difficulty == 2 && values.values.source_G == 1.0f &&
          values.values.source_H == 1.0f && values.minecraft == NULL);
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_classes_release(&classes, error) == MC_GAME_SETTINGS_OK);
    CHECK(mc_java_files_release(&files, &error->file) == MC_JAVA_FILE_OK);
    free(error);
    return 0;
}

static int ascii_parent(mc_java_files *files, const char *path, mc_java_file **out,
                        mc_java_file_error *error) {
    const size_t length = strlen(path);
    CHECK(length <= MC_GAME_SETTINGS_MAX_PATH_BYTES && length < SIZE_MAX / sizeof(uint16_t));
    uint16_t *units = calloc(length == 0 ? 1 : length, sizeof(*units));
    CHECK(units != NULL);
    for (size_t index = 0; index < length; ++index) {
        CHECK((unsigned char)path[index] < 128); /* Explicit owned ASCII fixture-root profile. */
        units[index] = (unsigned char)path[index];
    }
    mc_world_info_name *name = NULL;
    CHECK(mc_world_info_name_copy((mc_nbt_string){units, length}, 8u * 1048576u, &name) ==
          MC_WORLD_INFO_OK);
    free(units);
    CHECK(mc_java_file_allocate(files, out, error) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_string(*out, name, error) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(name);
    return 0;
}

static int filesystem(const char *directory, const char *kind) {
    mc_java_files *files = NULL;
    mc_game_settings_classes *classes = NULL;
    mc_java_file *parent = NULL;
    mc_game_settings *settings = NULL;
    mc_game_settings_source_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    CHECK(mc_java_files_create(NULL, &files, &error->file) == MC_JAVA_FILE_OK);
    CHECK(ascii_parent(files, directory, &parent, &error->file) == 0);
    CHECK(mc_game_settings_classes_create(files, NULL, NULL, &classes, error) ==
          MC_GAME_SETTINGS_OK);
    const mc_game_settings_profile input_profile = {MC_JAVA_READER_UTF8,
#ifdef _WIN32
                                                    MC_JAVA_WRITER_CRLF
#else
                                                    MC_JAVA_WRITER_LF
#endif
    };
    CHECK(mc_game_settings_source_allocate(classes, &input_profile, &settings, error) ==
          MC_GAME_SETTINGS_OK);
    const mc_game_settings_status result =
        mc_game_settings_source_construct(settings, NULL, parent, NULL, error);
    mc_game_settings_source_state source;
    mc_game_settings_state values;
    CHECK(mc_game_settings_source_inspect(settings, &source));
    CHECK(mc_game_settings_read_state(settings, &values));
    CHECK(source.N != NULL && source.N != parent && values.file_identity == source.N &&
          values.options_path == NULL && values.minecraft == NULL && source.source_stores == 36);
    mc_game_settings_class_state class_state;
    CHECK(mc_game_settings_classes_inspect(classes, &class_state));
    if (strcmp(kind, "absent") == 0 || strcmp(kind, "valid") == 0)
        CHECK(!class_state.stdout_captured && !class_state.stdout_borrowed);
    if (strcmp(kind, "directory") == 0) {
        CHECK(result == MC_GAME_SETTINGS_UNIMPLEMENTED_DEPENDENCY && error->site.pc == 500);
        CHECK(error->exception_class != NULL && error->exception_message == NULL &&
              error->file.status == MC_JAVA_FILE_IO_ERROR && error->output_completed);
        CHECK(source.phase == MC_GAME_SETTINGS_SOURCE_FAILED && source.pending_stream != NULL &&
              source.load_report->outer_exception_caught &&
              !source.load_report->source_close_reached);
    } else {
        CHECK(result == MC_GAME_SETTINGS_OK && source.phase == MC_GAME_SETTINGS_SOURCE_READY);
        if (strcmp(kind, "absent") == 0) {
            CHECK(source.last_load_pc == 10 && source.load_report->file_absent &&
                  !source.load_report->source_opened && source.pending_stream == NULL);
        } else {
            CHECK(source.last_load_pc == 484 && source.load_report->source_opened &&
                  source.load_report->source_close_reached &&
                  !source.load_report->outer_exception_caught);
            CHECK(values.values.music_volume == 0.25f && values.values.sound_volume == 0.75f);
            if (strcmp(kind, "bad") == 0) {
                CHECK(source.load_report->bad_options == 1 && source.failure->output_completed);
            } else
                CHECK(strcmp(kind, "valid") == 0 && source.load_report->bad_options == 0);
        }
    }
    CHECK(mc_game_settings_retire(&settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_classes_release(&classes, error) == MC_GAME_SETTINGS_OK);
    CHECK(mc_java_file_release(&parent, &error->file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_files_release(&files, &error->file) == MC_JAVA_FILE_OK);
    free(error);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc >= 2);
    if (strcmp(argv[1], "cpu") == 0)
        CHECK(cpu() == 0);
    else {
        CHECK(argc == 3);
        CHECK(filesystem(argv[2], argv[1]) == 0);
    }
    printf("Typed Settings source contracts: %u checks\n", checks);
    return 0;
}
