#include "color_lookup.h"
#include "resource_lookup_internal.h"
#include "string_key_internal.h"
#include <stdlib.h>
#include <string.h>

static mc_client_graphics_site site(int32_t pc) {
    return (mc_client_graphics_site){"ji", "a", "(Ljava/lang/String;)[I", pc};
}

static mc_client_texture_lookup_status fail(mc_client_color_lookup_error *e,
                                            mc_client_texture_lookup_status status,
                                            const char *operation, int32_t pc, bool native) {
    e->status = status;
    e->operation = operation;
    e->site = site(pc);
    e->native_policy = native;
    return status;
}

static void copy_error(mc_client_color_lookup_error *out, const mc_client_color_lookup_error *e) {
    if (out != NULL)
        *out = *e;
}

mc_client_color_lookup_options mc_client_color_lookup_default_options(void) {
    return (mc_client_color_lookup_options){4096, 65536, 1024u * 1024u,
                                            mc_client_color_array_default_limits()};
}

static bool options_equal(const mc_client_color_lookup_options *a,
                          const mc_client_color_lookup_options *b) {
    return a->max_cache_entries == b->max_cache_entries && a->max_key_units == b->max_key_units &&
           a->max_prefix_bytes == b->max_prefix_bytes &&
           a->arrays.max_native_bytes == b->arrays.max_native_bytes &&
           a->arrays.max_allocations == b->arrays.max_allocations;
}

static size_t cache_find(const mc_client_render_map *map, mc_texture_pack_text key) {
    size_t at = 0;
    while (at < map->count &&
           !mc_client_resource_key_equal(map->color_arrays[at].units, map->color_arrays[at].length,
                                         map->color_arrays[at].is_null, key))
        ++at;
    return at;
}

bool mc_client_render_engine_cached_colors(const mc_client_render_engine *o,
                                           mc_texture_pack_text key, bool *found,
                                           mc_client_color_array **out) {
    if (o == NULL || found == NULL || out == NULL || !mc_client_resource_key_shape(key) ||
        o->fields.c == NULL || (o->fields.c->count != 0 && o->fields.c->color_arrays == NULL))
        return false;
    const size_t at = cache_find(o->fields.c, key);
    *found = at < o->fields.c->count && o->fields.c->color_arrays[at].array != NULL;
    if (*found)
        *out = o->fields.c->color_arrays[at].array;
    return true;
}

static mc_client_texture_lookup_status array_failure(mc_client_texture_lookup_attempt *a,
                                                     int32_t caller, const char *operation) {
    mc_client_color_lookup_error *e = &a->failure;
    e->frames[0] = site(caller);
    e->frames[1] = e->array.site;
    e->frame_count = 2;
    if (e->array.status == MC_CLIENT_COLOR_ARRAY_NULL_REFERENCE)
        e->dependency =
            (mc_client_graphics_site){"java/awt/image/BufferedImage", "getWidth", "()I", -1};
    const mc_client_texture_lookup_status status =
        e->array.status == MC_CLIENT_COLOR_ARRAY_NULL_REFERENCE ? MC_CLIENT_LOOKUP_NULL_REFERENCE
        : e->array.status == MC_CLIENT_COLOR_ARRAY_NEGATIVE_SIZE
            ? MC_CLIENT_LOOKUP_SOURCE_NEGATIVE_SIZE
        : e->array.status == MC_CLIENT_COLOR_ARRAY_LIMIT ? MC_CLIENT_LOOKUP_LIMIT
        : e->array.status == MC_CLIENT_COLOR_ARRAY_ALLOCATION_FAILED
            ? MC_CLIENT_LOOKUP_ALLOCATION_FAILED
        : e->array.status == MC_CLIENT_COLOR_ARRAY_BUSY          ? MC_CLIENT_LOOKUP_BUSY
        : e->array.status == MC_CLIENT_COLOR_ARRAY_UNIMPLEMENTED ? MC_CLIENT_LOOKUP_UNIMPLEMENTED
                                                                 : MC_CLIENT_LOOKUP_ARRAY_FAILED;
    return fail(e, status, operation, caller, e->array.native_policy);
}

static mc_client_texture_lookup_status cache_put(mc_client_render_engine *o,
                                                 mc_client_texture_lookup_attempt *a,
                                                 mc_texture_pack_text key) {
    mc_client_color_lookup_error *e = &a->failure;
    mc_client_render_map *map = (mc_client_render_map *)(void *)o->fields.c;
    if (map == NULL)
        return fail(e, MC_CLIENT_LOOKUP_NULL_REFERENCE, "actual c HashMap.put receiver", 185,
                    false);
    if (map->count != 0 && map->color_arrays == NULL)
        return fail(e, MC_CLIENT_LOOKUP_INVALID_PHASE, "actual c native array-map role", 185, true);
    const size_t at = cache_find(map, key);
    const mc_client_color_lookup_options *options = &o->lookup->color_options;
    if (at == map->count &&
        (map->count >= options->max_cache_entries || key.length > options->max_key_units ||
         key.length > SIZE_MAX / sizeof(uint16_t)))
        return fail(e, MC_CLIENT_LOOKUP_LIMIT, "actual c native key/entry bound", 185, true);
    if (at == map->count && map->count == map->capacity) {
        const size_t limit = options->max_cache_entries;
        size_t capacity = map->capacity == 0 ? 8 : map->capacity;
        if (map->capacity != 0)
            capacity = capacity > SIZE_MAX / 2 ? limit : capacity * 2;
        if (capacity > limit)
            capacity = limit;
        if (capacity > SIZE_MAX / sizeof(mc_client_render_color_array_entry))
            return fail(e, MC_CLIENT_LOOKUP_LIMIT, "actual c native table byte bound", 185, true);
        mc_client_render_color_array_entry *entries =
            realloc(map->color_arrays, capacity * sizeof(*entries));
        if (entries == NULL)
            return fail(e, MC_CLIENT_LOOKUP_ALLOCATION_FAILED, "actual c native table growth", 185,
                        true);
        map->color_arrays = entries;
        map->capacity = capacity;
    }
    mc_client_color_array *previous = at < map->count ? map->color_arrays[at].array : NULL;
    if (!mc_client_color_array_can_release(previous))
        return fail(e, MC_CLIENT_LOOKUP_BUSY, "actual c displaced final array native active", 185,
                    true);
    if (mc_client_color_array_retain(a->array) != MC_CLIENT_COLOR_ARRAY_OK) {
        e->array = (mc_client_color_array_error){.status = MC_CLIENT_COLOR_ARRAY_LIMIT,
                                                 .operation = "actual c new array native hold",
                                                 .site = {"ji", "a", "(Ljava/lang/String;)[I", -1},
                                                 .native_policy = true};
        return fail(e, MC_CLIENT_LOOKUP_LIMIT, "actual c new array native hold", 185, true);
    }
    if (at < map->count) {
        map->color_arrays[at].array = a->array;
    } else {
        uint16_t *units = NULL;
        const mc_client_resource_key_status copied =
            mc_client_resource_key_copy(key, options->max_key_units, &units);
        if (copied != MC_CLIENT_RESOURCE_KEY_OK) {
            mc_client_color_array *reserved = a->array;
            (void)mc_client_color_array_release(&reserved,
                                                NULL); /* Unpublished nonfinal Map hold. */
            return fail(e,
                        copied == MC_CLIENT_RESOURCE_KEY_LIMIT ? MC_CLIENT_LOOKUP_LIMIT
                                                               : MC_CLIENT_LOOKUP_ALLOCATION_FAILED,
                        "actual c immutable key copy", 185, true);
        }
        map->color_arrays[map->count++] =
            (mc_client_render_color_array_entry){units, key.length, key.is_null, a->array};
    }
    (void)mc_client_color_array_release(&previous,
                                        NULL); /* Gain-before-store, exclusive preflight. */
    a->state.cache_published = true;
    mc_client_resource_lookup_touch(a, 185);
    return MC_CLIENT_LOOKUP_OK;
}

static mc_client_texture_lookup_status run_body(mc_client_render_engine *o,
                                                mc_texture_pack_text key,
                                                mc_client_texture_lookup_attempt *a) {
    mc_client_color_lookup_error *e = &a->failure;
    if (mc_client_resource_lookup_capture(o, a) != MC_CLIENT_LOOKUP_OK)
        return e->status;
    if (o->fields.c == NULL)
        return fail(e, MC_CLIENT_LOOKUP_NULL_REFERENCE, "actual c HashMap.get receiver", 13, false);
    bool found;
    mc_client_color_array *cached = NULL;
    if (!mc_client_render_engine_cached_colors(o, key, &found, &cached))
        return fail(e, MC_CLIENT_LOOKUP_INVALID_PHASE, "actual c native array-map role", 13, true);
    mc_client_resource_lookup_touch(a, 13);
    if (found) {
        mc_client_color_array_state state;
        if (!mc_client_color_array_inspect(cached, &state) ||
            state.phase != MC_CLIENT_COLOR_ARRAY_READY)
            return fail(e, MC_CLIENT_LOOKUP_INVALID_PHASE, "cached array native phase", -1, true);
        if (mc_client_color_array_retain(cached) != MC_CLIENT_COLOR_ARRAY_OK) {
            e->array =
                (mc_client_color_array_error){.status = MC_CLIENT_COLOR_ARRAY_LIMIT,
                                              .operation = "cached return array native hold",
                                              .site = {"ji", "a", "(Ljava/lang/String;)[I", -1},
                                              .native_policy = true};
            return fail(e, MC_CLIENT_LOOKUP_LIMIT, "cached return array native hold", -1, true);
        }
        a->array = cached;
        a->state.cache_hit = a->state.source_returned = true;
        mc_client_resource_lookup_touch(a, 25);
        return MC_CLIENT_LOOKUP_OK;
    }
    if (key.is_null)
        return fail(e, MC_CLIENT_LOOKUP_NULL_REFERENCE, "actual null String.startsWith receiver",
                    31, false);
    mc_client_resource_lookup_touch(a, 31);

    enum { NORMAL, STRIPS, CLAMP, BLUR } mode = NORMAL;

    size_t skip = 0;
    int32_t open_pc = 146, read_pc = 172, convert_pc = 175;
    if (mc_client_resource_key_starts(key, "##", 2)) {
        mode = STRIPS;
        skip = 2;
        open_pc = 46;
        read_pc = 49;
        convert_pc = 55;
    } else if (mc_client_resource_key_starts(key, "%clamp%", 7)) {
        mode = CLAMP;
        skip = 7;
        open_pc = 85;
        read_pc = 88;
        convert_pc = 91;
        o->fields.j = true;
        mc_client_resource_lookup_touch(a, 73);
    } else if (mc_client_resource_key_starts(key, "%blur%", 6)) {
        mode = BLUR;
        skip = 6;
        open_pc = 126;
        read_pc = 129;
        convert_pc = 132;
        o->fields.k = true;
        mc_client_resource_lookup_touch(a, 114);
    }
    const mc_texture_pack_text name = {skip == 0 ? key.units : key.units + skip, key.length - skip,
                                       false};
    if (mc_client_resource_lookup_open(a, name, open_pc) != MC_CLIENT_LOOKUP_OK)
        return e->status;
    mc_client_argb_image *image;
    if (mode == NORMAL && a->stream == NULL) {
        image = o->fields.m;
        convert_pc = 161;
    } else {
        if (mc_client_resource_lookup_read(a, read_pc) != MC_CLIENT_LOOKUP_OK)
            return e->status;
        if (mode == STRIPS) {
            e->dependency = (mc_client_graphics_site){
                "ji", "c", "(Ljava/awt/image/BufferedImage;)Ljava/awt/image/BufferedImage;", 0};
            return fail(e, MC_CLIENT_LOOKUP_UNIMPLEMENTED,
                        "actual ## Graphics strip transform body", 52, false);
        }
        image = a->image;
    }
    if (mc_client_color_array_from_image(image, &o->lookup->color_options.arrays, &a->array,
                                         &e->array) != MC_CLIENT_COLOR_ARRAY_OK)
        return array_failure(a, convert_pc, "actual new int[] getRGB conversion");
    mc_client_resource_lookup_touch(a, convert_pc);
    if (mode == CLAMP) {
        o->fields.j = false;
        mc_client_resource_lookup_touch(a, 97);
    }
    if (mode == BLUR) {
        o->fields.k = false;
        mc_client_resource_lookup_touch(a, 138);
    }
    if (cache_put(o, a, key) != MC_CLIENT_LOOKUP_OK)
        return e->status;
    a->state.source_returned = true;
    mc_client_resource_lookup_touch(a, 190);
    return MC_CLIENT_LOOKUP_OK;
}

mc_client_texture_lookup_status mc_client_render_engine_colors(
    mc_client_render_engine *o, const mc_client_color_lookup_options *requested,
    mc_texture_pack_text key, mc_client_color_array **out, mc_client_color_lookup_error *error) {
    mc_client_color_lookup_error admission = {0};
    if (o == NULL || out == NULL || !mc_client_resource_key_shape(key)) {
        fail(&admission, MC_CLIENT_LOOKUP_INVALID_ARGUMENT,
             "color lookup native receiver/key/output", -1, true);
        copy_error(error, &admission);
        return admission.status;
    }
    if (o->phase != MC_CLIENT_RENDER_ENGINE_READY || o->classes->resource_retirement_started ||
        o->lookup_retired) {
        fail(&admission, MC_CLIENT_LOOKUP_INVALID_PHASE, "color lookup native READY namespace", -1,
             true);
        copy_error(error, &admission);
        return admission.status;
    }
    if (o->active_resource_calls != 0 || o->classes->active_resource_calls != 0 ||
        o->classes->nw.phase == MC_CLIENT_RENDER_ENGINE_CONSTRUCTING ||
        o->classes->active_source_calls != 0 ||
        (o->lookup != NULL && (o->lookup->active || o->lookup->pending != NULL))) {
        fail(&admission, MC_CLIENT_LOOKUP_BUSY, "color lookup native active/pending prefix", -1,
             true);
        copy_error(error, &admission);
        return admission.status;
    }
    const mc_client_color_lookup_options options =
        requested == NULL ? (o->lookup != NULL && o->lookup->color_policy_set
                                 ? o->lookup->color_options
                                 : mc_client_color_lookup_default_options())
                          : *requested;
    if (o->lookup != NULL && o->lookup->color_policy_set &&
        !options_equal(&options, &o->lookup->color_options)) {
        fail(&admission, MC_CLIENT_LOOKUP_INVALID_ARGUMENT, "color lookup fixed native policy", -1,
             true);
        copy_error(error, &admission);
        return admission.status;
    }
    if (options.max_prefix_bytes <
        sizeof(mc_client_texture_lookup_attempt) + sizeof(struct mc_client_texture_lookup_native)) {
        fail(&admission, MC_CLIENT_LOOKUP_LIMIT, "color lookup native prefix byte bound", -1, true);
        copy_error(error, &admission);
        return admission.status;
    }
    if (o->lookup == NULL) {
        o->lookup = calloc(1, sizeof(*o->lookup));
        if (o->lookup == NULL) {
            fail(&admission, MC_CLIENT_LOOKUP_ALLOCATION_FAILED, "color lookup native policy owner",
                 -1, true);
            copy_error(error, &admission);
            return admission.status;
        }
    }
    if (!o->lookup->color_policy_set) {
        o->lookup->color_options = options;
        o->lookup->color_policy_set = true;
    }
    mc_client_texture_lookup_attempt *a = calloc(1, sizeof(*a));
    if (a == NULL) {
        fail(&admission, MC_CLIENT_LOOKUP_ALLOCATION_FAILED, "color lookup native bounded attempt",
             -1, true);
        copy_error(error, &admission);
        return admission.status;
    }
    a->method = site(0);
    a->state.color_lookup = true;
    mc_client_texture_lookup_status status = mc_client_resource_lookup_enter(o, a);
    if (status != MC_CLIENT_LOOKUP_OK) {
        copy_error(error, &a->failure);
        free(a);
        return status;
    }
    status = run_body(o, key, a);
    status = mc_client_resource_lookup_finish(o, a, status);
    copy_error(error, &a->failure);
    if (status == MC_CLIENT_LOOKUP_OK) {
        *out = a->array;
        a->array = NULL; /* Transfer the already-held native result occurrence. */
        o->lookup->pending = NULL;
        free(a);
    }
    (void)mc_client_render_engine_release(o); /* Balanced nonfinal call occurrence. */
    return status;
}
