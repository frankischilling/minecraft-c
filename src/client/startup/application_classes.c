#include "application_owner_internal.h"

#include "mc/entity/player/player.h"
#include <stdlib.h>

mc_client_application_owner_status
application_owner_fail(mc_client_application_owner_error *error,
                       mc_client_application_owner_status status, const char *operation,
                       const char *class_name, const char *method, const char *descriptor,
                       unsigned int pc, int native_error) {
    if (error != NULL)
        *error = (mc_client_application_owner_error){.status = status,
                                                     .operation = operation,
                                                     .site = {class_name, method, descriptor, pc},
                                                     .native_error = native_error,
                                                     .file = NULL,
                                                     .statistics = NULL,
                                                     .standard_output = NULL,
                                                     .cx = NULL};
    return status;
}

mc_client_application_owner_status
mc_client_application_classes_create(const mc_client_application_class_options *options,
                                     mc_client_application_classes **out,
                                     mc_client_application_owner_error *error) {
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    if (out == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Minecraft class native output", NULL, NULL, NULL, 0, 0);
    mc_client_application_classes *classes = calloc(1, sizeof(*classes));
    if (classes == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_ALLOCATION_FAILED,
                                      "Minecraft class native allocation", NULL, NULL, NULL, 0, 0);
    const int locked = mtx_init(&classes->mutex, mtx_plain);
    if (locked != thrd_success) {
        free(classes);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Minecraft class native mutex", NULL, NULL, NULL, 0, locked);
    }
    classes->options = options == NULL
                           ? (mc_client_application_class_options){16u * 1024u * 1024u, SIZE_MAX}
                           : *options;
    classes->holds = 1;
    *out = classes;
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

static void *array_allocate(mc_client_application_classes *classes, size_t count, size_t item,
                            unsigned int pc, mc_client_application_owner_error *error) {
    const size_t bytes = count * item; /* Three fixed, independently bounded source lengths. */
    ++classes->allocation_attempts;
    if (classes->array_allocations >= classes->options.maximum_array_allocations ||
        bytes > classes->options.maximum_array_bytes ||
        classes->array_bytes > classes->options.maximum_array_bytes - bytes) {
        application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_LIMIT,
                               "Minecraft array native capacity", "net/minecraft/client/Minecraft",
                               "<clinit>", "()V", pc, 0);
        return NULL;
    }
    void *storage = calloc(count, item);
    if (storage == NULL) {
        application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_ALLOCATION_FAILED,
                               "Minecraft source array allocation",
                               "net/minecraft/client/Minecraft", "<clinit>", "()V", pc, 0);
        return NULL;
    }
    classes->array_bytes += bytes;
    ++classes->array_allocations;
    return storage;
}

mc_client_application_owner_status
mc_client_application_classes_active_use(mc_client_application_classes *classes,
                                         mc_client_application_owner_error *error) {
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    if (classes == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Minecraft class native owner", NULL, NULL, NULL, 0, 0);
    const int locked = mtx_lock(&classes->mutex);
    if (locked != thrd_success)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Minecraft class native lock", NULL, NULL, NULL, 0, locked);
    if (classes->native_retiring) {
        (void)mtx_unlock(&classes->mutex);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft namespace retiring", NULL, NULL, NULL, 0, 0);
    }
    if (classes->phase == MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return MC_CLIENT_APPLICATION_OWNER_OK;
    }
    if (classes->phase == MC_CLIENT_APPLICATION_CLASS_ERRONEOUS) {
        if (error != NULL) {
            *error = classes->failure;
            error->status = MC_CLIENT_APPLICATION_OWNER_CLASS_ERRONEOUS;
        }
        (void)mtx_unlock(&classes->mutex);
        return MC_CLIENT_APPLICATION_OWNER_CLASS_ERRONEOUS;
    }
    classes->phase = MC_CLIENT_APPLICATION_CLASS_INITIALIZING;
    /* No source callback or child active use occurs in these fixed allocations.
     * Keeping the mutex through the initializer also protects diagnostic views. */
    mc_client_application_owner_error failure = {0};
    uint8_t *b =
        array_allocate(classes, MC_CLIENT_APPLICATION_RESERVE_BYTES, sizeof(*b), 2, &failure);
    if (b == NULL)
        goto failed;
    classes->b = b;
    classes->last_store_pc = 4;
    int64_t *E =
        array_allocate(classes, MC_CLIENT_APPLICATION_HISTORY_LENGTH, sizeof(*E), 10, &failure);
    if (E == NULL)
        goto failed;
    classes->E = E;
    classes->last_store_pc = 12;
    int64_t *F =
        array_allocate(classes, MC_CLIENT_APPLICATION_HISTORY_LENGTH, sizeof(*F), 18, &failure);
    if (F == NULL)
        goto failed;
    classes->F = F;
    classes->last_store_pc = 20;
    classes->G = 0;
    classes->last_store_pc = 24;
    classes->H = 0;
    classes->last_store_pc = 28;
    classes->af = NULL;
    classes->last_store_pc = 32;
    classes->phase = MC_CLIENT_APPLICATION_CLASS_READY;
    (void)mtx_unlock(&classes->mutex);
    return MC_CLIENT_APPLICATION_OWNER_OK;
failed:
    classes->failure = failure;
    classes->phase = MC_CLIENT_APPLICATION_CLASS_ERRONEOUS;
    if (error != NULL)
        *error = failure;
    (void)mtx_unlock(&classes->mutex);
    return failure.status;
}

mc_client_application_owner_status
mc_client_application_classes_retain(mc_client_application_classes *classes) {
    if (classes == NULL)
        return MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT;
    if (mtx_lock(&classes->mutex) != thrd_success)
        return MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED;
    mc_client_application_owner_status status = MC_CLIENT_APPLICATION_OWNER_OK;
    if (classes->native_retiring)
        status = MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE;
    else if (classes->holds == SIZE_MAX)
        status = MC_CLIENT_APPLICATION_OWNER_LIMIT;
    else
        ++classes->holds;
    (void)mtx_unlock(&classes->mutex);
    return status;
}

mc_client_application_owner_status
mc_client_application_classes_release(mc_client_application_classes **owner,
                                      mc_client_application_owner_error *error) {
    if (error != NULL)
        *error = (mc_client_application_owner_error){0};
    if (owner == NULL)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
                                      "Minecraft class native release output", NULL, NULL, NULL, 0,
                                      0);
    mc_client_application_classes *classes = *owner;
    if (classes == NULL)
        return MC_CLIENT_APPLICATION_OWNER_OK;
    if (mtx_lock(&classes->mutex) != thrd_success)
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
                                      "Minecraft class native release lock", NULL, NULL, NULL, 0,
                                      0);
    if (classes->phase == MC_CLIENT_APPLICATION_CLASS_INITIALIZING) {
        (void)mtx_unlock(&classes->mutex);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_BUSY,
                                      "Minecraft class initializing", NULL, NULL, NULL, 0, 0);
    }
    if (classes->holds == 1 && classes->native_app_borrows != 0) {
        (void)mtx_unlock(&classes->mutex);
        return application_owner_fail(error, MC_CLIENT_APPLICATION_OWNER_BUSY,
                                      "Minecraft live App namespace borrows", NULL, NULL, NULL, 0,
                                      0);
    }
    if (classes->holds > 1) {
        --classes->holds;
        (void)mtx_unlock(&classes->mutex);
        *owner = NULL;
        return MC_CLIENT_APPLICATION_OWNER_OK;
    }
    classes->native_retiring = true;
    (void)mtx_unlock(&classes->mutex);
    if (mc_client_screen_classes_release(&classes->startup_screen_classes,
                                         &classes->screen_retirement) != MC_CLIENT_SCREEN_OK)
        return application_owner_fail(error,
                                      classes->screen_retirement.status == MC_CLIENT_SCREEN_BUSY
                                          ? MC_CLIENT_APPLICATION_OWNER_BUSY
                                          : MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
                                      "Minecraft native screen namespace retirement", NULL, NULL,
                                      NULL, 0, 0);
    if (mc_client_gui_ingame_classes_release(&classes->startup_gui_ingame_classes,
                                             &classes->gui_ingame_retirement) !=
        MC_CLIENT_GUI_INGAME_OK)
        return application_owner_fail(
            error,
            classes->gui_ingame_retirement.status == MC_CLIENT_GUI_INGAME_BUSY
                ? MC_CLIENT_APPLICATION_OWNER_BUSY
                : MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
            "Minecraft native GuiIngame namespace retirement", NULL, NULL, NULL, 0, 0);
    classes->startup_gui_ingame_models = NULL;
    classes->startup_gui_ingame_profile = NULL;
    /* File retirement may fail/BUSY. Keep the final class occurrence and its
     * exact af identity for retry; no source putstatic af=NULL is performed. */
    if (classes->af != NULL &&
        mc_java_file_release(&classes->af, &classes->file_retirement) != MC_JAVA_FILE_OK) {
        application_owner_fail(error,
                               classes->file_retirement.status == MC_JAVA_FILE_BUSY
                                   ? MC_CLIENT_APPLICATION_OWNER_BUSY
                                   : MC_CLIENT_APPLICATION_OWNER_FILE_FAILED,
                               "Minecraft native af File retirement", NULL, NULL, NULL, 0, 0);
        if (error != NULL)
            error->file = &classes->file_retirement;
        return error == NULL ? (classes->file_retirement.status == MC_JAVA_FILE_BUSY
                                    ? MC_CLIENT_APPLICATION_OWNER_BUSY
                                    : MC_CLIENT_APPLICATION_OWNER_FILE_FAILED)
                             : error->status;
    }
    if (mc_client_stats_classes_release(&classes->startup_stats_classes,
                                        &classes->stats_retirement) != MC_CLIENT_STATS_OK) {
        const mc_client_application_owner_status status =
            classes->stats_retirement.status == MC_CLIENT_STATS_BUSY
                ? MC_CLIENT_APPLICATION_OWNER_BUSY
                : MC_CLIENT_APPLICATION_OWNER_STATS_FAILED;
        application_owner_fail(error, status, "Minecraft native xi literal scope release", NULL,
                               NULL, NULL, 0, 0);
        if (error != NULL)
            error->statistics = &classes->stats_retirement;
        return status;
    }
    if (mc_java_system_output_release(&classes->system_output, &classes->output_retirement) !=
        MC_JAVA_PRINT_OK) {
        const mc_client_application_owner_status status =
            classes->output_retirement.status == MC_JAVA_PRINT_BUSY
                ? MC_CLIENT_APPLICATION_OWNER_BUSY
                : MC_CLIENT_APPLICATION_OWNER_OUTPUT_FAILED;
        application_owner_fail(error, status, "Minecraft native System-output association release",
                               NULL, NULL, NULL, 0, 0);
        if (error != NULL)
            error->standard_output = &classes->output_retirement;
        return status;
    }
    if (mc_client_cx_classes_release(&classes->startup_cx_classes, &classes->cx_retirement) !=
        MC_CLIENT_CX_OK) {
        const mc_client_application_owner_status status =
            classes->cx_retirement.status == MC_CLIENT_CX_BUSY
                ? MC_CLIENT_APPLICATION_OWNER_BUSY
                : MC_CLIENT_APPLICATION_OWNER_CX_FAILED;
        application_owner_fail(error, status, "Minecraft native cx namespace release", NULL, NULL,
                               NULL, 0, 0);
        if (error != NULL)
            error->cx = &classes->cx_retirement;
        return status;
    }
    application_directory_classes_destroy(classes->directory_classes);
    client_application *last_app = classes->a;
    mc_player_string *timer_name = classes->timer_name;
    mc_player_string *applet_true = classes->applet_true;
    mc_player_string *applet_standalone = classes->applet_standalone;
    mc_player_string *startup_title = classes->startup_title;
    mc_player_string *startup_font_resource = classes->startup_font_resource;
    mc_player_string *startup_logo_resource = classes->startup_logo_resource;
    mc_player_string *startup_post_label = classes->startup_post_label;
    mc_player_string *startup_gl_labels[2] = {classes->startup_gl_labels[0],
                                              classes->startup_gl_labels[1]};
    mc_player_string *startup_color_literals[3] = {classes->startup_color_literals[0],
                                                   classes->startup_color_literals[1],
                                                   classes->startup_color_literals[2]};
    free(classes->F);
    free(classes->E);
    free(classes->b);
    mtx_destroy(&classes->mutex);
    free(classes);
    for (size_t index = 0; index < 3; ++index)
        mc_player_string_release(startup_color_literals[index]);
    mc_player_string_release(startup_font_resource);
    mc_player_string_release(startup_logo_resource);
    mc_player_string_release(startup_post_label);
    for (size_t index = 0; index < 2; ++index)
        mc_player_string_release(startup_gl_labels[index]);
    mc_player_string_release(startup_title);
    mc_player_string_release(timer_name);
    mc_player_string_release(applet_true);
    mc_player_string_release(applet_standalone);
    if (last_app != NULL)
        application_static_release(last_app);
    *owner = NULL;
    return MC_CLIENT_APPLICATION_OWNER_OK;
}

bool mc_client_application_classes_inspect(mc_client_application_classes *classes,
                                           mc_client_application_class_state *out) {
    if (classes == NULL || out == NULL || mtx_lock(&classes->mutex) != thrd_success)
        return false;
    *out = (mc_client_application_class_state){
        .phase = classes->phase,
        .b = classes->b,
        .E = classes->E,
        .F = classes->F,
        .G = classes->G,
        .H = classes->H,
        .a = classes->a,
        .af = classes->af,
        .native_retiring = classes->native_retiring,
        .file_retirement = &classes->file_retirement,
        .b_length = classes->b == NULL ? 0 : MC_CLIENT_APPLICATION_RESERVE_BYTES,
        .E_length = classes->E == NULL ? 0 : MC_CLIENT_APPLICATION_HISTORY_LENGTH,
        .F_length = classes->F == NULL ? 0 : MC_CLIENT_APPLICATION_HISTORY_LENGTH,
        .native_holds = classes->holds,
        .array_bytes = classes->array_bytes,
        .array_allocations = classes->array_allocations,
        .allocation_attempts = classes->allocation_attempts,
        .last_store_pc = classes->last_store_pc,
        .last_global_store_pc = classes->last_global_store_pc,
        .native_app_borrows = classes->native_app_borrows,
        .timer_name = classes->timer_name,
        .applet_true = classes->applet_true,
        .applet_standalone = classes->applet_standalone,
        .failure = classes->failure};
    (void)mtx_unlock(&classes->mutex);
    return true;
}
