#include "../texture_packs/image_io.h"
#include "context_internal.h"
#include "resource_lookup_internal.h"

static const char stream_to_image[] = "(Ljava/io/InputStream;)Ljava/awt/image/BufferedImage;";

mc_client_graphics_site mc_client_resource_lookup_site(const mc_client_texture_lookup_attempt *a,
                                                       int32_t pc) {
    mc_client_graphics_site site = a->method;
    site.pc = pc;
    return site;
}

mc_client_texture_lookup_status mc_client_resource_lookup_error_at(
    const mc_client_texture_lookup_attempt *a, mc_client_texture_lookup_error *e,
    mc_client_texture_lookup_status status, const char *operation, int32_t pc, bool native) {
    e->status = status;
    e->operation = operation;
    e->site = mc_client_resource_lookup_site(a, pc);
    e->native_policy = native;
    return status;
}

void mc_client_resource_lookup_touch(mc_client_texture_lookup_attempt *a, int32_t pc) {
    a->state.last_site = mc_client_resource_lookup_site(a, pc);
}

static mc_client_texture_lookup_status pack_failure(mc_client_texture_lookup_attempt *a,
                                                    int32_t caller, int32_t helper,
                                                    const char *operation) {
    mc_client_texture_lookup_error *e = &a->failure;
    e->frames[0] = mc_client_resource_lookup_site(a, caller);
    e->frame_count = 1;
    if (helper >= 0) {
        e->frames[1] = (mc_client_graphics_site){"ji", "a", stream_to_image, helper};
        e->frame_count = 2;
        e->dependency =
            helper == 1
                ? (mc_client_graphics_site){"javax/imageio/ImageIO", "read", stream_to_image, -1}
                : (mc_client_graphics_site){"java/io/InputStream", "close", "()V", -1};
    } else if (e->pack.source_class != NULL && e->pack.source_method != NULL) {
        e->dependency =
            (mc_client_graphics_site){e->pack.source_class, e->pack.source_method,
                                      e->pack.source_descriptor, (int32_t)e->pack.source_pc};
    }
    if (e->pack.status == MC_TEXTURE_PACK_SOURCE_IO) {
        e->caught_source_io = true;
        e->dependency =
            (mc_client_graphics_site){"java/io/IOException", "printStackTrace", "()V", -1};
        const int32_t diagnostic = a->state.color_lookup ? 193 : 221;
        mc_client_resource_lookup_touch(a, diagnostic);
        return mc_client_resource_lookup_error_at(
            a, e, MC_CLIENT_LOOKUP_UNIMPLEMENTED,
            a->state.color_lookup
                ? "actual IOException.printStackTrace body before fallback conversion201"
                : "actual IOException.printStackTrace body before fallback ge228",
            diagnostic, false);
    }
    return mc_client_resource_lookup_error_at(
        a, e,
        e->pack.status == MC_TEXTURE_PACK_SOURCE_ILLEGAL_ARGUMENT
            ? MC_CLIENT_LOOKUP_SOURCE_ILLEGAL_ARGUMENT
        : e->pack.status == MC_TEXTURE_PACK_UNSUPPORTED_NATIVE ||
                e->pack.status == MC_TEXTURE_PACK_DIAGNOSTIC_DEPENDENCY ||
                e->pack.status == MC_TEXTURE_PACK_RENDER_DEPENDENCY
            ? MC_CLIENT_LOOKUP_UNIMPLEMENTED
            : MC_CLIENT_LOOKUP_PACK_FAILED,
        operation, caller,
        e->pack.status != MC_TEXTURE_PACK_SOURCE_ILLEGAL_ARGUMENT &&
            e->pack.status != MC_TEXTURE_PACK_NULL_REFERENCE);
}

mc_client_texture_lookup_status
mc_client_resource_lookup_capture(mc_client_render_engine *o, mc_client_texture_lookup_attempt *a) {
    mc_client_texture_lookup_error *e = &a->failure;
    a->list = o->fields.l;
    a->state.captured_list = a->list;
    mc_client_resource_lookup_touch(a, 1);
    if (a->list == NULL)
        return mc_client_resource_lookup_error_at(a, e, MC_CLIENT_LOOKUP_NULL_REFERENCE,
                                                  "actual null ik selected-Pack receiver", 4,
                                                  false);
    (void)mc_client_texture_pack_selected(a->list, &a->pack);
    a->state.captured_pack = a->pack;
    mc_client_resource_lookup_touch(a, 4);
    e->pack.status = mc_client_texture_pack_list_retain(a->list);
    if (e->pack.status != MC_TEXTURE_PACK_OK)
        return mc_client_resource_lookup_error_at(a, e, MC_CLIENT_LOOKUP_LIMIT,
                                                  "captured ik native hold", -1, true);
    a->list_held = true;
    if (a->pack != NULL) {
        e->pack.status = mc_client_texture_pack_retain(a->pack);
        if (e->pack.status != MC_TEXTURE_PACK_OK)
            return mc_client_resource_lookup_error_at(a, e, MC_CLIENT_LOOKUP_LIMIT,
                                                      "captured Pack native hold", -1, true);
        a->pack_held = true;
    }
    return MC_CLIENT_LOOKUP_OK;
}

mc_client_texture_lookup_status mc_client_resource_lookup_open(mc_client_texture_lookup_attempt *a,
                                                               mc_texture_pack_text name,
                                                               int32_t pc) {
    if (a->pack == NULL)
        return mc_client_resource_lookup_error_at(a, &a->failure, MC_CLIENT_LOOKUP_NULL_REFERENCE,
                                                  "actual captured-null Pack invocation", pc,
                                                  false);
    if (mc_client_texture_pack_open(a->pack, name, &a->stream, &a->failure.pack) !=
        MC_TEXTURE_PACK_OK)
        return pack_failure(a, pc, -1, "actual captured Pack resource open");
    mc_client_resource_lookup_touch(a, pc);
    return MC_CLIENT_LOOKUP_OK;
}

mc_client_texture_lookup_status mc_client_resource_lookup_read(mc_client_texture_lookup_attempt *a,
                                                               int32_t pc) {
    mc_client_texture_lookup_error *e = &a->failure;
    if (texture_pack_image_read(a->stream, &a->image, &e->pack) != MC_TEXTURE_PACK_OK)
        return pack_failure(a, pc, 1, "actual private ImageIO.read call");
    a->state.image_io_returned = true;
    if (a->stream == NULL) {
        e->frames[0] = mc_client_resource_lookup_site(a, pc);
        e->frames[1] = (mc_client_graphics_site){"ji", "a", stream_to_image, 6};
        e->frame_count = 2;
        return mc_client_resource_lookup_error_at(a, e, MC_CLIENT_LOOKUP_NULL_REFERENCE,
                                                  "actual private InputStream.close receiver", pc,
                                                  false);
    }
    const mc_texture_pack_status closed = mc_client_texture_stream_close(a->stream, &e->pack);
    mc_texture_stream_state stream;
    if (mc_client_texture_stream_inspect(a->stream, &stream))
        a->state.source_closed = stream.source_closed;
    if (closed != MC_TEXTURE_PACK_OK)
        return pack_failure(a, pc, 6, "actual private InputStream.close call");
    mc_client_resource_lookup_touch(a, pc);
    return MC_CLIENT_LOOKUP_OK;
}

/* All non-BUSY actual stream retirements consume their pointer, including a
 * native close error. Source close6 is never credited by this native cleanup. */
mc_client_texture_lookup_status
mc_client_resource_lookup_cleanup(mc_client_texture_lookup_attempt *a, bool preserve_result) {
    mc_client_texture_lookup_error *e = &a->cleanup;
    if (a->stream != NULL) {
        (void)mc_client_texture_stream_inspect(a->stream, &a->state.stream_before_retirement);
        mc_texture_pack_error cause = {0};
        const mc_texture_pack_status status = mc_client_texture_stream_release(a->stream, &cause);
        if (status != MC_TEXTURE_PACK_BUSY) {
            a->stream = NULL;
            a->state.native_stream_consumed = true;
        }
        if (status != MC_TEXTURE_PACK_OK) {
            e->cleanup_pack = cause;
            e->stream_retirement = cause;
            return mc_client_resource_lookup_error_at(a, e,
                                                      status == MC_TEXTURE_PACK_BUSY
                                                          ? MC_CLIENT_LOOKUP_BUSY
                                                          : MC_CLIENT_LOOKUP_CLEANUP_FAILED,
                                                      "actual native stream retirement", -1, true);
        }
    }
    if (a->image != NULL) {
        mc_client_argb_image_release(a->image);
        a->image = NULL;
    }
    if (a->pack_held) {
        mc_texture_pack_error cause = {0};
        const mc_texture_pack_status status = mc_client_texture_pack_release(a->pack, &cause);
        if (status != MC_TEXTURE_PACK_BUSY) {
            a->pack_held = false;
            a->pack = NULL;
        }
        if (status != MC_TEXTURE_PACK_OK) {
            e->cleanup_pack = cause;
            e->pack_retirement = cause;
            return mc_client_resource_lookup_error_at(
                a, e,
                status == MC_TEXTURE_PACK_BUSY ? MC_CLIENT_LOOKUP_BUSY
                                               : MC_CLIENT_LOOKUP_CLEANUP_FAILED,
                "actual captured Pack native hold retirement", -1, true);
        }
    }
    if (a->list_held) {
        mc_texture_pack_error cause = {0};
        const mc_texture_pack_status status = mc_client_texture_pack_list_release(a->list, &cause);
        if (status != MC_TEXTURE_PACK_BUSY) {
            a->list_held = false;
            a->list = NULL;
        }
        if (status != MC_TEXTURE_PACK_OK) {
            e->cleanup_pack = cause;
            e->list_retirement = cause;
            return mc_client_resource_lookup_error_at(a, e,
                                                      status == MC_TEXTURE_PACK_BUSY
                                                          ? MC_CLIENT_LOOKUP_BUSY
                                                          : MC_CLIENT_LOOKUP_CLEANUP_FAILED,
                                                      "actual ik native hold retirement", -1, true);
        }
    }
    if (a->array != NULL &&
        !(preserve_result && a->state.color_lookup && a->state.source_returned)) {
        const mc_client_color_array_status status =
            mc_client_color_array_release(&a->array, &e->array);
        if (status != MC_CLIENT_COLOR_ARRAY_OK)
            return mc_client_resource_lookup_error_at(
                a, e,
                status == MC_CLIENT_COLOR_ARRAY_BUSY ? MC_CLIENT_LOOKUP_BUSY
                                                     : MC_CLIENT_LOOKUP_CLEANUP_FAILED,
                "actual native array occurrence retirement", -1, true);
    }
    return MC_CLIENT_LOOKUP_OK;
}

bool mc_client_resource_lookup_remaining(const mc_client_texture_lookup_attempt *a) {
    return a->stream != NULL || a->image != NULL || a->pack_held || a->list_held ||
           a->array != NULL;
}

mc_client_texture_lookup_status
mc_client_resource_lookup_enter(mc_client_render_engine *o, mc_client_texture_lookup_attempt *a) {
    a->failure.owner = mc_client_render_engine_retain(o);
    if (a->failure.owner != MC_CLIENT_RENDER_ENGINE_OK)
        return mc_client_resource_lookup_error_at(a, &a->failure, MC_CLIENT_LOOKUP_LIMIT,
                                                  "lookup native receiver call hold", -1, true);
    o->lookup->pending = a;
    o->lookup->active = a->state.active = true;
    ++o->active_resource_calls;
    ++o->classes->active_resource_calls;
    return MC_CLIENT_LOOKUP_OK;
}

mc_client_texture_lookup_status
mc_client_resource_lookup_finish(mc_client_render_engine *o, mc_client_texture_lookup_attempt *a,
                                 mc_client_texture_lookup_status status) {
    if (status == MC_CLIENT_LOOKUP_OK) {
        const mc_client_texture_lookup_status cleaned = mc_client_resource_lookup_cleanup(a, true);
        if (cleaned != MC_CLIENT_LOOKUP_OK) {
            a->failure.cleanup_pack = a->cleanup.cleanup_pack;
            a->failure.stream_retirement = a->cleanup.stream_retirement;
            a->failure.pack_retirement = a->cleanup.pack_retirement;
            a->failure.list_retirement = a->cleanup.list_retirement;
            a->failure.array = a->cleanup.array;
            status = mc_client_resource_lookup_error_at(
                a, &a->failure, cleaned, "lookup native cleanup after source return", -1, true);
        }
    }
    /* Color source performs no Display/context operation at all. */
    if (!a->state.color_lookup) {
        mc_client_graphics_error context = {0};
        if (!mc_client_render_context_unbind_empty(o->classes, &context)) {
            a->failure.cleanup_context = context;
            if (status == MC_CLIENT_LOOKUP_OK)
                status = mc_client_resource_lookup_error_at(
                    a, &a->failure, MC_CLIENT_LOOKUP_CLEANUP_FAILED,
                    "lookup native empty-context retirement", -1, true);
        }
    }
    --o->classes->active_resource_calls;
    --o->active_resource_calls;
    o->lookup->active = a->state.active = false;
    return status;
}
