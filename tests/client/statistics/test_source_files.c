#include "source_fixture.h"

/* Test-owned read-only inspection with the platform's real CRT operation. */
static FILE *inspection_open(const char *path) {
#ifdef _WIN32
    FILE *stream = NULL;
    return fopen_s(&stream, path, "rb") == 0 ? stream : NULL;
#else
    return fopen(path, "rb");
#endif
}

static bool ready(source_fixture *f) {
    SOURCE_CHECK(source_directory("normal"));
    mc_java_file *parent = NULL;
    SOURCE_CHECK(source_file(f, "normal", &parent));
    mc_client_stats_writer *writer = NULL;
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                 MC_CLIENT_STATS_OK);
    mc_client_stats_classes_state cold;
    SOURCE_CHECK(mc_client_stats_classes_read_state(f->classes, &cold));
    SOURCE_CHECK(!cold.resolved && cold.holds == 1);
    SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, f->classes, f->session, f->files,
                                                       parent, NULL, NULL) == MC_CLIENT_STATS_OK);
    mc_client_stats_state state;
    SOURCE_CHECK(mc_client_stats_writer_read_state(writer, &state));
    SOURCE_CHECK(state.writer_pc == 185 && state.synchronizer_pc == 558 &&
                 state.workers_started == 1 && state.workers_joined == 0 && !state.dirty &&
                 state.session == f->session);
    mc_client_stats_file_state files;
    SOURCE_CHECK(!mc_client_stats_writer_read_file_state(writer, &files));
    SOURCE_CHECK(mc_client_stats_classes_retire(&f->classes, NULL) == MC_CLIENT_STATS_BUSY &&
                 f->classes != NULL);
    SOURCE_CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_read_file_state(writer, &files));
    SOURCE_CHECK(files.parent == parent && files.directory != parent &&
                 files.pending_file == NULL && files.pending_name == NULL &&
                 !files.pending_builder);
    SOURCE_CHECK(mc_client_stats_classes_read_state(f->classes, &cold) && cold.resolved &&
                 cold.holds == 2);
    mc_java_file_state ds;
    SOURCE_CHECK(mc_java_file_read_state(files.directory, &ds, NULL) == MC_JAVA_FILE_OK);
    SOURCE_CHECK(ds.phase == MC_JAVA_FILE_READY && ds.path != NULL);
    for (size_t i = 0; i < 6; ++i) {
        SOURCE_CHECK(files.children[i] != NULL && state.file_identities[i] == files.children[i] &&
                     state.file_paths_utf8[i] == NULL);
        for (size_t j = 0; j < i; ++j)
            SOURCE_CHECK(files.children[i] != files.children[j]);
    }
    /* Native caller occurrences are consumed while the real writer keeps its
     * parent and namespace alive. No passed-in File identity is reconstructed. */
    SOURCE_CHECK(mc_java_file_release(&parent, NULL) == MC_JAVA_FILE_OK && parent == NULL);
    mc_client_stats_writer *second = NULL;
    mc_java_file *p2 = NULL;
    SOURCE_CHECK(source_directory("second") && source_file(f, "second", &p2));
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &second, NULL) ==
                 MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_construct_file(second, f->classes, f->session, f->files, p2,
                                                       NULL, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_join(second, NULL) == MC_CLIENT_STATS_OK);
    mc_client_stats_file_state other;
    SOURCE_CHECK(mc_client_stats_writer_read_file_state(second, &other));
    SOURCE_CHECK(mc_java_file_read_state(other.directory, &ds, NULL) == MC_JAVA_FILE_OK &&
                 ds.path != NULL);
    mc_client_stats_classes_state shared;
    SOURCE_CHECK(mc_client_stats_classes_read_state(f->classes, &shared) &&
                 shared.stats_literal == cold.stats_literal && shared.holds == 3);
    SOURCE_CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK &&
                 writer == NULL);
    SOURCE_CHECK(mc_client_stats_writer_retire(&second, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_java_file_release(&p2, NULL) == MC_JAVA_FILE_OK);
    return true;
}

static bool failure_prefixes(source_fixture *f) {
    mc_client_stats_writer *writer = NULL;
    mc_client_stats_error error;
    mc_client_stats_file_state files;
    mc_client_stats_state state;
    /* This call is intentionally in a fresh owned working directory. The
     * relative stats child exists/mkdir prefix precedes null parent list55. */
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                 MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, f->classes, f->session, f->files,
                                                       NULL, NULL, &error) ==
                     MC_CLIENT_STATS_SOURCE_EXCEPTION &&
                 error.bytecode == 55);
    SOURCE_CHECK(error.exception == MC_CLIENT_STATS_NULL_REFERENCE);
    SOURCE_CHECK(mc_client_stats_writer_read_state(writer, &state) && state.writer_pc >= 38 &&
                 state.synchronizer_identity == NULL && state.total_identity != NULL &&
                 state.unsent_identity != NULL);
    SOURCE_CHECK(mc_client_stats_writer_read_file_state(writer, &files) && files.parent == NULL &&
                 files.directory != NULL && files.listing == NULL && files.children[0] == NULL);
    mc_client_stats_classes_state cs;
    SOURCE_CHECK(mc_client_stats_classes_read_state(f->classes, &cs));
    mc_java_file_state fs;
    SOURCE_CHECK(mc_java_file_read_state(files.directory, &fs, NULL) == MC_JAVA_FILE_OK &&
                 fs.path == cs.stats_literal); /* Null File parent aliases literal String. */
    SOURCE_CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);

    SOURCE_CHECK(source_text("not-directory", "x"));
    mc_java_file *parent = NULL;
    SOURCE_CHECK(source_file(f, "not-directory", &parent));
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                 MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, f->classes, f->session, f->files,
                                                       parent, NULL, &error) ==
                     MC_CLIENT_STATS_SOURCE_EXCEPTION &&
                 error.bytecode == 62);
    SOURCE_CHECK(mc_client_stats_writer_read_file_state(writer, &files) && files.parent == parent &&
                 files.directory != NULL && files.listing == NULL);
    SOURCE_CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_java_file_release(&parent, NULL) == MC_JAVA_FILE_OK);

    SOURCE_CHECK(source_directory("null-session") && source_file(f, "null-session", &parent));
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                 MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, f->classes, NULL, f->files, parent,
                                                       NULL, &error) ==
                     MC_CLIENT_STATS_SOURCE_EXCEPTION &&
                 error.bytecode == 48 && strcmp(error.class_name, "cj") == 0);
    SOURCE_CHECK(mc_client_stats_writer_read_file_state(writer, &files) &&
                 files.pending_file != NULL && files.pending_builder && files.children[0] == NULL &&
                 files.failure->caller_count == 1 && files.failure->callers[0].bytecode == 182 &&
                 files.failure->base.bytecode == 48);
    SOURCE_CHECK(mc_java_file_read_state(files.pending_file, &fs, NULL) == MC_JAVA_FILE_OK &&
                 fs.phase == MC_JAVA_FILE_RAW);
    SOURCE_CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_java_file_release(&parent, NULL) == MC_JAVA_FILE_OK);

    mc_client_stats_classes *limited = NULL;
    SOURCE_CHECK(mc_client_stats_classes_create(1, &limited, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(source_directory("literal-limit") && source_file(f, "literal-limit", &parent));
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                 MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, limited, f->session, f->files,
                                                       parent, NULL,
                                                       &error) == MC_CLIENT_STATS_LIMIT &&
                 error.bytecode == 36);
    SOURCE_CHECK(mc_client_stats_writer_read_file_state(writer, &files) &&
                 files.pending_file != NULL && files.directory == NULL &&
                 files.pending_name == NULL);
    SOURCE_CHECK(mc_client_stats_classes_retire(&limited, NULL) == MC_CLIENT_STATS_BUSY &&
                 limited != NULL);
    SOURCE_CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_classes_retire(&limited, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_java_file_release(&parent, NULL) == MC_JAVA_FILE_OK);
    return true;
}

static bool source_names(source_fixture *f) {
    const uint16_t embedded[] = {'N', 0, 'M'};
    for (size_t mode = 0; mode < 2; ++mode) {
        const char *path = mode == 0 ? "null-user" : "nul-user";
        SOURCE_CHECK(source_directory(path));
        mc_java_file *parent = NULL;
        SOURCE_CHECK(source_file(f, path, &parent));
        mc_player_string *name = NULL;
        if (mode != 0)
            SOURCE_CHECK(mc_player_string_create(embedded, 3, &name) == MC_PLAYER_OK);
        mc_player_session *session = NULL;
        SOURCE_CHECK(mc_player_session_create(f->sessions, name, NULL, &session, NULL) ==
                     MC_PLAYER_OK);
        mc_player_string_release(name);
        mc_client_stats_writer *writer = NULL;
        SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                     MC_CLIENT_STATS_OK);
        mc_client_stats_error error;
        const mc_client_stats_status expected =
            mode == 0 ? MC_CLIENT_STATS_SOURCE_EXCEPTION : MC_CLIENT_STATS_OK;
        SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, f->classes, session, f->files,
                                                           parent, NULL, &error) == expected);
        if (mode == 0)
            SOURCE_CHECK(error.bytecode == 51 && error.exception == MC_CLIENT_STATS_NULL_REFERENCE);
        else
            SOURCE_CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
        mc_client_stats_file_state state;
        SOURCE_CHECK(mc_client_stats_writer_read_file_state(writer, &state));
        if (mode == 0)
            SOURCE_CHECK(state.pending_file != NULL && state.pending_builder &&
                         state.children[0] == NULL);
        else {
            mc_java_file_state file;
            SOURCE_CHECK(mc_java_file_read_state(state.children[0], &file, NULL) ==
                         MC_JAVA_FILE_OK);
            const mc_nbt_string units = mc_world_info_name_text(file.path);
            bool found = false;
            for (size_t i = 0; i < units.length; ++i)
                if (units.units[i] == 0)
                    found = true;
            SOURCE_CHECK(found);
            bool exists = true;
            SOURCE_CHECK(mc_java_file_exists((mc_java_file *)state.children[0], &exists, NULL) ==
                             MC_JAVA_FILE_OK &&
                         !exists);
        }
        SOURCE_CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
        mc_player_session_retire(&session);
        SOURCE_CHECK(mc_java_file_release(&parent, NULL) == MC_JAVA_FILE_OK);
    }
    return true;
}

static bool frontier(source_fixture *f) {
    SOURCE_CHECK(source_directory("migration"));
    SOURCE_CHECK(source_text("migration/stats_old.dat", "{}"));
    mc_java_file *parent = NULL;
    SOURCE_CHECK(source_file(f, "migration", &parent));
    mc_client_stats_writer *writer = NULL;
    mc_client_stats_error error;
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                 MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, f->classes, f->session, f->files,
                                                       parent, NULL, &error) ==
                     MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY &&
                 error.bytecode == 157);
    mc_client_stats_file_state s;
    SOURCE_CHECK(mc_client_stats_writer_read_file_state(writer, &s));
    SOURCE_CHECK(s.pending_file != NULL && s.pending_builder && s.pending_name != NULL &&
                 s.children[0] == NULL);
    FILE *stream = inspection_open("migration/stats_old.dat");
    SOURCE_CHECK(stream != NULL && fclose(stream) == 0);
    stream = inspection_open("migration/stats/stats_old.dat");
    SOURCE_CHECK(stream == NULL);
    SOURCE_CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_java_file_release(&parent, NULL) == MC_JAVA_FILE_OK);
    return true;
}

static bool io_and_write_frontiers(source_fixture *f) {
    SOURCE_CHECK(source_directory("parse-frontier") && source_directory("parse-frontier/stats"));
    SOURCE_CHECK(source_text("parse-frontier/stats/stats_player123_unsent.dat", "["));
    mc_java_file *parent = NULL;
    SOURCE_CHECK(source_file(f, "parse-frontier", &parent));
    mc_client_stats_writer *writer = NULL;
    mc_client_stats_error error;
    mc_client_stats_report report;
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                 MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, f->classes, f->session, f->files,
                                                       parent, &report, &error) ==
                     MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY &&
                 error.bytecode == 290 && strcmp(error.class_name, "xi") == 0 &&
                 report.syntax_caught);
    mc_client_stats_file_state files;
    SOURCE_CHECK(mc_client_stats_writer_read_file_state(writer, &files));
    SOURCE_CHECK(files.pending_reader && files.pending_stream != NULL);
    mc_java_file_stream_state stream;
    SOURCE_CHECK(
        mc_java_file_stream_read_state(files.pending_stream, &stream, NULL) == MC_JAVA_FILE_OK &&
        stream.native_open && !stream.source_close_reached && !stream.source_close_succeeded);
    SOURCE_CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK &&
                 writer == NULL);
    SOURCE_CHECK(mc_java_file_release(&parent, NULL) == MC_JAVA_FILE_OK);

    SOURCE_CHECK(source_directory("write-frontier") && source_file(f, "write-frontier", &parent));
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                 MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, f->classes, f->session, f->files,
                                                       parent, NULL, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_increment(writer, f->statistic, 1, NULL) ==
                 MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_save(writer, NULL, &error) ==
                     MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY &&
                 error.bytecode == 0);
    SOURCE_CHECK(mc_client_stats_writer_read_file_state(writer, &files) &&
                 files.failure->caller_count == 1 && files.failure->callers[0].bytecode == 53);
    mc_client_stats_state state;
    SOURCE_CHECK(mc_client_stats_writer_read_state(writer, &state) && state.busy && state.dirty &&
                 state.unsent_count == 1 && state.total_count == 1);
    SOURCE_CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_java_file_release(&parent, NULL) == MC_JAVA_FILE_OK);
    return true;
}

static bool worker_error_retirement(source_fixture *f) {
    SOURCE_CHECK(source_directory("worker-frontier") && source_directory("worker-frontier/stats"));
    SOURCE_CHECK(source_text("worker-frontier/stats/stats_player123.dat", "["));
    mc_java_file *parent = NULL;
    SOURCE_CHECK(source_file(f, "worker-frontier", &parent));
    mc_client_stats_writer *writer = NULL;
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                 MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, f->classes, f->session, f->files,
                                                       parent, NULL, NULL) == MC_CLIENT_STATS_OK);
    mc_client_stats_file_error error;
    SOURCE_CHECK(mc_client_stats_writer_retire_file(&writer, &error) ==
                     MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY &&
                 writer == NULL && error.base.bytecode == 290 &&
                 strcmp(error.base.class_name, "xi") == 0 &&
                 error.caught.exception == MC_CLIENT_STATS_JSON_SYNTAX);
    bool worker_frame = false;
    for (size_t i = 0; i < error.caller_count; ++i)
        if (strcmp(error.callers[i].class_name, "vk") == 0 && error.callers[i].bytecode == 90)
            worker_frame = true;
    SOURCE_CHECK(worker_frame); /* Value survives successful native owner consumption. */
    SOURCE_CHECK(mc_java_file_release(&parent, NULL) == MC_JAVA_FILE_OK);
    return true;
}

static bool fallback(source_fixture *f, bool typed) {
    const char *directory = typed ? "typed-order" : "legacy-order";
    SOURCE_CHECK(source_directory(directory));
    mc_client_stats_writer *writer = NULL;
    SOURCE_CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
                 MC_CLIENT_STATS_OK);
    mc_java_file *parent = NULL;
    if (typed) {
        SOURCE_CHECK(source_file(f, directory, &parent));
        SOURCE_CHECK(mc_client_stats_writer_construct_file(writer, f->classes, f->session, f->files,
                                                           parent, NULL,
                                                           NULL) == MC_CLIENT_STATS_OK);
    } else
        SOURCE_CHECK(mc_client_stats_writer_construct(writer, f->session, directory, NULL, NULL) ==
                     MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
    char tmp[128], old[128];
    SOURCE_CHECK(snprintf(tmp, sizeof(tmp), "%s/stats/stats_player123_unsent.tmp", directory) > 0);
    SOURCE_CHECK(snprintf(old, sizeof(old), "%s/stats/stats_player123_unsent.old", directory) > 0);
    SOURCE_CHECK(source_map_file(f, tmp, 17) && source_map_file(f, old, 43));
    mc_client_stats_map *map = NULL;
    mc_client_stats_report report = {0};
    mc_client_stats_error error;
    stats_error_clear(&error);
    SOURCE_CHECK(stats_enter(writer, &error));
    const mc_client_stats_status status = stats_read_file_set(writer, 0, &map, &report, &error);
    stats_leave(writer);
    SOURCE_CHECK(status == MC_CLIENT_STATS_OK && map != NULL);
    bool present = false;
    int32_t value = 0;
    SOURCE_CHECK(mc_client_stats_map_get(map, f->statistic, &present, &value, NULL) ==
                     MC_CLIENT_STATS_OK &&
                 present && value == 17);
    mc_client_stats_map_retire(&map);
    /* The real helper chooses the first EXISTING file. This is not a claim
     * that a malformed existing dat would be skipped after parser rejection. */
    SOURCE_CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_java_file_release(&parent, NULL) == MC_JAVA_FILE_OK);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3 || SOURCE_CD(argv[2]) != 0) {
        fputs("Stats source input: resources work-directory\n", stderr);
        return 2;
    }
    source_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return 1;
    if (!source_setup(f, argv[1]) || !ready(f) || !failure_prefixes(f) || !source_names(f) ||
        !frontier(f) || !io_and_write_frontiers(f) || !worker_error_retirement(f) ||
        !fallback(f, false) || !fallback(f, true) || !source_teardown(f))
        return 1;
    free(f);
    printf("Stats source files: %zu checks\n", source_checks);
    return 0;
}
