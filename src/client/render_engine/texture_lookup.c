#include "../texture_packs/image_io.h"
#include "context_internal.h"
#include "ge.h"
#include "resource_lookup_internal.h"
#include "string_key_internal.h"
#include <stdlib.h>
#include <string.h>

static const char string_to_int[] = "(Ljava/lang/String;)I";

static mc_client_graphics_site lookup_site(int32_t pc) {
    return (mc_client_graphics_site){"ji", "b", string_to_int, pc};
}

static mc_client_texture_lookup_status failure(mc_client_texture_lookup_error *e,
                                               mc_client_texture_lookup_status status,
                                               const char *operation, int32_t pc, bool native) {
    e->status = status;
    e->operation = operation;
    e->site = lookup_site(pc);
    e->native_policy = native;
    return status;
}

static void copy_error(mc_client_texture_lookup_error *out,
                       const mc_client_texture_lookup_error *e) {
    if (out != NULL)
        memcpy(out, e, sizeof(*out));
}

static bool shape(mc_texture_pack_text key) {
    return mc_client_resource_key_shape(key);
}

static bool equal(const mc_client_render_string_id_entry *a, mc_texture_pack_text b) {
    return mc_client_resource_key_equal(a->units, a->length, a->is_null, b);
}

static size_t cache_find(const mc_client_render_map *map, mc_texture_pack_text key) {
    size_t at = 0;
    while (at < map->count && !equal(&map->string_ids[at], key))
        ++at;
    return at;
}

bool mc_client_render_engine_cached_texture(const mc_client_render_engine *o,
                                            mc_texture_pack_text key, bool *found,
                                            mc_client_texture *out) {
    if (o == NULL || found == NULL || out == NULL || !shape(key) || o->fields.b == NULL ||
        (o->fields.b->count != 0 && o->fields.b->string_ids == NULL))
        return false;
    const size_t at = cache_find(o->fields.b, key);
    *found = at < o->fields.b->count;
    if (*found)
        *out = o->fields.b->string_ids[at].id;
    return true;
}

mc_client_texture_lookup_options mc_client_texture_lookup_default_options(void) {
    return (mc_client_texture_lookup_options){mc_client_render_texture_default_options(), 4096,
                                              65536, 1024u * 1024u};
}

static bool options_equal(const mc_client_texture_lookup_options *a,
                          const mc_client_texture_lookup_options *b) {
    return a->max_cache_entries == b->max_cache_entries && a->max_key_units == b->max_key_units &&
           a->max_prefix_bytes == b->max_prefix_bytes &&
           a->textures.max_retained_ids == b->textures.max_retained_ids &&
           a->textures.max_source_ledger_entries == b->textures.max_source_ledger_entries &&
           a->textures.max_image_entries == b->textures.max_image_entries &&
           a->textures.max_upload_scratch_bytes == b->textures.max_upload_scratch_bytes;
}

static void touch(mc_client_texture_lookup_attempt *a, int32_t pc) {
    a->state.last_site = lookup_site(pc);
}

static bool starts(mc_texture_pack_text key, const char *prefix, size_t length) {
    return mc_client_resource_key_starts(key, prefix, length);
}

static mc_client_texture_lookup_status cache_put(mc_client_render_engine *o,
                                                 mc_client_texture_lookup_attempt *a,
                                                 mc_texture_pack_text key, mc_client_texture id) {
    mc_client_texture_lookup_error *e = &a->failure;
    mc_client_render_map *map = (mc_client_render_map *)(void *)o->fields.b;
    if (map == NULL)
        return failure(e, MC_CLIENT_LOOKUP_NULL_REFERENCE, "actual b HashMap.put receiver", 213,
                       false);
    if (map->count != 0 && map->string_ids == NULL)
        return failure(e, MC_CLIENT_LOOKUP_INVALID_PHASE, "actual b native String-ID map kind", 213,
                       true);
    const mc_client_texture_lookup_options *options = &o->lookup->options;
    size_t at = cache_find(map, key);
    if (at < map->count) {
        map->string_ids[at].id = id; /* HashMap preserves an existing equal key identity. */
        a->state.cache_published = true;
        touch(a, 213);
        return MC_CLIENT_LOOKUP_OK;
    }
    if (map->count >= options->max_cache_entries || key.length > options->max_key_units ||
        key.length > SIZE_MAX / sizeof(uint16_t))
        return failure(e, MC_CLIENT_LOOKUP_LIMIT, "actual b key/entry native bound", 213, true);
    if (map->count == map->capacity) {
        const size_t limit = options->max_cache_entries;
        size_t capacity = map->capacity == 0 ? 8 : map->capacity;
        if (map->capacity != 0)
            capacity = capacity > SIZE_MAX / 2 ? limit : capacity * 2;
        if (capacity > limit)
            capacity = limit;
        if (capacity > SIZE_MAX / sizeof(mc_client_render_string_id_entry))
            return failure(e, MC_CLIENT_LOOKUP_LIMIT, "actual b native array byte bound", 213,
                           true);
        mc_client_render_string_id_entry *entries =
            realloc(map->string_ids, capacity * sizeof(*entries));
        if (entries == NULL)
            return failure(e, MC_CLIENT_LOOKUP_ALLOCATION_FAILED, "actual b native table growth",
                           213, true);
        map->string_ids = entries;
        map->capacity = capacity;
    }
    uint16_t *units = NULL;
    if (mc_client_resource_key_copy(key, options->max_key_units, &units) !=
        MC_CLIENT_RESOURCE_KEY_OK)
        return failure(e, MC_CLIENT_LOOKUP_ALLOCATION_FAILED, "actual b immutable key copy", 213,
                       true);
    map->string_ids[map->count++] =
        (mc_client_render_string_id_entry){units, key.length, key.is_null, id};
    a->state.cache_published = true;
    touch(a, 213);
    return MC_CLIENT_LOOKUP_OK;
}

static mc_client_texture_lookup_status run_body(mc_client_render_engine *o,
                                                mc_client_display *display,
                                                mc_texture_pack_text key,
                                                mc_client_texture_lookup_attempt *a) {
    mc_client_texture_lookup_error *e = &a->failure;
    if (mc_client_resource_lookup_capture(o, a) != MC_CLIENT_LOOKUP_OK)
        return e->status;
    if (o->fields.b == NULL)
        return failure(e, MC_CLIENT_LOOKUP_NULL_REFERENCE, "actual b HashMap.get receiver", 13,
                       false);
    bool found;
    mc_client_texture id;
    if (!mc_client_render_engine_cached_texture(o, key, &found, &id))
        return failure(e, MC_CLIENT_LOOKUP_INVALID_PHASE, "actual b native String-ID map kind", 13,
                       true);
    touch(a, 13);
    if (found) {
        a->state.cache_hit = a->state.source_returned = true;
        a->state.returned_id = id;
        touch(a, 28);
        return MC_CLIENT_LOOKUP_OK;
    }
    mc_client_direct_int_buffer *buffer = (mc_client_direct_int_buffer *)(void *)o->fields.e;
    if (!mc_client_ge_int_clear(buffer, lookup_site(33), &e->buffer))
        return failure(e,
                       e->buffer.status == MC_CLIENT_GRAPHICS_NULL_REFERENCE
                           ? MC_CLIENT_LOOKUP_NULL_REFERENCE
                           : MC_CLIENT_LOOKUP_BUFFER_FAILED,
                       "actual e.clear33", 33, e->buffer.native_policy);
    a->state.clear_completed = true;
    touch(a, 33);
    const bool generated =
        mc_client_texture_generate(o, display, &o->lookup->options.textures, buffer,
                                   lookup_site(41), &a->generation, &e->texture);
    a->state.generated = e->texture.driver_generated;
    a->state.source_ledger_appended = e->texture.source_ledger_appended;
    a->state.generated_id = e->texture.generated_id;
    if (!generated)
        return failure(e,
                       e->texture.status == MC_CLIENT_RENDER_TEXTURE_UNIMPLEMENTED
                           ? MC_CLIENT_LOOKUP_UNIMPLEMENTED
                           : MC_CLIENT_LOOKUP_TEXTURE_FAILED,
                       "actual first ge texture generation", 41, true);
    touch(a, 41);
    int32_t actual;
    if (!mc_client_ge_int_get(buffer, 0, &actual, lookup_site(49), &e->buffer))
        return failure(e, MC_CLIENT_LOOKUP_BUFFER_FAILED, "actual e.get(0)49", 49,
                       e->buffer.native_policy);
    memcpy(&id, &actual, sizeof(id)); /* Preserve the full source Integer payload. */
    a->state.generated_id = id;
    touch(a, 49);
    if (key.is_null)
        return failure(e, MC_CLIENT_LOOKUP_NULL_REFERENCE, "actual null String.startsWith receiver",
                       56, false);
    touch(a, 56);

    enum { NORMAL, STRIPS, CLAMP, BLUR } mode = NORMAL;

    size_t skip = 0;
    int32_t open_pc = 171, read_pc = 197, upload_pc = 201;
    if (starts(key, "##", 2)) {
        mode = STRIPS;
        skip = 2;
        open_pc = 71;
        read_pc = 74;
        upload_pc = 81;
    } else if (starts(key, "%clamp%", 7)) {
        mode = CLAMP;
        skip = 7;
        open_pc = 110;
        read_pc = 113;
        upload_pc = 117;
        o->fields.j = true;
        touch(a, 98);
    } else if (starts(key, "%blur%", 6)) {
        mode = BLUR;
        skip = 6;
        open_pc = 151;
        read_pc = 154;
        upload_pc = 158;
        o->fields.k = true;
        touch(a, 139);
    }
    const mc_texture_pack_text name = {skip == 0 ? key.units : key.units + skip, key.length - skip,
                                       false};
    if (mc_client_resource_lookup_open(a, name, open_pc) != MC_CLIENT_LOOKUP_OK)
        return e->status;
    mc_client_argb_image *image;
    if (mode == NORMAL && a->stream == NULL) {
        image = o->fields.m;
        upload_pc = 187;
    } else {
        if (mc_client_resource_lookup_read(a, read_pc) != MC_CLIENT_LOOKUP_OK)
            return e->status;
        if (mode == STRIPS) {
            e->dependency = (mc_client_graphics_site){
                "ji", "c", "(Ljava/awt/image/BufferedImage;)Ljava/awt/image/BufferedImage;", 0};
            return failure(e, MC_CLIENT_LOOKUP_UNIMPLEMENTED,
                           "actual ## Graphics strip transform body", 77, false);
        }
        image = a->image; /* Successful ImageIO NULL still reaches actual upload. */
    }
    if (!mc_client_texture_upload(o, a->generation->resource, image, lookup_site(upload_pc),
                                  &e->texture))
        return failure(e,
                       e->texture.status == MC_CLIENT_RENDER_TEXTURE_UNIMPLEMENTED
                           ? MC_CLIENT_LOOKUP_UNIMPLEMENTED
                       : e->texture.status == MC_CLIENT_RENDER_TEXTURE_NULL_REFERENCE
                           ? MC_CLIENT_LOOKUP_NULL_REFERENCE
                           : MC_CLIENT_LOOKUP_TEXTURE_FAILED,
                       "actual same-ID image upload", upload_pc,
                       e->texture.status != MC_CLIENT_RENDER_TEXTURE_NULL_REFERENCE);
    touch(a, upload_pc);
    if (mode == CLAMP) {
        o->fields.j = false;
        touch(a, 122);
    }
    if (mode == BLUR) {
        o->fields.k = false;
        touch(a, 163);
    }
    if (cache_put(o, a, key, id) != MC_CLIENT_LOOKUP_OK)
        return e->status;
    a->state.source_returned = true;
    a->state.returned_id = id;
    touch(a, 218);
    return MC_CLIENT_LOOKUP_OK;
}

bool mc_client_texture_lookup_pending(const mc_client_render_engine *o) {
    return o != NULL && o->lookup != NULL && o->lookup->pending != NULL;
}

mc_client_texture_lookup_status
mc_client_render_engine_texture(mc_client_render_engine *o, mc_client_display *display,
                                const mc_client_texture_lookup_options *requested,
                                mc_texture_pack_text key, mc_client_texture *out,
                                mc_client_texture_lookup_error *error) {
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
        error->site.pc = -1;
    }
    mc_client_texture_lookup_error admission = {0};
    if (o == NULL || out == NULL || !shape(key)) {
        failure(&admission, MC_CLIENT_LOOKUP_INVALID_ARGUMENT, "lookup native receiver/key/output",
                -1, true);
        copy_error(error, &admission);
        return admission.status;
    }
    if (o->lookup_retired) {
        failure(&admission, MC_CLIENT_LOOKUP_INVALID_PHASE, "lookup final native stop", -1, true);
        copy_error(error, &admission);
        return admission.status;
    }
    mc_client_texture_lookup_options options =
        requested == NULL ? (o->lookup == NULL || !o->lookup->texture_policy_set
                                 ? mc_client_texture_lookup_default_options()
                                 : o->lookup->options)
                          : *requested;
    if (requested == NULL && (o->lookup == NULL || !o->lookup->texture_policy_set))
        options.textures = mc_client_texture_body_options(o);
    if (o->lookup != NULL &&
        ((o->lookup->texture_policy_set && !options_equal(&options, &o->lookup->options)) ||
         o->lookup->active || o->lookup->pending != NULL)) {
        failure(&admission,
                o->lookup->active || o->lookup->pending != NULL ? MC_CLIENT_LOOKUP_BUSY
                                                                : MC_CLIENT_LOOKUP_INVALID_ARGUMENT,
                "lookup fixed native policy/pending prefix", -1, true);
        copy_error(error, &admission);
        return admission.status;
    }
    if (!mc_client_texture_body_admit(o, display, &options.textures, &admission.texture)) {
        failure(&admission,
                admission.texture.status == MC_CLIENT_RENDER_TEXTURE_BUSY ? MC_CLIENT_LOOKUP_BUSY
                : admission.texture.status == MC_CLIENT_RENDER_TEXTURE_INVALID_PHASE
                    ? MC_CLIENT_LOOKUP_INVALID_PHASE
                    : MC_CLIENT_LOOKUP_INVALID_ARGUMENT,
                "lookup native resource namespace", -1, true);
        copy_error(error, &admission);
        return admission.status;
    }
    if (options.max_prefix_bytes <
        sizeof(mc_client_texture_lookup_attempt) + sizeof(struct mc_client_texture_lookup_native)) {
        failure(&admission, MC_CLIENT_LOOKUP_LIMIT, "lookup native prefix byte budget", -1, true);
        copy_error(error, &admission);
        return admission.status;
    }
    if (o->lookup == NULL) {
        o->lookup = calloc(1, sizeof(*o->lookup));
        if (o->lookup == NULL) {
            failure(&admission, MC_CLIENT_LOOKUP_ALLOCATION_FAILED, "lookup native policy owner",
                    -1, true);
            copy_error(error, &admission);
            return admission.status;
        }
    }
    if (!o->lookup->texture_policy_set) {
        o->lookup->options = options;
        o->lookup->texture_policy_set = true;
    }
    mc_client_texture_lookup_attempt *a = calloc(1, sizeof(*a));
    if (a == NULL) {
        failure(&admission, MC_CLIENT_LOOKUP_ALLOCATION_FAILED, "lookup native bounded attempt", -1,
                true);
        copy_error(error, &admission);
        return admission.status;
    }
    a->method = lookup_site(0);
    mc_client_texture_lookup_status status = mc_client_resource_lookup_enter(o, a);
    if (status != MC_CLIENT_LOOKUP_OK) {
        copy_error(error, &a->failure);
        free(a);
        return status;
    }
    status = run_body(o, display, key, a);
    status = mc_client_resource_lookup_finish(o, a, status);
    copy_error(error, &a->failure);
    if (status == MC_CLIENT_LOOKUP_OK) {
        *out = a->state.returned_id;
        o->lookup->pending = NULL;
        free(a);
    }
    (void)mc_client_render_engine_release(o); /* Balanced nonfinal call occurrence. */
    return status;
}

bool mc_client_render_engine_lookup_inspect(const mc_client_render_engine *o,
                                            mc_client_texture_lookup_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = (mc_client_texture_lookup_state){0};
    if (o->lookup == NULL)
        return true;
    out->cleanup = &o->lookup->last_cleanup;
    const mc_client_texture_lookup_attempt *a = o->lookup->pending;
    if (a == NULL)
        return true;
    *out = a->state;
    out->pending = true;
    out->stream = a->stream;
    out->image = a->image;
    out->array = a->array;
    out->captured_list = a->list_held ? a->list : NULL;
    out->captured_pack = a->pack_held ? a->pack : NULL;
    out->failure = &a->failure;
    out->cleanup = &a->cleanup;
    return true;
}

mc_client_texture_lookup_status
mc_client_render_engine_retire_lookup(mc_client_render_engine *o,
                                      mc_client_texture_lookup_error *error) {
    if (error != NULL) {
        memset(error, 0, sizeof(*error));
        error->site.pc = -1;
    }
    if (o == NULL) {
        if (error != NULL)
            failure(error, MC_CLIENT_LOOKUP_INVALID_ARGUMENT, "lookup retirement receiver", -1,
                    true);
        return MC_CLIENT_LOOKUP_INVALID_ARGUMENT;
    }
    if (o->phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING ||
        (o->lookup != NULL && o->lookup->active) || o->active_resource_calls != 0 ||
        o->classes->active_resource_calls != 0 || o->classes->active_source_calls != 0) {
        if (error != NULL)
            failure(error, MC_CLIENT_LOOKUP_BUSY, "lookup prefix retirement active", -1, true);
        return MC_CLIENT_LOOKUP_BUSY;
    }
    if (o->lookup == NULL || o->lookup->pending == NULL)
        return MC_CLIENT_LOOKUP_OK;
    mc_client_texture_lookup_attempt *a = o->lookup->pending;
    o->lookup->active = true;
    ++o->active_resource_calls;
    ++o->classes->active_resource_calls;
    const mc_client_texture_lookup_status status = mc_client_resource_lookup_cleanup(a, false);
    if (status != MC_CLIENT_LOOKUP_OK) {
        o->lookup->last_cleanup = a->cleanup;
        copy_error(error, &a->cleanup);
    } else if (a->cleanup.status != MC_CLIENT_LOOKUP_OK) {
        o->lookup->last_cleanup = a->cleanup;
    } else if (a->failure.cleanup_context.status != MC_CLIENT_GRAPHICS_OK) {
        o->lookup->last_cleanup = a->failure;
    }
    --o->classes->active_resource_calls;
    --o->active_resource_calls;
    o->lookup->active = false;
    if (!mc_client_resource_lookup_remaining(a)) {
        o->lookup->pending = NULL;
        free(a);
    }
    return status;
}

bool mc_client_texture_lookup_free_metadata(mc_client_render_engine *o) {
    if (o == NULL)
        return false;
    if (mc_client_render_engine_retire_lookup(o, NULL) != MC_CLIENT_LOOKUP_OK)
        return false;
    free(o->lookup);
    o->lookup = NULL;
    o->lookup_retired = true;
    return true;
}
