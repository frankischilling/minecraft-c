#include "internal.h"
#include <stdlib.h>

mc_texture_pack_status texture_source_file_error(mc_java_file_status status,
                                                 const mc_java_file_error *cause,
                                                 mc_texture_pack_error *error, unsigned int pc) {
    mc_texture_pack_status mapped =
        status == MC_JAVA_FILE_OK                  ? MC_TEXTURE_PACK_OK
        : status == MC_JAVA_FILE_BUSY              ? MC_TEXTURE_PACK_BUSY
        : status == MC_JAVA_FILE_LIMIT             ? MC_TEXTURE_PACK_LIMIT
        : status == MC_JAVA_FILE_ALLOCATION_FAILED ? MC_TEXTURE_PACK_ALLOCATION_FAILED
        : status == MC_JAVA_FILE_SOURCE_NULL_FILE || status == MC_JAVA_FILE_SOURCE_NULL_STRING
            ? MC_TEXTURE_PACK_NULL_REFERENCE
        : status == MC_JAVA_FILE_INVALID_ARGUMENT ? MC_TEXTURE_PACK_INVALID_ARGUMENT
        : status == MC_JAVA_FILE_STATE_ERROR      ? MC_TEXTURE_PACK_INVALID_PHASE
                                                  : MC_TEXTURE_PACK_UNSUPPORTED_NATIVE;
    error->system_error = cause->system_error;
    return texture_pack_fail(error, mapped, cause->operation, "ik", "a", "()V", pc, pc);
}

/* Native facade retains the existing exact source String occurrence. It does
 * not copy/decode names or intern equal Strings into one source identity. */
mc_client_texture_string *texture_string_name(mc_world_info_name *name, size_t limit,
                                              mc_texture_pack_error *error) {
    const mc_nbt_string units = mc_world_info_name_text(name);
    if (units.length > limit) {
        texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "Pack name native units bound", NULL, NULL,
                          NULL, 0, 0);
        return NULL;
    }
    mc_client_texture_string *text = calloc(1, sizeof(*text));
    if (text == NULL || mc_world_info_name_retain(name) != MC_WORLD_INFO_OK) {
        const mc_texture_pack_status status =
            text == NULL ? MC_TEXTURE_PACK_ALLOCATION_FAILED : MC_TEXTURE_PACK_LIMIT;
        free(text);
        texture_pack_fail(error, status, "Pack name native facade occurrence", NULL, NULL, NULL, 0,
                          0);
        return NULL;
    }
    text->backing = name;
    text->units = (uint16_t *)units.units; /* Immutable; backing owns the units. */
    text->length = units.length;
    text->holds = 1;
    return text;
}

mc_texture_pack_status texture_pack_source_directory(mc_client_texture_pack_list *list,
                                                     mc_java_file *parent,
                                                     mc_texture_pack_error *error) {
    mc_java_file_error *cause = &list->source_failure.file;
    mc_java_file_status status =
        mc_java_file_allocate(list->source_files, &list->pending_file, cause);
    unsigned int pc = 43;
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    /* Native literal occurrence scope, explicit and local to this retained
     * attempt. No cross-module global Java interning is claimed. */
    static const uint16_t literal[] = {'t', 'e', 'x', 't', 'u', 'r', 'e', 'p', 'a', 'c', 'k', 's'};
    const mc_world_info_status copied =
        mc_world_info_name_copy((mc_nbt_string){literal, sizeof(literal) / sizeof(*literal)},
                                SIZE_MAX, &list->directory_literal);
    if (copied != MC_WORLD_INFO_OK)
        return texture_pack_fail(error,
                                 copied == MC_WORLD_INFO_LIMIT ? MC_TEXTURE_PACK_LIMIT
                                                               : MC_TEXTURE_PACK_ALLOCATION_FAILED,
                                 "Pack texturepacks native literal occurrence", "ik", "<init>",
                                 "(Lnet/minecraft/client/Minecraft;Ljava/io/File;)V", 48, 48);
    pc = 50;
    status =
        mc_java_file_construct_child(list->pending_file, parent, list->directory_literal, cause);
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    list->source_directory = list->pending_file;
    list->pending_file = NULL;
    list->last_store_pc = 53;
    bool exists = false;
    list->exists_reached = true;
    pc = 60;
    status = mc_java_file_exists(list->source_directory, &exists, cause);
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    if (!exists) {
        list->mkdirs_reached = true;
        pc = 70;
        status = mc_java_file_mkdirs(list->source_directory, &list->mkdirs_result, cause);
        if (status != MC_JAVA_FILE_OK)
            goto failed;
        /* Actual boolean result is ignored, including false. */
    }
    return MC_TEXTURE_PACK_OK;
failed:
    texture_source_file_error(status, cause, error, pc);
    error->source_method = "<init>";
    error->source_descriptor = "(Lnet/minecraft/client/Minecraft;Ljava/io/File;)V";
    return error->status;
}

mc_texture_pack_status texture_source_scan_files(mc_client_texture_pack_list *list, size_t *count,
                                                 mc_texture_pack_error *error) {
    mc_java_file_error *cause = &list->source_failure.file;
    bool exists = false, directory = false;
    unsigned int pc = 28;
    mc_java_file_status status = mc_java_file_exists(list->source_directory, &exists, cause);
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    *count = 0;
    if (!exists)
        return MC_TEXTURE_PACK_OK;
    pc = 38;
    status = mc_java_file_is_directory(list->source_directory, &directory, cause);
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    if (!directory)
        return MC_TEXTURE_PACK_OK;
    pc = 48;
    status = mc_java_file_list(list->source_directory, &list->scan_files, cause);
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    if (list->scan_files == NULL)
        return texture_pack_fail(error, MC_TEXTURE_PACK_NULL_REFERENCE,
                                 "ik File.listFiles null arraylength receiver", "ik", "a", "()V",
                                 55, 55);
    *count = mc_java_file_array_length(list->scan_files);
    if (*count > list->options.max_directory_entries)
        return texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT,
                                 "ik File[] native Pack enumeration bound", "ik", "a", "()V", 55,
                                 55);
    return MC_TEXTURE_PACK_OK;
failed:
    return texture_source_file_error(status, cause, error, pc);
}

mc_texture_pack_status texture_source_scan_name(mc_client_texture_pack_list *list, unsigned int pc,
                                                mc_client_texture_string **out,
                                                mc_texture_pack_error *error) {
    mc_world_info_name *name = NULL;
    mc_java_file_error *cause = &list->source_failure.file;
    mc_java_file_status status = mc_java_file_get_name(
        mc_java_file_array_at(list->scan_files, list->scan_index), &name, cause);
    if (status != MC_JAVA_FILE_OK)
        return texture_source_file_error(status, cause, error, pc);
    mc_client_texture_string *wrapped =
        texture_string_name(name, list->options.max_name_units, error);
    mc_world_info_name_release(name);
    if (wrapped == NULL) {
        error->source_class = "ik";
        error->source_method = "a";
        error->source_descriptor = "()V";
        error->source_pc = error->ik_pc = pc;
        return error->status;
    }
    *out = wrapped;
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status texture_zip_construct_file(mc_client_texture_pack *pack, mc_java_file *file,
                                                  mc_texture_pack_error *error) {
    pack->icon_id = -1; /* Actual vp constructor6 after i constructor1. */
    mc_world_info_name *name = NULL;
    mc_java_file_status status = mc_java_file_get_name(file, &name, &pack->file_error);
    unsigned int pc = 11;
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    pack->name = texture_string_name(name, pack->options.max_name_units, error);
    mc_world_info_name_release(name);
    if (pack->name == NULL) {
        error->source_class = "vp";
        error->source_method = "<init>";
        error->source_descriptor = "(Ljava/io/File;)V";
        error->source_pc = 11;
        error->ik_pc = 164;
        return error->status;
    } /* Actual SAME getName result stored a14. */
    pc = 19;
    status = mc_java_file_retain(file, &pack->file_error);
    if (status != MC_JAVA_FILE_OK)
        goto failed;
    pack->source_file = file; /* Actual SAME File[] element h19. */
    return MC_TEXTURE_PACK_OK;
failed:
    texture_source_file_error(status, &pack->file_error, error, pc);
    error->source_class = "vp";
    error->source_method = "<init>";
    error->source_descriptor = "(Ljava/io/File;)V";
    error->ik_pc = 164;
    return error->status;
}

mc_texture_pack_status texture_source_scan_regular(mc_client_texture_pack_list *list, bool *out,
                                                   mc_texture_pack_error *error) {
    mc_java_file_error *cause = &list->source_failure.file;
    const mc_java_file_status status =
        mc_java_file_is_file(mc_java_file_array_at(list->scan_files, list->scan_index), out, cause);
    return status == MC_JAVA_FILE_OK ? MC_TEXTURE_PACK_OK
                                     : texture_source_file_error(status, cause, error, 76);
}

mc_texture_pack_status texture_source_scan_numbers(mc_client_texture_pack_list *list,
                                                   int64_t *bytes, int64_t *modified,
                                                   mc_texture_pack_error *error) {
    mc_java_file *file = mc_java_file_array_at(list->scan_files, list->scan_index);
    mc_java_file_error *cause = &list->source_failure.file;
    mc_java_file_status status = mc_java_file_length(file, bytes, cause);
    if (status != MC_JAVA_FILE_OK)
        return texture_source_file_error(status, cause, error, 120);
    status = mc_java_file_last_modified(file, modified, cause);
    return status == MC_JAVA_FILE_OK ? MC_TEXTURE_PACK_OK
                                     : texture_source_file_error(status, cause, error, 133);
}
