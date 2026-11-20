#include "internal.h"
#include <stdlib.h>
#include <string.h>

static const char resource_method[] = "(Ljava/lang/String;)Ljava/io/InputStream;";

mc_texture_pack_status texture_pack_fail(mc_texture_pack_error *error,
                                         mc_texture_pack_status status, const char *operation,
                                         const char *source, const char *method,
                                         const char *descriptor, unsigned int pc,
                                         unsigned int ik_pc) {
    error->status = status;
    error->operation = operation;
    error->source_class = source;
    error->source_method = method;
    error->source_descriptor = descriptor;
    error->source_pc = pc;
    error->ik_pc = ik_pc;
    return status;
}

mc_texture_pack_status texture_pack_zip_error(mc_texture_pack_error *error,
                                              const texture_zip_error *cause) {
    const mc_texture_pack_status statuses[] = {MC_TEXTURE_PACK_OK,
                                               MC_TEXTURE_PACK_SOURCE_IO,
                                               MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
                                               MC_TEXTURE_PACK_LIMIT,
                                               MC_TEXTURE_PACK_ALLOCATION_FAILED,
                                               MC_TEXTURE_PACK_INVALID_ARGUMENT};
    error->system_error = cause->system_error;
    error->zlib_error = cause->zlib_error;
    error->native_close_error = cause->native_close_error;
    return texture_pack_fail(error, statuses[cause->status], cause->operation, error->source_class,
                             error->source_method, error->source_descriptor, error->source_pc,
                             error->ik_pc);
}

mc_texture_pack_status texture_pack_base_open(mc_client_texture_pack *pack,
                                              mc_texture_pack_text name,
                                              mc_client_texture_stream **out,
                                              mc_texture_pack_error *error) {
    const char *source = pack->kind == MC_TEXTURE_PACK_ZIP ? "vp" : "i";
    const unsigned int pc = pack->kind == MC_TEXTURE_PACK_ZIP ? 35u : 3u;
    if (name.is_null)
        return texture_pack_fail(error, MC_TEXTURE_PACK_NULL_REFERENCE, "Class resource null name",
                                 source, "a", resource_method, pc, 0);
    char *encoded = texture_text_utf8(name, pack->options.max_name_units, false, error);
    if (encoded == NULL)
        return error->status;
    /* Both real i and the vp fallback use default-package i.class. Convert
     * its relative resource name to the resource source's absolute namespace;
     * retain the caller's unmodified counted String as the ji cache key. */
    if (encoded[0] != '/') {
        const size_t length = strlen(encoded);
        char *absolute = malloc(length + 2);
        if (absolute == NULL) {
            free(encoded);
            return texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED,
                                     "Texture class resource absolute name", source, "a",
                                     resource_method, pc, 0);
        }
        absolute[0] = '/';
        memcpy(absolute + 1, encoded, length + 1);
        free(encoded);
        encoded = absolute;
    }
    if (pack->streams >= pack->options.max_streams) {
        free(encoded);
        return texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "Texture pack native stream bound",
                                 source, "a", resource_method, pc, 0);
    }
    mc_client_texture_stream *stream = calloc(1, sizeof(*stream));
    if (stream == NULL) {
        free(encoded);
        return texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED,
                                 "Texture pack native stream owner", source, "a", resource_method,
                                 pc, 0);
    }
    if (pack->resources == NULL || pack->resources->open == NULL) {
        free(encoded);
        free(stream);
        return texture_pack_fail(error, MC_TEXTURE_PACK_PROVIDER_FAILED,
                                 "Texture pack real class resource provider", source, "a",
                                 resource_method, pc, 0);
    }
    const mc_texture_pack_status held = mc_client_texture_pack_retain(pack);
    if (held != MC_TEXTURE_PACK_OK) {
        free(encoded);
        free(stream);
        return texture_pack_fail(error, held, "Texture stream native pack hold", source, "a",
                                 resource_method, pc, 0);
    }
    stream->pack = pack;
    ++pack->streams;
    const mc_registry_status opened =
        pack->resources->open(pack->resources->context, encoded, &stream->resource);
    free(encoded);
    error->resource_status = opened;
    if (opened == MC_REGISTRY_NULL_REFERENCE) {
        --pack->streams;
        (void)mc_client_texture_pack_release(pack, NULL);
        free(stream);
        *out = NULL;
        return MC_TEXTURE_PACK_OK;
    }
    if (opened != MC_REGISTRY_OK || stream->resource.read == NULL) {
        /* The provider contract owns every returned context. A broken provider
         * is not a source missing-resource result or a fabricated IOException. */
        --pack->streams;
        (void)mc_client_texture_pack_release(pack, NULL);
        free(stream);
        return texture_pack_fail(error, MC_TEXTURE_PACK_PROVIDER_FAILED,
                                 "Texture pack resource provider open", source, "a",
                                 resource_method, pc, 0);
    }
    stream->resource_open = true;
    *out = stream;
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status mc_client_texture_pack_open(mc_client_texture_pack *pack,
                                                   mc_texture_pack_text name,
                                                   mc_client_texture_stream **out,
                                                   mc_texture_pack_error *error) {
    mc_texture_pack_error e = {0};
    if (pack == NULL || out == NULL || (name.is_null && (name.units != NULL || name.length != 0)) ||
        (!name.is_null && name.length != 0 && name.units == NULL)) {
        texture_pack_fail(&e, MC_TEXTURE_PACK_INVALID_ARGUMENT,
                          "Texture pack native open arguments", NULL, NULL, NULL, 0, 0);
        goto done;
    }
    if (pack->kind == MC_TEXTURE_PACK_DEFAULT) {
        texture_pack_base_open(pack, name, out, &e);
        goto done;
    }
    /* Source vp catches Exception over e0/substring6/lookup9/getInputStream22,
     * then executes base Class.getResourceAsStream35. Native unsupported/bounds
     * remain distinct and are not swept into this source fallback. */
    if (name.is_null || name.length == 0 || pack->zip == NULL) {
        texture_pack_base_open(pack, name, out, &e);
        goto done;
    }
    const mc_texture_pack_text relative = {name.units + 1, name.length - 1, false};
    char *encoded = texture_text_utf8(relative, pack->options.max_name_units, false, &e);
    if (encoded == NULL)
        goto done;
    texture_zip_stream *zip_stream = NULL;
    texture_zip_error z = {0};
    const texture_zip_status status =
        texture_zip_lookup(pack->zip, (const uint8_t *)encoded, strlen(encoded), &zip_stream, &z);
    free(encoded);
    if (status == TEXTURE_ZIP_SOURCE_IO || (status == TEXTURE_ZIP_OK && zip_stream == NULL)) {
        texture_pack_base_open(pack, name, out, &e);
        goto done;
    }
    if (status != TEXTURE_ZIP_OK) {
        texture_pack_zip_error(&e, &z);
        goto done;
    }
    texture_stream_from_zip(pack, zip_stream, out, &e);
done:
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_texture_pack_status texture_stream_from_zip(mc_client_texture_pack *pack,
                                               texture_zip_stream *zip_stream,
                                               mc_client_texture_stream **out,
                                               mc_texture_pack_error *error) {
    if (pack->streams >= pack->options.max_streams) {
        texture_zip_stream_release(zip_stream, NULL);
        texture_pack_fail(error, MC_TEXTURE_PACK_LIMIT, "Texture pack native stream bound", "vp",
                          "a", resource_method, 22, 0);
        return error->status;
    }
    mc_client_texture_stream *stream = calloc(1, sizeof(*stream));
    if (stream == NULL) {
        texture_zip_stream_release(zip_stream, NULL);
        texture_pack_fail(error, MC_TEXTURE_PACK_ALLOCATION_FAILED,
                          "Texture ZIP stream native owner", "vp", "a", resource_method, 22, 0);
        return error->status;
    }
    const mc_texture_pack_status held = mc_client_texture_pack_retain(pack);
    if (held != MC_TEXTURE_PACK_OK) {
        texture_zip_stream_release(zip_stream, NULL);
        free(stream);
        texture_pack_fail(error, held, "Texture ZIP stream pack hold", "vp", "a", resource_method,
                          22, 0);
        return error->status;
    }
    stream->pack = pack;
    stream->zip = zip_stream;
    ++pack->streams;
    *out = stream;
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status mc_client_texture_stream_read(mc_client_texture_stream *stream,
                                                     uint8_t *bytes, size_t capacity, size_t *count,
                                                     mc_texture_pack_error *error) {
    mc_texture_pack_error e = {0};
    if (stream == NULL || count == NULL || (bytes == NULL && capacity != 0))
        texture_pack_fail(&e, MC_TEXTURE_PACK_INVALID_ARGUMENT, "Texture stream native read view",
                          NULL, NULL, NULL, 0, 0);
    else if (stream->active)
        texture_pack_fail(&e, MC_TEXTURE_PACK_BUSY, "Texture stream read operation active", NULL,
                          NULL, NULL, 0, 0);
    else if (stream->source_closed && stream->zip == NULL)
        texture_pack_fail(&e, MC_TEXTURE_PACK_SOURCE_IO, "Texture stream read after source close",
                          NULL, NULL, NULL, 0, 0);
    else if (stream->zip != NULL) {
        texture_zip_error cause = {0};
        size_t read = 0;
        if (texture_zip_read(stream->zip, bytes, capacity, &read, &cause) != TEXTURE_ZIP_OK)
            texture_pack_zip_error(&e, &cause);
        else if (read > SIZE_MAX - stream->bytes_read)
            texture_pack_fail(&e, MC_TEXTURE_PACK_LIMIT, "Texture stream native byte count", NULL,
                              NULL, NULL, 0, 0);
        else {
            stream->bytes_read += read;
            *count = read;
        }
    } else {
        size_t read = 0;
        stream->active = true;
        e.read_status = stream->resource.read(stream->resource.context, bytes, capacity, &read);
        stream->active = false;
        if (e.read_status != MC_JAVA_PROPERTIES_OK || read > capacity) {
            const mc_texture_pack_status status =
                e.read_status == MC_JAVA_PROPERTIES_IO_ERROR ? MC_TEXTURE_PACK_SOURCE_IO
                : e.read_status == MC_JAVA_PROPERTIES_LIMIT  ? MC_TEXTURE_PACK_LIMIT
                : e.read_status == MC_JAVA_PROPERTIES_ALLOCATION_FAILED
                    ? MC_TEXTURE_PACK_ALLOCATION_FAILED
                    : MC_TEXTURE_PACK_PROVIDER_FAILED;
            texture_pack_fail(&e, status, "Texture resource stream read", NULL, NULL, NULL, 0, 0);
        } else if (read > SIZE_MAX - stream->bytes_read)
            texture_pack_fail(&e, MC_TEXTURE_PACK_LIMIT, "Texture stream native byte count", NULL,
                              NULL, NULL, 0, 0);
        else {
            stream->bytes_read += read;
            *count = read;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

static mc_texture_pack_status close_stream(mc_client_texture_stream *stream, bool source,
                                           mc_texture_pack_error *e) {
    if (!source)
        ++stream->native_close_calls;
    if (stream->zip != NULL) {
        texture_zip_error cause = {0};
        if (texture_zip_stream_close(stream->zip, &cause) != TEXTURE_ZIP_OK)
            return texture_pack_zip_error(e, &cause);
    } else if (stream->resource_open) {
        const mc_registry_resources *resources = stream->pack->resources;
        if (resources->close == NULL)
            return texture_pack_fail(e, MC_TEXTURE_PACK_UNSUPPORTED_NATIVE,
                                     "Texture resource source close dependency", NULL, NULL, NULL,
                                     0, 0);
        const mc_registry_status closed = resources->close(resources->context, &stream->resource);
        stream->resource_open = false;
        if (source)
            e->resource_status = closed;
        else
            e->native_close_status = closed;
        if (closed != MC_REGISTRY_OK)
            return texture_pack_fail(e,
                                     source && closed == MC_REGISTRY_IO_FAILED
                                         ? MC_TEXTURE_PACK_SOURCE_IO
                                         : MC_TEXTURE_PACK_PROVIDER_FAILED,
                                     "Texture resource source close", NULL, NULL, NULL, 0, 0);
    }
    return MC_TEXTURE_PACK_OK;
}

mc_texture_pack_status mc_client_texture_stream_close(mc_client_texture_stream *stream,
                                                      mc_texture_pack_error *error) {
    mc_texture_pack_error e = {0};
    if (stream == NULL)
        texture_pack_fail(&e, MC_TEXTURE_PACK_INVALID_ARGUMENT, "Texture stream close receiver",
                          NULL, NULL, NULL, 0, 0);
    else if (stream->active)
        texture_pack_fail(&e, MC_TEXTURE_PACK_BUSY, "Texture stream close operation active", NULL,
                          NULL, NULL, 0, 0);
    else {
        ++stream->source_close_calls;
        if (!stream->source_closed) {
            stream->source_closed = true;
            stream->active = true;
            close_stream(stream, true, &e);
            stream->active = false;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_texture_pack_status mc_client_texture_stream_release(mc_client_texture_stream *stream,
                                                        mc_texture_pack_error *error) {
    mc_texture_pack_error e = {0};
    if (stream != NULL && stream->active) {
        texture_pack_fail(&e, MC_TEXTURE_PACK_BUSY, "Texture stream native operation active", NULL,
                          NULL, NULL, 0, 0);
        if (error != NULL)
            *error = e;
        return e.status;
    }
    if (stream != NULL) {
        if (!stream->source_closed || stream->resource_open) {
            stream->active = true;
            close_stream(stream, false, &e);
            stream->active = false;
        }
        texture_zip_error zip_close = {0};
        if (texture_zip_stream_release(stream->zip, &zip_close) != TEXTURE_ZIP_OK &&
            e.status == MC_TEXTURE_PACK_OK) {
            texture_pack_zip_error(&e, &zip_close);
            e.cause_status = e.status;
            e.status = MC_TEXTURE_PACK_PROVIDER_FAILED;
            e.native_close_error = zip_close.system_error;
            e.operation = "Texture ZIP stream native retirement";
        }
        mc_client_texture_pack *pack = stream->pack;
        --pack->streams;
        mc_texture_pack_error cause = {0};
        const mc_texture_pack_status released = mc_client_texture_pack_release(pack, &cause);
        if (e.status == MC_TEXTURE_PACK_OK && released != MC_TEXTURE_PACK_OK)
            e = cause;
        free(stream);
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_texture_stream_inspect(const mc_client_texture_stream *stream,
                                      mc_texture_stream_state *out) {
    if (stream == NULL || out == NULL)
        return false;
    *out = (mc_texture_stream_state){stream->bytes_read, stream->source_closed,
                                     stream->source_close_calls, stream->native_close_calls,
                                     stream->pack};
    return true;
}
