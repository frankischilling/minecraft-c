#include "internal.h"
#include <stdlib.h>

static mc_texture_pack_status report(mc_client_texture_pack_list *list,
                                     const mc_texture_pack_source_error *error,
                                     mc_texture_pack_source_error *out) {
    if (list != NULL)
        list->source_retirement = *error;
    if (out != NULL)
        *out = *error;
    return error->native.status;
}

mc_texture_pack_status mc_client_texture_pack_list_source_allocate(
    mc_java_files *files, const mc_registry_resources *resources,
    const mc_texture_pack_options *options, mc_client_texture_pack_list **out,
    mc_texture_pack_source_error *error) {
    mc_texture_pack_source_error cause = {0};
    if (files == NULL || resources == NULL || out == NULL) {
        texture_pack_fail(&cause.native, MC_TEXTURE_PACK_INVALID_ARGUMENT,
                          "typed Pack native dependencies/output", NULL, NULL, NULL, 0, 0);
        return report(NULL, &cause, error);
    }
    mc_client_texture_pack_list *list = NULL;
    mc_texture_pack_status status = mc_client_texture_pack_list_allocate(resources, options, &list);
    if (status != MC_TEXTURE_PACK_OK) {
        texture_pack_fail(&cause.native, status, "actual new ik native allocation", "ik", "<init>",
                          "(Lnet/minecraft/client/Minecraft;Ljava/io/File;)V", 0, 0);
        return report(NULL, &cause, error);
    }
    mc_java_file_status retained = mc_java_files_retain(files, &cause.file);
    if (retained != MC_JAVA_FILE_OK) {
        texture_source_file_error(retained, &cause.file, &cause.native, 0);
        free(list); /* No source work or owned child exists. */
        return report(NULL, &cause, error);
    }
    list->source_files = files;
    *out = list;
    if (error != NULL)
        *error = cause;
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status
mc_client_texture_pack_list_source_construct(mc_client_texture_pack_list *list,
                                             client_application *app, mc_java_file *parent,
                                             mc_texture_pack_source_error *error) {
    mc_texture_pack_source_error cause = {0};
    if (list == NULL || list->source_files == NULL || list->source_retiring ||
        list->phase != MC_TEXTURE_PACK_RAW ||
        (parent != NULL && mc_java_file_namespace(parent) != list->source_files)) {
        texture_pack_fail(&cause.native, MC_TEXTURE_PACK_INVALID_PHASE,
                          "typed Pack RAW/canonical File namespace admission", NULL, NULL, NULL, 0,
                          0);
        if (error != NULL)
            *error = cause;
        return cause.native.status;
    }
    texture_pack_construct_body(list, app, NULL, parent, &cause.native);
    cause.file = list->source_failure.file;
    if (list->pending_pack != NULL && list->pending_pack->file_error.status != MC_JAVA_FILE_OK)
        cause.file = list->pending_pack->file_error;
    else if (list->selected != NULL && list->selected->file_error.status != MC_JAVA_FILE_OK)
        cause.file = list->selected->file_error;
    list->source_failure = cause;
    if (error != NULL)
        *error = cause;
    return cause.native.status;
}

bool mc_client_texture_pack_list_source_inspect(const mc_client_texture_pack_list *list,
                                                mc_texture_pack_source_state *out) {
    if (list == NULL || out == NULL || list->source_files == NULL || list->active_calls != 0)
        return false;
    *out = (mc_texture_pack_source_state){
        .files = list->source_files,
        .f = list->source_directory,
        .pending_file = list->pending_file,
        .scan_files = list->scan_files,
        .scan_file = list->scan_files == NULL
                         ? NULL
                         : mc_java_file_array_at(list->scan_files, list->scan_index),
        .pending_pack = list->pending_pack,
        .pending_key = list->pending_key,
        .pending_list = list->pending_next,
        .scan_index = list->scan_index,
        .retained_pack_count = list->source_keepalive == NULL ? 0 : list->source_keepalive->count,
        .retiring = list->source_retiring,
        .failed_scan = list->source_scan_failed,
        .failure = &list->source_failure,
        .retirement = &list->source_retirement};
    return true;
}

bool mc_client_texture_pack_source_inspect(const mc_client_texture_pack *pack,
                                           mc_texture_pack_file_state *out) {
    if (pack == NULL || out == NULL)
        return false;
    *out = (mc_texture_pack_file_state){pack->source_file,
                                        pack->name == NULL ? NULL : pack->name->backing,
                                        pack->file_error, pack->native_retirement};
    return true;
}

static size_t vector_holds(const texture_pack_vector *vector, const mc_client_texture_pack *pack) {
    size_t count = 0;
    if (vector != NULL)
        for (size_t i = 0; i < vector->count; ++i)
            count += vector->entries[i] == pack;
    return count;
}

static size_t internal_holds(const mc_client_texture_pack_list *list,
                             const mc_client_texture_pack *pack) {
    size_t count = vector_holds(list->listed, pack) + vector_holds(list->pending_next, pack) +
                   vector_holds(list->source_keepalive, pack);
    if (list->cached != NULL)
        for (size_t i = 0; i < list->cached->count; ++i)
            count += list->cached->entries[i].pack == pack;
    count += list->default_pack == pack;
    count += list->selected == pack;
    count += list->failed_default == pack;
    count += list->pending_pack == pack;
    return count;
}

static bool exclusive_pack(const mc_client_texture_pack_list *list,
                           const mc_client_texture_pack *pack) {
    return pack == NULL || (pack->streams == 0 && pack->holds == internal_holds(list, pack));
}

static bool exclusive_graph(const mc_client_texture_pack_list *list) {
    if (list->holds != 1 || list->active_calls != 0)
        return false;
    const texture_pack_vector *vectors[] = {list->listed, list->pending_next,
                                            list->source_keepalive};
    for (size_t v = 0; v < sizeof(vectors) / sizeof(*vectors); ++v)
        if (vectors[v] != NULL)
            for (size_t i = 0; i < vectors[v]->count; ++i)
                if (!exclusive_pack(list, vectors[v]->entries[i]))
                    return false;
    if (list->cached != NULL)
        for (size_t i = 0; i < list->cached->count; ++i)
            if (!exclusive_pack(list, list->cached->entries[i].pack))
                return false;
    return exclusive_pack(list, list->default_pack) && exclusive_pack(list, list->selected) &&
           exclusive_pack(list, list->failed_default) && exclusive_pack(list, list->pending_pack);
}

static bool retire_vector(texture_pack_vector **slot, mc_texture_pack_source_error *error) {
    texture_pack_vector *vector = *slot;
    if (vector == NULL)
        return true;
    for (size_t i = 0; i < vector->count; ++i) {
        mc_client_texture_pack *pack = vector->entries[i];
        if (texture_pack_release_slot(&vector->entries[i], &error->native) != MC_TEXTURE_PACK_OK) {
            if (vector->entries[i] != NULL)
                error->file = pack->file_error;
            return false; /* NULL slot means consumed close failure; never fclose twice. */
        }
    }
    free(vector->entries);
    free(vector);
    *slot = NULL;
    return true;
}

mc_texture_pack_status
mc_client_texture_pack_list_source_retire(mc_client_texture_pack_list **slot,
                                          mc_texture_pack_source_error *out) {
    mc_texture_pack_source_error error = {0};
    if (slot == NULL) {
        texture_pack_fail(&error.native, MC_TEXTURE_PACK_INVALID_ARGUMENT,
                          "typed Pack retirement output", NULL, NULL, NULL, 0, 0);
        return report(NULL, &error, out);
    }
    mc_client_texture_pack_list *list = *slot;
    if (list == NULL)
        return report(NULL, &error, out);
    if (list->source_files == NULL || !exclusive_graph(list)) {
        texture_pack_fail(&error.native, MC_TEXTURE_PACK_BUSY,
                          "typed Pack retained list/pack/stream dependency", NULL, NULL, NULL, 0,
                          0);
        return report(list, &error, out);
    }
    list->source_retiring = true;
    if (!retire_vector(&list->pending_next, &error) || !retire_vector(&list->listed, &error))
        return report(list, &error, out);
    if (list->cached != NULL) {
        for (size_t i = 0; i < list->cached->count; ++i) {
            texture_pack_cache_entry *entry = &list->cached->entries[i];
            if (texture_pack_release_slot(&entry->pack, &error.native) != MC_TEXTURE_PACK_OK) {
                if (entry->pack != NULL)
                    error.file = entry->pack->file_error;
                return report(list, &error, out);
            }
            texture_string_free(entry->key);
            entry->key = NULL;
        }
        free(list->cached->entries);
        free(list->cached);
        list->cached = NULL;
    }
    mc_client_texture_pack **packs[] = {&list->default_pack, &list->selected, &list->failed_default,
                                        &list->pending_pack};
    for (size_t i = 0; i < sizeof(packs) / sizeof(*packs); ++i) {
        if (texture_pack_release_slot(packs[i], &error.native) != MC_TEXTURE_PACK_OK) {
            if (*packs[i] != NULL)
                error.file = (*packs[i])->file_error;
            return report(list, &error, out);
        }
    }
    if (!retire_vector(&list->source_keepalive, &error))
        return report(list, &error, out);
    if (mc_java_file_array_release(&list->scan_files, &error.file) != MC_JAVA_FILE_OK ||
        mc_java_file_release(&list->pending_file, &error.file) != MC_JAVA_FILE_OK ||
        mc_java_file_release(&list->source_directory, &error.file) != MC_JAVA_FILE_OK ||
        mc_java_files_release(&list->source_files, &error.file) != MC_JAVA_FILE_OK) {
        texture_source_file_error(error.file.status, &error.file, &error.native, 0);
        return report(list, &error, out);
    }
    texture_string_free(list->pending_key);
    texture_string_free(list->selected_name_owned);
    mc_world_info_name_release(list->directory_literal);
    free(list->directory);
    free(list);
    *slot = NULL;
    return report(NULL, &error, out);
}

mc_texture_pack_status texture_source_register_pack(mc_client_texture_pack_list *list,
                                                    mc_client_texture_pack *pack,
                                                    mc_texture_pack_error *error) {
    if (list->source_files == NULL)
        return MC_TEXTURE_PACK_OK;
    if (list->source_keepalive == NULL) {
        list->source_keepalive = calloc(1, sizeof(*list->source_keepalive));
        if (list->source_keepalive == NULL)
            return texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED,
                                     "Pack native borrower closure storage", NULL, NULL, NULL, 0,
                                     0);
    }
    texture_pack_vector *vector = list->source_keepalive;
    if (vector->count >= list->options.max_packs ||
        vector->count >= SIZE_MAX / sizeof(*vector->entries))
        return texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "Pack native retained graph bound",
                                 NULL, NULL, NULL, 0, 0);
    if (vector->count == vector->capacity) {
        size_t capacity = vector->capacity == 0              ? 8u
                          : vector->capacity > SIZE_MAX / 2u ? SIZE_MAX
                                                             : vector->capacity * 2u;
        if (capacity > list->options.max_packs)
            capacity = list->options.max_packs;
        if (capacity > SIZE_MAX / sizeof(*vector->entries))
            return texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT,
                                     "Pack native borrower closure extent", NULL, NULL, NULL, 0, 0);
        mc_client_texture_pack **entries = realloc(vector->entries, capacity * sizeof(*entries));
        if (entries == NULL)
            return texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED,
                                     "Pack native borrower closure entries", NULL, NULL, NULL, 0,
                                     0);
        vector->entries = entries;
        vector->capacity = capacity;
    }
    const mc_texture_pack_status held = mc_client_texture_pack_retain(pack);
    if (held != MC_TEXTURE_PACK_OK)
        return texture_pack_fail(error, held, "Pack native borrower closure occurrence", NULL, NULL,
                                 NULL, 0, 0);
    vector->entries[vector->count++] = pack;
    return MC_TEXTURE_PACK_OK;
}
