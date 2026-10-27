#include "client/startup/application_construction_internal.h"
#include "client/texture_packs/internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/client/settings/construction.h"
#include "mc/client/texture_packs/construction.h"
#include "mc/resources/files/files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Authored SOURCE-ONLY controls. Each nonnull App is genuinely whole-constructed.
 * Direct Pack cases are prepared-App scope, not normal startup completion.
 * Test-only fixture constructors below are retained from sealed q. */
static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "File Pack check%u line%d: %s\n", checks, __LINE__, #x);               \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct fixture {
    mc_client_application_constructor_services services;
    mc_materials *materials;
    mc_resource_files *files;
    mc_client_render_engine_classes *graphics;
    mc_client_application_construction_error *error;
} fixture;

static int fixture_open(fixture *f, const char *resources) {
    memset(f, 0, sizeof(*f));
    f->error = calloc(1, sizeof(*f->error));
    CHECK(f->error != NULL);
    CHECK(mc_java_random_profile_create(SIZE_MAX, &f->services.profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_resource_files_create(resources, &f->files) == MC_REGISTRY_OK);
    const mc_registry_options opts = {.side = MC_REGISTRY_CLIENT,
                                      .random_profile = f->services.profile,
                                      .materials = f->materials,
                                      .resources = mc_resource_files_source(f->files)};
    CHECK(mc_registry_classes_allocate(&opts, &f->services.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->services.profile, NULL,
                                   &f->services.entities) == MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(f->services.entities, f->services.profile,
                                          &f->services.render_classes) == MC_CLIENT_MODEL_OK);
    CHECK(mc_client_render_engine_classes_create(NULL, &f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_client_loading_classes_create(f->graphics, &f->services.gui_classes) ==
          MC_CLIENT_LOADING_OK);
    CHECK(mc_client_application_classes_create(NULL, &f->services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_sound_backend_create(f->services.profile, NULL, &f->services.sound_backend, NULL) ==
          MC_SOUND_BACKEND_OK);
    mc_sound_manager_backend backend;
    CHECK(mc_sound_backend_manager_interface(f->services.sound_backend, &backend));
    CHECK(mc_sound_manager_shared_create(&backend, &f->services.sound_shared, NULL) == MC_SOUND_OK);
    return 0;
}

static int fixture_close(fixture *f) {
    CHECK(mc_client_application_classes_release(&f->services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_loading_classes_release(f->services.gui_classes) == MC_CLIENT_LOADING_OK);
    CHECK(mc_client_render_engine_classes_release(f->graphics) == MC_CLIENT_RENDER_ENGINE_OK);
    CHECK(mc_sound_manager_shared_retire(&f->services.sound_shared, NULL) == MC_SOUND_OK);
    CHECK(mc_sound_backend_retire(&f->services.sound_backend, NULL) == MC_SOUND_BACKEND_OK);
    CHECK(mc_client_render_classes_free(f->services.render_classes) == MC_CLIENT_MODEL_OK);
    CHECK(mc_entity_classes_free(f->services.entities) == MC_ENTITY_OK);
    CHECK(mc_registry_classes_free(f->services.registry, NULL) == MC_REGISTRY_OK);
    CHECK(mc_resource_files_free(f->files) == MC_REGISTRY_OK);
    mc_materials_destroy(f->materials);
    mc_java_random_profile_free(f->services.profile);
    free(f->error);
    return 0;
}

static int actual_app(fixture *f, bool fullscreen, client_application **out) {
    CHECK(mc_client_application_constructor_allocate(&f->services, NULL, out, f->error) ==
          MC_CLIENT_APP_OK);
    const mc_client_application_constructor_arguments arguments = {NULL, NULL, NULL,
                                                                   64,   64,   fullscreen};
    CHECK(mc_client_application_constructor_construct(*out, &arguments, f->error) ==
          MC_CLIENT_APP_OK);
    CHECK((*out)->constructor_phase == MC_CLIENT_APP_READY);
    return 0;
}

static int ascii_parent(mc_java_files *files, const char *path, mc_java_file **out,
                        mc_java_file_error *error) {
    const size_t length = strlen(path);
    uint16_t *units = malloc((length == 0 ? 1 : length) * sizeof(*units));
    CHECK(units != NULL);
    for (size_t i = 0; i < length; ++i) {
        CHECK((unsigned char)path[i] < 128); /* Declared owned test path profile. */
        units[i] = (uint16_t)(unsigned char)path[i];
    }
    mc_world_info_name *name = NULL;
    CHECK(mc_world_info_name_copy((mc_nbt_string){units, length}, SIZE_MAX, &name) ==
          MC_WORLD_INFO_OK);
    free(units);
    CHECK(mc_java_file_allocate(files, out, error) == MC_JAVA_FILE_OK);
    CHECK(mc_java_file_construct_string(*out, name, error) == MC_JAVA_FILE_OK);
    mc_world_info_name_release(name);
    return 0;
}

static int finish_app(fixture *f, client_application **app) {
    CHECK(mc_client_application_constructor_retire(app, f->error) == MC_CLIENT_APP_BUSY);
    CHECK(*app != NULL && (*app)->constructor_phase == MC_CLIENT_APP_RETIRING);
    CHECK(mc_client_application_classes_release(&f->services.application_classes, NULL) ==
          MC_CLIENT_APPLICATION_OWNER_OK);
    CHECK(mc_client_application_constructor_retire(app, f->error) == MC_CLIENT_APP_OK &&
          *app == NULL);
    return 0;
}

static int source_case(const char *constructor_resources, const char *pack_resources,
                       const char *parent_path, const char *kind) {
    fixture f;
    CHECK(fixture_open(&f, constructor_resources) == 0);
    client_application *app = NULL;
    CHECK(actual_app(&f, false, &app) == 0);
    mc_resource_files *resources = NULL;
    CHECK(mc_resource_files_create(pack_resources, &resources) == MC_REGISTRY_OK);
    mc_java_files *files = NULL;
    mc_java_file *parent = NULL;
    mc_texture_pack_source_error error = {0};
    CHECK(mc_java_files_create(NULL, &files, &error.file) == MC_JAVA_FILE_OK);
    CHECK(ascii_parent(files, parent_path, &parent, &error.file) == 0);
    const bool null_app = strcmp(kind, "null-app") == 0;
    const bool null_settings = strcmp(kind, "null-settings") == 0;
    const bool missing_icon = strcmp(kind, "missing-icon") == 0;
    const bool bound = strcmp(kind, "bound") == 0;
    const bool metadata_limit = strcmp(kind, "metadata-limit") == 0;
    const bool removed = strcmp(kind, "removed") == 0;
    const bool zip = strcmp(kind, "stored") == 0 || strcmp(kind, "deflated") == 0 || removed;
    const bool normal = strcmp(kind, "empty") == 0 || strcmp(kind, "absent") == 0 ||
                        strcmp(kind, "non-directory") == 0 || strcmp(kind, "mkdir-false") == 0;
    CHECK(null_app || null_settings || missing_icon || bound || metadata_limit || zip || normal);
    mc_game_settings_classes *settings_classes = NULL;
    mc_game_settings_source_error settings_error = {0};
    if (!null_app && !null_settings && !missing_icon && !bound) {
        CHECK(mc_game_settings_classes_create(files, NULL, NULL, &settings_classes,
                                              &settings_error) == MC_GAME_SETTINGS_OK);
        mc_game_settings *settings = NULL;
        CHECK(mc_game_settings_source_allocate(settings_classes, NULL, &settings,
                                               &settings_error) == MC_GAME_SETTINGS_OK);
        CHECK(mc_game_settings_source_construct(settings, app, parent, NULL, &settings_error) ==
              MC_GAME_SETTINGS_OK);
        app->settings = settings; /* Prepared native view after genuine kv READY. */
    } /* Genuine constructor-READY App and genuine kv owner; prepared profile only. */
    mc_texture_pack_options options = mc_client_texture_pack_default_options();
    if (bound)
        options.max_packs = 0;
    if (metadata_limit)
        options.max_archive_bytes = 0;
    mc_client_texture_pack_list *list = NULL;
    CHECK(mc_client_texture_pack_list_source_allocate(files, mc_resource_files_source(resources),
                                                      &options, &list,
                                                      &error) == MC_TEXTURE_PACK_OK);
    mc_texture_pack_source_state raw;
    CHECK(mc_client_texture_pack_list_source_inspect(list, &raw));
    CHECK(raw.f == NULL && raw.pending_file == NULL && raw.retained_pack_count == 0);
    const mc_texture_pack_status result =
        mc_client_texture_pack_list_source_construct(list, null_app ? NULL : app, parent, &error);
    mc_texture_pack_list_state state;
    mc_texture_pack_source_state source;
    CHECK(mc_client_texture_pack_list_inspect(list, &state));
    CHECK(mc_client_texture_pack_list_source_inspect(list, &source));
    CHECK(state.directory == NULL && source.files == files);
    if (bound) {
        CHECK(result == MC_TEXTURE_PACK_LIMIT && error.native.ik_pc == 16);
        CHECK(state.last_store_pc == 12 && source.f == NULL && !state.exists_reached);
    } else if (missing_icon) {
        CHECK(result == MC_TEXTURE_PACK_SOURCE_ILLEGAL_ARGUMENT && error.native.source_pc == 29);
        CHECK(state.last_store_pc == 12 && state.failed_default_pack != NULL && source.f == NULL);
    } else {
        CHECK(source.f != NULL && source.f != parent && state.exists_reached);
        CHECK(mc_java_file_namespace((mc_java_file *)source.f) == files);
        if (null_app || null_settings) {
            CHECK(result == MC_TEXTURE_PACK_NULL_REFERENCE && state.last_store_pc == 53);
            CHECK(error.native.source_pc == (null_app ? 76u : 79u));
        } else if (metadata_limit) {
            CHECK(result == MC_TEXTURE_PACK_LIMIT && error.native.ik_pc == 196);
            CHECK(source.failed_scan && source.scan_files != NULL && source.pending_pack != NULL);
            CHECK(state.cached_count == 1 && state.listed_count == 0 &&
                  source.pending_list != NULL);
            const mc_client_texture_string *key = NULL;
            mc_client_texture_pack *cached = NULL;
            CHECK(mc_client_texture_pack_cache_at(list, 0, &key, &cached));
            mc_texture_pack_state pack;
            mc_texture_pack_file_state typed;
            CHECK(mc_client_texture_pack_inspect(cached, &pack));
            CHECK(mc_client_texture_pack_source_inspect(cached, &typed));
            CHECK(cached == source.pending_pack && key == pack.cache_key &&
                  key == source.pending_key);
            CHECK(typed.h == source.scan_file &&
                  typed.h == mc_java_file_array_at(source.scan_files, source.scan_index));
            CHECK(typed.name_identity != NULL && pack.file == NULL);
            mc_texture_pack_text name;
            CHECK(mc_client_texture_string_read(pack.name, &name));
            const mc_nbt_string identity = mc_world_info_name_text(typed.name_identity);
            CHECK(name.units == identity.units && name.length == identity.length);
        } else {
            CHECK(result == MC_TEXTURE_PACK_OK && state.phase == MC_TEXTURE_PACK_READY);
            CHECK(state.last_store_pc == 82 && state.rescan_store_pc == 343 &&
                  state.selected_pack != NULL);
            CHECK(!source.failed_scan && source.scan_files == NULL && source.pending_pack == NULL &&
                  source.pending_key == NULL);
            if (strcmp(kind, "absent") == 0 || strcmp(kind, "mkdir-false") == 0)
                CHECK(state.mkdirs_reached);
            if (strcmp(kind, "mkdir-false") == 0)
                CHECK(!state.mkdirs_result);
            if (zip)
                CHECK(state.cached_count == 1 && state.listed_count == 2 &&
                      state.selected_pack != state.default_pack);
            else
                CHECK(state.cached_count == 0 && state.listed_count == 1 &&
                      state.selected_pack == state.default_pack);
            mc_client_texture_pack *selected = NULL;
            CHECK(mc_client_texture_pack_selected(list, &selected) && selected != NULL);
            CHECK(mc_client_texture_pack_list_retain(list) == MC_TEXTURE_PACK_OK);
            CHECK(mc_client_texture_pack_list_source_retire(&list, &error) ==
                      MC_TEXTURE_PACK_BUSY &&
                  list != NULL);
            CHECK(mc_client_texture_pack_list_release(list, NULL) == MC_TEXTURE_PACK_OK);
            CHECK(mc_client_texture_pack_retain(selected) == MC_TEXTURE_PACK_OK);
            CHECK(mc_client_texture_pack_list_source_retire(&list, &error) ==
                      MC_TEXTURE_PACK_BUSY &&
                  list != NULL);
            CHECK(mc_client_texture_pack_release(selected, NULL) == MC_TEXTURE_PACK_OK);
            if (zip) {
                static const uint16_t entry[] = {'/', 'a', '.', 't', 'x', 't'};
                mc_client_texture_stream *stream = NULL;
                CHECK(mc_client_texture_pack_open(selected, (mc_texture_pack_text){entry, 6, false},
                                                  &stream, &error.native) == MC_TEXTURE_PACK_OK &&
                      stream != NULL);
                CHECK(mc_client_texture_pack_list_source_retire(&list, &error) ==
                          MC_TEXTURE_PACK_BUSY &&
                      list != NULL);
                unsigned char bytes[8];
                size_t count = 0;
                CHECK(mc_client_texture_stream_read(stream, bytes, sizeof(bytes), &count,
                                                    &error.native) == MC_TEXTURE_PACK_OK);
                CHECK(count == 5 && memcmp(bytes, "first", 5) == 0);
                if (removed) {
                    CHECK(texture_pack_unload(selected, app, &error.native) == MC_TEXTURE_PACK_OK);
                    char path[4096];
                    const int length =
                        snprintf(path, sizeof(path), "%s/texturepacks/stored.zip", parent_path);
                    CHECK(length > 0 && (size_t)length < sizeof(path) && remove(path) == 0);
                    CHECK(mc_client_texture_pack_list_rescan(list, &error.native) ==
                          MC_TEXTURE_PACK_OK);
                    CHECK(mc_client_texture_pack_list_inspect(list, &state));
                    CHECK(state.cached_count == 0 && state.listed_count == 1 &&
                          state.selected_pack == state.default_pack);
                    CHECK(mc_client_texture_pack_list_source_retire(&list, &error) ==
                              MC_TEXTURE_PACK_BUSY &&
                          list != NULL);
                    CHECK(mc_client_texture_pack_retain(selected) == MC_TEXTURE_PACK_OK);
                }
                CHECK(mc_client_texture_stream_close(stream, &error.native) == MC_TEXTURE_PACK_OK);
                CHECK(mc_client_texture_stream_release(stream, &error.native) ==
                      MC_TEXTURE_PACK_OK);
                if (removed) {
                    CHECK(mc_client_texture_pack_list_source_retire(&list, &error) ==
                              MC_TEXTURE_PACK_BUSY &&
                          list != NULL);
                    CHECK(mc_client_texture_pack_release(selected, NULL) == MC_TEXTURE_PACK_OK);
                }
            }
        }
    }
    CHECK(mc_client_texture_pack_list_source_retire(&list, &error) == MC_TEXTURE_PACK_OK &&
          list == NULL);
    CHECK(mc_game_settings_retire(&app->settings, NULL) == MC_GAME_SETTINGS_OK);
    CHECK(mc_game_settings_classes_release(&settings_classes, &settings_error) ==
          MC_GAME_SETTINGS_OK);
    CHECK(mc_java_file_release(&parent, &error.file) == MC_JAVA_FILE_OK);
    CHECK(mc_java_files_release(&files, &error.file) == MC_JAVA_FILE_OK);
    CHECK(mc_resource_files_free(resources) == MC_REGISTRY_OK);
    CHECK(finish_app(&f, &app) == 0);
    CHECK(fixture_close(&f) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 5);
    CHECK(source_case(argv[1], argv[2], argv[3], argv[4]) == 0);
    printf("Typed File Pack contracts: %u checks\n", checks);
    return 0;
}
