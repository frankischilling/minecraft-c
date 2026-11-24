#include "sdk_icons_internal.h"

#include <stdlib.h>

mc_client_sdk_icons_status mc_client_sdk_icons_fail(mc_client_sdk_icons_error *e,
                                                    mc_client_sdk_icons_status status,
                                                    const char *operation) {
    e->status = status;
    e->operation = operation;
    return status;
}

mc_client_sdk_icons_status mc_client_sdk_icons_allocate(const mc_registry_resources *source,
                                                        mc_client_sdk_icons **out,
                                                        mc_client_sdk_icons_error *error) {
    mc_client_sdk_icons_error e = {0};
    if (out == NULL || source == NULL || source->open == NULL || source->close == NULL)
        mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_INVALID_ARGUMENT,
                                 "imported SDK icon resource source/output");
    else {
        mc_client_sdk_icons *o = calloc(1, sizeof(*o));
        if (o == NULL)
            mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_ALLOCATION_FAILED,
                                     "imported SDK icon owner allocation");
        else {
            o->source = source;
            o->holds = 1;
            *out = o;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

static bool read_asset(mc_client_sdk_icons *o, const char *name, uint8_t *bytes, size_t capacity,
                       int edge, mc_client_sdk_icons_error *e) {
    e->asset_edge = edge;
    e->bytes_read = 0;
    e->open = o->source->open(o->source->context, name, &o->stream);
    if (e->open != MC_REGISTRY_OK) {
        mc_client_sdk_icons_fail(e, MC_CLIENT_SDK_ICONS_RESOURCE_FAILED,
                                 "open actual imported SDK icon");
        return false;
    }
    o->stream_open = true;
    e->stream_open = true;
    if (o->stream.read == NULL) {
        mc_client_sdk_icons_fail(e, MC_CLIENT_SDK_ICONS_RESOURCE_FAILED,
                                 "imported SDK icon has no byte reader");
        return false;
    }
    while (e->bytes_read < capacity) {
        size_t count = 0;
        const size_t remaining = capacity - e->bytes_read;
        e->read = o->stream.read(o->stream.context, bytes + e->bytes_read, remaining, &count);
        if (e->read != MC_JAVA_PROPERTIES_OK || count > remaining) {
            mc_client_sdk_icons_fail(e, MC_CLIENT_SDK_ICONS_RESOURCE_FAILED,
                                     "read actual imported SDK icon");
            return false;
        }
        if (count == 0) {
            mc_client_sdk_icons_fail(e, MC_CLIENT_SDK_ICONS_WRONG_EXTENT,
                                     "short imported SDK RGBA asset");
            return false;
        }
        e->bytes_read += count;
    }
    uint8_t extra;
    size_t count = 0;
    e->read = o->stream.read(o->stream.context, &extra, 1, &count);
    if (e->read != MC_JAVA_PROPERTIES_OK) {
        mc_client_sdk_icons_fail(e, MC_CLIENT_SDK_ICONS_RESOURCE_FAILED,
                                 "check imported SDK icon EOF");
        return false;
    }
    if (count != 0) {
        mc_client_sdk_icons_fail(e, MC_CLIENT_SDK_ICONS_WRONG_EXTENT,
                                 "overlong imported SDK RGBA asset");
        return false;
    }
    return true;
}

static bool close_stream(mc_client_sdk_icons *o, mc_client_sdk_icons_error *e) {
    if (!o->stream_open)
        return true;
    e->close = o->source->close(o->source->context, &o->stream);
    if (e->close != MC_REGISTRY_OK) {
        e->stream_open = true;
        if (e->status == MC_CLIENT_SDK_ICONS_OK)
            mc_client_sdk_icons_fail(e, MC_CLIENT_SDK_ICONS_RESOURCE_FAILED,
                                     "close actual imported SDK icon");
        return false;
    }
    o->stream_open = false;
    o->stream = (mc_java_properties_input){0};
    e->stream_open = false;
    return true;
}

mc_client_sdk_icons_status mc_client_sdk_icons_construct(mc_client_sdk_icons *o,
                                                         mc_client_sdk_icons_error *error) {
    mc_client_sdk_icons_error e = {0};
    if (o == NULL || o->phase != MC_CLIENT_SDK_ICONS_RAW || o->active)
        mc_client_sdk_icons_fail(&e,
                                 o != NULL && o->active ? MC_CLIENT_SDK_ICONS_BUSY
                                                        : MC_CLIENT_SDK_ICONS_INVALID_PHASE,
                                 "imported SDK icon once-only construction");
    else {
        o->active = true;
        o->phase = MC_CLIENT_SDK_ICONS_LOADING;
        bool complete =
            read_asset(o, "/sdk/lwjgl-icon-16.rgba", o->rgba16, sizeof(o->rgba16), 16, &e);
        const bool first_closed = close_stream(o, &e);
        if (complete && first_closed) {
            complete =
                read_asset(o, "/sdk/lwjgl-icon-32.rgba", o->rgba32, sizeof(o->rgba32), 32, &e);
            if (!close_stream(o, &e))
                complete = false;
        } else
            complete = false;
        o->phase = complete ? MC_CLIENT_SDK_ICONS_READY : MC_CLIENT_SDK_ICONS_FAILED;
        if (!complete)
            o->failure = e;
        o->active = false;
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_sdk_icons_status mc_client_sdk_icons_retain(mc_client_sdk_icons *o,
                                                      mc_client_sdk_icons_error *error) {
    mc_client_sdk_icons_error e = {0};
    if (o == NULL || o->phase != MC_CLIENT_SDK_ICONS_READY || o->active)
        mc_client_sdk_icons_fail(&e,
                                 o != NULL && o->active ? MC_CLIENT_SDK_ICONS_BUSY
                                                        : MC_CLIENT_SDK_ICONS_INVALID_PHASE,
                                 "retain READY imported SDK icon occurrence");
    else if (o->holds == SIZE_MAX)
        mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_LIMIT, "imported SDK icon hold bound");
    else
        ++o->holds;
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_sdk_icons_status mc_client_sdk_icons_release(mc_client_sdk_icons **slot,
                                                       mc_client_sdk_icons_error *error) {
    mc_client_sdk_icons_error e = {0};
    if (slot == NULL)
        mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_INVALID_ARGUMENT,
                                 "imported SDK icon release slot");
    else if (*slot != NULL) {
        mc_client_sdk_icons *o = *slot;
        if (o->active)
            mc_client_sdk_icons_fail(&e, MC_CLIENT_SDK_ICONS_BUSY,
                                     "active imported SDK icon release");
        else if (o->holds > 1) {
            --o->holds;
            *slot = NULL;
        } else {
            o->phase = MC_CLIENT_SDK_ICONS_RETIRING;
            o->active = true;
            if (close_stream(o, &e)) {
                free(o);
                *slot = NULL;
            } else {
                o->failure = e;
                o->active = false;
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

bool mc_client_sdk_icons_inspect(const mc_client_sdk_icons *o, mc_client_sdk_icons_state *out) {
    if (o == NULL || out == NULL)
        return false;
    *out = (mc_client_sdk_icons_state){o->phase,  o->holds,  o->active,  o->stream_open,
                                       o->rgba16, o->rgba32, &o->failure};
    return true;
}
