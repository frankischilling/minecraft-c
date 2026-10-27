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
    if (cache->count >= owner->options.max_packs ||
        cache->count >= SIZE_MAX / sizeof(*cache->entries)) {
        texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik Map native entry bound", "ik", "a", "()V",
                          186, 186);
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
                              "()V", 186, 186);
            return false;
        }
        void *grown = realloc(cache->entries, capacity * sizeof(*cache->entries));
        if (grown == NULL) {
            texture_pack_fail(e, MC_TEXTURE_PACK_ALLOCATION_FAILED, "ik actual Map storage", "ik",
                              "a", "()V", 186, 186);
            return false;
        }
        cache->entries = grown;
        cache->capacity = capacity;
    }
    if (texture_string_retain(key) != MC_TEXTURE_PACK_OK) {
        texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik Map native String hold", "ik", "a", "()V",
                          186, 186);
        return false;
    }
    if (mc_client_texture_pack_retain(pack) != MC_TEXTURE_PACK_OK) {
        texture_string_free(key);
        texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik Map native pack hold", "ik", "a", "()V",
                          186, 186);
        return false;
    }
    cache->entries[cache->count++] = (texture_pack_cache_entry){key, pack};
    return true;
}

static mc_client_texture_string *make_key(const texture_directory_entry *file,
                                          const mc_texture_pack_options *options,
                                          mc_texture_pack_error *e) {
    texture_file_info info;
    if (texture_file_stat(file->path, &info, e) != MC_TEXTURE_PACK_OK) {
        site(e, 120);
        return NULL;
    }
    if (!info.bytes_valid) {
        texture_pack_fail(e, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE, "ik File.length representation",
                          "ik", "a", "()V", 120, 120);
        return NULL;
    }
    const int64_t bytes = info.bytes;
    if (texture_file_stat(file->path, &info, e) != MC_TEXTURE_PACK_OK) {
        site(e, 133);
        return NULL;
    }
    if (!info.modified_valid) {
        texture_pack_fail(e, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
                          "ik File.lastModified representation", "ik", "a", "()V", 133, 133);
        return NULL;
    }
    char suffix[48];
    const int count =
        snprintf(suffix, sizeof(suffix), ":%" PRId64 ":%" PRId64, bytes, info.modified);
    if (count < 0 || (size_t)count >= sizeof(suffix) ||
        file->name->length > options->max_name_units ||
        (size_t)count > options->max_name_units - file->name->length) {
        texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik filename/length/mtime String bound", "ik",
                          "a", "()V", 139, 139);
        return NULL;
    }
    const size_t units = file->name->length + (size_t)count;
    uint16_t *joined = malloc((units == 0 ? 1 : units) * sizeof(uint16_t));
    if (joined == NULL) {
        texture_pack_fail(e, MC_TEXTURE_PACK_ALLOCATION_FAILED, "ik cache-key String storage", "ik",
                          "a", "()V", 139, 139);
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
    return key;
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
    texture_file_info info;
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
    for (size_t i = 0; i < files.count; ++i) {
        const texture_directory_entry *file = &files.entries[i];
        if (texture_file_stat(file->path, &info, e) != MC_TEXTURE_PACK_OK) {
            site(e, 76);
            goto done;
        }
        if (!info.regular)
            continue;
        bool zip = false;
        if (texture_name_zip(text(file->name), &zip, e) != MC_TEXTURE_PACK_OK) {
            site(e, 87);
            goto done;
        }
        if (!zip)
            continue;
        mc_client_texture_string *key = make_key(file, &owner->options, e);
        if (key == NULL)
            goto done;
        size_t at = find(owner->cached, text(key));
        if (at == owner->cached->count) {
            mc_client_texture_pack *created = NULL;
            if (owner->options.max_packs == 0 ||
                owner->cached->count >= owner->options.max_packs - 1) {
                texture_string_free(key);
                texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "ik new ZIP native graph bound", "ik",
                                  "a", "()V", 158, 158);
                goto done;
            }
            if (texture_pack_new(MC_TEXTURE_PACK_ZIP, owner->resources, &owner->options, &created,
                                 e) != MC_TEXTURE_PACK_OK) {
                texture_string_free(key);
                site(e, 158);
                goto done;
            }
            if (texture_zip_construct(created, file->path, text(file->name), e) !=
                MC_TEXTURE_PACK_OK) {
                texture_string_free(key);
                (void)mc_client_texture_pack_release(created, NULL);
                site(e, 164);
                goto done;
            }
            if (texture_string_retain(key) != MC_TEXTURE_PACK_OK) {
                texture_string_free(key);
                (void)mc_client_texture_pack_release(created, NULL);
                texture_pack_fail(e, MC_TEXTURE_PACK_LIMIT, "vp.d native String hold", "ik", "a",
                                  "()V", 173, 173);
                goto done;
            }
            created->cache_key = key; /* Actual same String identity at i.d173. */
            if (!cache_put(owner, key, created, e)) {
                texture_string_free(key);
                (void)mc_client_texture_pack_release(created, NULL);
                goto done;
            }
            const mc_texture_pack_status loaded = texture_zip_metadata(created, e);
            (void)mc_client_texture_pack_release(created, NULL);
            if (loaded != MC_TEXTURE_PACK_OK) {
                texture_string_free(key);
                e->ik_pc = 196;
                goto done;
            }
            at = find(owner->cached, text(key));
        }
        texture_string_free(key);
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
    release_vector(next);
    return e->status;
}
