#include "../application_access.h"
#include "internal.h"
#include <stdlib.h>

static const char constructor[] = "(Lnet/minecraft/client/Minecraft;Ljava/io/File;)V";

mc_texture_pack_options mc_client_texture_pack_default_options(void) {
    return (mc_texture_pack_options){.max_packs = 256,
                                     .max_directory_entries = 4096,
                                     .max_path_bytes = 1048576,
                                     .max_name_units = 65535,
                                     .max_streams = 256,
                                     .max_archive_bytes = 64u * 1024u * 1024u,
                                     .max_zip_entries = 4096,
                                     .max_encoded_image_bytes = 16u * 1024u * 1024u,
                                     .max_decoded_image_bytes = 16u * 1024u * 1024u,
                                     .max_stream_allocation_bytes = 256u * 1024u,
                                     .metadata_charset = MC_JAVA_READER_DEFAULT};
}

mc_texture_pack_status texture_pack_new(mc_texture_pack_kind kind,
                                        const mc_registry_resources *resources,
                                        const mc_texture_pack_options *options,
                                        mc_client_texture_pack **out,
                                        mc_texture_pack_error *error) {
    mc_client_texture_pack *pack = calloc(1, sizeof(*pack));
    if (pack == NULL)
        return texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED,
                                 "Texture pack actual object", NULL, NULL, NULL, 0, 0);
    pack->holds = 1;
    pack->resources = resources;
    pack->options = *options;
    pack->kind = kind;
    *out = pack;
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status mc_client_texture_pack_retain(mc_client_texture_pack *pack) {
    if (pack == NULL)
        return MC_TEXTURE_PACK_INVALID_ARGUMENT;
    if (pack->holds == SIZE_MAX)
        return MC_TEXTURE_PACK_LIMIT;
    ++pack->holds;
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status mc_client_texture_pack_release(mc_client_texture_pack *pack,
                                                      mc_texture_pack_error *error) {
    mc_texture_pack_error e = {0};
    if (pack != NULL) {
        if (pack->holds == 1 && pack->streams != 0)
            texture_pack_fail(&e, MC_TEXTURE_PACK_BUSY, "Texture pack retained stream active", NULL,
                              NULL, NULL, 0, 0);
        else if (--pack->holds == 0) {
            if (pack->zip != NULL) {
                texture_zip_error cause = {0};
                if (texture_zip_close(pack->zip, &cause) != TEXTURE_ZIP_OK) {
                    texture_pack_zip_error(&e, &cause);
                    e.cause_status = e.status;
                    e.status = MC_TEXTURE_PACK_PROVIDER_FAILED;
                    e.native_close_error = cause.system_error;
                    e.operation = "Texture pack native archive retirement";
                }
                texture_zip_release(pack->zip, NULL);
            }
            texture_string_free(pack->name);
            texture_string_free(pack->description);
            texture_string_free(pack->description2);
            texture_string_free(pack->cache_key);
            mc_client_argb_image_release(pack->icon);
            free(pack->file);
            free(pack);
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_texture_pack_inspect(const mc_client_texture_pack *pack,
                                    mc_texture_pack_state *out) {
    if (pack == NULL || out == NULL)
        return false;
    *out =
        (mc_texture_pack_state){pack->kind,        pack->holds,        pack->streams,    pack->name,
                                pack->description, pack->description2, pack->cache_key,  pack->file,
                                pack->icon,        pack->icon_id,      pack->zip != NULL};
    return true;
}

mc_texture_pack_status mc_client_texture_pack_list_allocate(const mc_registry_resources *resources,
                                                            const mc_texture_pack_options *options,
                                                            mc_client_texture_pack_list **out) {
    if (out == NULL || resources == NULL ||
        (options != NULL && (unsigned int)options->metadata_charset > MC_JAVA_READER_US_ASCII))
        return MC_TEXTURE_PACK_INVALID_ARGUMENT;
    mc_client_texture_pack_list *list = calloc(1, sizeof(*list));
    if (list == NULL)
        return MC_TEXTURE_PACK_ALLOCATION_FAILED;
    list->holds = 1;
    list->resources = resources;
    list->options = options == NULL ? mc_client_texture_pack_default_options() : *options;
    *out = list;
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status mc_client_texture_pack_list_retain(mc_client_texture_pack_list *list) {
    if (list == NULL)
        return MC_TEXTURE_PACK_INVALID_ARGUMENT;
    if (list->holds == SIZE_MAX)
        return MC_TEXTURE_PACK_LIMIT;
    ++list->holds;
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status mc_client_texture_pack_list_construct(mc_client_texture_pack_list *list,
                                                             client_application *app,
                                                             const char *parent,
                                                             mc_texture_pack_error *error) {
    mc_texture_pack_error e = {0};
    if (list == NULL || list->phase != MC_TEXTURE_PACK_RAW) {
        texture_pack_fail(
            &e, list == NULL ? MC_TEXTURE_PACK_INVALID_ARGUMENT : MC_TEXTURE_PACK_INVALID_PHASE,
            "Texture pack RAW constructor admission", NULL, NULL, NULL, 0, 0);
        goto done;
    }
    list->phase = MC_TEXTURE_PACK_CONSTRUCTING;
    ++list->active_calls;
    list->listed = calloc(1, sizeof(*list->listed));
    if (list->listed == NULL) {
        texture_pack_fail(&e, MC_TEXTURE_PACK_ALLOCATION_FAILED, "ik actual ArrayList allocation",
                          "ik", "<init>", constructor, 5, 5);
        goto constructed;
    }
    list->last_store_pc = 12;
    if (list->options.max_packs == 0) {
        texture_pack_fail(&e, MC_TEXTURE_PACK_LIMIT, "ik actual default pack native bound", "ik",
                          "<init>", constructor, 16, 16);
        goto constructed;
    }
    if (texture_pack_new(MC_TEXTURE_PACK_DEFAULT, list->resources, &list->options,
                         &list->failed_default, &e) != MC_TEXTURE_PACK_OK) {
        e.source_class = "ik";
        e.source_method = "<init>";
        e.source_descriptor = constructor;
        e.source_pc = e.ik_pc = 16;
        goto constructed;
    }
    if (texture_default_construct(list->failed_default, &e) != MC_TEXTURE_PACK_OK) {
        e.ik_pc = 20;
        goto constructed;
    }
    list->default_pack = list->failed_default;
    list->failed_default = NULL;
    list->last_store_pc = 23;
    list->cached = calloc(1, sizeof(*list->cached));
    if (list->cached == NULL) {
        texture_pack_fail(&e, MC_TEXTURE_PACK_ALLOCATION_FAILED, "ik actual HashMap allocation",
                          "ik", "<init>", constructor, 27, 27);
        goto constructed;
    }
    list->last_store_pc = 34;
    list->app = app;
    list->last_store_pc = 39;
    list->directory = texture_path_join(parent, "texturepacks", list->options.max_path_bytes, &e);
    if (list->directory == NULL) {
        e.source_class = "ik";
        e.source_method = "<init>";
        e.source_descriptor = constructor;
        e.source_pc = e.ik_pc = 50;
        goto constructed;
    }
    list->last_store_pc = 53;
    texture_file_info info = {0};
    list->exists_reached = true;
    if (texture_file_stat(list->directory, &info, &e) != MC_TEXTURE_PACK_OK) {
        e.source_class = "ik";
        e.source_method = "<init>";
        e.source_descriptor = constructor;
        e.source_pc = e.ik_pc = 60;
        goto constructed;
    }
    if (!info.exists) {
        list->mkdirs_reached = true;
        list->mkdirs_result = texture_file_mkdirs(list->directory, &e);
        if (e.status != MC_TEXTURE_PACK_OK) {
            e.source_class = "ik";
            e.source_method = "<init>";
            e.source_descriptor = constructor;
            e.source_pc = e.ik_pc = 70;
            goto constructed;
        }
    }
    if (app == NULL) {
        texture_pack_fail(&e, MC_TEXTURE_PACK_NULL_REFERENCE, "ik Minecraft.z receiver", "ik",
                          "<init>", constructor, 76, 76);
        goto constructed;
    }
    mc_game_settings *settings = mc_client_application_live_settings(app);
    mc_game_settings_state state;
    if (settings == NULL || !mc_game_settings_read_state(settings, &state)) {
        texture_pack_fail(&e, MC_TEXTURE_PACK_NULL_REFERENCE, "ik Settings.l receiver", "ik",
                          "<init>", constructor, 79, 79);
        goto constructed;
    }
    /* Settings retains every replaced String in its actual native arena. This
     * field borrows that same stable immutable view; no guessed default copy. */
    list->selected_name =
        (mc_texture_pack_text){state.values.texture_pack.units, state.values.texture_pack.length,
                               state.values.texture_pack.is_null};
    list->last_store_pc = 82;
    if (texture_pack_rescan_body(list, &e) != MC_TEXTURE_PACK_OK) {
        e.constructor_pc = 86;
        goto constructed;
    }
    if (list->selected == NULL)
        texture_pack_fail(&e, MC_TEXTURE_PACK_NULL_REFERENCE, "ik selected pack load receiver",
                          "ik", "<init>", constructor, 93, 93);
    else if (texture_pack_load(list->selected, &e) != MC_TEXTURE_PACK_OK)
        e.constructor_pc = e.ik_pc = 93;
constructed:
    --list->active_calls;
    list->phase = e.status == MC_TEXTURE_PACK_OK ? MC_TEXTURE_PACK_READY : MC_TEXTURE_PACK_FAILED;
    list->failure = e;
done:
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_texture_pack_status mc_client_texture_pack_list_rescan(mc_client_texture_pack_list *list,
                                                          mc_texture_pack_error *error) {
    mc_texture_pack_error e = {0};
    if (list == NULL || list->phase != MC_TEXTURE_PACK_READY)
        texture_pack_fail(
            &e, list == NULL ? MC_TEXTURE_PACK_INVALID_ARGUMENT : MC_TEXTURE_PACK_INVALID_PHASE,
            "Texture pack rescan READY admission", NULL, NULL, NULL, 0, 0);
    else if (list->active_calls != 0)
        texture_pack_fail(&e, MC_TEXTURE_PACK_BUSY, "Texture pack list operation active", NULL,
                          NULL, NULL, 0, 0);
    else {
        ++list->active_calls;
        texture_pack_rescan_body(list, &e);
        --list->active_calls;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_texture_pack_status mc_client_texture_pack_list_release(mc_client_texture_pack_list *list,
                                                           mc_texture_pack_error *error) {
    mc_texture_pack_error e = {0};
    if (list != NULL) {
        if (list->active_calls != 0)
            texture_pack_fail(&e, MC_TEXTURE_PACK_BUSY, "Texture pack list operation active", NULL,
                              NULL, NULL, 0, 0);
        else if (--list->holds == 0) {
            if (list->listed != NULL) {
                for (size_t i = 0; i < list->listed->count; ++i) {
                    mc_texture_pack_error cause = {0};
                    mc_client_texture_pack_release(list->listed->entries[i], &cause);
                    if (e.status == MC_TEXTURE_PACK_OK && cause.status != MC_TEXTURE_PACK_OK)
                        e = cause;
                }
                free(list->listed->entries);
                free(list->listed);
            }
            if (list->cached != NULL) {
                for (size_t i = 0; i < list->cached->count; ++i) {
                    mc_texture_pack_error cause = {0};
                    texture_string_free(list->cached->entries[i].key);
                    mc_client_texture_pack_release(list->cached->entries[i].pack, &cause);
                    if (e.status == MC_TEXTURE_PACK_OK && cause.status != MC_TEXTURE_PACK_OK)
                        e = cause;
                }
                free(list->cached->entries);
                free(list->cached);
            }
            mc_client_texture_pack *fields[] = {list->default_pack, list->selected,
                                                list->failed_default};
            for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
                mc_texture_pack_error cause = {0};
                mc_client_texture_pack_release(fields[i], &cause);
                if (e.status == MC_TEXTURE_PACK_OK && cause.status != MC_TEXTURE_PACK_OK)
                    e = cause;
            }
            texture_string_free(list->selected_name_owned);
            free(list->directory);
            free(list);
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_texture_pack_list_inspect(const mc_client_texture_pack_list *list,
                                         mc_texture_pack_list_state *out) {
    if (list == NULL || out == NULL)
        return false;
    *out =
        (mc_texture_pack_list_state){.phase = list->phase,
                                     .native_holds = list->holds,
                                     .listed_count = list->listed == NULL ? 0 : list->listed->count,
                                     .cached_count = list->cached == NULL ? 0 : list->cached->count,
                                     .active_calls = list->active_calls,
                                     .last_store_pc = list->last_store_pc,
                                     .rescan_store_pc = list->rescan_store_pc,
                                     .application = list->app,
                                     .directory = list->directory,
                                     .list_identity = list->listed,
                                     .cache_identity = list->cached,
                                     .default_pack = list->default_pack,
                                     .selected_pack = list->selected,
                                     .failed_default_pack = list->failed_default,
                                     .selected_name = list->selected_name,
                                     .exists_reached = list->exists_reached,
                                     .mkdirs_reached = list->mkdirs_reached,
                                     .mkdirs_result = list->mkdirs_result,
                                     .failure = list->failure};
    return true;
}

bool mc_client_texture_pack_selected(const mc_client_texture_pack_list *list,
                                     mc_client_texture_pack **out) {
    if (list == NULL || out == NULL)
        return false;
    *out = list->selected;
    return true;
}

bool mc_client_texture_pack_at(const mc_client_texture_pack_list *list, size_t at,
                               mc_client_texture_pack **out) {
    if (list == NULL || out == NULL || list->listed == NULL || at >= list->listed->count)
        return false;
    *out = list->listed->entries[at];
    return true;
}

bool mc_client_texture_pack_cache_at(const mc_client_texture_pack_list *list, size_t at,
                                     const mc_client_texture_string **key,
                                     mc_client_texture_pack **out) {
    if (list == NULL || key == NULL || out == NULL || list->cached == NULL ||
        at >= list->cached->count)
        return false;
    *key = list->cached->entries[at].key;
    *out = list->cached->entries[at].pack;
    return true;
}
