#include "application_directory_classes.h"
#include "application_owner_internal.h"
#include "application_startup_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct application_directory_enum {
    mc_world_info_name *name;
    int32_t ordinal;
} application_directory_enum;

struct application_directory_classes {
    mc_client_application_class_phase pd_phase, kn_phase;
    application_directory_enum *pd[5], *pending_enum;
    application_directory_enum **values;
    int32_t *switch_map;
    mc_world_info_name *names[5], *literals[APPLICATION_DIRECTORY_LITERAL_COUNT];
    size_t enum_allocations, values_allocations, clone_allocations, map_allocations;
    unsigned int last_pd_store, last_kn_store;
    mc_client_startup_error failure;
};

static mc_client_startup_status class_lock(mc_client_application_classes *classes,
                                           mc_client_startup_error *error) {
    if (classes == NULL || mtx_lock(&classes->mutex) != thrd_success)
        return application_startup_fail(error, MC_CLIENT_STARTUP_OWNER_FAILED,
                                        "directory dispatch native class lock", NULL, NULL, NULL,
                                        0);
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return application_startup_fail(error, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "directory dispatch live MC namespace", NULL, NULL, NULL,
                                        0);
    }
    if (classes->directory_classes == NULL) {
        classes->directory_classes = calloc(1, sizeof(*classes->directory_classes));
        if (classes->directory_classes == NULL) {
            (void)mtx_unlock(&classes->mutex);
            return application_startup_fail(error, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                            "directory native class storage", NULL, NULL, NULL, 0);
        }
    }
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status ascii_name(const char *text, size_t maximum_bytes,
                                           mc_world_info_name **out, mc_client_startup_error *error,
                                           const char *class_name, const char *method,
                                           const char *descriptor, unsigned int pc) {
    /* These finite compiler literals contain ASCII only. This is native UTF16
     * literal resolution, never filename conversion or text normalization. */
    uint16_t units[64];
    const size_t length = strlen(text);
    if (length > sizeof(units) / sizeof(units[0]))
        return application_startup_fail(error, MC_CLIENT_STARTUP_LIMIT,
                                        "directory literal native bound", NULL, NULL, NULL, 0);
    for (size_t index = 0; index < length; ++index)
        units[index] = (unsigned char)text[index];
    error->string = mc_world_info_name_copy((mc_nbt_string){units, length}, maximum_bytes, out);
    if (error->string != MC_WORLD_INFO_OK)
        return application_startup_fail(error, MC_CLIENT_STARTUP_STRING_FAILED,
                                        "actual directory ldc native resolution", class_name,
                                        method, descriptor, pc);
    return MC_CLIENT_STARTUP_OK;
}

mc_client_startup_status application_directory_literal(mc_client_application_classes *classes,
                                                       application_directory_literal_id id,
                                                       size_t maximum_bytes,
                                                       mc_world_info_name **out,
                                                       mc_client_startup_error *error) {
    static const char *const text[] = {
        "minecraft", "user.home", ".", "APPDATA", "Library/Application Support/", "saves"};
    static const unsigned int pc[] = {6, 0, 2, 84, 178, 179};
    if (out == NULL || (unsigned int)id >= APPLICATION_DIRECTORY_LITERAL_COUNT)
        return application_startup_fail(error, MC_CLIENT_STARTUP_INVALID_ARGUMENT,
                                        "directory literal native output/id", NULL, NULL, NULL, 0);
    if (class_lock(classes, error) != MC_CLIENT_STARTUP_OK)
        return error->status;
    struct application_directory_classes *directory = classes->directory_classes;
    mc_world_info_name **slot = &directory->literals[id];
    const char *method = id == APPLICATION_DIRECTORY_MINECRAFT ? "b" : "a";
    const char *descriptor = id == APPLICATION_DIRECTORY_MINECRAFT ? "()Ljava/io/File;"
                             : id == APPLICATION_DIRECTORY_SAVES
                                 ? "()V"
                                 : "(Ljava/lang/String;)Ljava/io/File;";
    if (*slot == NULL &&
        ascii_name(text[id], maximum_bytes, slot, error, "net/minecraft/client/Minecraft", method,
                   descriptor, pc[id]) != MC_CLIENT_STARTUP_OK) {
        (void)mtx_unlock(&classes->mutex);
        return error->status;
    }
    *out = *slot;
    (void)mtx_unlock(&classes->mutex);
    return MC_CLIENT_STARTUP_OK;
}

static mc_client_startup_status pd_activate(struct application_directory_classes *directory,
                                            size_t maximum_bytes, mc_client_startup_error *error) {
    if (directory->pd_phase == MC_CLIENT_APPLICATION_CLASS_READY)
        return MC_CLIENT_STARTUP_OK;
    if (directory->pd_phase == MC_CLIENT_APPLICATION_CLASS_ERRONEOUS) {
        *error = directory->failure;
        return error->status;
    }
    directory->pd_phase = MC_CLIENT_APPLICATION_CLASS_INITIALIZING;
    static const char *const names[] = {"linux", "solaris", "windows", "macos", "unknown"};
    for (unsigned int index = 0; index < 5; ++index) {
        const unsigned int allocation_pc = 13u * index;
        directory->pending_enum = calloc(1, sizeof(*directory->pending_enum));
        if (directory->pending_enum == NULL) {
            application_startup_fail(error, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                     "actual pd enum allocation", "pd", "<clinit>", "()V",
                                     allocation_pc);
            goto failed;
        }
        ++directory->enum_allocations;
        if (ascii_name(names[index], maximum_bytes, &directory->names[index], error, "pd",
                       "<clinit>", "()V", allocation_pc + 4) != MC_CLIENT_STARTUP_OK)
            goto failed;
        /* Bounded native equivalent of Enum(String,int): immutable actual
         * name identity and ordinal. No replacement class or result provider. */
        error->string = mc_world_info_name_retain(directory->names[index]);
        if (error->string != MC_WORLD_INFO_OK) {
            application_startup_fail(error, MC_CLIENT_STARTUP_STRING_FAILED,
                                     "pd actual Enum name occurrence", "pd", "<init>",
                                     "(Ljava/lang/String;I)V", 3);
            goto failed;
        }
        directory->pending_enum->name = directory->names[index];
        directory->pending_enum->ordinal = (int32_t)index;
        directory->pd[index] = directory->pending_enum;
        directory->pending_enum = NULL;
        directory->last_pd_store = allocation_pc + 10;
    }
    directory->values = calloc(5, sizeof(*directory->values)); /* anewarray66 */
    if (directory->values == NULL) {
        application_startup_fail(error, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                 "actual pd values array allocation", "pd", "<clinit>", "()V", 66);
        goto failed;
    }
    ++directory->values_allocations;
    for (size_t index = 0; index < 5; ++index)
        directory->values[index] = directory->pd[index];
    directory->last_pd_store = 99;
    directory->pd_phase = MC_CLIENT_APPLICATION_CLASS_READY;
    return MC_CLIENT_STARTUP_OK;
failed:
    directory->pd_phase = MC_CLIENT_APPLICATION_CLASS_ERRONEOUS;
    directory->failure = *error;
    return error->status;
}

mc_client_startup_status
application_directory_dispatch_activate(mc_client_application_classes *classes,
                                        size_t maximum_bytes, mc_client_startup_error *error) {
    if (class_lock(classes, error) != MC_CLIENT_STARTUP_OK)
        return error->status;
    struct application_directory_classes *directory = classes->directory_classes;
    if (directory->kn_phase == MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return MC_CLIENT_STARTUP_OK;
    }
    if (directory->kn_phase == MC_CLIENT_APPLICATION_CLASS_ERRONEOUS) {
        *error = directory->failure;
        (void)mtx_unlock(&classes->mutex);
        return error->status;
    }
    directory->kn_phase = MC_CLIENT_APPLICATION_CLASS_INITIALIZING;
    if (pd_activate(directory, maximum_bytes, error) != MC_CLIENT_STARTUP_OK)
        goto failed;
    application_directory_enum **clone = malloc(5 * sizeof(*clone)); /* pd.values clone3 */
    if (clone == NULL) {
        application_startup_fail(error, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                 "actual pd.values distinct array clone", "pd", "values", "()[Lpd;",
                                 3);
        goto failed;
    }
    memcpy(clone, directory->values, 5 * sizeof(*clone));
    ++directory->clone_allocations;
    /* kn arraylength3 consumes its only live clone reference before newarray4. */
    free(clone);
    directory->switch_map = calloc(5, sizeof(*directory->switch_map));
    if (directory->switch_map == NULL) {
        application_startup_fail(error, MC_CLIENT_STARTUP_ALLOCATION_FAILED,
                                 "actual kn switch array allocation", "kn", "<clinit>", "()V", 4);
        goto failed;
    }
    ++directory->map_allocations;
    directory->last_kn_store = 6;
    /* Exact unchanged pd has all four fields. No NoSuchFieldError is raised. */
    for (unsigned int index = 0; index < 4; ++index) {
        directory->switch_map[directory->pd[index]->ordinal] = (int32_t)index + 1;
        directory->last_kn_store = 19 + 15 * index;
    }
    directory->kn_phase = MC_CLIENT_APPLICATION_CLASS_READY;
    (void)mtx_unlock(&classes->mutex);
    return MC_CLIENT_STARTUP_OK;
failed:
    directory->kn_phase = MC_CLIENT_APPLICATION_CLASS_ERRONEOUS;
    directory->failure = *error;
    (void)mtx_unlock(&classes->mutex);
    return error->status;
}

mc_client_startup_status application_directory_dispatch_case(mc_client_application_classes *classes,
                                                             unsigned int ordinal, int32_t *out,
                                                             mc_client_startup_error *error) {
    if (out == NULL || ordinal >= 5)
        return application_startup_fail(error, MC_CLIENT_STARTUP_INVALID_ARGUMENT,
                                        "directory actual enum ordinal admission", NULL, NULL, NULL,
                                        0);
    if (class_lock(classes, error) != MC_CLIENT_STARTUP_OK)
        return error->status;
    const struct application_directory_classes *directory = classes->directory_classes;
    if (directory->kn_phase != MC_CLIENT_APPLICATION_CLASS_READY) {
        (void)mtx_unlock(&classes->mutex);
        return application_startup_fail(error, MC_CLIENT_STARTUP_INVALID_PHASE,
                                        "directory kn must activate before platform query", NULL,
                                        NULL, NULL, 0);
    }
    const application_directory_enum *selected = directory->pd[ordinal]; /* actual y result */
    *out = directory->switch_map[selected->ordinal];                     /* ordinal14, iaload17 */
    (void)mtx_unlock(&classes->mutex);
    return MC_CLIENT_STARTUP_OK;
}

bool mc_client_application_directory_dispatch_inspect(mc_client_application_classes *classes,
                                                      mc_client_directory_dispatch_state *out) {
    if (classes == NULL || out == NULL || mtx_lock(&classes->mutex) != thrd_success)
        return false;
    const struct application_directory_classes *directory = classes->directory_classes;
    *out = (mc_client_directory_dispatch_state){0};
    if (directory != NULL) {
        out->pd_phase = directory->pd_phase;
        out->kn_phase = directory->kn_phase;
        for (size_t index = 0; index < 5; ++index) {
            out->pd[index] = directory->pd[index];
            out->names[index] = directory->pd[index] == NULL ? NULL : directory->pd[index]->name;
            out->values_entries[index] =
                directory->values == NULL ? NULL : directory->values[index];
        }
        out->values_identity = directory->values;
        out->switch_map = directory->switch_map;
        out->enum_allocations = directory->enum_allocations;
        out->values_allocations = directory->values_allocations;
        out->clone_allocations = directory->clone_allocations;
        out->map_allocations = directory->map_allocations;
        out->last_pd_store = directory->last_pd_store;
        out->last_kn_store = directory->last_kn_store;
        out->failure = &directory->failure;
    }
    (void)mtx_unlock(&classes->mutex);
    return true;
}

void application_directory_classes_destroy(struct application_directory_classes *directory) {
    if (directory == NULL)
        return;
    if (directory->pending_enum != NULL) {
        mc_world_info_name_release(directory->pending_enum->name);
        free(directory->pending_enum);
    }
    for (size_t index = 0; index < 5; ++index) {
        if (directory->pd[index] != NULL) {
            mc_world_info_name_release(directory->pd[index]->name);
            free(directory->pd[index]);
        }
        mc_world_info_name_release(directory->names[index]);
    }
    for (size_t index = 0; index < APPLICATION_DIRECTORY_LITERAL_COUNT; ++index)
        mc_world_info_name_release(directory->literals[index]);
    free(directory->switch_map);
    free(directory->values);
    free(directory);
}
