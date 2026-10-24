#include "assets.h"

#include <stdlib.h>
#include <string.h>

typedef union asset_allocation asset_allocation;

union asset_allocation {
    struct {
        asset_allocation *next;
        size_t bytes;
    } record;

    long double scalar_alignment;
    void *pointer_alignment;
    uint64_t extent_alignment[4];
};

_Static_assert(sizeof(asset_allocation) % 16u == 0, "preserve allocator alignment");

typedef struct asset_arena {
    asset_allocation *head;
    size_t bytes;
    mc_client_assets_status failure;
} asset_arena;

typedef struct image_entry {
    struct image_entry *next;
    const char *name;
    mc_image *image;
} image_entry;

typedef struct label_entry {
    struct label_entry *next;
    const char *key;
    mc_font_text text;
} label_entry;

struct mc_client_assets {
    const mc_registry_resources *resources;
    mc_registry_classes *classes;
    asset_arena arena;
    image_entry *images;
    label_entry *labels;
    size_t image_count, image_bytes, label_count;
    bool font_loaded;
    mc_font_text font;
};

typedef struct asset_stream {
    const mc_registry_resources *source;
    mc_java_properties_input input;
    size_t bytes;
    mc_java_properties_status read_failure;
    mc_registry_status close_status;
    bool opened, closed;
} asset_stream;

typedef struct text_buffer {
    uint16_t *units;
    size_t length, capacity;
} text_buffer;

typedef struct splash_line {
    struct splash_line *next;
    mc_font_text text;
} splash_line;

static mc_client_assets_status fail(mc_client_assets_error *error, mc_client_assets_status status,
                                    const char *operation) {
    error->status = status;
    error->operation = operation;
    return status;
}

static mc_client_assets_status finish(mc_client_assets_status status,
                                      const mc_client_assets_error *detail,
                                      mc_client_assets_error *error) {
    if (error != NULL)
        *error = status == MC_CLIENT_ASSETS_OK ? (mc_client_assets_error){0} : *detail;
    return status;
}

static void *arena_allocate(void *context, size_t size) {
    asset_arena *arena = context;
    const size_t payload = size == 0 ? 1u : size;
    if (payload > SIZE_MAX - sizeof(asset_allocation) ||
        payload + sizeof(asset_allocation) > MC_CLIENT_ASSETS_MAX_ARENA_BYTES - arena->bytes) {
        arena->failure = MC_CLIENT_ASSETS_LIMIT;
        return NULL;
    }
    asset_allocation *allocation = malloc(sizeof(*allocation) + payload);
    if (allocation == NULL) {
        arena->failure = MC_CLIENT_ASSETS_ALLOCATION_FAILED;
        return NULL;
    }
    allocation->record.next = arena->head;
    allocation->record.bytes = sizeof(*allocation) + payload;
    arena->head = allocation;
    arena->bytes += allocation->record.bytes;
    return allocation + 1;
}

static void arena_free(asset_arena *arena) {
    while (arena->head != NULL) {
        asset_allocation *next = arena->head->record.next;
        free(arena->head);
        arena->head = next;
    }
    *arena = (asset_arena){0};
}

static mc_client_assets_status ascii_name(const char *text, size_t *length) {
    if (text == NULL)
        return MC_CLIENT_ASSETS_INVALID_ARGUMENT;
    for (size_t i = 0; i <= MC_CLIENT_ASSETS_MAX_NAME_BYTES; ++i) {
        const unsigned char unit = (unsigned char)text[i];
        if (unit == 0) {
            *length = i;
            return MC_CLIENT_ASSETS_OK;
        }
        if (unit > 127u)
            return MC_CLIENT_ASSETS_INVALID_ARGUMENT;
    }
    return MC_CLIENT_ASSETS_LIMIT;
}

static mc_client_assets_status
resource_failure(mc_registry_status status, mc_client_assets_error *error, const char *operation) {
    error->resource_status = status;
    return fail(error,
                status == MC_REGISTRY_LIMIT               ? MC_CLIENT_ASSETS_LIMIT
                : status == MC_REGISTRY_ALLOCATION_FAILED ? MC_CLIENT_ASSETS_ALLOCATION_FAILED
                                                          : MC_CLIENT_ASSETS_RESOURCE_FAILED,
                operation);
}

static mc_client_assets_status stream_open(mc_client_assets *assets, const char *name,
                                           asset_stream *stream, mc_client_assets_error *error) {
    *stream = (asset_stream){.source = assets->resources};
    /* Reached native stream ownership requires a real closer before opening. */
    if (stream->source->open == NULL || stream->source->close == NULL)
        return resource_failure(MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY, error, "resource callbacks");
    const mc_registry_status status =
        stream->source->open(stream->source->context, name, &stream->input);
    if (status != MC_REGISTRY_OK)
        return resource_failure(status, error, "resource open");
    stream->opened = true;
    if (stream->input.read == NULL)
        return resource_failure(MC_REGISTRY_INVALID_ARGUMENT, error, "resource read callback");
    return MC_CLIENT_ASSETS_OK;
}

static mc_java_properties_status stream_read(void *context, uint8_t *destination, size_t capacity,
                                             size_t *count) {
    asset_stream *stream = context;
    if (destination == NULL || count == NULL || capacity == 0 || !stream->opened ||
        stream->closed || stream->input.read == NULL)
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    const size_t remaining = MC_CLIENT_ASSETS_MAX_ENCODED_BYTES - stream->bytes;
    const size_t request = capacity > remaining ? remaining + 1u : capacity;
    size_t actual = 0;
    mc_java_properties_status status =
        stream->input.read(stream->input.context, destination, request, &actual);
    if (actual > request || (status != MC_JAVA_PROPERTIES_OK && actual != 0))
        status = MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    if (status == MC_JAVA_PROPERTIES_OK && actual > remaining)
        status = MC_JAVA_PROPERTIES_LIMIT;
    if (status != MC_JAVA_PROPERTIES_OK) {
        stream->read_failure = status;
        *count = 0;
        return status;
    }
    stream->bytes += actual;
    *count = actual;
    return MC_JAVA_PROPERTIES_OK;
}

static bool stream_ready(void *context) {
    asset_stream *stream = context;
    return stream->source->ready != NULL &&
           stream->source->ready(stream->source->context, &stream->input);
}

static mc_java_reader_status stream_close(void *context) {
    asset_stream *stream = context;
    if (!stream->opened || stream->closed)
        return MC_JAVA_READER_OK;
    stream->closed = true;
    stream->close_status = stream->source->close == NULL
                               ? MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY
                               : stream->source->close(stream->source->context, &stream->input);
    switch (stream->close_status) {
        case MC_REGISTRY_OK:
            return MC_JAVA_READER_OK;
        case MC_REGISTRY_IO_FAILED:
            return MC_JAVA_READER_IO_ERROR;
        case MC_REGISTRY_LIMIT:
            return MC_JAVA_READER_LIMIT;
        case MC_REGISTRY_ALLOCATION_FAILED:
            return MC_JAVA_READER_ALLOCATION_FAILED;
        case MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY:
            return MC_JAVA_READER_UNIMPLEMENTED_CLOSE;
        default:
            return MC_JAVA_READER_INVALID_ARGUMENT;
    }
}

static mc_client_assets_status reader_failure(mc_java_reader_status status,
                                              const asset_stream *stream, const asset_arena *arena,
                                              mc_client_assets_error *error,
                                              const char *operation) {
    error->reader_status = status;
    error->read_status = stream->read_failure;
    mc_client_assets_status result = MC_CLIENT_ASSETS_READER_FAILED;
    if (status == MC_JAVA_READER_LIMIT || stream->read_failure == MC_JAVA_PROPERTIES_LIMIT ||
        arena->failure == MC_CLIENT_ASSETS_LIMIT)
        result = MC_CLIENT_ASSETS_LIMIT;
    else if (status == MC_JAVA_READER_ALLOCATION_FAILED ||
             stream->read_failure == MC_JAVA_PROPERTIES_ALLOCATION_FAILED ||
             arena->failure == MC_CLIENT_ASSETS_ALLOCATION_FAILED)
        result = MC_CLIENT_ASSETS_ALLOCATION_FAILED;
    return fail(error, result, operation);
}

static mc_client_assets_status close_after(asset_stream *stream, mc_java_reader *reader,
                                           asset_arena *arena, bool font_catches_io,
                                           mc_client_assets_status status,
                                           mc_client_assets_error *error) {
    if (!stream->opened)
        return status;
    const mc_java_reader_status closed =
        reader == NULL ? stream_close(stream) : mc_java_reader_close(reader);
    error->close_status = stream->close_status;
    if (status != MC_CLIENT_ASSETS_OK || closed == MC_JAVA_READER_OK ||
        (font_catches_io && closed == MC_JAVA_READER_IO_ERROR))
        return status;
    return reader_failure(closed, stream, arena, error, "resource close");
}

static bool caught_open_io(const mc_client_assets_error *error) {
    return error->status == MC_CLIENT_ASSETS_RESOURCE_FAILED &&
           (error->resource_status == MC_REGISTRY_NULL_REFERENCE ||
            error->resource_status == MC_REGISTRY_IO_FAILED);
}

static mc_java_reader_status reader_create(asset_stream *stream, asset_arena *arena,
                                           mc_java_reader **out) {
    const mc_java_properties_allocator allocator = {.context = arena, .allocate = arena_allocate};
    const mc_java_reader_input input = {.bytes = {.context = stream, .read = stream_read},
                                        .ready = stream_ready,
                                        .close = stream_close};
    return mc_java_reader_create(&allocator, &input, MC_JAVA_READER_UTF8, out);
}

static mc_client_assets_status append_line(asset_arena *arena, text_buffer *text,
                                           const mc_java_reader_line *line,
                                           mc_client_assets_error *error) {
    if (line->length >= MC_CLIENT_ASSETS_MAX_TEXT_UNITS - text->length)
        return fail(error, MC_CLIENT_ASSETS_LIMIT, "font text units");
    const size_t required = text->length + line->length + 1u;
    if (required > text->capacity) {
        size_t capacity = text->capacity == 0 ? 256u : text->capacity;
        while (capacity < required)
            capacity *= 2u;
        uint16_t *units = arena_allocate(arena, capacity * sizeof(*units));
        if (units == NULL)
            return fail(error, arena->failure, "font text allocation");
        if (text->length != 0)
            memcpy(units, text->units, text->length * sizeof(*units));
        text->units = units;
        text->capacity = capacity;
    }
    if (line->length != 0)
        memcpy(text->units + text->length, line->units, line->length * sizeof(*text->units));
    text->units[required - 1u] = '\n';
    text->length = required;
    return MC_CLIENT_ASSETS_OK;
}

static mc_client_assets_status copy_owned(const mc_font_text *text, mc_client_string *out,
                                          mc_client_assets_error *error) {
    if (text->length > MC_CLIENT_ASSETS_MAX_TEXT_UNITS)
        return fail(error, MC_CLIENT_ASSETS_LIMIT, "owned text units");
    uint16_t *units = NULL;
    if (text->length != 0) {
        units = malloc(text->length * sizeof(*units));
        if (units == NULL)
            return fail(error, MC_CLIENT_ASSETS_ALLOCATION_FAILED, "owned text allocation");
        memcpy(units, text->units, text->length * sizeof(*units));
    }
    *out = (mc_client_string){.units = units, .length = text->length};
    return MC_CLIENT_ASSETS_OK;
}

const char *mc_client_assets_status_string(mc_client_assets_status status) {
    switch (status) {
        case MC_CLIENT_ASSETS_OK:
            return "ok";
        case MC_CLIENT_ASSETS_INVALID_ARGUMENT:
            return "invalid client asset argument";
        case MC_CLIENT_ASSETS_LIMIT:
            return "client asset limit";
        case MC_CLIENT_ASSETS_ALLOCATION_FAILED:
            return "client asset allocation failed";
        case MC_CLIENT_ASSETS_RESOURCE_FAILED:
            return "client resource failed";
        case MC_CLIENT_ASSETS_IMAGE_FAILED:
            return "client PNG decode failed";
        case MC_CLIENT_ASSETS_UNSUPPORTED_IMAGE_FORMAT:
            return "client ImageIO format support unfinished";
        case MC_CLIENT_ASSETS_TRANSLATION_FAILED:
            return "client translation failed";
        case MC_CLIENT_ASSETS_READER_FAILED:
            return "client text reader failed";
        case MC_CLIENT_ASSETS_RANDOM_FAILED:
            return "client splash Random failed";
    }
    return "unknown client asset status";
}

mc_client_assets_status mc_client_assets_create(const mc_registry_resources *resources,
                                                mc_registry_classes *classes,
                                                mc_client_assets **out) {
    if (resources == NULL || classes == NULL || out == NULL)
        return MC_CLIENT_ASSETS_INVALID_ARGUMENT;
    mc_client_assets *assets = calloc(1, sizeof(*assets));
    if (assets == NULL)
        return MC_CLIENT_ASSETS_ALLOCATION_FAILED;
    assets->resources = resources;
    assets->classes = classes;
    *out = assets;
    return MC_CLIENT_ASSETS_OK;
}

void mc_client_assets_free(mc_client_assets *assets) {
    if (assets == NULL)
        return;
    for (image_entry *entry = assets->images; entry != NULL; entry = entry->next)
        mc_image_free(entry->image);
    arena_free(&assets->arena);
    free(assets);
}

mc_client_assets_status mc_client_assets_image(mc_client_assets *assets, const char *name,
                                               const mc_image **out,
                                               mc_client_assets_error *error) {
    mc_client_assets_error detail = {0};
    size_t length = 0;
    if (assets == NULL || out == NULL)
        return finish(fail(&detail, MC_CLIENT_ASSETS_INVALID_ARGUMENT, "image arguments"), &detail,
                      error);
    const mc_client_assets_status name_status = ascii_name(name, &length);
    if (name_status != MC_CLIENT_ASSETS_OK)
        return finish(fail(&detail, name_status, "image resource name"), &detail, error);
    if (length < 2u || name[0] != '/')
        return finish(fail(&detail, MC_CLIENT_ASSETS_INVALID_ARGUMENT, "image arguments"), &detail,
                      error);
    for (image_entry *entry = assets->images; entry != NULL; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            *out = entry->image;
            return finish(MC_CLIENT_ASSETS_OK, &detail, error);
        }
    }
    if (assets->image_count == MC_CLIENT_ASSETS_MAX_IMAGES)
        return finish(fail(&detail, MC_CLIENT_ASSETS_LIMIT, "image cache entries"), &detail, error);
    asset_stream stream;
    mc_client_assets_status status = stream_open(assets, name, &stream, &detail);
    uint8_t *encoded = NULL;
    size_t used = 0, capacity = 0;
    mc_image *image = NULL;
    while (status == MC_CLIENT_ASSETS_OK) {
        uint8_t buffer[8192];
        size_t count = 0;
        const mc_java_properties_status read = stream_read(&stream, buffer, sizeof(buffer), &count);
        if (read != MC_JAVA_PROPERTIES_OK) {
            detail.read_status = read;
            status = fail(&detail,
                          read == MC_JAVA_PROPERTIES_LIMIT ? MC_CLIENT_ASSETS_LIMIT
                          : read == MC_JAVA_PROPERTIES_ALLOCATION_FAILED
                              ? MC_CLIENT_ASSETS_ALLOCATION_FAILED
                              : MC_CLIENT_ASSETS_RESOURCE_FAILED,
                          "image resource read");
            break;
        }
        if (count == 0)
            break;
        const size_t required = used + count;
        if (required > capacity) {
            size_t next = capacity == 0 ? sizeof(buffer) : capacity;
            while (next < required)
                next *= 2u;
            uint8_t *replacement = realloc(encoded, next);
            if (replacement == NULL) {
                status =
                    fail(&detail, MC_CLIENT_ASSETS_ALLOCATION_FAILED, "encoded image allocation");
                break;
            }
            encoded = replacement;
            capacity = next;
        }
        memcpy(encoded + used, buffer, count);
        used = required;
    }
    if (status == MC_CLIENT_ASSETS_OK) {
        detail.image_status = mc_image_decode_png(encoded, used, NULL, &image);
        if (detail.image_status != MC_IMAGE_OK)
            status = fail(&detail,
                          detail.image_status == MC_IMAGE_LIMIT ? MC_CLIENT_ASSETS_LIMIT
                          : detail.image_status == MC_IMAGE_ALLOCATION_FAILED
                              ? MC_CLIENT_ASSETS_ALLOCATION_FAILED
                              : MC_CLIENT_ASSETS_IMAGE_FAILED,
                          "image decode");
    }
    if (status == MC_CLIENT_ASSETS_OK) {
        /* Successful generic decode has validated the complete PNG framing. */
        detail.png_bit_depth = encoded[24];
        detail.png_color_type = encoded[25];
        if (encoded[24] != 8u || (encoded[25] != 2u && encoded[25] != 3u && encoded[25] != 6u))
            status = fail(&detail, MC_CLIENT_ASSETS_UNSUPPORTED_IMAGE_FORMAT, "image format");
    }
    asset_arena unused = {0};
    status = close_after(&stream, NULL, &unused, false, status, &detail);
    free(encoded);
    if (status == MC_CLIENT_ASSETS_OK) {
        mc_image_view view;
        if (!mc_image_view_get(image, &view) || view.height == 0 ||
            view.stride > SIZE_MAX / view.height)
            status = fail(&detail, MC_CLIENT_ASSETS_IMAGE_FAILED, "decoded image view");
        else if (view.stride * view.height > MC_CLIENT_ASSETS_MAX_IMAGE_BYTES - assets->image_bytes)
            status = fail(&detail, MC_CLIENT_ASSETS_LIMIT, "cached image bytes");
        else {
            image_entry *entry = arena_allocate(&assets->arena, sizeof(*entry) + length + 1u);
            if (entry == NULL)
                status = fail(&detail, assets->arena.failure, "image cache allocation");
            else {
                char *copy = (char *)(entry + 1);
                memcpy(copy, name, length + 1u);
                *entry = (image_entry){.next = assets->images, .name = copy, .image = image};
                assets->images = entry;
                ++assets->image_count;
                assets->image_bytes += view.stride * view.height;
                *out = image;
                image = NULL;
            }
        }
    }
    mc_image_free(image);
    return finish(status, &detail, error);
}

mc_client_assets_status mc_client_assets_label(mc_client_assets *assets, const char *key,
                                               mc_font_text *out, mc_client_assets_error *error) {
    mc_client_assets_error detail = {0};
    size_t length = 0;
    if (assets == NULL || out == NULL)
        return finish(fail(&detail, MC_CLIENT_ASSETS_INVALID_ARGUMENT, "label arguments"), &detail,
                      error);
    const mc_client_assets_status key_status = ascii_name(key, &length);
    if (key_status != MC_CLIENT_ASSETS_OK)
        return finish(fail(&detail, key_status, "label key"), &detail, error);
    for (label_entry *entry = assets->labels; entry != NULL; entry = entry->next) {
        if (strcmp(entry->key, key) == 0) {
            *out = entry->text;
            return finish(MC_CLIENT_ASSETS_OK, &detail, error);
        }
    }
    if (assets->label_count == MC_CLIENT_ASSETS_MAX_LABELS)
        return finish(fail(&detail, MC_CLIENT_ASSETS_LIMIT, "label cache entries"), &detail, error);
    mc_registry_status registry =
        mc_registry_classes_initialize_translation(assets->classes, &detail.registry_error);
    mc_registry_translation_view translation = {0};
    if (registry == MC_REGISTRY_OK)
        registry = mc_registry_classes_inspect_translation(assets->classes, &translation,
                                                           &detail.registry_error);
    uint16_t units[MC_CLIENT_ASSETS_MAX_NAME_BYTES];
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)key[i];
    const mc_registry_string *string = NULL;
    if (registry == MC_REGISTRY_OK)
        registry = mc_translation_property(translation.singleton_a, units, length, &string,
                                           &detail.registry_error);
    if (registry != MC_REGISTRY_OK)
        return finish(fail(&detail,
                           registry == MC_REGISTRY_LIMIT ? MC_CLIENT_ASSETS_LIMIT
                           : registry == MC_REGISTRY_ALLOCATION_FAILED
                               ? MC_CLIENT_ASSETS_ALLOCATION_FAILED
                               : MC_CLIENT_ASSETS_TRANSLATION_FAILED,
                           "translation property"),
                      &detail, error);
    mc_font_text text = {0};
    if (string != NULL) {
        mc_registry_string_view view;
        if (!mc_registry_string_read(string, &view))
            return finish(
                fail(&detail, MC_CLIENT_ASSETS_TRANSLATION_FAILED, "translation String view"),
                &detail, error);
        text = (mc_font_text){.units = view.units, .length = view.length};
    }
    const size_t fallback = string == NULL ? length * sizeof(uint16_t) : 0u;
    label_entry *entry = arena_allocate(&assets->arena, sizeof(*entry) + fallback + length + 1u);
    if (entry == NULL)
        return finish(fail(&detail, assets->arena.failure, "label cache allocation"), &detail,
                      error);
    if (string == NULL) {
        uint16_t *copy = (uint16_t *)(entry + 1);
        if (length != 0)
            memcpy(copy, units, fallback);
        text = (mc_font_text){.units = length == 0 ? NULL : copy, .length = length};
    }
    char *copy = (char *)(entry + 1) + fallback;
    memcpy(copy, key, length + 1u);
    *entry = (label_entry){.next = assets->labels, .key = copy, .text = text};
    assets->labels = entry;
    ++assets->label_count;
    *out = text;
    return finish(MC_CLIENT_ASSETS_OK, &detail, error);
}

mc_client_assets_status mc_client_assets_font_txt(mc_client_assets *assets, mc_font_text *out,
                                                  mc_client_assets_error *error) {
    mc_client_assets_error detail = {0};
    if (assets == NULL || out == NULL)
        return finish(fail(&detail, MC_CLIENT_ASSETS_INVALID_ARGUMENT, "font text arguments"),
                      &detail, error);
    if (assets->font_loaded) {
        *out = assets->font;
        return finish(MC_CLIENT_ASSETS_OK, &detail, error);
    }
    asset_arena temporary = {0};
    asset_stream stream;
    text_buffer text = {0};
    mc_java_reader *reader = NULL;
    mc_client_assets_status status = stream_open(assets, "/font.txt", &stream, &detail);
    if (caught_open_io(&detail))
        status = MC_CLIENT_ASSETS_OK;
    else if (status == MC_CLIENT_ASSETS_OK) {
        const mc_java_reader_status created = reader_create(&stream, &temporary, &reader);
        if (created != MC_JAVA_READER_OK)
            status = reader_failure(created, &stream, &temporary, &detail, "font reader create");
        while (status == MC_CLIENT_ASSETS_OK) {
            const mc_java_reader_line *line = NULL;
            const mc_java_reader_status read = mc_java_reader_read_line(reader, &line);
            if (read == MC_JAVA_READER_IO_ERROR)
                break;
            if (read != MC_JAVA_READER_OK) {
                status = reader_failure(read, &stream, &temporary, &detail, "font reader line");
                break;
            }
            if (line == NULL)
                break;
            status = append_line(&temporary, &text, line, &detail);
        }
    }
    status = close_after(&stream, reader, &temporary, true, status, &detail);
    if (status == MC_CLIENT_ASSETS_OK) {
        uint16_t *copy = NULL;
        if (text.length != 0) {
            copy = arena_allocate(&assets->arena, text.length * sizeof(*copy));
            if (copy == NULL)
                status = fail(&detail, assets->arena.failure, "font cache allocation");
            else
                memcpy(copy, text.units, text.length * sizeof(*copy));
        }
        if (status == MC_CLIENT_ASSETS_OK) {
            assets->font = (mc_font_text){.units = copy, .length = text.length};
            assets->font_loaded = true;
            *out = assets->font;
        }
    }
    arena_free(&temporary);
    return finish(status, &detail, error);
}

mc_client_assets_status mc_client_assets_choose_splash(mc_client_assets *assets,
                                                       mc_java_random *random,
                                                       mc_client_string *out,
                                                       mc_client_assets_error *error) {
    static const uint16_t missing[] = {'m', 'i', 's', 's', 'i', 'n', 'g', 'n', 'o'};
    mc_client_assets_error detail = {0};
    if (assets == NULL || random == NULL || out == NULL)
        return finish(fail(&detail, MC_CLIENT_ASSETS_INVALID_ARGUMENT, "splash arguments"), &detail,
                      error);
    asset_arena temporary = {0};
    asset_stream stream;
    mc_java_reader *reader = NULL;
    splash_line *first = NULL, *last = NULL;
    size_t count = 0;
    bool complete = false;
    mc_client_assets_status status = stream_open(assets, "/title/splashes.txt", &stream, &detail);
    if (caught_open_io(&detail))
        status = MC_CLIENT_ASSETS_OK;
    else if (status == MC_CLIENT_ASSETS_OK) {
        const mc_java_reader_status created = reader_create(&stream, &temporary, &reader);
        if (created != MC_JAVA_READER_OK)
            status = reader_failure(created, &stream, &temporary, &detail, "splash reader create");
        while (status == MC_CLIENT_ASSETS_OK) {
            const mc_java_reader_line *line = NULL;
            const mc_java_reader_status read = mc_java_reader_read_line(reader, &line);
            if (read == MC_JAVA_READER_IO_ERROR)
                break;
            if (read != MC_JAVA_READER_OK) {
                status = reader_failure(read, &stream, &temporary, &detail, "splash reader line");
                break;
            }
            if (line == NULL) {
                complete = true;
                break;
            }
            size_t begin = 0, end = line->length;
            while (begin < end && line->units[begin] <= 32u)
                ++begin;
            while (end > begin && line->units[end - 1u] <= 32u)
                --end;
            if (begin == end)
                continue;
            if (end - begin > MC_CLIENT_ASSETS_MAX_TEXT_UNITS ||
                count == MC_CLIENT_ASSETS_MAX_SPLASHES) {
                status = fail(&detail, MC_CLIENT_ASSETS_LIMIT, "splash entries/text units");
                break;
            }
            splash_line *item = arena_allocate(&temporary, sizeof(*item));
            if (item == NULL) {
                status = fail(&detail, temporary.failure, "splash list allocation");
                break;
            }
            *item = (splash_line){.text = {.units = line->units + begin, .length = end - begin}};
            if (last == NULL)
                first = item;
            else
                last->next = item;
            last = item;
            ++count;
        }
    }
    status = close_after(&stream, reader, &temporary, false, status, &detail);
    mc_font_text selected = {.units = missing, .length = sizeof(missing) / sizeof(missing[0])};
    if (status == MC_CLIENT_ASSETS_OK && complete && count != 0) {
        int32_t index = -1;
        if (!mc_java_random_next_bounded(random, (int32_t)count, &index) || index < 0 ||
            (size_t)index >= count)
            status = fail(&detail, MC_CLIENT_ASSETS_RANDOM_FAILED, "splash nextInt");
        else {
            splash_line *item = first;
            for (int32_t i = 0; i < index; ++i)
                item = item->next;
            selected = item->text;
        }
    }
    mc_client_string result = {0};
    if (status == MC_CLIENT_ASSETS_OK)
        status = copy_owned(&selected, &result, &detail);
    arena_free(&temporary);
    if (status == MC_CLIENT_ASSETS_OK)
        *out = result;
    return finish(status, &detail, error);
}

void mc_client_string_free(mc_client_string *string) {
    if (string == NULL)
        return;
    free(string->units);
    *string = (mc_client_string){0};
}
