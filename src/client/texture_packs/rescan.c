#include "internal.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static mc_texture_pack_text text(const mc_client_texture_string *value) {
    return value == NULL ? (mc_texture_pack_text){NULL, 0, true}
                         : (mc_texture_pack_text){value->units, value->length, false};
}

static mc_texture_pack_status site(mc_texture_pack_error *e, unsigned int pc) {
    if (e->source_class == NULL) {
        e->source_class = "ik";
        e->source_method = "a";
        e->source_descriptor = "()V";
        e->source_pc = pc;
    }
    e->ik_pc = pc;
    return e->status;
}

static bool set_selected(mc_client_texture_pack_list *list, mc_client_texture_pack *pack,
                         unsigned int pc, mc_texture_pack_error *e) {
    if (pack != NULL) {
        const mc_texture_pack_status held = mc_client_texture_pack_retain(pack);
        if (held != MC_TEXTURE_PACK_OK) {
            texture_pack_fail(e, held, "ik selected native pack hold", "ik", "a", "()V", pc, pc);
            return false;
        }
    }
    mc_client_texture_pack *old = list->selected;
    list->selected = pack;
    list->rescan_store_pc = pc;
    mc_texture_pack_error cause = {0};
    if (mc_client_texture_pack_release(old, &cause) != MC_TEXTURE_PACK_OK) {
        *e = cause;
        site(e, pc);
        return false;
    }
    return true;
}

static bool append(texture_pack_vector *list, mc_client_texture_pack *pack, size_t limit,
                   unsigned int pc, mc_texture_pack_error *e) {
    if (list->count >= limit || list->count >= SIZE_MAX / sizeof(*list->entries)) {
        texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik List native entry bound", "ik", "a", "()V",
                          pc, pc);
        return false;
    }
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 8 : list->capacity;
        if (capacity > SIZE_MAX / 2)
            capacity = limit;
        else if (list->capacity != 0)
            capacity *= 2;
        if (capacity > limit)
            capacity = limit;
        if (capacity > SIZE_MAX / sizeof(*list->entries)) {
            texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik List native byte bound", "ik", "a",
                              "()V", pc, pc);
            return false;
        }
        void *grown = realloc(list->entries, capacity * sizeof(*list->entries));
        if (grown == NULL) {
            texture_pack_fail(e, MC_TEXTURE_PACK_ALLOCATION_FAILED, "ik actual List storage", "ik",
                              "a", "()V", pc, pc);
            return false;
        }
        list->entries = grown;
        list->capacity = capacity;
    }
    if (pack != NULL && mc_client_texture_pack_retain(pack) != MC_TEXTURE_PACK_OK) {
        texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik List native pack hold", "ik", "a", "()V",
                          pc, pc);
        return false;
    }
    list->entries[list->count++] = pack;
    return true;
}

static void release_vector(texture_pack_vector *list) {
    if (list == NULL)
        return;
    for (size_t i = 0; i < list->count; ++i)
        (void)mc_client_texture_pack_release(list->entries[i], NULL);
    free(list->entries);
    free(list);
}

static size_t find(texture_pack_cache *cache, mc_texture_pack_text key) {
    for (size_t i = 0; i < cache->count; ++i)
        if (texture_text_equal(text(cache->entries[i].key), key))
            return i;
    return cache->count;
}

static bool cache_put(mc_client_texture_pack_list *owner, mc_client_texture_string *key,
                      mc_client_texture_pack *pack, mc_texture_pack_error *e) {
    texture_pack_cache *cache = owner->cached;
    const unsigned int put_pc = owner->source_files == NULL ? 186u : 184u;
    if (cache->count >= owner->options.max_packs ||
        cache->count >= SIZE_MAX / sizeof(*cache->entries)) {
        texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik Map native entry bound", "ik", "a", "()V",
                          put_pc, put_pc);
        return false;
    }
    if (cache->count == cache->capacity) {
        size_t capacity = cache->capacity == 0 ? 8 : cache->capacity;
        if (capacity > SIZE_MAX / 2)
            capacity = owner->options.max_packs;
        else if (cache->capacity != 0)
            capacity *= 2;
        if (capacity > owner->options.max_packs)
            capacity = owner->options.max_packs;
        if (capacity > SIZE_MAX / sizeof(*cache->entries)) {
            texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik Map native byte bound", "ik", "a",
                              "()V", put_pc, put_pc);
            return false;
        }
        void *grown = realloc(cache->entries, capacity * sizeof(*cache->entries));
        if (grown == NULL) {
            texture_pack_fail(e, MC_TEXTURE_PACK_ALLOCATION_FAILED, "ik actual Map storage", "ik",
                              "a", "()V", put_pc, put_pc);
            return false;
        }
        cache->entries = grown;
        cache->capacity = capacity;
    }
    if (texture_string_retain(key) != MC_TEXTURE_PACK_OK) {
        texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik Map native String hold", "ik", "a", "()V",
                          put_pc, put_pc);
        return false;
    }
    if (mc_client_texture_pack_retain(pack) != MC_TEXTURE_PACK_OK) {
        texture_string_free(key);
        texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik Map native pack hold", "ik", "a", "()V",
                          put_pc, put_pc);
        return false;
    }
    cache->entries[cache->count++] = (texture_pack_cache_entry){key, pack};
    return true;
}

static mc_client_texture_string *make_key(mc_client_texture_pack_list *owner,
                                          const texture_directory_entry *file,
                                          mc_texture_pack_error *e) {
    const mc_texture_pack_options *options = &owner->options;
    texture_file_info info = {0};
    mc_client_texture_string *name = NULL;
    const texture_directory_entry *key_file = file;
    texture_directory_entry typed = {0};
    if (owner->source_files != NULL) {
        if (texture_source_scan_name(owner, 107, &name, e) != MC_TEXTURE_PACK_OK)
            return NULL;
        typed.name = name;
        key_file = &typed;
        if (texture_source_scan_numbers(owner, &info.bytes, &info.modified, e) !=
            MC_TEXTURE_PACK_OK) {
            texture_string_free(name);
            return NULL;
        }
    } else {
        if (texture_file_stat(file->path, &info, e) != MC_TEXTURE_PACK_OK) {
            site(e, 120);
            return NULL;
        }
        if (!info.bytes_valid) {
            texture_pack_fail(e, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
                              "ik File.length representation", "ik", "a", "()V", 120, 120);
            return NULL;
        }
        const int64_t bytes = info.bytes;
        if (texture_file_stat(file->path, &info, e) != MC_TEXTURE_PACK_OK) {
            site(e, 133);
            return NULL;
        }
        info.bytes = bytes;
        if (!info.modified_valid) {
            texture_pack_fail(e, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
                              "ik File.lastModified representation", "ik", "a", "()V", 133, 133);
            return NULL;
        }
    }
    const int64_t bytes = info.bytes;
    file = key_file;
    char suffix[48];
    const int count =
        snprintf(suffix, sizeof(suffix), ":%" PRId64 ":%" PRId64, bytes, info.modified);
    if (count < 0 || (size_t)count >= sizeof(suffix) ||
        file->name->length > options->max_name_units ||
        (size_t)count > options->max_name_units - file->name->length) {
        texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik filename/length/mtime String bound", "ik",
                          "a", "()V", 139, 139);
        texture_string_free(name);
        return NULL;
    }
    const size_t units = file->name->length + (size_t)count;
    uint16_t *joined = malloc((units == 0 ? 1 : units) * sizeof(uint16_t));
    if (joined == NULL) {
        texture_pack_fail(e, MC_TEXTURE_PACK_ALLOCATION_FAILED, "ik cache-key String storage", "ik",
                          "a", "()V", 139, 139);
        texture_string_free(name);
        return NULL;
    }
    memcpy(joined, file->name->units, file->name->length * sizeof(uint16_t));
    for (size_t i = 0; i < (size_t)count; ++i)
        joined[file->name->length + i] = (uint16_t)(unsigned char)suffix[i];
    mc_client_texture_string *key = texture_string_copy(
        (mc_texture_pack_text){joined, units, false}, options->max_name_units, e);
    free(joined);
    if (key == NULL)
        site(e, 139);
    texture_string_free(name);
    return key;
}

static void finish_key(mc_client_texture_pack_list *owner, mc_client_texture_string *key,
                       const mc_texture_pack_error *error) {
    if (owner->source_files != NULL && error->status != MC_TEXTURE_PACK_OK)
        return;
    if (owner->source_files != NULL)
        owner->pending_key = NULL;
    texture_string_free(key);
}

static void finish_pack(mc_client_texture_pack_list *owner, mc_client_texture_pack *pack,
                        const mc_texture_pack_error *error) {
    if (owner->source_files != NULL && error->status != MC_TEXTURE_PACK_OK)
        return;
    if (owner->source_files != NULL)
        owner->pending_pack = NULL;
    (void)mc_client_texture_pack_release(pack, NULL);
}

mc_texture_pack_status texture_pack_rescan_body(mc_client_texture_pack_list *owner,
                                                mc_texture_pack_error *e) {
    texture_pack_vector *next = calloc(1, sizeof(*next));
    texture_directory files = {0};
    if (next == NULL)
        return texture_pack_fail(e, MC_TEXTURE_PACK_ALLOCATION_FAILED, "ik rescan actual ArrayList",
                                 "ik", "a", "()V", 0, 0);
    if (!set_selected(owner, NULL, 10, e) ||
        !append(next, owner->default_pack, owner->options.max_packs, 18, e))
        goto done;
    texture_file_info info = {0};
    size_t file_count = 0;
    if (owner->source_files != NULL) {
        if (texture_source_scan_files(owner, &file_count, e) != MC_TEXTURE_PACK_OK)
            goto done;
    } else {
        if (texture_file_stat(owner->directory, &info, e) != MC_TEXTURE_PACK_OK) {
            site(e, 28);
            goto done;
        }
        if (info.exists) {
            if (texture_file_stat(owner->directory, &info, e) != MC_TEXTURE_PACK_OK) {
                site(e, 41);
                goto done;
            }
            if (info.directory && texture_directory_read(owner->directory, &owner->options, &files,
                                                         e) != MC_TEXTURE_PACK_OK)
                goto done;
        }
        file_count = files.count;
    }
    for (size_t i = 0; i < file_count; ++i) {
        owner->scan_index = i;
        const texture_directory_entry *file =
            owner->source_files == NULL ? &files.entries[i] : NULL;
        mc_client_texture_string *source_name = NULL;
        if (owner->source_files != NULL) {
            if (texture_source_scan_regular(owner, &info.regular, e) != MC_TEXTURE_PACK_OK)
                goto done;
        } else if (texture_file_stat(file->path, &info, e) != MC_TEXTURE_PACK_OK) {
            site(e, 76);
            goto done;
        }
        if (!info.regular)
            continue;
        if (owner->source_files != NULL &&
            texture_source_scan_name(owner, 84, &source_name, e) != MC_TEXTURE_PACK_OK)
            goto done;
        bool zip = false;
        const mc_texture_pack_status named =
            texture_name_zip(text(source_name != NULL ? source_name : file->name), &zip, e);
        texture_string_free(source_name);
        if (named != MC_TEXTURE_PACK_OK) {
            site(e, 87);
            goto done;
        }
        if (!zip)
            continue;
        mc_client_texture_string *key = make_key(owner, file, e);
        if (key == NULL)
            goto done;
        if (owner->source_files != NULL)
            owner->pending_key = key;
        size_t at = find(owner->cached, text(key));
        if (at == owner->cached->count) {
            mc_client_texture_pack *created = NULL;
            if (owner->options.max_packs == 0 ||
                owner->cached->count >= owner->options.max_packs - 1) {
                texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik new ZIP native graph bound", "ik",
                                  "a", "()V", 158, 158);
                finish_key(owner, key, e);
                goto done;
            }
            if (texture_pack_new(MC_TEXTURE_PACK_ZIP, owner->resources, &owner->options, &created,
                                 e) != MC_TEXTURE_PACK_OK) {
                finish_key(owner, key, e);
                site(e, 158);
                goto done;
            }
            if (owner->source_files != NULL)
                owner->pending_pack = created;
            if (texture_source_register_pack(owner, created, e) != MC_TEXTURE_PACK_OK) {
                finish_key(owner, key, e);
                finish_pack(owner, created, e);
                site(e, 158);
                goto done;
            }
            if ((owner->source_files != NULL
                     ? texture_zip_construct_file(created,
                                                  mc_java_file_array_at(owner->scan_files, i), e)
                     : texture_zip_construct(created, file->path, text(file->name), e)) !=
                MC_TEXTURE_PACK_OK) {
                finish_key(owner, key, e);
                finish_pack(owner, created, e);
                site(e, 164);
                goto done;
            }
            if (texture_string_retain(key) != MC_TEXTURE_PACK_OK) {
                texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "vp.d native String hold", "ik", "a",
                                  "()V", 173, 173);
                finish_key(owner, key, e);
                finish_pack(owner, created, e);
                goto done;
            }
            created->cache_key = key; /* Actual same String identity at i.d173. */
            if (!cache_put(owner, key, created, e)) {
                finish_key(owner, key, e);
                finish_pack(owner, created, e);
                goto done;
            }
            const mc_texture_pack_status loaded = texture_zip_metadata(created, e);
            finish_pack(owner, created, e);
            if (loaded != MC_TEXTURE_PACK_OK) {
                finish_key(owner, key, e);
                e->ik_pc = 196;
                goto done;
            }
            at = find(owner->cached, text(key));
        }
        finish_key(owner, key, e);
        mc_client_texture_pack *pack = owner->cached->entries[at].pack;
        if (pack == NULL || pack->name == NULL) {
            texture_pack_fail(
                e, MC_TEXTURE_PACK_NULL_REFERENCE,
                pack == NULL ? "ik cached pack receiver" : "ik pack-name equals receiver", "ik",
                "a", "()V", pack == NULL ? 217u : 224u, pack == NULL ? 217u : 224u);
            goto done;
        }
        if (texture_text_equal(text(pack->name), owner->selected_name) &&
            !set_selected(owner, pack, 233, e))
            goto done;
        if (!append(next, pack, owner->options.max_packs, 239, e))
            goto done;
    }
    if (owner->selected == NULL && !set_selected(owner, owner->default_pack, 273, e))
        goto done;
    if (owner->listed == NULL) {
        texture_pack_fail(e, MC_TEXTURE_PACK_NULL_REFERENCE, "ik old List.removeAll receiver", "ik",
                          "a", "()V", 281, 281);
        goto done;
    }
    /* Real identity removeAll: retain only the old objects absent from next.
     * The old List itself is mutated before any removed pack's unload callback. */
    for (size_t i = 0; i < owner->listed->count;) {
        mc_client_texture_pack *pack = owner->listed->entries[i];
        size_t j = 0;
        while (j < next->count && next->entries[j] != pack)
            ++j;
        if (j == next->count) {
            ++i;
            continue;
        }
        --owner->listed->count;
        memmove(owner->listed->entries + i, owner->listed->entries + i + 1,
                (owner->listed->count - i) * sizeof(*owner->listed->entries));
        (void)mc_client_texture_pack_release(pack, NULL);
    }
    for (size_t i = 0; i < owner->listed->count; ++i) {
        mc_client_texture_pack *pack = owner->listed->entries[i];
        if (pack == NULL) {
            texture_pack_fail(e, MC_TEXTURE_PACK_NULL_REFERENCE, "ik removed pack receiver", "ik",
                              "a", "()V", 321, 321);
            goto done;
        }
        if (texture_pack_unload(pack, owner->app, e) != MC_TEXTURE_PACK_OK)
            goto done;
        const size_t at = find(owner->cached, text(pack->cache_key));
        if (at < owner->cached->count) {
            texture_pack_cache_entry old = owner->cached->entries[at];
            --owner->cached->count;
            memmove(owner->cached->entries + at, owner->cached->entries + at + 1,
                    (owner->cached->count - at) * sizeof(*owner->cached->entries));
            texture_string_free(old.key);
            (void)mc_client_texture_pack_release(old.pack, NULL);
        }
    }
    texture_pack_vector *old = owner->listed;
    owner->listed = next;
    next = NULL;
    owner->rescan_store_pc = 343;
    release_vector(old);
done:
    texture_directory_free(&files);
    if (owner->source_files != NULL) {
        if (e->status == MC_TEXTURE_PACK_OK &&
            mc_java_file_array_release(&owner->scan_files, &owner->source_failure.file) !=
                MC_JAVA_FILE_OK)
            texture_source_file_error(owner->source_failure.file.status,
                                      &owner->source_failure.file, e, 0);
        if (e->status != MC_TEXTURE_PACK_OK) {
            owner->pending_next = next;
            owner->source_scan_failed = true;
            owner->source_failure.native = *e;
            return e->status;
        }
    }
    release_vector(next);
    return e->status;
}
