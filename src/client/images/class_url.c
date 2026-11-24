#include "class_url.h"
#include <stdlib.h>
#include <string.h>

struct mc_client_class_url {
    mc_resource_files *files;
    const mc_registry_resources *resources;
    mc_player_string *name;
    mc_client_class_stream *stream;
    bool active, attempted;
    mc_client_class_url_error state;
};

static mc_client_class_url_status fail(mc_client_class_url_error *e,
                                       mc_client_class_url_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    return status;
}

mc_client_class_url_status mc_client_class_url_create(mc_resource_files *files,
                                                      const mc_registry_resources *resources,
                                                      const char *name, mc_client_class_url **out,
                                                      mc_client_class_url_error *error) {
    mc_client_class_url_error e = {0};
    if (files == NULL || resources != mc_resource_files_source(files) || name == NULL ||
        out == NULL) {
        fail(&e, MC_CLIENT_CLASS_URL_INVALID_ARGUMENT,
             "Class URL actual files/namespace/name/output");
        goto done;
    }
    bool found = false;
    e.locate = mc_resource_files_locate(files, name, &found);
    if (e.locate != MC_REGISTRY_OK) {
        fail(&e, MC_CLIENT_CLASS_URL_PROVIDER_FAILED, "Class.getResource concrete lookup");
        goto done;
    }
    e.url_found = found;
    if (!found) {
        *out = NULL;
        goto done;
    }
    const size_t length = strlen(name);
    if (length == 0 || length > 65535) {
        fail(&e, MC_CLIENT_CLASS_URL_LIMIT, "Class URL native counted name bound");
        goto done;
    }
    uint16_t *units = malloc(length * sizeof(*units));
    mc_client_class_url *url = calloc(1, sizeof(*url));
    if (units == NULL || url == NULL) {
        free(units);
        free(url);
        fail(&e, MC_CLIENT_CLASS_URL_ALLOCATION_FAILED, "Class URL native storage");
        goto done;
    }
    for (size_t n = 0; n < length; ++n)
        units[n] = (unsigned char)name[n];
    e.string = mc_player_string_create(units, length, &url->name);
    free(units);
    if (e.string != MC_PLAYER_OK) {
        free(url);
        fail(&e, MC_CLIENT_CLASS_URL_ALLOCATION_FAILED, "Class URL immutable name");
        goto done;
    }
    url->files = files;
    url->resources = resources;
    url->state = e;
    *out = url;
done:
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_class_url_status mc_client_class_url_read_image(mc_client_class_url *url,
                                                          const mc_client_image_io_limits *limits,
                                                          mc_client_argb_image **out,
                                                          mc_client_class_url_error *error) {
    mc_client_class_url_error e = {0};
    if (url == NULL) {
        fail(&e, MC_CLIENT_CLASS_URL_SOURCE_ILLEGAL_ARGUMENT, "ImageIO null URL");
        goto done;
    }
    e = url->state;
    if (url->active) {
        fail(&e, MC_CLIENT_CLASS_URL_BUSY, "Class URL entered");
        goto done;
    }
    if (url->attempted || out == NULL || limits == NULL) {
        fail(&e, MC_CLIENT_CLASS_URL_INVALID_ARGUMENT, "Class URL one-attempt/output/limits");
        goto done;
    }
    url->active = true;
    url->attempted = true;
    e.read_entered = true;
    const mc_client_class_stream_status opened = mc_client_class_stream_open(
        url->resources, url->name, 65535, limits->max_encoded_bytes, &url->stream, &e.stream);
    if (opened != MC_CLIENT_CLASS_STREAM_OK) {
        fail(&e,
             e.stream.resource == MC_REGISTRY_IO_FAILED ? MC_CLIENT_CLASS_URL_SOURCE_IO
                                                        : MC_CLIENT_CLASS_URL_PROVIDER_FAILED,
             "ImageIO(URL) actual stream open");
        goto leave;
    }
    if (url->stream == NULL) {
        /* URL was real at lookup; deletion before open is a concrete file-open
         * IOException, not ImageIO's null-URL IllegalArgumentException. */
        fail(&e, MC_CLIENT_CLASS_URL_SOURCE_IO, "ImageIO URL no longer exists");
        goto leave;
    }
    mc_client_argb_image *image = NULL;
    const mc_client_image_io_input input = mc_client_class_stream_image_input(url->stream);
    const mc_client_image_io_status read =
        mc_client_image_io_read(&input, limits, &image, &e.image);
    if (read != MC_CLIENT_IMAGE_IO_OK && read != MC_CLIENT_IMAGE_IO_SOURCE_IO &&
        read != MC_CLIENT_IMAGE_IO_SOURCE_ILLEGAL_ARGUMENT) {
        fail(&e, MC_CLIENT_CLASS_URL_IMAGE_FAILED, "ImageIO URL native/provider frontier");
        goto leave;
    }
    e.source_close_reached = true;
    /* Canonical concrete close transport, not a second decoder or fclose.
     * The source URL wrapper owns this close site. Stream context storage
     * remains owned by the real resource source until its native retirement. */
    const mc_client_class_stream_status closed =
        mc_client_class_stream_retire(&url->stream, &e.stream);
    if (closed != MC_CLIENT_CLASS_STREAM_OK) {
        mc_client_argb_image_release(image);
        fail(&e,
             closed == MC_CLIENT_CLASS_STREAM_NATIVE_CLOSE_FAILED
                 ? MC_CLIENT_CLASS_URL_SOURCE_IO
                 : MC_CLIENT_CLASS_URL_PROVIDER_FAILED,
             "ImageIO URL source close-finally");
        goto leave;
    }
    if (read != MC_CLIENT_IMAGE_IO_OK) {
        mc_client_argb_image_release(image);
        fail(&e,
             read == MC_CLIENT_IMAGE_IO_SOURCE_IO ? MC_CLIENT_CLASS_URL_SOURCE_IO
                                                  : MC_CLIENT_CLASS_URL_SOURCE_ILLEGAL_ARGUMENT,
             "ImageIO URL actual source failure");
        goto leave;
    }
    e.source_returned = true;
    *out = image; /* Includes genuine unrecognized-image NULL. */
leave:
    url->active = false;
    url->state = e;
done:
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_class_url_status mc_client_class_url_release(mc_client_class_url **slot,
                                                       mc_client_class_url_error *error) {
    mc_client_class_url_error e = {0};
    if (slot == NULL)
        fail(&e, MC_CLIENT_CLASS_URL_INVALID_ARGUMENT, "Class URL release slot");
    else if (*slot != NULL) {
        mc_client_class_url *url = *slot;
        e = url->state;
        e.status = MC_CLIENT_CLASS_URL_OK;
        if (url->active)
            fail(&e, MC_CLIENT_CLASS_URL_BUSY, "Class URL active retirement");
        else {
            url->active = true;
            const mc_client_class_stream_status status =
                mc_client_class_stream_retire(&url->stream, &e.stream);
            url->active = false;
            if (status != MC_CLIENT_CLASS_STREAM_OK)
                fail(&e, MC_CLIENT_CLASS_URL_PROVIDER_FAILED,
                     "Class URL retained native stream retirement");
            else {
                mc_player_string_release(url->name);
                free(url);
                *slot = NULL;
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
