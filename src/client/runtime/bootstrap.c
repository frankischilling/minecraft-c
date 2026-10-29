#include "bootstrap.h"
#include "../application_state.h"
#include "../render_engine/textures.h"
#include "../startup/application_construction.h"
#include "../startup/application_startup_internal.h"
#include "../startup/application_startup_menu.h"
#include "mc/block/registry/runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mc_client_bootstrap {
    mc_client_application_constructor_services constructor;
    mc_client_startup_services services;
    mc_materials *materials;
    mc_resource_files *imported_files, *class_files;
    mc_client_render_engine_classes *graphics;
    mc_math_trig *trig;
    client_application *app;
    mc_client_application_startup *startup;
    mc_client_render_manager_classes *manager;
    mc_client_application_construction_error construction_error;
    mc_client_startup_error startup_error;
    mc_client_statistics_owner_error session_error;
    mc_client_render_texture_error texture_error;
    bool retiring, startup_returned;
};

static const char *text(const char *value) {
    return value == NULL ? "unknown" : value;
}

static mc_client_bootstrap_status owner_failure(const char *operation, int status) {
    fprintf(stderr, "Client bootstrap: %s (status%d)\n", operation, status);
    return MC_CLIENT_BOOTSTRAP_OWNER_FAILED;
}

static mc_client_bootstrap_status allocation_failure(const char *operation) {
    fprintf(stderr, "Client bootstrap: allocation failed for %s\n", operation);
    return MC_CLIENT_BOOTSTRAP_ALLOCATION_FAILED;
}

static void construction_failure(const mc_client_application_construction_error *error) {
    fprintf(stderr, "Client constructor: %s (status%d, Minecraft%d, child%d)\n",
            text(error->operation), (int)error->status, error->minecraft_pc, (int)error->child);
    if (error->frontier.class_name != NULL)
        fprintf(stderr, "  %s.%s%s at %d\n", error->frontier.class_name,
                text(error->frontier.method), text(error->frontier.descriptor), error->frontier.pc);
    if (error->child == MC_CLIENT_APP_CHILD_REGISTRY)
        fprintf(stderr, "  Registry: %s (status%d, class%s, pc%d)\n",
                text(error->cause.registry.message), (int)error->cause.registry.status,
                text(error->cause.registry.active_initializer_class),
                error->cause.registry.active_initializer_pc);
}

static void startup_failure(const mc_client_startup_error *error) {
    fprintf(stderr, "Client startup: %s (status%d) at %s.%s%s:%u\n", text(error->operation),
            (int)error->status, text(error->caller.class_name), text(error->caller.method),
            text(error->caller.descriptor), error->caller.pc);
    if (error->child.class_name != NULL)
        fprintf(stderr, "  %s.%s%s at %u\n", error->child.class_name, text(error->child.method),
                text(error->child.descriptor), error->child.pc);
    if (error->display.display.operation != NULL)
        fprintf(stderr, "  Display: %s: %s (status%d, GL%u)\n", error->display.display.operation,
                error->display.display.detail, (int)error->display.display.status,
                error->display.display.gl_error);
    if (error->directory.operation != NULL)
        fprintf(stderr, "  Directory: %s (status%d, native%d)\n", error->directory.operation,
                (int)error->directory.status, error->directory.system_error);
    if (error->file.operation != NULL)
        fprintf(stderr, "  File: %s (status%d, native%d)\n", error->file.operation,
                (int)error->file.status, error->file.system_error);
    if (error->icons.operation != NULL)
        fprintf(stderr, "  Icons: %s (status%d, edge%d, native%lu)\n", error->icons.operation,
                (int)error->icons.status, error->icons.asset_edge, error->icons.platform_error);
}

#define OPEN_OWNER(call, expected, operation)                                                      \
    do {                                                                                           \
        const int bootstrap_owner_code = (int)(call);                                              \
        if (bootstrap_owner_code != (int)(expected))                                               \
            return owner_failure(operation, bootstrap_owner_code);                                 \
    } while (0)

static mc_client_bootstrap_status open_resources(mc_client_bootstrap *context, const char *assets) {
    OPEN_OWNER(mc_resource_files_create(assets, &context->imported_files), MC_REGISTRY_OK,
               "imported SDK resource namespace");
    const size_t length = strlen(assets);
    if (length > SIZE_MAX - 5u)
        return MC_CLIENT_BOOTSTRAP_INVALID_ARGUMENT;
    char *jar = malloc(length + 5u);
    if (jar == NULL)
        return allocation_failure("class resource path");
    memcpy(jar, assets, length);
    memcpy(jar + length, "/jar", 5u);
    const mc_registry_status status = mc_resource_files_create(jar, &context->class_files);
    free(jar);
    if (status != MC_REGISTRY_OK)
        return owner_failure("imported class resource namespace", (int)status);
    context->services.class_files = context->class_files;
    context->services.texture_resources = mc_resource_files_source(context->class_files);
    context->trig = malloc(sizeof(*context->trig));
    if (context->trig == NULL)
        return allocation_failure("native trigonometric table");
    if (!mc_math_trig_init(context->trig))
        return owner_failure("immutable native trigonometric prerequisite", 0);
    context->services.texture_effect_trig = context->trig;
    return MC_CLIENT_BOOTSTRAP_OK;
}

static mc_client_bootstrap_status open_constructor_services(mc_client_bootstrap *context) {
    mc_client_application_constructor_services *services = &context->constructor;
    OPEN_OWNER(mc_java_random_profile_create(SIZE_MAX, &services->profile),
               MC_JAVA_RANDOM_PROFILE_OK, "Random profile");
    if (!mc_materials_create(&context->materials))
        return allocation_failure("Material namespace");
    const mc_registry_options registry = {.side = MC_REGISTRY_CLIENT,
                                          .random_profile = services->profile,
                                          .materials = context->materials,
                                          .resources = context->services.texture_resources};
    OPEN_OWNER(mc_registry_classes_allocate(&registry, &services->registry, NULL), MC_REGISTRY_OK,
               "Registry class allocation");
    OPEN_OWNER(
        mc_entity_classes_create(MC_ENTITY_CLIENT, services->profile, NULL, &services->entities),
        MC_ENTITY_OK, "Entity class namespace");
    OPEN_OWNER(mc_client_render_classes_create(services->entities, services->profile,
                                               &services->render_classes),
               MC_CLIENT_MODEL_OK, "model class namespace");
    OPEN_OWNER(mc_client_render_engine_classes_create(NULL, &context->graphics),
               MC_CLIENT_RENDER_ENGINE_OK, "RenderEngine class namespace");
    OPEN_OWNER(mc_client_loading_classes_create(context->graphics, &services->gui_classes),
               MC_CLIENT_LOADING_OK, "GUI class namespace");
    OPEN_OWNER(mc_client_application_classes_create(NULL, &services->application_classes, NULL),
               MC_CLIENT_APPLICATION_OWNER_OK, "Minecraft class namespace");
    OPEN_OWNER(mc_sound_backend_create(services->profile, NULL, &services->sound_backend, NULL),
               MC_SOUND_BACKEND_OK, "sound backend");
    mc_sound_manager_backend backend;
    if (!mc_sound_backend_manager_interface(services->sound_backend, &backend))
        return owner_failure("sound backend manager interface", 0);
    OPEN_OWNER(mc_sound_manager_shared_create(&backend, &services->sound_shared, NULL), MC_SOUND_OK,
               "shared SoundManager namespace");
    return MC_CLIENT_BOOTSTRAP_OK;
}

static mc_client_bootstrap_status open_startup_services(mc_client_bootstrap *context) {
    mc_client_startup_services *services = &context->services;
    mc_client_startup_error *error = &context->startup_error;
    OPEN_OWNER(mc_java_files_create(NULL, &services->files, &error->file), MC_JAVA_FILE_OK,
               "File namespace");
    OPEN_OWNER(mc_client_save_formats_create(services->files, NULL, NULL, &services->save_formats,
                                             &error->save),
               MC_CLIENT_SAVE_OK, "SaveFormat namespace");
    OPEN_OWNER(mc_game_settings_classes_create(services->files, NULL, NULL,
                                               &services->settings_classes, &error->settings),
               MC_GAME_SETTINGS_OK, "GameSettings namespace");
    OPEN_OWNER(mc_client_directory_inputs_create(NULL, 4u * 1048576u, &services->directory_inputs,
                                                 &error->directory),
               MC_CLIENT_GAME_DIRECTORY_OK, "native game directory inputs");
    OPEN_OWNER(mc_client_sdk_icons_allocate(mc_resource_files_source(context->imported_files),
                                            &services->icons, &error->icons),
               MC_CLIENT_SDK_ICONS_OK, "imported SDK icon allocation");
    const mc_client_sdk_icons_status icons =
        mc_client_sdk_icons_construct(services->icons, &error->icons);
    if (icons != MC_CLIENT_SDK_ICONS_OK) {
        startup_failure(error);
        return owner_failure("imported SDK icon construction", (int)icons);
    }
    return MC_CLIENT_BOOTSTRAP_OK;
}

/* This explicit CLI boundary accepts UTF8 and creates the real immutable
 * UTF16 Session argument. It does not reinterpret bytes as Java characters. */
static mc_player_status username_string(const char *argument, mc_player_string **out) {
    const unsigned char *input = (const unsigned char *)(argument == NULL ? "Player" : argument);
    const size_t bytes = strlen((const char *)input);
    if (bytes > (size_t)MC_PLAYER_MAX_TEXT_UNITS * 4u)
        return MC_PLAYER_LIMIT;
    uint16_t *units = malloc((bytes == 0 ? 1 : bytes) * sizeof(*units));
    if (units == NULL)
        return MC_PLAYER_ALLOCATION_FAILED;
    size_t at = 0, used = 0;
    mc_player_status status = MC_PLAYER_INVALID_ARGUMENT;
    while (at < bytes) {
        const unsigned int first = input[at++];
        unsigned int scalar = first, remaining = 0, minimum = 0;
        if (first >= 0xc2u && first <= 0xdfu) {
            scalar = first & 0x1fu;
            remaining = 1;
            minimum = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            scalar = first & 0x0fu;
            remaining = 2;
            minimum = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            scalar = first & 7u;
            remaining = 3;
            minimum = 0x10000u;
        } else if (first >= 0x80u)
            goto done;
        if (remaining > bytes - at)
            goto done;
        for (unsigned int i = 0; i < remaining; ++i) {
            const unsigned int next = input[at++];
            if ((next & 0xc0u) != 0x80u)
                goto done;
            scalar = (scalar << 6) | (next & 0x3fu);
        }
        if (scalar < minimum || scalar > 0x10ffffu || (scalar >= 0xd800u && scalar <= 0xdfffu))
            goto done;
        const size_t count = scalar > 0xffffu ? 2u : 1u;
        if (used > MC_PLAYER_MAX_TEXT_UNITS - count) {
            status = MC_PLAYER_LIMIT;
            goto done;
        }
        if (count == 2) {
            scalar -= 0x10000u;
            units[used++] = (uint16_t)(0xd800u + (scalar >> 10));
            units[used++] = (uint16_t)(0xdc00u + (scalar & 0x3ffu));
        } else
            units[used++] = (uint16_t)scalar;
    }
    status = mc_player_string_create(units, used, out);
done:
    free(units);
    return status;
}

static mc_client_bootstrap_status open_session(mc_client_bootstrap *context, const char *username) {
    mc_player_string *name = NULL, *id = NULL;
    mc_player_status status = username_string(username, &name);
    if (status == MC_PLAYER_OK)
        status = mc_player_string_create(NULL, 0, &id);
    if (status != MC_PLAYER_OK) {
        mc_player_string_release(name);
        return owner_failure("local username/session String", (int)status);
    }
    const mc_client_statistics_owner_status session =
        mc_client_application_create_session(context->app, name, id, &context->session_error);
    mc_player_string_release(id);
    mc_player_string_release(name);
    if (session != MC_CLIENT_STATISTICS_OWNER_OK)
        return owner_failure(text(context->session_error.operation), (int)session);
    return MC_CLIENT_BOOTSTRAP_OK;
}

mc_client_bootstrap_status mc_client_bootstrap_open(const mc_client_application_options *options,
                                                    mc_client_bootstrap **out) {
    if (options == NULL || out == NULL || *out != NULL || options->assets == NULL ||
        options->assets[0] == '\0' || options->width <= 0 || options->height <= 0)
        return MC_CLIENT_BOOTSTRAP_INVALID_ARGUMENT;
    if (options->game_directory != NULL || options->fixed_time || options->gui_scale_override ||
        options->inspect_world != NULL) {
        fprintf(stderr, "The normal client does not yet support --game-directory, --time-millis, "
                        "--gui-scale or --inspect-world overrides.\n");
        return MC_CLIENT_BOOTSTRAP_UNSUPPORTED_OPTION;
    }
    mc_client_bootstrap *context = calloc(1, sizeof(*context));
    if (context == NULL)
        return allocation_failure("native application context");
    *out = context;
    mc_client_bootstrap_status status = open_resources(context, options->assets);
    if (status != MC_CLIENT_BOOTSTRAP_OK)
        return status;
    status = open_constructor_services(context);
    if (status != MC_CLIENT_BOOTSTRAP_OK)
        return status;
    status = open_startup_services(context);
    if (status != MC_CLIENT_BOOTSTRAP_OK)
        return status;
    mc_client_application_construction_status constructed =
        mc_client_application_constructor_allocate(&context->constructor, NULL, &context->app,
                                                   &context->construction_error);
    if (constructed == MC_CLIENT_APP_OK) {
        const mc_client_application_constructor_arguments arguments = {
            NULL, NULL, NULL, options->width, options->height, false};
        constructed = mc_client_application_constructor_construct(context->app, &arguments,
                                                                  &context->construction_error);
    }
    if (constructed != MC_CLIENT_APP_OK) {
        construction_failure(&context->construction_error);
        return MC_CLIENT_BOOTSTRAP_OWNER_FAILED;
    }
    context->app->options = *options; /* Native CLI context, never a source field publication. */
    status = open_session(context, options->username);
    if (status != MC_CLIENT_BOOTSTRAP_OK)
        return status;
    mc_client_startup_options startup = mc_client_application_startup_default_options();
    startup.hidden = options->hidden;
    mc_client_startup_status started = mc_client_application_startup_allocate(
        context->app, &context->services, &startup, &context->startup, &context->startup_error);
    if (started == MC_CLIENT_STARTUP_OK)
        started = mc_client_application_run_prefix(context->startup, &context->startup_error);
    if (context->startup != NULL) {
        context->manager = context->startup->manager_classes;
        context->startup_returned = context->startup->menu != NULL &&
                                    context->startup->menu->source_startup_returned &&
                                    context->startup->menu->last_pc == 749;
    }
    if (started != MC_CLIENT_STARTUP_UNIMPLEMENTED || !context->startup_returned ||
        context->startup_error.caller.pc != 32 ||
        context->startup_error.caller.class_name == NULL ||
        strcmp(context->startup_error.caller.class_name, "net/minecraft/client/Minecraft") != 0 ||
        context->startup_error.caller.method == NULL ||
        strcmp(context->startup_error.caller.method, "run") != 0) {
        startup_failure(&context->startup_error);
        return MC_CLIENT_BOOTSTRAP_OWNER_FAILED;
    }
    return MC_CLIENT_BOOTSTRAP_OK;
}

#undef OPEN_OWNER

static mc_client_bootstrap_status close_startup(mc_client_bootstrap *context) {
    if (context->startup == NULL)
        return MC_CLIENT_BOOTSTRAP_OK;
    mc_client_startup_status status =
        mc_client_application_startup_retire(&context->startup, &context->startup_error);
    if (status == MC_CLIENT_STARTUP_BUSY && context->startup != NULL &&
        !context->startup->manager_consumer.attached && context->manager != NULL) {
        mc_client_manager_status manager = mc_client_render_manager_retire_hands(
            context->manager, &context->startup_error.manager);
        if (manager == MC_CLIENT_MANAGER_OK)
            manager = mc_client_render_manager_classes_retire(&context->manager,
                                                              &context->startup_error.manager);
        if (manager != MC_CLIENT_MANAGER_OK)
            return manager == MC_CLIENT_MANAGER_BUSY
                       ? MC_CLIENT_BOOTSTRAP_BUSY
                       : owner_failure("terminal RenderManager retirement", (int)manager);
        status = mc_client_application_startup_retire(&context->startup, &context->startup_error);
    }
    if (status == MC_CLIENT_STARTUP_BUSY && context->startup != NULL &&
        !context->startup->manager_consumer.attached && context->manager == NULL &&
        context->startup->entity_renderer_candidate == NULL &&
        context->app->font_renderer_retained == NULL && context->graphics != NULL &&
        mc_client_render_engine_resources_pending(context->graphics)) {
        const mc_client_render_texture_status graphics =
            mc_client_render_engine_retire_resources(context->graphics, &context->texture_error);
        if (graphics != MC_CLIENT_RENDER_TEXTURE_OK)
            return graphics == MC_CLIENT_RENDER_TEXTURE_BUSY
                       ? MC_CLIENT_BOOTSTRAP_BUSY
                       : owner_failure(text(context->texture_error.operation), (int)graphics);
        status = mc_client_application_startup_retire(&context->startup, &context->startup_error);
    }
    if (status == MC_CLIENT_STARTUP_OK)
        return MC_CLIENT_BOOTSTRAP_OK;
    if (status == MC_CLIENT_STARTUP_BUSY)
        return MC_CLIENT_BOOTSTRAP_BUSY;
    startup_failure(&context->startup_error);
    return MC_CLIENT_BOOTSTRAP_OWNER_FAILED;
}

static mc_client_bootstrap_status close_application(mc_client_bootstrap *context) {
    /* An early startup failure can leave a cold/partial manager association
     * without Font leases, so startup retirement can finish before it needs
     * the outer-manager gate. That association still owns its Registry hold. */
    if (context->manager != NULL) {
        mc_client_manager_status manager = mc_client_render_manager_retire_hands(
            context->manager, &context->startup_error.manager);
        if (manager == MC_CLIENT_MANAGER_OK)
            manager = mc_client_render_manager_classes_retire(&context->manager,
                                                              &context->startup_error.manager);
        if (manager != MC_CLIENT_MANAGER_OK)
            return manager == MC_CLIENT_MANAGER_BUSY
                       ? MC_CLIENT_BOOTSTRAP_BUSY
                       : owner_failure("terminal RenderManager retirement", (int)manager);
    }
    if (context->app == NULL)
        return MC_CLIENT_BOOTSTRAP_OK;
    if (context->app->session != NULL || context->app->session_classes != NULL ||
        context->app->stats_writer_retained != NULL) {
        const mc_client_statistics_owner_status status =
            mc_client_application_retire_statistics(context->app, &context->session_error);
        if (status != MC_CLIENT_STATISTICS_OWNER_OK)
            return status == MC_CLIENT_STATISTICS_OWNER_BUSY ||
                           context->session_error.writer.status == MC_CLIENT_STATS_BUSY
                       ? MC_CLIENT_BOOTSTRAP_BUSY
                       : owner_failure(text(context->session_error.operation), (int)status);
    }
    mc_client_application_construction_status status =
        mc_client_application_constructor_retire(&context->app, &context->construction_error);
    if (status == MC_CLIENT_APP_BUSY && context->app != NULL &&
        !context->app->source_namespace_borrowed && context->app->registry == NULL) {
        /* The genuine partial App retirement dropped its Registry/namespace
         * occurrences. End the actual static hold and owned kh callbacks before
         * retrying final App destruction; never substitute field/null stores. */
        const mc_client_application_owner_status classes =
            mc_client_application_classes_release(&context->constructor.application_classes, NULL);
        if (classes != MC_CLIENT_APPLICATION_OWNER_OK)
            return classes == MC_CLIENT_APPLICATION_OWNER_BUSY
                       ? MC_CLIENT_BOOTSTRAP_BUSY
                       : owner_failure("Minecraft namespace retirement", (int)classes);
        if (context->constructor.registry != NULL) {
            const mc_registry_status registry =
                mc_registry_classes_free(context->constructor.registry, NULL);
            if (registry != MC_REGISTRY_OK)
                return registry == MC_REGISTRY_BUSY
                           ? MC_CLIENT_BOOTSTRAP_BUSY
                           : owner_failure("Registry callback retirement", (int)registry);
            context->constructor.registry = NULL;
        }
        status =
            mc_client_application_constructor_retire(&context->app, &context->construction_error);
    }
    if (status == MC_CLIENT_APP_OK)
        return MC_CLIENT_BOOTSTRAP_OK;
    if (status == MC_CLIENT_APP_BUSY)
        return MC_CLIENT_BOOTSTRAP_BUSY;
    construction_failure(&context->construction_error);
    return MC_CLIENT_BOOTSTRAP_OWNER_FAILED;
}

#define CLOSE_OWNER(call, expected, busy, operation)                                               \
    do {                                                                                           \
        const int bootstrap_owner_code = (int)(call);                                              \
        if (bootstrap_owner_code != (int)(expected))                                               \
            return bootstrap_owner_code == (int)(busy)                                             \
                       ? MC_CLIENT_BOOTSTRAP_BUSY                                                  \
                       : owner_failure(operation, bootstrap_owner_code);                           \
    } while (0)

mc_client_bootstrap_status mc_client_bootstrap_close(mc_client_bootstrap **slot) {
    if (slot == NULL)
        return MC_CLIENT_BOOTSTRAP_INVALID_ARGUMENT;
    mc_client_bootstrap *context = *slot;
    if (context == NULL)
        return MC_CLIENT_BOOTSTRAP_OK;
    context->retiring = true;
    mc_client_bootstrap_status status = close_startup(context);
    if (status != MC_CLIENT_BOOTSTRAP_OK)
        return status;
    status = close_application(context);
    if (status != MC_CLIENT_BOOTSTRAP_OK)
        return status;
    mc_client_startup_services *services = &context->services;
    CLOSE_OWNER(mc_client_sdk_icons_release(&services->icons, &context->startup_error.icons),
                MC_CLIENT_SDK_ICONS_OK, MC_CLIENT_SDK_ICONS_BUSY, "SDK icon retirement");
    CLOSE_OWNER(mc_game_settings_classes_release(&services->settings_classes,
                                                 &context->startup_error.settings),
                MC_GAME_SETTINGS_OK, MC_GAME_SETTINGS_BUSY, "GameSettings namespace retirement");
    CLOSE_OWNER(mc_client_directory_inputs_release(&services->directory_inputs,
                                                   &context->startup_error.directory),
                MC_CLIENT_GAME_DIRECTORY_OK, MC_CLIENT_GAME_DIRECTORY_BUSY,
                "directory input retirement");
    CLOSE_OWNER(
        mc_client_save_formats_release(&services->save_formats, &context->startup_error.save),
        MC_CLIENT_SAVE_OK, MC_CLIENT_SAVE_BUSY, "SaveFormat namespace retirement");
    CLOSE_OWNER(mc_java_files_release(&services->files, &context->startup_error.file),
                MC_JAVA_FILE_OK, MC_JAVA_FILE_BUSY, "File namespace retirement");
    mc_client_application_constructor_services *constructor = &context->constructor;
    CLOSE_OWNER(mc_client_application_classes_release(&constructor->application_classes, NULL),
                MC_CLIENT_APPLICATION_OWNER_OK, MC_CLIENT_APPLICATION_OWNER_BUSY,
                "Minecraft namespace retirement");
    if (constructor->gui_classes != NULL) {
        CLOSE_OWNER(mc_client_loading_classes_release(constructor->gui_classes),
                    MC_CLIENT_LOADING_OK, MC_CLIENT_LOADING_BUSY, "GUI namespace retirement");
        constructor->gui_classes = NULL;
    }
    if (context->graphics != NULL) {
        CLOSE_OWNER(mc_client_render_engine_classes_release(context->graphics),
                    MC_CLIENT_RENDER_ENGINE_OK, MC_CLIENT_RENDER_ENGINE_BUSY,
                    "RenderEngine namespace retirement");
        context->graphics = NULL;
    }
    CLOSE_OWNER(mc_sound_manager_shared_retire(&constructor->sound_shared, NULL), MC_SOUND_OK,
                MC_SOUND_BUSY, "shared SoundManager namespace retirement");
    CLOSE_OWNER(mc_sound_backend_retire(&constructor->sound_backend, NULL), MC_SOUND_BACKEND_OK,
                MC_SOUND_BACKEND_BUSY, "sound backend retirement");
    if (constructor->render_classes != NULL) {
        CLOSE_OWNER(mc_client_render_classes_free(constructor->render_classes), MC_CLIENT_MODEL_OK,
                    MC_CLIENT_MODEL_BUSY, "model namespace retirement");
        constructor->render_classes = NULL;
    }
    if (constructor->entities != NULL) {
        CLOSE_OWNER(mc_entity_classes_free(constructor->entities), MC_ENTITY_OK, MC_ENTITY_BUSY,
                    "Entity namespace retirement");
        constructor->entities = NULL;
    }
    if (constructor->registry != NULL) {
        CLOSE_OWNER(mc_registry_classes_free(constructor->registry, NULL), MC_REGISTRY_OK,
                    MC_REGISTRY_BUSY, "Registry namespace retirement");
        constructor->registry = NULL;
    }
    mc_materials_destroy(context->materials);
    context->materials = NULL;
    mc_java_random_profile_free(constructor->profile);
    constructor->profile = NULL;
    free(context->trig);
    context->trig = NULL;
    if (context->class_files != NULL) {
        const mc_registry_status resource = mc_resource_files_free(context->class_files);
        context->class_files = NULL; /* Provider consumes storage even on a terminal close error. */
        if (resource != MC_REGISTRY_OK)
            return owner_failure("class resource provider retirement", (int)resource);
    }
    if (context->imported_files != NULL) {
        const mc_registry_status resource = mc_resource_files_free(context->imported_files);
        context->imported_files = NULL;
        if (resource != MC_REGISTRY_OK)
            return owner_failure("SDK resource provider retirement", (int)resource);
    }
    free(context);
    *slot = NULL;
    return MC_CLIENT_BOOTSTRAP_OK;
}

#undef CLOSE_OWNER

client_application *mc_client_bootstrap_app(mc_client_bootstrap *context) {
    return context == NULL || context->retiring || !context->startup_returned ? NULL : context->app;
}

const mc_registry_resources *mc_client_bootstrap_resources(const mc_client_bootstrap *context) {
    return context == NULL || context->retiring ? NULL : context->services.texture_resources;
}

mc_resource_files *mc_client_bootstrap_class_files(const mc_client_bootstrap *context) {
    return context == NULL || context->retiring ? NULL : context->class_files;
}

const mc_math_trig *mc_client_bootstrap_trig(const mc_client_bootstrap *context) {
    return context == NULL || context->retiring ? NULL : context->trig;
}

mc_client_render_engine_classes *mc_client_bootstrap_graphics(const mc_client_bootstrap *context) {
    return context == NULL || context->retiring ? NULL : context->graphics;
}
