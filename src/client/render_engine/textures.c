#include "textures.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>

enum { RENDER_RETAINED_IDS = 128 };

struct mc_client_render_device {
    mc_client_display *display;
    mc_client_render_texture_options options;
    size_t count;
    mc_client_texture_generation records[RENDER_RETAINED_IDS];
};

static const char image_to_int[] = "(Ljava/awt/image/BufferedImage;)I";
static const char image_and_int[] = "(Ljava/awt/image/BufferedImage;I)V";

static mc_client_render_texture_status fail(mc_client_render_texture_error *e,
                                            mc_client_render_texture_status status,
                                            const char *operation, const char *source,
                                            const char *method, const char *descriptor, unsigned pc,
                                            unsigned caller) {
    e->status = status;
    e->operation = operation;
    e->source_class = source;
    e->source_method = method;
    e->source_descriptor = descriptor;
    e->source_pc = pc;
    e->register_pc = caller;
    return status;
}

static bool device_failed(mc_client_render_texture_error *e, const char *source, const char *method,
                          const char *descriptor, unsigned pc, unsigned caller) {
    const mc_client_render_texture_status status =
        e->display.status == MC_CLIENT_DISPLAY_BUSY ? MC_CLIENT_RENDER_TEXTURE_BUSY
        : e->display.status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL
            ? MC_CLIENT_RENDER_TEXTURE_UNIMPLEMENTED
            : MC_CLIENT_RENDER_TEXTURE_DISPLAY_FAILED;
    fail(e, status, e->display.operation, source, method, descriptor, pc, caller);
    return false;
}

mc_client_render_texture_options mc_client_render_texture_default_options(void) {
    return (mc_client_render_texture_options){128, 4096, 128, 32u * 1024u * 1024u};
}

static bool options_equal(const mc_client_render_texture_options *a,
                          const mc_client_render_texture_options *b) {
    return a->max_retained_ids == b->max_retained_ids &&
           a->max_source_ledger_entries == b->max_source_ledger_entries &&
           a->max_image_entries == b->max_image_entries &&
           a->max_upload_scratch_bytes == b->max_upload_scratch_bytes;
}

static bool prepare_device(mc_client_render_engine *o, mc_client_display *display,
                           const mc_client_render_texture_options *requested,
                           mc_client_render_texture_error *e) {
    mc_client_render_engine_classes *c = o->classes;
    const mc_client_render_texture_options options =
        requested == NULL
            ? (c->device == NULL ? mc_client_render_texture_default_options() : c->device->options)
            : *requested;
    if (options.max_retained_ids > RENDER_RETAINED_IDS ||
        (c->device != NULL && (!options_equal(&options, &c->device->options) ||
                               (c->device->display != NULL && c->device->display != display)))) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_INVALID_ARGUMENT,
             "RenderEngine resource namespace admission", NULL, NULL, NULL, 0, 0);
        return false;
    }
    if (c->device == NULL) {
        c->device = calloc(1, sizeof(*c->device));
        if (c->device == NULL) {
            fail(e, MC_CLIENT_RENDER_TEXTURE_ALLOCATION_FAILED,
                 "RenderEngine native resource bookkeeping", "ge", "a", "(Ljava/nio/IntBuffer;)V",
                 1, 12);
            return false;
        }
        c->device->options = options;
    }
    return true;
}

static bool append_source_id(mc_client_render_engine *o, mc_client_texture id,
                             mc_client_render_texture_error *e) {
    mc_client_render_list *list = o->classes->b;
    const size_t limit = o->classes->device->options.max_source_ledger_entries;
    if (list == NULL) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_NULL_REFERENCE, "ge actual texture List receiver", "ge",
             "a", "(Ljava/nio/IntBuffer;)V", 28, 12);
        return false;
    }
    if (list->count >= limit) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_LIMIT, "ge source texture ledger native bound", "ge", "a",
             "(Ljava/nio/IntBuffer;)V", 28, 12);
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
        if (capacity > SIZE_MAX / sizeof(uint32_t)) {
            fail(e, MC_CLIENT_RENDER_TEXTURE_LIMIT, "ge source texture ledger byte bound", "ge",
                 "a", "(Ljava/nio/IntBuffer;)V", 28, 12);
            return false;
        }
        void *entries = realloc(list->entries, capacity * sizeof(uint32_t));
        if (entries == NULL) {
            fail(e, MC_CLIENT_RENDER_TEXTURE_ALLOCATION_FAILED, "ge source texture ledger growth",
                 "ge", "a", "(Ljava/nio/IntBuffer;)V", 28, 12);
            return false;
        }
        list->entries = entries;
        list->capacity = capacity;
    }
    ((uint32_t *)(void *)list->entries)[list->count++] = id;
    e->source_ledger_appended = true;
    return true;
}

static bool parameter(mc_client_texture_resource *resource, mc_client_texture_parameter which,
                      int32_t value, unsigned pc, mc_client_render_texture_error *e) {
    if (!mc_client_display_texture_parameter(resource, which, value, &e->display))
        return device_failed(e, "ji", "a", image_and_int, pc, 27);
    return true;
}

static bool upload(mc_client_render_engine *o, mc_client_texture_resource *resource,
                   mc_client_argb_image *image, mc_client_render_texture_error *e) {
    if (!mc_client_display_bind_texture(resource, &e->display))
        return device_failed(e, "ji", "a", image_and_int, 4, 27);
    if (o->classes->ji_a) {
        if (!parameter(resource, MC_CLIENT_TEXTURE_MIN_FILTER, 9986, 22, e) ||
            !parameter(resource, MC_CLIENT_TEXTURE_MAG_FILTER, 9728, 34, e))
            return false;
    } else if (!parameter(resource, MC_CLIENT_TEXTURE_MIN_FILTER, 9728, 49, e) ||
               !parameter(resource, MC_CLIENT_TEXTURE_MAG_FILTER, 9728, 61, e))
        return false;
    if (o->fields.k && (!parameter(resource, MC_CLIENT_TEXTURE_MIN_FILTER, 9729, 77, e) ||
                        !parameter(resource, MC_CLIENT_TEXTURE_MAG_FILTER, 9729, 89, e)))
        return false;
    if (o->fields.j) {
        if (!parameter(resource, MC_CLIENT_TEXTURE_WRAP_S, 10496, 111, e) ||
            !parameter(resource, MC_CLIENT_TEXTURE_WRAP_T, 10496, 123, e))
            return false;
    } else if (!parameter(resource, MC_CLIENT_TEXTURE_WRAP_S, 10497, 141, e) ||
               !parameter(resource, MC_CLIENT_TEXTURE_WRAP_T, 10497, 150, e))
        return false;
    if (image == NULL) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_NULL_REFERENCE, "BufferedImage getWidth receiver", "ji",
             "a", image_and_int, 154, 27);
        return false;
    }
    const size_t count = image->count, bytes = count * sizeof(uint32_t);
    const size_t scratch_limit = o->classes->device->options.max_upload_scratch_bytes;
    uint32_t *argb = NULL;
    uint8_t *rgba = NULL;
    bool ok = false;
    if (bytes > scratch_limit) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_LIMIT, "RenderEngine int[] scratch bound", "ji", "a",
             image_and_int, 168, 27);
        goto done;
    }
    argb = malloc(bytes);
    if (argb == NULL) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_ALLOCATION_FAILED, "RenderEngine actual getRGB array",
             "ji", "a", image_and_int, 168, 27);
        goto done;
    }
    if (bytes > scratch_limit - bytes) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_LIMIT, "RenderEngine byte[] scratch bound", "ji", "a",
             image_and_int, 178, 27);
        goto done;
    }
    rgba = malloc(bytes);
    if (rgba == NULL) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_ALLOCATION_FAILED, "RenderEngine actual RGBA array", "ji",
             "a", image_and_int, 178, 27);
        goto done;
    }
    memcpy(argb, image->pixels, bytes); /* Actual TYPE_INT_ARGB getRGB192. */
    for (size_t i = 0; i < count; ++i) {
        const uint32_t value = argb[i];
        unsigned a = (value >> 24) & 255u, r = (value >> 16) & 255u, g = (value >> 8) & 255u,
                 b = value & 255u;
        /* Actual i261 null branch, then its fresh i268/g271 reload. No
         * callback occurs in this pure CPU loop; keep source reads explicit. */
        if (o->fields.i != NULL) {
            mc_game_settings_state settings;
            if (!mc_game_settings_read_state(o->fields.i, &settings)) {
                fail(e, MC_CLIENT_RENDER_TEXTURE_NULL_REFERENCE,
                     "RenderEngine anaglyph settings receiver", "ji", "a", image_and_int, 271, 27);
                goto done;
            }
            if (settings.values.anaglyph) {
                const unsigned nr = (r * 30u + g * 59u + b * 11u) / 100u;
                const unsigned ng = (r * 30u + g * 70u) / 100u;
                const unsigned nb = (r * 30u + b * 70u) / 100u;
                r = nr;
                g = ng;
                b = nb;
            }
        }
        rgba[i * 4] = (uint8_t)r;
        rgba[i * 4 + 1] = (uint8_t)g;
        rgba[i * 4 + 2] = (uint8_t)b;
        rgba[i * 4 + 3] = (uint8_t)a;
    }
    mc_client_direct_byte_buffer *buffer = (mc_client_direct_byte_buffer *)(void *)o->fields.f;
    if (buffer == NULL) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_NULL_REFERENCE, "RenderEngine transfer buffer receiver",
             "ji", "a", image_and_int, 401, 27);
        goto done;
    }
    buffer->position = 0;
    buffer->limit = buffer->capacity; /* clear401 */
    if (bytes > buffer->limit - buffer->position) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_BUFFER_OVERFLOW, "RenderEngine ByteBuffer.put array", "ji",
             "a", image_and_int, 411, 27);
        goto done;
    }
    memcpy(buffer->data + buffer->position, rgba, bytes);
    buffer->position += bytes;
    buffer->position = 0;  /* position420 */
    buffer->limit = bytes; /* limit426 */
    const mc_image_view view = {image->width, image->height, (size_t)image->width * 4,
                                buffer->data};
    if (!mc_client_display_upload_existing_texture(resource, &view, &e->display)) {
        device_failed(e, "ji", "a", image_and_int, 451, 27);
        goto done;
    }
    e->base_uploaded = true;
    if (o->classes->ji_a) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_UNIMPLEMENTED,
             "RenderEngine actual four-level mipmap body", "ji", "a", image_and_int, 460, 27);
        goto done;
    }
    ok = true;
done:
    free(rgba);
    free(argb);
    return ok;
}

static bool publish_image(mc_client_render_engine *o, mc_client_texture id,
                          mc_client_argb_image *image, mc_client_render_texture_error *e) {
    mc_client_render_map *map = (mc_client_render_map *)(void *)o->fields.d;
    if (map == NULL) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_NULL_REFERENCE, "RenderEngine image Map receiver", "ji",
             "a", image_to_int, 39, 39);
        return false;
    }
    size_t at = 0;
    while (at < map->count && map->entries[at].key != id)
        ++at;
    if (at == map->count && map->count >= o->classes->device->options.max_image_entries) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_LIMIT, "RenderEngine image Map native bound", "ji", "a",
             image_to_int, 39, 39);
        return false;
    }
    if (at == map->count && map->count == map->capacity) {
        const size_t limit = o->classes->device->options.max_image_entries;
        size_t capacity = map->capacity == 0 ? 8 : map->capacity;
        if (capacity > SIZE_MAX / 2)
            capacity = limit;
        else if (map->capacity != 0)
            capacity *= 2;
        if (capacity > limit)
            capacity = limit;
        if (capacity > SIZE_MAX / sizeof(mc_client_render_map_entry)) {
            fail(e, MC_CLIENT_RENDER_TEXTURE_LIMIT, "RenderEngine image Map byte bound", "ji", "a",
                 image_to_int, 39, 39);
            return false;
        }
        mc_client_render_map_entry *entries = realloc(map->entries, capacity * sizeof(*entries));
        if (entries == NULL) {
            fail(e, MC_CLIENT_RENDER_TEXTURE_ALLOCATION_FAILED, "RenderEngine image Map growth",
                 "ji", "a", image_to_int, 39, 39);
            return false;
        }
        map->entries = entries;
        map->capacity = capacity;
    }
    e->owner = mc_client_argb_image_retain(image);
    if (e->owner != MC_CLIENT_RENDER_ENGINE_OK) {
        fail(e, MC_CLIENT_RENDER_TEXTURE_LIMIT, "RenderEngine image Map native image hold", "ji",
             "a", image_to_int, 39, 39);
        return false;
    }
    mc_client_argb_image *old = at < map->count ? map->entries[at].image : NULL;
    map->entries[at] = (mc_client_render_map_entry){id, image};
    if (at == map->count)
        ++map->count;
    mc_client_argb_image_release(old);
    e->image_published = true;
    return true;
}

mc_client_render_texture_status
mc_client_render_engine_register_image(mc_client_render_engine *o, mc_client_display *display,
                                       const mc_client_render_texture_options *options,
                                       mc_client_argb_image *image, mc_client_texture *out,
                                       mc_client_render_texture_error *error) {
    mc_client_render_texture_error e = {0};
    bool held = false, image_held = false, entered = false;
    if (o == NULL || display == NULL || out == NULL || (image != NULL && !image->constructed)) {
        fail(&e, MC_CLIENT_RENDER_TEXTURE_INVALID_ARGUMENT,
             "RenderEngine registration native arguments", NULL, NULL, NULL, 0, 0);
        goto done;
    }
    if (o->phase != MC_CLIENT_RENDER_ENGINE_READY || o->classes->resource_retirement_started) {
        fail(&e, MC_CLIENT_RENDER_TEXTURE_INVALID_PHASE,
             "RenderEngine registration READY namespace", NULL, NULL, NULL, 0, 0);
        goto done;
    }
    if (options != NULL &&
        (options->max_retained_ids > RENDER_RETAINED_IDS ||
         (o->classes->device != NULL && !options_equal(options, &o->classes->device->options)))) {
        fail(&e, MC_CLIENT_RENDER_TEXTURE_INVALID_ARGUMENT,
             "RenderEngine resource policy admission", NULL, NULL, NULL, 0, 0);
        goto done;
    }
    if (o->classes->device != NULL && o->classes->device->display != NULL &&
        o->classes->device->display != display) {
        fail(&e, MC_CLIENT_RENDER_TEXTURE_INVALID_ARGUMENT, "RenderEngine actual Display namespace",
             NULL, NULL, NULL, 0, 0);
        goto done;
    }
    if (o->active_resource_calls != 0 || o->classes->active_resource_calls != 0) {
        fail(&e, MC_CLIENT_RENDER_TEXTURE_BUSY, "RenderEngine resource operation active", NULL,
             NULL, NULL, 0, 0);
        goto done;
    }
    e.owner = mc_client_render_engine_retain(o);
    if (e.owner != MC_CLIENT_RENDER_ENGINE_OK) {
        fail(&e, MC_CLIENT_RENDER_TEXTURE_LIMIT, "RenderEngine registration receiver hold", NULL,
             NULL, NULL, 0, 0);
        goto done;
    }
    held = true;
    if (image != NULL) {
        e.owner = mc_client_argb_image_retain(image);
        if (e.owner != MC_CLIENT_RENDER_ENGINE_OK) {
            fail(&e, MC_CLIENT_RENDER_TEXTURE_LIMIT, "RenderEngine registration argument hold",
                 NULL, NULL, NULL, 0, 0);
            goto done;
        }
        image_held = true;
    }
    ++o->active_resource_calls;
    ++o->classes->active_resource_calls;
    entered = true;
    mc_client_direct_int_buffer *buffer = (mc_client_direct_int_buffer *)(void *)o->fields.e;
    if (buffer == NULL) {
        fail(&e, MC_CLIENT_RENDER_TEXTURE_NULL_REFERENCE, "RenderEngine IntBuffer.clear receiver",
             "ji", "a", image_to_int, 4, 4);
        goto done;
    }
    buffer->position = 0;
    buffer->limit = buffer->capacity;
    if (!prepare_device(o, display, options, &e))
        goto done;
    struct mc_client_render_device *device = o->classes->device;
    if (device->count >= device->options.max_retained_ids) {
        fail(&e, MC_CLIENT_RENDER_TEXTURE_LIMIT, "RenderEngine native retained texture bound", "ge",
             "a", "(Ljava/nio/IntBuffer;)V", 1, 12);
        goto done;
    }
    mc_client_texture_generation *generated = &device->records[device->count];
    const bool generated_ok = mc_client_display_generate_texture(
        display, (mc_client_texture *)(void *)buffer->bytes->data, generated, &e.display);
    e.generated_id = generated->buffer_value;
    e.driver_generated = generated->driver_generated;
    if (generated->resource != NULL) {
        ++device->count;
        ++o->classes->native_resource_count;
        device->display = display;
    }
    if (!generated_ok) {
        device_failed(&e, "ge", "a", "(Ljava/nio/IntBuffer;)V", 1, 12);
        goto done;
    }
    if (!append_source_id(o, generated->buffer_value, &e))
        goto done;
    if (!upload(o, generated->resource, image, &e))
        goto done;
    if (!publish_image(o, generated->buffer_value, image, &e))
        goto done;
    *out = generated->buffer_value;
done:
    if (entered) {
        --o->classes->active_resource_calls;
        --o->active_resource_calls;
    }
    if (image_held)
        mc_client_argb_image_release(image);
    if (held)
        (void)mc_client_render_engine_release(o);
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_render_texture_status
mc_client_render_engine_retire_resources(mc_client_render_engine_classes *classes,
                                         mc_client_render_texture_error *error) {
    mc_client_render_texture_error e = {0};
    if (classes == NULL)
        fail(&e, MC_CLIENT_RENDER_TEXTURE_INVALID_ARGUMENT,
             "RenderEngine resource retirement receiver", NULL, NULL, NULL, 0, 0);
    else if (classes->active_resource_calls != 0)
        fail(&e, MC_CLIENT_RENDER_TEXTURE_BUSY, "RenderEngine resource operation active", NULL,
             NULL, NULL, 0, 0);
    else {
        classes->resource_retirement_started = true;
        struct mc_client_render_device *device = classes->device;
        if (device != NULL) {
            for (size_t i = 0; i < device->count; ++i) {
                if (device->records[i].resource == NULL)
                    continue;
                mc_client_display_error cause = {0};
                const bool ok =
                    mc_client_display_retire_texture(&device->records[i].resource, &cause);
                if (device->records[i].resource == NULL)
                    --classes->native_resource_count;
                if (!ok && e.status == MC_CLIENT_RENDER_TEXTURE_OK) {
                    e.display = cause;
                    device_failed(&e, NULL, NULL, NULL, 0, 0);
                }
            }
            if (classes->native_resource_count == 0)
                device->display = NULL;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_render_engine_registered_image(const mc_client_render_engine *o,
                                              mc_client_texture id, mc_client_argb_image **out) {
    if (o == NULL || out == NULL || o->fields.d == NULL)
        return false;
    const mc_client_render_map *map = o->fields.d;
    for (size_t i = 0; i < map->count; ++i)
        if (map->entries[i].key == id) {
            *out = map->entries[i].image;
            return true;
        }
    *out = NULL;
    return true;
}
