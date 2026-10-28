#ifndef MC_STATS_SOURCE_FIXTURE_H
#define MC_STATS_SOURCE_FIXTURE_H

#include "../../../src/client/statistics/internal.h"
#include "mc/resources/files/files.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define SOURCE_MKDIR(p) _mkdir(p)
#define SOURCE_CD(p) _chdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define SOURCE_MKDIR(p) mkdir((p), 0777)
#define SOURCE_CD(p) chdir(p)
#endif

static size_t source_checks;
#define SOURCE_CHECK(c)                                                                            \
    do {                                                                                           \
        ++source_checks;                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Stats source contract:%d: %s\n", __LINE__, #c);                       \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct source_fixture {
    mc_resource_files *resources;
    mc_materials *materials;
    mc_java_random_profile *profile;
    mc_entity_classes *entities;
    mc_registry_classes *registry;
    mc_player_session_classes *sessions;
    mc_player_session *session;
    mc_java_files *files;
    mc_client_stats_classes *classes;
    const mc_statistic *statistic;
    mc_client_stats_options options;
} source_fixture;

static inline bool source_directory(const char *path) {
    return SOURCE_MKDIR(path) == 0 || errno == EEXIST;
}

static inline bool source_text(const char *path, const char *text) {
    FILE *stream = fopen(path, "wb");
    SOURCE_CHECK(stream != NULL);
    const size_t length = strlen(text);
    SOURCE_CHECK(fwrite(text, 1, length, stream) == length);
    SOURCE_CHECK(fclose(stream) == 0);
    return true;
}

static inline bool source_file(source_fixture *f, const char *path, mc_java_file **out) {
    uint16_t units[512];
    const size_t length = strlen(path);
    SOURCE_CHECK(length < sizeof(units) / sizeof(units[0]));
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)path[i];
    mc_world_info_name *name = NULL;
    SOURCE_CHECK(mc_world_info_name_copy((mc_nbt_string){units, length}, SIZE_MAX, &name) ==
                 MC_WORLD_INFO_OK);
    SOURCE_CHECK(mc_java_file_allocate(f->files, out, NULL) == MC_JAVA_FILE_OK);
    const mc_java_file_status status = mc_java_file_construct_string(*out, name, NULL);
    mc_world_info_name_release(name);
    SOURCE_CHECK(status == MC_JAVA_FILE_OK);
    return true;
}

static inline bool source_setup(source_fixture *f, const char *resources) {
    SOURCE_CHECK(mc_resource_files_create(resources, &f->resources) == MC_REGISTRY_OK);
    SOURCE_CHECK(mc_materials_create(&f->materials));
    SOURCE_CHECK(mc_java_random_profile_create(SIZE_MAX, &f->profile) == MC_JAVA_RANDOM_PROFILE_OK);
    SOURCE_CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entities) ==
                 MC_ENTITY_OK);
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0,
                                         mc_resource_files_source(f->resources)};
    SOURCE_CHECK(mc_registry_classes_allocate(&options, &f->registry, NULL) == MC_REGISTRY_OK);
    SOURCE_CHECK(mc_registry_classes_initialize_statistics(f->registry, NULL) == MC_REGISTRY_OK);
    SOURCE_CHECK(mc_player_session_classes_create(f->entities, f->registry, &f->sessions, NULL) ==
                 MC_PLAYER_OK);
    static const uint16_t user[] = {'P', 'l', 'a', 'y', 'e', 'r', '1', '2', '3'};
    mc_player_string *username = NULL;
    SOURCE_CHECK(mc_player_string_create(user, 9, &username) == MC_PLAYER_OK);
    SOURCE_CHECK(mc_player_session_create(f->sessions, username, NULL, &f->session, NULL) ==
                 MC_PLAYER_OK);
    mc_player_string_release(username);
    SOURCE_CHECK(mc_java_files_create(NULL, &f->files, NULL) == MC_JAVA_FILE_OK);
    SOURCE_CHECK(mc_client_stats_classes_create(0, &f->classes, NULL) == MC_CLIENT_STATS_OK);
    mc_registry_achievement_view view;
    SOURCE_CHECK(mc_registry_classes_inspect_achievements(f->registry, &view, NULL) ==
                 MC_REGISTRY_OK);
    f->statistic = view.named[0];
    f->options = mc_client_stats_default_options();
    f->options.charset = MC_JAVA_READER_UTF8;
    return true;
}

static inline bool source_teardown(source_fixture *f) {
    SOURCE_CHECK(mc_client_stats_classes_retire(&f->classes, NULL) == MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_java_files_release(&f->files, NULL) == MC_JAVA_FILE_OK);
    mc_player_session_retire(&f->session);
    SOURCE_CHECK(mc_player_session_classes_free(&f->sessions, NULL) == MC_PLAYER_OK);
    SOURCE_CHECK(mc_registry_classes_free(f->registry, NULL) == MC_REGISTRY_OK);
    SOURCE_CHECK(mc_entity_classes_free(f->entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(f->profile);
    mc_materials_destroy(f->materials);
    mc_resource_files_free(f->resources);
    return true;
}

static inline bool source_map_file(source_fixture *f, const char *path, int32_t value) {
    mc_client_stats_map *map = NULL;
    mc_client_stats_text *text = NULL;
    SOURCE_CHECK(mc_client_stats_map_create(f->registry, &f->options, &map, NULL) ==
                 MC_CLIENT_STATS_OK);
    SOURCE_CHECK(mc_client_stats_map_put(map, f->statistic, value, NULL) == MC_CLIENT_STATS_OK);
    static const uint16_t salt[] = {'l', 'o', 'c', 'a', 'l'};
    SOURCE_CHECK(mc_client_stats_serialize(map, (mc_client_stats_string){.is_null = true},
                                           (mc_client_stats_string){salt, 5, false}, &text,
                                           NULL) == MC_CLIENT_STATS_OK);
    mc_client_stats_string view;
    SOURCE_CHECK(mc_client_stats_text_read(text, &view));
    FILE *stream = fopen(path, "wb");
    SOURCE_CHECK(stream != NULL);
    for (size_t i = 0; i < view.length; ++i) {
        SOURCE_CHECK(view.units[i] <= 127);
        SOURCE_CHECK(fputc((int)view.units[i], stream) != EOF);
    }
    SOURCE_CHECK(fclose(stream) == 0);
    mc_client_stats_text_retire(&text);
    mc_client_stats_map_retire(&map);
    return true;
}
#endif
