#include "source_terrain_internal.h"

#include "../../entity/player/tick_math_internal.h"
#include "../application_state.h"
#include "../render_engine/internal.h"

#include <limits.h>
#include <string.h>

void mc_terrain_pc(mc_client_render_global_submit_call *c, unsigned int pc) {
    c->state.source_pc = pc;
    if (c->depth != 0)
        c->frames[c->depth - 1].pc = (int32_t)pc;
}

void mc_terrain_store(mc_client_render_global_submit_call *c, unsigned int pc) {
    mc_terrain_pc(c, pc);
    c->state.last_store_pc = pc;
}

bool mc_terrain_push(mc_client_render_global_submit_call *c, const char *type, const char *method,
                     const char *descriptor) {
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    if (c->depth == sizeof(c->frames) / sizeof(c->frames[0]))
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT, "N submission native frame extent",
                               c->state.source_pc, true);
    c->frames[c->depth++] = (mc_client_graphics_site){type, method, descriptor, 0};
    return true;
}

void mc_terrain_pop(mc_client_render_global_submit_call *c) {
    if (c->failure.status == MC_CLIENT_RENDER_GLOBAL_OK && c->depth != 0)
        --c->depth;
}

bool mc_terrain_fail(mc_client_render_global_submit_call *c, mc_client_render_global_status status,
                     const char *operation, unsigned int pc, bool native) {
    if (c->failure.status != MC_CLIENT_RENDER_GLOBAL_OK)
        return false;
    mc_terrain_pc(c, pc);
    const mc_client_graphics_site *f = c->depth == 0 ? NULL : &c->frames[c->depth - 1];
    mc_render_global_source_fail(&c->failure, status, operation, f == NULL ? NULL : f->class_name,
                                 f == NULL ? NULL : f->method, f == NULL ? NULL : f->descriptor, pc,
                                 native);
    return false;
}

bool mc_terrain_fp(mc_client_render_global_submit_call *c, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP,
                           "N submission reached strict floating-point prerequisite", pc, true);
}

static void child_frame(mc_client_render_global_submit_call *c, mc_client_graphics_site f) {
    if (f.class_name == NULL || c->depth == sizeof(c->frames) / sizeof(c->frames[0]))
        return;
    if (c->depth != 0) {
        const mc_client_graphics_site *old = &c->frames[c->depth - 1];
        if (old->class_name != NULL && old->method != NULL && old->descriptor != NULL &&
            f.method != NULL && f.descriptor != NULL &&
            strcmp(old->class_name, f.class_name) == 0 && strcmp(old->method, f.method) == 0 &&
            strcmp(old->descriptor, f.descriptor) == 0) {
            c->frames[c->depth - 1].pc = f.pc;
            return;
        }
    }
    c->frames[c->depth++] = f;
}

bool mc_terrain_child(mc_client_render_global_submit_call *c, bool ok, unsigned int pc) {
    if (ok)
        return true;
    const mc_client_graphics_site actual = {c->failure.source_class, c->failure.source_method,
                                            c->failure.source_descriptor,
                                            (int32_t)c->failure.source_pc};
    mc_terrain_pc(c, pc);
    child_frame(c, c->failure.ancestor);
    child_frame(c, c->failure.parent);
    child_frame(c, actual);
    if (c->failure.status == MC_CLIENT_RENDER_GLOBAL_OK)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                               "native child failure without source diagnostic", pc, true);
    return false;
}

bool mc_terrain_display(mc_client_render_global_submit_call *c, bool ok, unsigned int pc) {
    if (ok)
        return true;
    const mc_client_display_error *e = &c->failure.display;
    return mc_terrain_fail(
        c,
        e->status == MC_CLIENT_DISPLAY_BUSY             ? MC_CLIENT_RENDER_GLOBAL_BUSY
        : e->status == MC_CLIENT_DISPLAY_UNSUPPORTED_FP ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
        : e->status == MC_CLIENT_DISPLAY_UNSUPPORTED_GL
            ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
        : e->status == MC_CLIENT_DISPLAY_ALLOCATION_FAILED
            ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
        : e->status == MC_CLIENT_DISPLAY_LIMIT ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                                               : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
        e->operation == NULL ? "N Display source child" : e->operation, pc, true);
}

bool mc_terrain_graphics(mc_client_render_global_submit_call *c, bool ok, unsigned int pc) {
    if (ok)
        return true;
    const mc_client_graphics_error *e = &c->failure.graphics;
    c->failure.child = e->site;
    return mc_terrain_fail(
        c,
        e->status == MC_CLIENT_GRAPHICS_NULL_REFERENCE   ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
        : e->status == MC_CLIENT_GRAPHICS_BUSY           ? MC_CLIENT_RENDER_GLOBAL_BUSY
        : e->status == MC_CLIENT_GRAPHICS_LIMIT          ? MC_CLIENT_RENDER_GLOBAL_LIMIT
        : e->status == MC_CLIENT_GRAPHICS_UNSUPPORTED_FP ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
        : e->status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED
            ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
        : e->status == MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY
            ? MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY
            : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
        e->operation == NULL ? "N graphics source child" : e->operation, pc, e->native_policy);
}

bool mc_terrain_array(mc_client_render_global_submit_call *c, bool sorted, int32_t index,
                      unsigned int pc, mc_client_source_dk **out) {
    mc_client_render_global *n = c->state.owner;
    mc_client_source_dk **array = sorted ? n->fields.n : n->fields.o;
    const size_t length = sorted ? n->n_length : n->o_length;
    if (array == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N ChunkRenderer array", pc, false);
    if (length > INT32_MAX)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_LIMIT,
                               "N ChunkRenderer native array extent", pc, true);
    if (index < 0 || (size_t)index >= length)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_SOURCE_INDEX,
                               "actual N ChunkRenderer array index", pc, false);
    mc_client_source_dk *dk = array[index];
    if (dk != NULL) {
        if (dk->owner != n)
            return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_ARGUMENT,
                                   "N ChunkRenderer native occurrence provenance", pc, true);
    }
    *out = dk;
    return true;
}

bool mc_terrain_dk_receiver(mc_client_render_global_submit_call *c, const mc_client_source_dk *dk,
                            unsigned int pc) {
    return dk != NULL || mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                                         "actual ChunkRenderer receiver", pc, false);
}

bool mc_terrain_settings(mc_client_render_global_submit_call *c, unsigned int app_pc,
                         unsigned int settings_pc, unsigned int field_pc,
                         mc_game_settings_state *out) {
    mc_terrain_pc(c, app_pc);
    client_application *app = c->state.owner->fields.t;
    if (app == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N Settings App receiver", settings_pc, false);
    mc_game_settings *settings = app->settings;
    if (settings == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual N Settings field receiver", field_pc, false);
    if (!mc_game_settings_read_state(settings, out))
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                               "N canonical Settings storage", field_pc, true);
    mc_terrain_pc(c, field_pc);
    return true;
}

bool mc_terrain_math(mc_client_render_global_submit_call *c, unsigned int pc) {
    if (c->state.math == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                               "actual MathHelper defining namespace", pc, true);
    if (mc_math_classes_ensure(c->state.math, &c->math_error) == MC_MATH_CLASS_OK)
        return true;
    c->failure.math = c->math_error;
    c->failure.child = (mc_client_graphics_site){"in", "<clinit>", "()V", c->math_error.source_pc};
    return mc_terrain_fail(c,
                           c->math_error.status == MC_MATH_CLASS_UNSUPPORTED_FP
                               ? MC_CLIENT_RENDER_GLOBAL_UNSUPPORTED_FP
                           : c->math_error.status == MC_MATH_CLASS_LIMIT
                               ? MC_CLIENT_RENDER_GLOBAL_LIMIT
                           : c->math_error.status == MC_MATH_CLASS_ALLOCATION_FAILED
                               ? MC_CLIENT_RENDER_GLOBAL_ALLOCATION_FAILED
                               : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                           c->math_error.operation, pc, true);
}

bool mc_terrain_distance(mc_client_render_global_submit_call *c, mc_client_source_dk *dk,
                         unsigned int pc, float *out) {
    mc_terrain_pc(c, pc);
    if (!mc_terrain_dk_receiver(c, dk, pc) || !mc_terrain_push(c, "dk", "a", "(Lsn;)F"))
        return false;
    if (c->state.camera == NULL)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                               "actual DK distance camera", 1, false);
    mc_entity_snapshot snapshot;
    c->failure.entity.status = mc_entity_get_snapshot(c->state.camera, &snapshot);
    if (c->failure.entity.status != MC_ENTITY_OK)
        return mc_terrain_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                               "DK canonical camera fields", 1, true);
    if (!mc_terrain_fp(c, 9))
        return false;
    const float x = at_narrow(at_sub(snapshot.base.x, (double)dk->fields.q));
    const float y = at_narrow(at_sub(snapshot.base.y, (double)dk->fields.r));
    const float z = at_narrow(at_sub(snapshot.base.z, (double)dk->fields.s));
    *out = at_fadd(at_fadd(at_fmul(x, x), at_fmul(y, y)), at_fmul(z, z));
    mc_terrain_pc(c, 50);
    mc_terrain_pop(c);
    return true;
}
