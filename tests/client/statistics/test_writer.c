#include "../../../src/client/statistics/internal.h"
#include "mc/resources/files/files.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#define CHANGE_DIR(p) _chdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MAKE_DIR(p) mkdir((p), 0777)
#define CHANGE_DIR(p) chdir(p)
#endif

static size_t checks;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Stats contract:%d: %s\n", __LINE__, #c);                              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_resource_files *resources;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_player_session_classes *sessions;
    mc_player_session *session;
    const mc_statistic *inventory, *child;
    size_t diagnostics;
    mc_client_stats_options options;
} fixture;

static mc_client_stats_status diagnostic(void *context, const mc_client_stats_diagnostic *event) {
    fixture *f = context;
    (void)event;
    ++f->diagnostics;
    return MC_CLIENT_STATS_OK;
}

static bool make_directory(const char *path) {
    return MAKE_DIR(path) == 0 || errno == EEXIST;
}

static bool bytes_file(const char *path, const char *text) {
    FILE *stream = fopen(path, "wb");
    CHECK(stream != NULL);
    CHECK(fwrite(text, 1, strlen(text), stream) == strlen(text));
    CHECK(fclose(stream) == 0);
    return true;
}

static bool map_file(const char *path, const mc_client_stats_map *map) {
    mc_client_stats_text *text = NULL;
    static const uint16_t local[] = {'l', 'o', 'c', 'a', 'l'};
    CHECK(mc_client_stats_serialize(map, (mc_client_stats_string){.is_null = true},
                                    (mc_client_stats_string){local, 5, false}, &text,
                                    NULL) == MC_CLIENT_STATS_OK);
    mc_client_stats_string view;
    CHECK(mc_client_stats_text_read(text, &view));
    FILE *stream = fopen(path, "wb");
    CHECK(stream != NULL);
    for (size_t i = 0; i < view.length; ++i) {
        CHECK(view.units[i] <= 127);
        CHECK(fputc((int)view.units[i], stream) != EOF);
    }
    CHECK(fclose(stream) == 0);
    mc_client_stats_text_retire(&text);
    return true;
}

static bool setup(fixture *f, const char *resources) {
    CHECK(mc_resource_files_create(resources, &f->resources) == MC_REGISTRY_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
          MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->resources)};
    CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_registry_classes_initialize_statistics(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
          MC_PLAYER_OK);
    static const uint16_t name[] = {'P', 'l', 'a', 'y', 'e', 'r', '1', '2', '3'};
    mc_player_string *username = NULL;
    CHECK(mc_player_string_create(name, 9, &username) == MC_PLAYER_OK);
    CHECK(mc_player_session_create(f->sessions, username, NULL, &f->session, NULL) == MC_PLAYER_OK);
    mc_player_string_release(username);
    mc_registry_achievement_view view;
    CHECK(mc_registry_classes_inspect_achievements(f->registry, &view, NULL) == MC_REGISTRY_OK);
    f->inventory = view.named[0];
    f->child = view.named[1];
    f->options = mc_client_stats_default_options();
    f->options.charset = MC_JAVA_READER_UTF8;
    f->options.diagnostics = (mc_client_stats_diagnostics){f, diagnostic};
    return true;
}

static bool teardown(fixture *f) {
    mc_player_session_retire(&f->session);
    CHECK(mc_player_session_classes_free(&f->sessions, NULL) == MC_PLAYER_OK);
    CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    mc_resource_files_free(f->resources);
    return true;
}

static bool project(const char *label, fixture *f, mc_client_stats_writer *writer) {
    mc_client_stats_state state;
    bool present;
    int32_t value;
    CHECK(mc_client_stats_writer_read_state(writer, &state));
    CHECK(mc_client_stats_writer_contains(writer, f->inventory, &present, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_value(writer, f->inventory, &value, NULL) == MC_CLIENT_STATS_OK);
    printf("STATS\t%s\t%zu\t%zu\t%d\t%d\t%ld\t%d\t%d\t%d\t%ld\n", label, state.total_count,
           state.unsent_count, (int)state.dirty, (int)present, (long)value, (int)state.busy,
           (int)(state.pending_main_identity != NULL), (int)(state.pending_other_identity != NULL),
           (long)state.delay);
    return true;
}

static bool fresh(fixture *f) {
    CHECK(make_directory("fresh"));
    mc_client_stats_writer *writer = NULL;
    CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
          MC_CLIENT_STATS_OK);
    mc_client_stats_state state;
    CHECK(mc_client_stats_writer_read_state(writer, &state));
    CHECK(state.phase == MC_CLIENT_STATS_RAW && state.total_identity == NULL &&
          state.workers_started == 0);
    bool present = true;
    mc_client_stats_error error;
    CHECK(mc_client_stats_writer_contains(writer, f->inventory, &present, &error) ==
          MC_CLIENT_STATS_SOURCE_EXCEPTION);
    CHECK(error.bytecode == 5 && present);
    CHECK(mc_client_stats_writer_start_read(writer, &error) == MC_CLIENT_STATS_INVALID_PHASE);
    CHECK(mc_client_stats_writer_construct(writer, f->session, "fresh", NULL, &error) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_read_state(writer, &state));
    CHECK(state.phase == MC_CLIENT_STATS_READY && state.writer_pc == 185 &&
          state.workers_started == 1);
    CHECK(state.total_identity != state.unsent_identity && state.synchronizer_identity != NULL &&
          !state.dirty);
    for (size_t i = 0; i < 6; ++i) {
        CHECK(state.file_identities[i] != NULL);
        for (size_t j = 0; j < i; ++j)
            CHECK(state.file_identities[i] != state.file_identities[j]);
    }
    CHECK(mc_client_stats_writer_join(writer, &error) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_read_state(writer, &state));
    CHECK(!state.busy && state.workers_completed == 1 && state.workers_joined == 1 &&
          state.delay == 100);
    CHECK(project("fresh", f, writer));
    CHECK(mc_client_stats_writer_contains(writer, f->inventory, &present, NULL) ==
              MC_CLIENT_STATS_OK &&
          !present);
    CHECK(mc_client_stats_writer_prerequisite(writer, f->inventory, &present, NULL) ==
              MC_CLIENT_STATS_OK &&
          present);
    CHECK(mc_client_stats_writer_prerequisite(writer, f->child, &present, NULL) ==
              MC_CLIENT_STATS_OK &&
          !present);
    CHECK(mc_client_stats_writer_increment(writer, f->inventory, 0, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_contains(writer, f->inventory, &present, NULL) ==
              MC_CLIENT_STATS_OK &&
          present);
    CHECK(project("zero", f, writer));
    CHECK(mc_client_stats_writer_prerequisite(writer, f->child, &present, NULL) ==
              MC_CLIENT_STATS_OK &&
          present);
    CHECK(mc_client_stats_writer_increment(writer, f->inventory, INT32_MAX, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_increment(writer, f->inventory, 1, NULL) == MC_CLIENT_STATS_OK);
    int32_t value = 0;
    CHECK(mc_client_stats_writer_value(writer, f->inventory, &value, NULL) == MC_CLIENT_STATS_OK &&
          value == INT32_MIN);
    CHECK(project("wrapped", f, writer));
    CHECK(mc_client_stats_writer_increment(writer, f->inventory, -1, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_value(writer, f->inventory, &value, NULL) == MC_CLIENT_STATS_OK &&
          value == INT32_MAX);
    CHECK(mc_client_stats_writer_save(writer, NULL, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_read_state(writer, &state));
    CHECK(state.dirty && state.unsent_count == 1 && state.total_count == 1 && !state.busy);
    CHECK(project("saved", f, writer));
    for (size_t i = 0; i < 100; ++i)
        CHECK(mc_client_stats_writer_service(writer, NULL, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_read_state(writer, &state) && state.delay == 0 &&
          state.workers_started == 1);
    CHECK(mc_client_stats_writer_service(writer, NULL, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_read_state(writer, &state) && state.delay == 99 && state.dirty &&
          state.workers_started == 2 && state.workers_joined == 2);
    CHECK(project("async", f, writer));
    CHECK(mc_client_stats_writer_increment(writer, NULL, -7, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_contains(writer, NULL, &present, NULL) == MC_CLIENT_STATS_OK &&
          present);
    CHECK(mc_client_stats_writer_value(writer, NULL, &value, NULL) == MC_CLIENT_STATS_OK &&
          value == -7);
    mc_client_stats_report report;
    CHECK(mc_client_stats_writer_save(writer, &report, &error) == MC_CLIENT_STATS_OK);
    CHECK(report.file_exception_caught &&
          report.caught.exception == MC_CLIENT_STATS_NULL_REFERENCE &&
          report.caught.bytecode == 149);
    mc_client_stats_map *copy = NULL;
    CHECK(mc_client_stats_writer_copy_unsent(writer, &copy, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_map_size(copy) == 2);
    CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK && writer == NULL);
    CHECK(mc_client_stats_map_get(copy, f->inventory, &present, &value, NULL) ==
              MC_CLIENT_STATS_OK &&
          present && value == INT32_MAX);
    mc_client_stats_map_retire(&copy);
    return true;
}

static bool loaded(fixture *f) {
    CHECK(make_directory("loaded") && make_directory("loaded/stats"));
    mc_client_stats_map *map = NULL;
    CHECK(mc_client_stats_map_create(f->registry, &f->options, &map, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_map_put(map, f->inventory, 3, NULL) == MC_CLIENT_STATS_OK);
    CHECK(map_file("loaded/stats/stats_player123_unsent.dat", map));
    CHECK(mc_client_stats_map_put(map, f->inventory, 40, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_map_put(map, f->child, -1, NULL) == MC_CLIENT_STATS_OK);
    CHECK(map_file("loaded/stats/stats_player123.dat", map));
    mc_client_stats_writer *writer = NULL;
    mc_client_stats_error error;
    CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_construct(writer, f->session, "loaded", NULL, &error) ==
          MC_CLIENT_STATS_OK);
    int32_t value;
    CHECK(mc_client_stats_writer_value(writer, f->inventory, &value, NULL) == MC_CLIENT_STATS_OK &&
          value == 3);
    CHECK(mc_client_stats_writer_join(writer, &error) == MC_CLIENT_STATS_OK);
    mc_client_stats_state state;
    CHECK(mc_client_stats_writer_read_state(writer, &state));
    CHECK(state.pending_main_identity != NULL && state.dirty && state.total_count == 1 &&
          state.delay == 100);
    CHECK(project("pending", f, writer));
    const void *pending = state.pending_main_identity;
    /* Refresh takes the actual pending-map write branch, retaining it. */
    CHECK(mc_client_stats_writer_start_read(writer, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_read_state(writer, &state) &&
          state.pending_main_identity == pending);
    CHECK(mc_client_stats_writer_service(writer, NULL, &error) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_value(writer, f->inventory, &value, NULL) == MC_CLIENT_STATS_OK &&
          value == 43);
    CHECK(mc_client_stats_writer_value(writer, f->child, &value, NULL) == MC_CLIENT_STATS_OK &&
          value == -1);
    CHECK(mc_client_stats_writer_read_state(writer, &state) &&
          state.pending_main_identity == NULL && state.delay == 99 && state.unsent_count == 1);
    CHECK(project("serviced", f, writer));
    CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    mc_client_stats_map_retire(&map);
    return true;
}

static bool files_and_prefixes(fixture *f) {
    const char *const roots[] = {"backup",       "syntax",    "mismatch",  "numeric", "plain",
                                 "null-session", "null-name", "migration", "case"};
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i)
        CHECK(make_directory(roots[i]));
    CHECK(make_directory("backup/stats"));
    CHECK(bytes_file("backup/stats/stats_player123_unsent.old", "{broken"));
    CHECK(bytes_file("backup/stats/stats_player123.old", "{broken"));
    CHECK(make_directory("syntax/stats") &&
          bytes_file("syntax/stats/stats_player123_unsent.dat", "{broken"));
    CHECK(make_directory("mismatch/stats") &&
          bytes_file("mismatch/stats/stats_player123_unsent.dat",
                     "{\"stats-change\":[],\"checksum\":\"bad\"}"));
    CHECK(make_directory("numeric/stats") &&
          bytes_file("numeric/stats/stats_player123_unsent.dat",
                     "{\"stats-change\":[{\"x\":1}],\"checksum\":\"bad\"}"));
    CHECK(bytes_file("plain/stats", "authored plain file"));
    mc_client_stats_map *map = NULL;
    CHECK(mc_client_stats_map_create(f->registry, &f->options, &map, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_map_put(map, f->inventory, 8, NULL) == MC_CLIENT_STATS_OK);
    CHECK(map_file("migration/stats_Player123_unsent.dat", map));
    CHECK(make_directory("case/stats") && map_file("case/stats/stats_Player123_unsent.dat", map));
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
        mc_client_stats_writer *writer = NULL;
        CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
              MC_CLIENT_STATS_OK);
        mc_player_session *session = f->session, *null_name = NULL;
        if (i == 5)
            session = NULL;
        if (i == 6) {
            CHECK(mc_player_session_create(f->sessions, NULL, NULL, &null_name, NULL) ==
                  MC_PLAYER_OK);
            session = null_name;
        }
        mc_client_stats_report report;
        mc_client_stats_error error;
        const mc_client_stats_status status =
            mc_client_stats_writer_construct(writer, session, roots[i], &report, &error);
        mc_client_stats_state state;
        CHECK(mc_client_stats_writer_read_state(writer, &state));
        if (i == 5 || i == 6) {
            CHECK(status == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
                  error.exception == MC_CLIENT_STATS_NULL_REFERENCE &&
                  error.bytecode == (i == 5 ? 48u : 51u));
            CHECK(state.phase == MC_CLIENT_STATS_FAILED_CONSTRUCTION && state.writer_pc == 174 &&
                  state.synchronizer_identity == NULL && state.synchronizer_pc == 26 &&
                  state.file_identities[0] == NULL && state.workers_started == 0);
        } else {
            CHECK(status == MC_CLIENT_STATS_OK);
            CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
            CHECK(state.dirty == (i == 1 || i >= 7));
            CHECK(state.total_count == (i >= 7 ? 1u : 0u));
            if (i == 1)
                CHECK(report.syntax_caught && !report.file_exception_caught);
            if (i == 2)
                CHECK(report.checksum_mismatch && !report.file_exception_caught);
            if (i == 3)
                CHECK(report.file_exception_caught &&
                      report.caught.exception == MC_CLIENT_STATS_NUMBER_FORMAT &&
                      report.caught.bytecode == 121);
            if (i == 4) {
                CHECK(mc_client_stats_writer_increment(writer, f->inventory, 1, NULL) ==
                      MC_CLIENT_STATS_OK);
                CHECK(mc_client_stats_writer_save(writer, &report, NULL) == MC_CLIENT_STATS_OK &&
                      report.file_exception_caught &&
                      report.caught.exception == MC_CLIENT_STATS_SOURCE_IO);
            }
        }
        CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
        mc_player_session_retire(&null_name);
    }
    mc_client_stats_map_retire(&map);
    const char *const bad[] = {NULL, "does-not-exist/child"};
    for (size_t i = 0; i < 2; ++i) {
        mc_client_stats_writer *writer = NULL;
        CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
              MC_CLIENT_STATS_OK);
        mc_client_stats_error error;
        CHECK(mc_client_stats_writer_construct(writer, f->session, bad[i], NULL, &error) ==
              MC_CLIENT_STATS_SOURCE_EXCEPTION);
        CHECK(error.bytecode == (i == 0 ? 55u : 62u));
        CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    }
    return true;
}

static bool file_provider_edges(fixture *f) {
    CHECK(make_directory("directory-input") && make_directory("directory-input/stats") &&
          make_directory("directory-input/stats/stats_player123_unsent.dat"));
    mc_client_stats_writer *writer = NULL;
    CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
          MC_CLIENT_STATS_OK);
    mc_client_stats_report report;
    CHECK(mc_client_stats_writer_construct(writer, f->session, "directory-input", &report, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(report.file_exception_caught && report.caught.exception == MC_CLIENT_STATS_SOURCE_IO &&
          report.caught.bytecode == 11);
    CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
    mc_client_stats_state state;
    CHECK(mc_client_stats_writer_read_state(writer, &state) && !state.dirty &&
          state.total_count == 0 && state.unsent_count == 0);
    CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    CHECK(make_directory("old-directory"));
    CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_construct(writer, f->session, "old-directory", NULL, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_increment(writer, f->inventory, 3, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_save(writer, NULL, NULL) == MC_CLIENT_STATS_OK);
    CHECK(make_directory("old-directory/stats/stats_player123_unsent.old"));
    CHECK(mc_client_stats_writer_increment(writer, f->inventory, 1, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_save(writer, NULL, NULL) == MC_CLIENT_STATS_OK);
    FILE *old = fopen("old-directory/stats/stats_player123_unsent.old", "rb");
    CHECK(old != NULL && fgetc(old) == '{');
    CHECK(fclose(old) == 0);
    FILE *current = fopen("old-directory/stats/stats_player123_unsent.dat", "rb");
    CHECK(current != NULL);
    char document[1024] = {0};
    const size_t length = fread(document, 1, sizeof(document) - 1u, current);
    CHECK(length < sizeof(document) - 1u && !ferror(current) &&
          strstr(document, "\"5242880\":4") != NULL);
    CHECK(fclose(current) == 0);
    CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    return true;
}

static bool trailing_backslash_parent(fixture *f) {
#ifdef _WIN32
    (void)f;
#else
    CHECK(make_directory("ending\\"));
    mc_client_stats_writer *writer = NULL;
    CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &writer, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_construct(writer, f->session, "ending\\", NULL, NULL) ==
          MC_CLIENT_STATS_OK);
    mc_client_stats_state state;
    CHECK(mc_client_stats_writer_read_state(writer, &state));
    CHECK(strcmp(state.file_paths_utf8[0], "ending\\/stats/stats_player123_unsent.dat") == 0);
    CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_increment(writer, f->inventory, 2, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_save(writer, NULL, NULL) == MC_CLIENT_STATS_OK);
    FILE *inside = fopen("ending\\/stats/stats_player123_unsent.dat", "rb");
    CHECK(inside != NULL && fgetc(inside) == '{');
    CHECK(fclose(inside) == 0);
    FILE *sibling = fopen("ending\\stats/stats_player123_unsent.dat", "rb");
    CHECK(sibling == NULL);
    CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
#endif
    return true;
}

static bool codec(fixture *f) {
    mc_client_stats_map *map = NULL, *parsed = NULL;
    CHECK(mc_client_stats_map_create(f->registry, &f->options, &map, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_map_put(map, f->inventory, INT32_MIN, NULL) == MC_CLIENT_STATS_OK);
    CHECK(map_file("single.json", map));
    mc_client_stats_map_retire(&map);
    CHECK(mc_client_stats_map_create(f->registry, &f->options, &map, NULL) == MC_CLIENT_STATS_OK);
    mc_registry_statistic_view catalog;
    CHECK(mc_registry_classes_inspect_statistics(f->registry, &catalog, NULL) == MC_REGISTRY_OK);
    CHECK(catalog.map_size != 0 && catalog.map_size < f->options.limits.max_map_entries);
    for (size_t i = 0; i < catalog.map_size; ++i) {
        const mc_statistic *statistic;
        int32_t id;
        CHECK(mc_registry_classes_statistic_entry(f->registry, i, &id, &statistic) ==
              MC_REGISTRY_OK);
        CHECK(mc_client_stats_map_put(map, statistic, id, NULL) == MC_CLIENT_STATS_OK);
    }
    static const uint16_t local[] = {'l', 'o', 'c', 'a', 'l'};
    static const uint16_t raw_user[] = {'a', '"', '\n', 0, 0xd800u};
    mc_client_stats_text *text = NULL;
    mc_client_stats_error error;
    CHECK(mc_client_stats_serialize(map, (mc_client_stats_string){.is_null = true},
                                    (mc_client_stats_string){local, 5, false}, &text,
                                    &error) == MC_CLIENT_STATS_OK);
    mc_client_stats_string view;
    CHECK(mc_client_stats_text_read(text, &view));
    CHECK(mc_client_stats_parse(f->registry, &f->options, view, &parsed, NULL, &error) ==
          MC_CLIENT_STATS_OK);
    CHECK(parsed != NULL && mc_client_stats_map_size(parsed) == catalog.map_size);
    for (size_t i = 0; i < catalog.map_size; ++i) {
        const mc_statistic *key;
        int32_t expected, value;
        bool present;
        CHECK(mc_client_stats_map_entry(map, i, &key, &expected, NULL) == MC_CLIENT_STATS_OK);
        CHECK(mc_client_stats_map_get(parsed, key, &present, &value, NULL) == MC_CLIENT_STATS_OK &&
              present && value == expected);
    }
    printf("Stats catalog: %zu entries, %zu JSON units\n", catalog.map_size, view.length);
    mc_client_stats_map_retire(&parsed);
    mc_client_stats_text_retire(&text);
    CHECK(mc_client_stats_serialize(map, (mc_client_stats_string){raw_user, 5, false},
                                    (mc_client_stats_string){local, 5, false}, &text,
                                    NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_text_read(text, &view) &&
          view.units[28] != 0); /* No escaping/filtering or malformed UTF16 admission. */
    mc_client_stats_text_retire(&text);
    CHECK(mc_client_stats_map_put(map, NULL, 0, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_serialize(map, (mc_client_stats_string){.is_null = true},
                                    (mc_client_stats_string){.is_null = true}, &text,
                                    &error) == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
          error.bytecode == 149);
    mc_client_stats_map_retire(&map);

    static const struct {
        const char *text;
        mc_client_stats_exception exception;
        unsigned int pc;
        bool syntax;
    } malformed[] = {
        {"{", MC_CLIENT_STATS_EXCEPTION_NONE, 0, true},
        {"{}", MC_CLIENT_STATS_JSON_PATH, 43, false},
        {"{\"stats-change\":[]}", MC_CLIENT_STATS_JSON_PATH, 266, false},
        {"{\"stats-change\":[{}]}", MC_CLIENT_STATS_NO_SUCH_ELEMENT, 98, false},
        {"{\"stats-change\":[[]]}", MC_CLIENT_STATS_ILLEGAL_STATE, 81, false},
        {"{\"stats-change\":[{\"x\":1}]}", MC_CLIENT_STATS_NUMBER_FORMAT, 121, false},
        {"{\"stats-change\":[{\"5242880\":null}]}", MC_CLIENT_STATS_ILLEGAL_STATE, 136, false},
        {"{\"stats-change\":[{\"5242880\":1.5}]}", MC_CLIENT_STATS_NUMBER_FORMAT, 139, false},
        {"1", MC_CLIENT_STATS_EXCEPTION_NONE, 0, true},
        {"{\"stats-change\":[],}", MC_CLIENT_STATS_EXCEPTION_NONE, 0, true}};

    for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
        uint16_t units[256];
        const size_t length = strlen(malformed[i].text);
        CHECK(length < 256);
        for (size_t j = 0; j < length; ++j)
            units[j] = (unsigned char)malformed[i].text[j];
        mc_client_stats_report report;
        const mc_client_stats_status status = mc_client_stats_parse(
            f->registry, &f->options, (mc_client_stats_string){units, length, false}, &parsed,
            &report, &error);
        if (malformed[i].syntax)
            CHECK(status == MC_CLIENT_STATS_OK && parsed != NULL && report.syntax_caught &&
                  mc_client_stats_map_size(parsed) == 0);
        else
            CHECK(status == MC_CLIENT_STATS_SOURCE_EXCEPTION &&
                  error.exception == malformed[i].exception && error.bytecode == malformed[i].pc &&
                  parsed == NULL);
        const char *result = status == MC_CLIENT_STATS_OK                   ? "OK"
                             : error.exception == MC_CLIENT_STATS_JSON_PATH ? "ao"
                             : error.exception == MC_CLIENT_STATS_NUMBER_FORMAT
                                 ? "java.lang.NumberFormatException"
                             : error.exception == MC_CLIENT_STATS_NO_SUCH_ELEMENT
                                 ? "java.util.NoSuchElementException"
                                 : "java.lang.IllegalStateException";
        printf("CODEC\t%zu\t%s\n", i, result);
        mc_client_stats_map_retire(&parsed);
    }
    stats_memory *memory = stats_memory_create(65536);
    CHECK(memory != NULL);
    char digest[33];
    CHECK(stats_checksum(
              memory, 1000, MC_JAVA_READER_UTF8, (mc_client_stats_string){.is_null = true},
              (mc_client_stats_string){NULL, 0, false}, digest, &error) == MC_CLIENT_STATS_OK);
    CHECK(strcmp(digest, "37a6259cc0c1dae299a7866489dff0bd") == 0); /* MD5("null"), unpadded. */
    printf("HASH\t0\t%s\n", digest);
    const uint16_t empty[] = {0};
    CHECK(stats_checksum(
              memory, 1000, MC_JAVA_READER_UTF8, (mc_client_stats_string){empty, 0, false},
              (mc_client_stats_string){empty, 0, false}, digest, &error) == MC_CLIENT_STATS_OK);
    CHECK(strcmp(digest, "d41d8cd98f00b204e9800998ecf8427e") == 0);
    printf("HASH\t1\t%s\n", digest);
    const uint16_t letter[] = {'a'}, accent[] = {0xe9u}, surrogate[] = {0xd800u};
    CHECK(stats_checksum(
              memory, 1000, MC_JAVA_READER_UTF8, (mc_client_stats_string){empty, 0, false},
              (mc_client_stats_string){letter, 1, false}, digest, &error) == MC_CLIENT_STATS_OK);
    CHECK(strcmp(digest, "cc175b9c0f1b6a831c399e269772661") == 0);
    printf("HASH\t2\t%s\n", digest);
    CHECK(stats_checksum(
              memory, 1000, MC_JAVA_READER_UTF8, (mc_client_stats_string){local, 5, false},
              (mc_client_stats_string){accent, 1, false}, digest, &error) == MC_CLIENT_STATS_OK);
    printf("HASH\t3\t%s\n", digest);
    CHECK(stats_checksum(
              memory, 1000, MC_JAVA_READER_UTF8, (mc_client_stats_string){local, 5, false},
              (mc_client_stats_string){surrogate, 1, false}, digest, &error) == MC_CLIENT_STATS_OK);
    printf("HASH\t4\t%s\n", digest);
    const uint16_t pair[] = {0xd800u, 0xdc00u};
    CHECK(stats_checksum(memory, 1000, MC_JAVA_READER_US_ASCII,
                         (mc_client_stats_string){empty, 0, false},
                         (mc_client_stats_string){pair, 2, false}, digest,
                         &error) == MC_CLIENT_STATS_SOURCE_EXCEPTION);
    CHECK(error.exception == MC_CLIENT_STATS_INDEX_OUT_OF_BOUNDS && error.bytecode == 38);
    stats_memory_release(memory);
    return true;
}

static bool native_ownership(fixture *f) {
    CHECK(make_directory("shared"));
    mc_client_stats_writer *first = NULL, *second = NULL;
    CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &first, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_allocate(f->registry, &f->options, &second, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_construct(first, f->session, "shared", NULL, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_construct(second, f->session, "shared", NULL, NULL) ==
          MC_CLIENT_STATS_OK);
    mc_player_session_state session_state;
    CHECK(mc_player_session_read(f->session, &session_state) && session_state.native_holds == 3);
    mc_player_session *saved = f->session;
    mc_player_session_retire(&f->session);
    CHECK(mc_player_session_read(saved, &session_state) && session_state.native_holds == 2);
    CHECK(mc_player_session_classes_free(&f->sessions, NULL) == MC_PLAYER_BUSY);
    CHECK(mc_client_stats_writer_retire(&first, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_player_session_read(saved, &session_state) && session_state.native_holds == 1);
    CHECK(mc_client_stats_writer_join(second, NULL) == MC_CLIENT_STATS_OK);
    /* Native resource failure after the actual delay/busy stores is not a
     * fabricated source finally. A completed owner remains safely retireable. */
    second->memory->maximum = stats_memory_bytes(second->memory);
    mc_client_stats_error error;
    CHECK(mc_client_stats_writer_start_read(second, &error) == MC_CLIENT_STATS_LIMIT);
    mc_client_stats_state state;
    CHECK(mc_client_stats_writer_read_state(second, &state) && state.busy && state.delay == 100 &&
          state.workers_started == 1);
    CHECK(mc_client_stats_writer_join(second, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_read_state(second, &state) && state.busy);
    CHECK(mc_client_stats_writer_retire(&second, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_player_session_classes_free(&f->sessions, NULL) == MC_PLAYER_OK);
    /* Recreate the genuine class owner solely for ordinary teardown. */
    CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
          MC_PLAYER_OK);
    return true;
}

static bool partial_and_worker_failure(fixture *f) {
    CHECK(make_directory("partial") && make_directory("partial/stats"));
    mc_client_stats_map *map = NULL;
    CHECK(mc_client_stats_map_create(f->registry, &f->options, &map, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_map_put(map, f->inventory, 5, NULL) == MC_CLIENT_STATS_OK);
    CHECK(map_file("partial/stats/stats_player123.dat", map));
    mc_client_stats_options options = f->options;
    options.limits.max_map_entries = 1;
    mc_client_stats_writer *writer = NULL;
    CHECK(mc_client_stats_writer_allocate(f->registry, &options, &writer, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_construct(writer, f->session, "partial", NULL, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_service(writer, NULL, NULL) == MC_CLIENT_STATS_OK);
    mc_client_stats_error error;
    CHECK(mc_client_stats_writer_increment(writer, f->child, 7, &error) == MC_CLIENT_STATS_LIMIT);
    mc_client_stats_state state;
    CHECK(mc_client_stats_writer_read_state(writer, &state) && state.total_count == 1 &&
          state.unsent_count == 1 && !state.dirty);
    bool present;
    int32_t value;
    CHECK(mc_client_stats_map_get(writer->unsent, f->child, &present, &value, NULL) ==
              MC_CLIENT_STATS_OK &&
          present && value == 7);
    CHECK(mc_client_stats_writer_value(writer, f->child, &value, NULL) == MC_CLIENT_STATS_OK &&
          value == 0);
    /* Real worker starts, then encounters its own native allocation limit.
     * This must execute finally, join its handle and report a native failure. */
    mc_client_stats_map *captured = NULL;
    CHECK(mc_client_stats_writer_copy_unsent(writer, &captured, NULL) == MC_CLIENT_STATS_OK);
    writer->memory->maximum = stats_memory_bytes(writer->memory) + sizeof(stats_worker) + 64u;
    stats_error_clear(&error);
    CHECK(stats_enter(writer, &error));
    CHECK(stats_start_worker(writer, true, captured, &error) == MC_CLIENT_STATS_OK);
    stats_leave(writer);
    CHECK(mc_client_stats_writer_join(writer, &error) == MC_CLIENT_STATS_LIMIT);
    CHECK(mc_client_stats_writer_read_state(writer, &state) && !state.busy &&
          state.last_worker_error.status == MC_CLIENT_STATS_LIMIT && state.workers_started == 2 &&
          state.workers_completed == 2 && state.workers_joined == 2);
    FILE *temporary = fopen("partial/stats/stats_player123_unsent.tmp", "rb");
    CHECK(temporary != NULL && fgetc(temporary) == EOF && !ferror(temporary));
    CHECK(fclose(temporary) == 0);
    CHECK(mc_client_stats_map_size(map) == 1);
    mc_client_stats_map_retire(&captured);
    CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    mc_client_stats_map_retire(&map);
    return true;
}

static bool pending_failure(fixture *f) {
    CHECK(make_directory("pending-failure") && make_directory("pending-failure/stats"));
    mc_client_stats_map *unsent = NULL, *main_map = NULL;
    CHECK(mc_client_stats_map_create(f->registry, &f->options, &unsent, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_map_create(f->registry, &f->options, &main_map, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_map_put(unsent, f->inventory, 3, NULL) == MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_map_put(main_map, f->child, 8, NULL) == MC_CLIENT_STATS_OK);
    CHECK(map_file("pending-failure/stats/stats_player123_unsent.dat", unsent));
    CHECK(map_file("pending-failure/stats/stats_player123.dat", main_map));
    mc_client_stats_options options = f->options;
    options.limits.max_map_entries = 1;
    mc_client_stats_writer *writer = NULL;
    CHECK(mc_client_stats_writer_allocate(f->registry, &options, &writer, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_construct(writer, f->session, "pending-failure", NULL, NULL) ==
          MC_CLIENT_STATS_OK);
    CHECK(mc_client_stats_writer_join(writer, NULL) == MC_CLIENT_STATS_OK);
    mc_client_stats_state before, after;
    CHECK(mc_client_stats_writer_read_state(writer, &before) &&
          before.pending_main_identity != NULL);
    CHECK(mc_client_stats_writer_service(writer, NULL, NULL) == MC_CLIENT_STATS_LIMIT);
    CHECK(mc_client_stats_writer_read_state(writer, &after) &&
          after.pending_main_identity == before.pending_main_identity && after.delay == 99 &&
          after.total_count == 1 && after.unsent_count == 1 && after.dirty);
    CHECK(mc_client_stats_writer_retire(&writer, NULL) == MC_CLIENT_STATS_OK);
    mc_client_stats_map_retire(&unsent);
    mc_client_stats_map_retire(&main_map);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("Stats contract input: resources work-directory\n", stderr);
        return 2;
    }
    if (CHANGE_DIR(argv[2]) != 0) {
        fputs("Stats contract input: work-directory\n", stderr);
        return 2;
    }
    fixture f = {0};
    if (!setup(&f, argv[1]) || !fresh(&f) || !loaded(&f) || !files_and_prefixes(&f) ||
        !file_provider_edges(&f) || !trailing_backslash_parent(&f) || !codec(&f) ||
        !partial_and_worker_failure(&f) || !pending_failure(&f) || !native_ownership(&f) ||
        !teardown(&f))
        return 1;
    printf("Client stats file: %zu checks\n", checks);
    return 0;
}
