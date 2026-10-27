#include "application_directory_classes.h"
#include "application_owner_internal.h"
#include "application_startup_internal.h"

#include <stdlib.h>
#include <string.h>

#define MC_CLASS "net/minecraft/client/Minecraft"
#define DIRECTORY_DESCRIPTOR "(Ljava/lang/String;)Ljava/io/File;"

static mc_client_startup_status directory_fail(mc_client_application_startup *startup,
                                               mc_client_startup_status status,
                                               const char *operation, unsigned int pc) {
    startup->last_directory_pc = (int32_t)pc;
    return application_startup_fail(&startup->failure, status, operation, MC_CLASS, "a",
                                    DIRECTORY_DESCRIPTOR, pc);
}

static mc_client_startup_status file_failure(mc_client_application_startup *startup,
                                             unsigned int pc) {
    return directory_fail(startup, MC_CLIENT_STARTUP_FILE_FAILED,
                          "actual working-directory File child", pc);
}

void application_directory_builder_destroy(application_directory_builder **slot) {
    if (slot == NULL || *slot == NULL)
        return;
    free((*slot)->units);
    free(*slot);
    *slot = NULL;
}

static mc_client_startup_status builder_allocate(mc_client_application_startup *startup,
                                                 unsigned int allocation_pc,
                                                 unsigned int constructor_pc) {
    startup->last_directory_pc = (int32_t)allocation_pc;
    startup->builder = calloc(1, sizeof(*startup->builder));
    if (startup->builder == NULL)
        return directory_fail(startup, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                              "actual StringBuilder native allocation", allocation_pc);
    startup->last_directory_pc = (int32_t)constructor_pc;
    startup->builder->units = malloc(16 * sizeof(*startup->builder->units));
    if (startup->builder->units == NULL)
        return directory_fail(startup, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                              "actual StringBuilder initial UTF16 capacity", constructor_pc);
    startup->builder->capacity = 16;
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status builder_append(mc_client_application_startup *startup,
                                               mc_nbt_string text, unsigned int pc) {
    application_directory_builder *builder = startup->builder;
    startup->last_directory_pc = (int32_t)pc;
    if (text.length > startup->options.maximum_builder_units ||
        builder->length > startup->options.maximum_builder_units - text.length)
        return directory_fail(startup, MC_CLIENT_STARTUP_LIMIT,
                              "StringBuilder native UTF16 resource limit", pc);
    const size_t required = builder->length + text.length;
    if (required > builder->capacity) {
        size_t capacity = builder->capacity;
        if (capacity <= (SIZE_MAX - 2) / 2)
            capacity = capacity * 2 + 2;
        else
            capacity = required;
        if (capacity < required)
            capacity = required;
        if (capacity > startup->options.maximum_builder_units)
            capacity = startup->options.maximum_builder_units;
        if (capacity > SIZE_MAX / sizeof(*builder->units))
            return directory_fail(startup, MC_CLIENT_STARTUP_LIMIT,
                                  "StringBuilder native addressable capacity", pc);
        uint16_t *units = realloc(builder->units, capacity * sizeof(*units));
        if (units == NULL)
            return directory_fail(startup, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                  "StringBuilder real UTF16 expansion", pc);
        builder->units = units;
        builder->capacity = capacity;
    }
    if (text.length != 0)
        memcpy(builder->units + builder->length, text.units, text.length * sizeof(*text.units));
    builder->length = required;
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status builder_character(mc_client_application_startup *startup,
                                                  uint16_t unit, unsigned int pc) {
    return builder_append(startup, (mc_nbt_string){&unit, 1}, pc);
}

static mc_client_startup_status builder_name(mc_client_application_startup *startup,
                                             mc_world_info_name *name, unsigned int pc) {
    /* These source arguments are all nonnull. Do not silently turn a provider
     * refusal into StringBuilder's source-null textual "null" behavior. */
    if (name == NULL)
        return directory_fail(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                              "nonnull directory String argument occurrence", pc);
    return builder_append(startup, mc_world_info_name_text(name), pc);
}

static mc_client_startup_status builder_string(mc_client_application_startup *startup,
                                               unsigned int pc) {
    startup->last_directory_pc = (int32_t)pc;
    const application_directory_builder *builder = startup->builder;
    startup->failure.string =
        mc_world_info_name_copy((mc_nbt_string){builder->units, builder->length},
                                startup->options.maximum_string_bytes, &startup->directory_child);
    if (startup->failure.string != MC_WORLD_INFO_OK)
        return directory_fail(startup, MC_CLIENT_STARTUP_STRING_FAILED,
                              "actual StringBuilder.toString immutable identity", pc);
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status
dot_child(mc_client_application_startup *startup, mc_world_info_name *name,
          unsigned int raw_file_pc, unsigned int builder_pc, unsigned int constructor_pc,
          unsigned int dot_pc, unsigned int name_pc, unsigned int slash_pc, unsigned int string_pc,
          unsigned int file_constructor_pc, mc_world_info_name *parent) {
    startup->last_directory_pc = (int32_t)raw_file_pc;
    if (mc_java_file_allocate(startup->services.files, &startup->directory_candidate,
                              &startup->failure.file) != MC_JAVA_FILE_OK)
        return file_failure(startup, raw_file_pc);
    if (builder_allocate(startup, builder_pc, constructor_pc) != MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    if (raw_file_pc == 94) {
        /* The actual APPDATA-present branch uses ldc "."106/String append108;
         * the other two dot branches use bipush46/char append. Keep the same
         * canonical dot literal already resolved at the earlier ldc2. */
        mc_world_info_name *dot = NULL;
        startup->last_directory_pc = 106;
        if (application_directory_literal(startup->app->source_namespace, APPLICATION_DIRECTORY_DOT,
                                          startup->options.maximum_string_bytes, &dot,
                                          &startup->failure) != MC_CLIENT_STARTUP_OK ||
            builder_name(startup, dot, dot_pc) != MC_CLIENT_STARTUP_OK)
            return startup->failure.status;
    } else if (builder_character(startup, '.', dot_pc) != MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    if (builder_name(startup, name, name_pc) != MC_CLIENT_STARTUP_OK ||
        builder_character(startup, '/', slash_pc) != MC_CLIENT_STARTUP_OK ||
        builder_string(startup, string_pc) != MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    startup->last_directory_pc = (int32_t)file_constructor_pc;
    if (mc_java_file_construct_strings(startup->directory_candidate, parent,
                                       startup->directory_child,
                                       &startup->failure.file) != MC_JAVA_FILE_OK)
        return file_failure(startup, file_constructor_pc);
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status plain_child(mc_client_application_startup *startup,
                                            mc_world_info_name *name, bool mac) {
    const unsigned int allocation_pc = mac ? 166u : 197u;
    startup->last_directory_pc = (int32_t)allocation_pc;
    if (mc_java_file_allocate(startup->services.files, &startup->directory_candidate,
                              &startup->failure.file) != MC_JAVA_FILE_OK)
        return file_failure(startup, allocation_pc);
    if (builder_allocate(startup, mac ? 171u : 202u, mac ? 175u : 206u) != MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    if (mac) {
        mc_world_info_name *prefix = NULL;
        if (application_directory_literal(startup->app->source_namespace,
                                          APPLICATION_DIRECTORY_MAC_PREFIX,
                                          startup->options.maximum_string_bytes, &prefix,
                                          &startup->failure) != MC_CLIENT_STARTUP_OK ||
            builder_name(startup, prefix, 180) != MC_CLIENT_STARTUP_OK ||
            builder_name(startup, name, 184) != MC_CLIENT_STARTUP_OK ||
            builder_string(startup, 187) != MC_CLIENT_STARTUP_OK)
            return startup->failure.status;
    } else if (builder_name(startup, name, 210) != MC_CLIENT_STARTUP_OK ||
               builder_character(startup, '/', 215) != MC_CLIENT_STARTUP_OK ||
               builder_string(startup, 218) != MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    const unsigned int constructor_pc = mac ? 190u : 221u;
    startup->last_directory_pc = (int32_t)constructor_pc;
    if (mc_java_file_construct_strings(startup->directory_candidate, startup->home,
                                       startup->directory_child,
                                       &startup->failure.file) != MC_JAVA_FILE_OK)
        return file_failure(startup, constructor_pc);
    return MC_CLIENT_STARTUP_OK;
}

static unsigned int platform_ordinal(mc_client_directory_platform platform) {
    switch (platform) {
        case MC_CLIENT_DIRECTORY_LINUX:
            return 0;
        case MC_CLIENT_DIRECTORY_SOLARIS:
            return 1;
        case MC_CLIENT_DIRECTORY_WINDOWS:
            return 2;
        case MC_CLIENT_DIRECTORY_MACOS:
            return 3;
        default:
            return 4;
    }
}

static mc_client_startup_status select_directory(mc_client_application_startup *startup,
                                                 mc_world_info_name *name) {
    mc_client_application_classes *classes = startup->app->source_namespace;
    mc_world_info_name *home_key = NULL, *default_dot = NULL;
    if (application_directory_literal(classes, APPLICATION_DIRECTORY_HOME_KEY,
                                      startup->options.maximum_string_bytes, &home_key,
                                      &startup->failure) != MC_CLIENT_STARTUP_OK ||
        application_directory_literal(classes, APPLICATION_DIRECTORY_DOT,
                                      startup->options.maximum_string_bytes, &default_dot,
                                      &startup->failure) != MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    /* Native default-environment profile. The concrete provider reuses the
     * existing real home-discovery bodies, caches once, and returns a separate
     * counted occurrence. It does NOT claim mutable System-properties support
     * or a fabricated missing-property/default-dot result. */
    startup->last_directory_pc = 4;
    if (mc_client_directory_inputs_home(startup->services.directory_inputs, &startup->home,
                                        &startup->failure.directory_prefix,
                                        &startup->failure.directory) != MC_CLIENT_GAME_DIRECTORY_OK)
        return directory_fail(startup, MC_CLIENT_STARTUP_DIRECTORY_FAILED,
                              "native equivalent of actual default user.home query", 4);
    if (startup->home == NULL)
        return directory_fail(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                              "provider must preserve successful present home identity", 4);
    (void)home_key;
    (void)default_dot; /* Actual ldc identities resolved before the provider call. */
    startup->last_directory_pc = 8;
    if (application_directory_dispatch_activate(classes, startup->options.maximum_string_bytes,
                                                &startup->failure) != MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    mc_client_directory_platform platform;
    startup->last_directory_pc = 11;
    if (mc_client_directory_inputs_platform(startup->services.directory_inputs, &platform,
                                            &startup->failure.directory) !=
        MC_CLIENT_GAME_DIRECTORY_OK)
        return directory_fail(startup, MC_CLIENT_STARTUP_DIRECTORY_FAILED,
                              "native equivalent of actual default os.name classification", 11);
    int32_t branch = 0;
    if (application_directory_dispatch_case(classes, platform_ordinal(platform), &branch,
                                            &startup->failure) != MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    if (branch == 1 || branch == 2) {
        if (dot_child(startup, name, 48, 53, 57, 62, 66, 71, 74, 77, startup->home) !=
            MC_CLIENT_STARTUP_OK)
            return startup->failure.status;
    } else if (branch == 3) {
        mc_world_info_name *appdata_key = NULL;
        if (application_directory_literal(classes, APPLICATION_DIRECTORY_APPDATA_KEY,
                                          startup->options.maximum_string_bytes, &appdata_key,
                                          &startup->failure) != MC_CLIENT_STARTUP_OK)
            return startup->failure.status;
        (void)appdata_key;
        startup->last_directory_pc = 86;
        if (mc_client_directory_inputs_appdata(
                startup->services.directory_inputs, &startup->appdata_present, &startup->appdata,
                &startup->failure.directory_prefix,
                &startup->failure.directory) != MC_CLIENT_GAME_DIRECTORY_OK)
            return directory_fail(startup, MC_CLIENT_STARTUP_DIRECTORY_FAILED,
                                  "actual reached native APPDATA query", 86);
        if (startup->appdata_present) {
            if (startup->appdata == NULL)
                return directory_fail(startup, MC_CLIENT_STARTUP_INVALID_PHASE,
                                      "present APPDATA must retain nonnull even when empty", 86);
            if (dot_child(startup, name, 94, 99, 103, 108, 112, 117, 120, 123, startup->appdata) !=
                MC_CLIENT_STARTUP_OK)
                return startup->failure.status;
        } else if (dot_child(startup, name, 130, 135, 139, 144, 148, 153, 156, 159,
                             startup->home) != MC_CLIENT_STARTUP_OK)
            return startup->failure.status;
    } else if (plain_child(startup, name, branch == 4) != MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    bool exists = false;
    startup->last_directory_pc = 226;
    if (mc_java_file_exists(startup->directory_candidate, &exists, &startup->failure.file) !=
        MC_JAVA_FILE_OK)
        return file_failure(startup, 226);
    if (!exists) {
        bool created = false;
        startup->last_directory_pc = 233;
        if (mc_java_file_mkdirs(startup->directory_candidate, &created, &startup->failure.file) !=
            MC_JAVA_FILE_OK)
            return file_failure(startup, 233);
        if (!created)
            return directory_fail(startup, MC_CLIENT_STARTUP_UNIMPLEMENTED,
                                  "actual RuntimeException allocation/diagnostic dependency", 239);
    }
    startup->last_directory_pc = 267;
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status cached_directory(mc_client_application_startup *startup) {
    mc_client_application_classes *classes = startup->app->source_namespace;
    if (mtx_lock(&classes->mutex) != thrd_success)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_OWNER_FAILED,
                                        "actual b File-cache native lock", NULL, NULL, NULL, 0);
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "actual b live File-cache namespace", NULL, NULL, NULL, 0);
    }
    mc_java_file *cached = classes->af; /* actual b getstatic0 */
    if (cached != NULL) {
        if (mc_java_file_namespace(cached) != startup->services.files) {
            (void)mtx_unlock(&classes->mutex);
            return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                            "native canonical File namespace mismatch", NULL, NULL,
                                            NULL, 0);
        }
        const mc_java_file_status status = mc_java_file_retain(cached, &startup->failure.file);
        (void)mtx_unlock(&classes->mutex);
        if (status != MC_JAVA_FILE_OK)
            return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_FILE_FAILED,
                                            "actual b cached File native occurrence", NULL, NULL,
                                            NULL, 0);
        startup->app->game_directory = cached; /* reload14 -> same File return -> Z163 */
    } else {
        (void)mtx_unlock(&classes->mutex);
        mc_world_info_name *name = NULL;
        if (application_directory_literal(classes, APPLICATION_DIRECTORY_MINECRAFT,
                                          startup->options.maximum_string_bytes, &name,
                                          &startup->failure) != MC_CLIENT_STARTUP_OK ||
            select_directory(startup, name) != MC_CLIENT_STARTUP_OK)
            return startup->failure.status;
        if (mtx_lock(&classes->mutex) != thrd_success)
            return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_OWNER_FAILED,
                                            "actual b cache publication native lock", NULL, NULL,
                                            NULL, 0);
        if (classes->native_retiring || classes->af != NULL) {
            (void)mtx_unlock(&classes->mutex);
            return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_INVALID_PHASE,
                                            "exclusive actual b cache publication", NULL, NULL,
                                            NULL, 0);
        }
        const mc_java_file_status status =
            mc_java_file_retain(startup->directory_candidate, &startup->failure.file);
        if (status != MC_JAVA_FILE_OK) {
            (void)mtx_unlock(&classes->mutex);
            return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_FILE_FAILED,
                                            "actual af File occurrence", NULL, NULL, NULL, 0);
        }
        classes->af = startup->directory_candidate; /* actual b putstatic11 */
        startup->app->game_directory = classes->af; /* actual b reload14 -> Z163 */
        startup->directory_candidate = NULL;        /* Transfer native occurrence to Z. */
        (void)mtx_unlock(&classes->mutex);
    }
    startup->Z_stored = true;
    startup->published_Z = startup->app->game_directory;
    startup->last_startup_store = 163;
    return MC_CLIENT_STARTUP_OK;
}

mc_client_startup_status application_startup_directory(mc_client_application_startup *startup) {
    if (cached_directory(startup) != MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    startup->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 167};
    if (mc_client_save_format_allocate(startup->services.save_formats, &startup->format_candidate,
                                       &startup->failure.save) != MC_CLIENT_SAVE_OK)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_SAVE_FAILED,
                                        "actual startup new hi", MC_CLASS, "a", "()V", 167);
    if (mc_java_file_allocate(startup->services.files, &startup->saves_candidate,
                              &startup->failure.file) != MC_JAVA_FILE_OK)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_FILE_FAILED,
                                        "actual startup new saves File", MC_CLASS, "a", "()V", 171);
    mc_java_file *parent = startup->app->game_directory; /* actual getfield Z176 */
    mc_world_info_name *saves = NULL;
    if (application_directory_literal(startup->app->source_namespace, APPLICATION_DIRECTORY_SAVES,
                                      startup->options.maximum_string_bytes, &saves,
                                      &startup->failure) != MC_CLIENT_STARTUP_OK)
        return startup->failure.status;
    if (mc_java_file_construct_child(startup->saves_candidate, parent, saves,
                                     &startup->failure.file) != MC_JAVA_FILE_OK)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_FILE_FAILED,
                                        "actual startup saves File constructor", MC_CLASS, "a",
                                        "()V", 181);
    if (mc_client_save_format_construct(startup->format_candidate, startup->saves_candidate,
                                        &startup->failure.save) != MC_CLIENT_SAVE_OK)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_SAVE_FAILED,
                                        "actual startup hi(File) constructor", MC_CLASS, "a", "()V",
                                        184);
    startup->app->aa = startup->format_candidate; /* actual putfield187 */
    startup->aa_stored = true;
    startup->last_startup_store = 187;
    startup->failure.caller = (mc_client_application_owner_site){MC_CLASS, "a", "()V", 191};
    if (mc_game_settings_source_allocate(startup->services.settings_classes, NULL,
                                         &startup->settings_candidate,
                                         &startup->failure.settings) != MC_GAME_SETTINGS_OK)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_SETTINGS_FAILED,
                                        "actual new kv/class activation", MC_CLASS, "a", "()V",
                                        191);
    /* The real App receiver and its SAME canonical Z are evaluated only after
     * new191 succeeded; no native GameDirectory/path masquerades as File. */
    client_application *receiver = startup->app;
    mc_java_file *directory = receiver->game_directory;
    startup->failure.caller.pc = 200;
    if (mc_game_settings_source_construct(startup->settings_candidate, receiver, directory, NULL,
                                          &startup->failure.settings) != MC_GAME_SETTINGS_OK)
        return application_startup_fail(&startup->failure, MC_CLIENT_STARTUP_SETTINGS_FAILED,
                                        "actual kv(Minecraft,File) constructor", MC_CLASS, "a",
                                        "()V", 200);
    receiver->settings = startup->settings_candidate; /* actual putfield z203 */
    startup->z_stored = true;
    startup->last_startup_store = 203;
    return application_startup_pack(startup);
}
