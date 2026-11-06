#include "class_stream.h"
#include <stdlib.h>
#include <string.h>

struct mc_client_class_stream {
    const mc_registry_resources *source;
    mc_java_properties_input bytes;
    char *resolved_name;
    size_t bytes_read, max_bytes;
    bool active;
    mc_client_class_stream_error state;
};

static mc_client_class_stream_status
fail(mc_client_class_stream_error *e, mc_client_class_stream_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    return status;
}

mc_client_class_stream_status mc_client_class_stream_open(const mc_registry_resources *resources,
                                                          const mc_player_string *name,
                                                          size_t max_name, size_t max_bytes,
                                                          mc_client_class_stream **out,
                                                          mc_client_class_stream_error *error) {
    mc_client_class_stream_error e = {0};
    mc_client_class_stream_status status = MC_CLIENT_CLASS_STREAM_OK;
    if (out == NULL) {
        status = fail(&e, MC_CLIENT_CLASS_STREAM_INVALID_ARGUMENT,
                      "class-resource native source/output");
        goto done;
    }
    if (name == NULL) {
        status =
            fail(&e, MC_CLIENT_CLASS_STREAM_SOURCE_NULL_NAME, "actual ji.class null resource name");
        goto done;
    }
    if (resources == NULL) {
        status = fail(&e, MC_CLIENT_CLASS_STREAM_INVALID_ARGUMENT, "class-resource native source");
        goto done;
    }
    mc_player_string_view text;
    if (!mc_player_string_read(name, &text) || text.length > max_name ||
        text.length > SIZE_MAX - 2) {
        status = fail(&e, MC_CLIENT_CLASS_STREAM_LIMIT, "class-resource native name bound");
        goto done;
    }
    if (resources->open == NULL || text.length == 0) {
        status = fail(&e, MC_CLIENT_CLASS_STREAM_UNIMPLEMENTED,
                      "class-resource real open/empty-name path");
        goto done;
    }
    for (size_t i = 0; i < text.length; ++i)
        if (text.units[i] == 0 || text.units[i] > 127) {
            status = fail(&e, MC_CLIENT_CLASS_STREAM_UNIMPLEMENTED,
                          "class-resource non-ASCII/NUL namespace path");
            goto done;
        }
    mc_client_class_stream *stream = calloc(1, sizeof(*stream));
    if (stream == NULL) {
        status = fail(&e, MC_CLIENT_CLASS_STREAM_ALLOCATION_FAILED,
                      "class-resource native stream owner");
        goto done;
    }
    const size_t prefix = text.units[0] == '/' ? 0 : 1;
    stream->resolved_name = malloc(prefix + text.length + 1);
    if (stream->resolved_name == NULL) {
        free(stream);
        status = fail(&e, MC_CLIENT_CLASS_STREAM_ALLOCATION_FAILED,
                      "class-resource resolved namespace name");
        goto done;
    }
    if (prefix != 0)
        stream->resolved_name[0] = '/';
    for (size_t i = 0; i < text.length; ++i)
        stream->resolved_name[prefix + i] = (char)text.units[i];
    stream->resolved_name[prefix + text.length] = '\0';
    stream->source = resources;
    stream->max_bytes = max_bytes;
    e.resource = resources->open(resources->context, stream->resolved_name, &stream->bytes);
    if (e.resource == MC_REGISTRY_NULL_REFERENCE) {
        e.missing_stream = true;
        free(stream->resolved_name);
        free(stream);
        *out = NULL;
        goto done;
    }
    if (e.resource != MC_REGISTRY_OK) {
        /* Generic provider open failures are native diagnostics. They do not
         * invent a Java IOException from ClassLoader's caught URL-open path. */
        status = fail(&e,
                      e.resource == MC_REGISTRY_LIMIT ? MC_CLIENT_CLASS_STREAM_LIMIT
                      : e.resource == MC_REGISTRY_ALLOCATION_FAILED
                          ? MC_CLIENT_CLASS_STREAM_ALLOCATION_FAILED
                          : MC_CLIENT_CLASS_STREAM_PROVIDER_FAILED,
                      "class-resource actual provider open");
        free(stream->resolved_name);
        free(stream);
        goto done;
    }
    stream->state = e;
    *out = stream;
done:
    if (error != NULL)
        *error = e;
    return status;
}

static mc_client_image_io_status read_image(void *context, uint8_t *bytes, size_t capacity,
                                            size_t *count, mc_client_image_io_error *error) {
    mc_client_class_stream *stream = context;
    if (count != NULL)
        *count = 0;
    if (stream == NULL || bytes == NULL || count == NULL || capacity == 0)
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_INVALID_ARGUMENT,
                                       "class-resource read arguments");
    if (stream->active)
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_BUSY,
                                       "class-resource read active");
    if (stream->bytes.read == NULL)
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_UNSUPPORTED_NATIVE,
                                       "class-resource actual read body missing");
    if (capacity > stream->max_bytes - stream->bytes_read)
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_LIMIT,
                                       "class-resource read native bound");
    stream->active = true;
    size_t actual = 0;
    const mc_java_properties_status status =
        stream->source == NULL || stream->bytes.read == NULL
            ? MC_JAVA_PROPERTIES_INVALID_ARGUMENT
            : stream->bytes.read(stream->bytes.context, bytes, capacity, &actual);
    stream->active = false;
    stream->state.read = status;
    if (actual > capacity || (status != MC_JAVA_PROPERTIES_OK && actual != 0)) {
        stream->state.read = MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
        *count = 0;
        return mc_client_image_io_fail(error, MC_CLIENT_IMAGE_IO_INVALID_ARGUMENT,
                                       "class-resource malformed native byte result");
    }
    if (status != MC_JAVA_PROPERTIES_OK) {
        *count = 0;
        return mc_client_image_io_fail(
            error,
            status == MC_JAVA_PROPERTIES_IO_ERROR            ? MC_CLIENT_IMAGE_IO_SOURCE_IO
            : status == MC_JAVA_PROPERTIES_LIMIT             ? MC_CLIENT_IMAGE_IO_LIMIT
            : status == MC_JAVA_PROPERTIES_ALLOCATION_FAILED ? MC_CLIENT_IMAGE_IO_ALLOCATION_FAILED
                                                             : MC_CLIENT_IMAGE_IO_PROVIDER_FAILED,
            "class-resource actual byte read");
    }
    stream->bytes_read += actual;
    stream->state.bytes_read = stream->bytes_read;
    *count = actual;
    return MC_CLIENT_IMAGE_IO_OK;
}

mc_client_image_io_input mc_client_class_stream_image_input(mc_client_class_stream *stream) {
    return (mc_client_image_io_input){stream, read_image, stream == NULL};
}

bool mc_client_class_stream_inspect(const mc_client_class_stream *stream,
                                    mc_client_class_stream_error *out) {
    if (stream == NULL || out == NULL)
        return false;
    *out = stream->state;
    return true;
}

mc_client_class_stream_status mc_client_class_stream_retire(mc_client_class_stream **slot,
                                                            mc_client_class_stream_error *error) {
    mc_client_class_stream_error e = {0};
    if (slot == NULL) {
        fail(&e, MC_CLIENT_CLASS_STREAM_INVALID_ARGUMENT, "class-resource retire slot");
    } else if (*slot != NULL) {
        mc_client_class_stream *stream = *slot;
        e = stream->state;
        e.status = MC_CLIENT_CLASS_STREAM_OK;
        if (stream->active) {
            fail(&e, MC_CLIENT_CLASS_STREAM_BUSY, "class-resource native close during read/close");
        } else if (stream->source->close == NULL) {
            fail(&e, MC_CLIENT_CLASS_STREAM_UNIMPLEMENTED, "class-resource real native closer");
        } else {
            e.native_close_attempted = true;
            stream->active = true;
            e.close = stream->source->close(stream->source->context, &stream->bytes);
            stream->active = false;
            if (e.close == MC_REGISTRY_OK) {
                free(stream->resolved_name);
                free(stream);
                *slot = NULL;
            } else {
                fail(&e, MC_CLIENT_CLASS_STREAM_NATIVE_CLOSE_FAILED,
                     "class-resource actual native file close");
                stream->state = e;
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_class_stream_status mc_client_class_stream_ready(mc_client_class_stream *stream,
                                                           bool *out,
                                                           mc_client_class_stream_error *error) {
    mc_client_class_stream_error e = {0};
    if (stream == NULL || out == NULL)
        fail(&e, MC_CLIENT_CLASS_STREAM_INVALID_ARGUMENT, "class-resource availability arguments");
    else if (stream->active)
        fail(&e, MC_CLIENT_CLASS_STREAM_BUSY,
             "class-resource availability during read/ready/close");
    else if (stream->source == NULL)
        fail(&e, MC_CLIENT_CLASS_STREAM_UNIMPLEMENTED, "class-resource availability source");
    else {
        bool ready = false;
        if (stream->source->ready != NULL) {
            stream->active = true;
            ready = stream->source->ready(stream->source->context, &stream->bytes);
            stream->active = false;
        }
        *out = ready;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
