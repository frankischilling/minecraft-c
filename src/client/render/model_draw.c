#include "mc/client/render/model_draw.h"
#include "../../entity/player/tick_math_internal.h"
#include "../display/camera_surface.h"
#include "../render_engine/nw.h"

#include <string.h>

static bool fail(mc_client_model_draw_error *error, const char *operation, const char *cl,
                 const char *method, unsigned int pc, bool native) {
    error->operation = operation;
    error->site = (mc_client_graphics_site){cl, method, "(F)V", (int32_t)pc};
    if (strcmp(cl, "tz") == 0)
        error->site.descriptor = "(Lnw;F)V";
    error->source_pc = pc;
    error->native_policy = native;
    return false;
}

static bool null_reference(mc_client_model_draw_error *error, const char *operation, const char *cl,
                           const char *method, unsigned int pc) {
    error->throwable = MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION;
    error->exception_class = "java/lang/NullPointerException";
    return fail(error, operation, cl, method, pc, false);
}

static void graphics_throwable(mc_client_model_draw_error *error) {
    if (error->graphics.native_policy)
        return;
    const char *name;
    switch (error->graphics.status) {
        case MC_CLIENT_GRAPHICS_NULL_REFERENCE:
            name = "java/lang/NullPointerException";
            break;
        case MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT:
            name = "java/lang/IllegalArgumentException";
            break;
        case MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE:
            name = "java/lang/IllegalStateException";
            break;
        case MC_CLIENT_GRAPHICS_ARRAY_INDEX:
            name = "java/lang/ArrayIndexOutOfBoundsException";
            break;
        case MC_CLIENT_GRAPHICS_BUFFER_OVERFLOW:
            name = "java/nio/BufferOverflowException";
            break;
        case MC_CLIENT_GRAPHICS_BUFFER_UNDERFLOW:
            name = "java/nio/BufferUnderflowException";
            break;
        default:
            error->throwable = MC_CLIENT_MODEL_DRAW_UNCLASSIFIED_THROWABLE;
            return;
    }
    error->throwable = MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION;
    error->exception_class = name;
}

static bool fp(mc_client_model_draw_error *error, const char *cl, const char *method,
               unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(error, "model draw native floating environment", cl, method, pc, true);
}

static bool gl(mc_client_model_draw_error *error, bool ok, const char *method, unsigned int pc) {
    return ok || fail(error, error->display.operation, "ps", method, pc, true);
}

static bool graphics(mc_client_model_draw_error *error, bool ok, const char *cl, const char *method,
                     unsigned int pc) {
    if (ok)
        return true;
    error->child = error->graphics.site;
    graphics_throwable(error);
    return fail(error, error->graphics.operation, cl, method, pc, error->graphics.native_policy);
}

static bool pool(mc_client_model_draw_error *error, mc_entity_classes *entities, double x, double y,
                 double z, mc_entity_vector **out, unsigned int pc, const char *method) {
    if (mc_entity_vector_pool_get(entities, x, y, z, out, &error->vector) == MC_ENTITY_OK)
        return true;
    error->child =
        (mc_client_graphics_site){"bt", method, "(Lbt;)Lbt;", strcmp(method, "b") == 0 ? 57 : 27};
    return fail(error, error->vector.operation, "tz", "a", pc, true);
}

static bool quad(mc_client_model_quad *face, mc_client_tessellator *nw, mc_entity_classes *entities,
                 mc_client_display *display, float scale, mc_client_model_draw_error *error) {
    mc_entity_vector *vectors[4] = {0};
    bool ok = false;
    if (face == NULL || face->a == NULL) {
        null_reference(error, "actual model face vertex array", "tz", "a", face == NULL ? 1u : 5u);
        goto done;
    }
    /* Genuine ps box construction gives four source vertices per face. The
     * source reverse subtraction is other - receiver, including its pooled
     * factory effects. Each lexical Vec3 hold ends after this child returns. */
    for (size_t i = 0; i < 2; ++i)
        if (face->a[i] == NULL || face->a[i]->a == NULL) {
            null_reference(error, "actual model face position receiver", "tz", "a", 18);
            goto done;
        }
    if (!fp(error, "tz", "a", 18))
        goto done;
    const mc_entity_vector *v0 = face->a[0]->a, *v1 = face->a[1]->a;
    if (!pool(error, entities, at_sub(v0->x, v1->x), at_sub(v0->y, v1->y), at_sub(v0->z, v1->z),
              &vectors[0], 18, "a"))
        goto done;
    if (face->a[2] == NULL || face->a[2]->a == NULL) {
        null_reference(error, "actual model face second position receiver", "tz", "a", 37);
        goto done;
    }
    const mc_entity_vector *v2 = face->a[2]->a;
    if (!pool(error, entities, at_sub(v2->x, v1->x), at_sub(v2->y, v1->y), at_sub(v2->z, v1->z),
              &vectors[1], 40, "a"))
        goto done;
    const mc_entity_vector *left = vectors[1], *right = vectors[0];
    if (!pool(error, entities, at_sub(at_mul(left->y, right->z), at_mul(left->z, right->y)),
              at_sub(at_mul(left->z, right->x), at_mul(left->x, right->z)),
              at_sub(at_mul(left->x, right->y), at_mul(left->y, right->x)), &vectors[2], 48, "b"))
        goto done;
    if (mc_entity_vector_normalize(entities, vectors[2], &vectors[3], &error->vector) !=
        MC_ENTITY_OK) {
        error->child = (mc_client_graphics_site){"bt", "c", "()Lbt;", error->vector.bytecode};
        fail(error, error->vector.operation, "tz", "a", 51, true);
        goto done;
    }
    if (!graphics(error, mc_client_nw_begin_quads(nw, &error->graphics), "tz", "a", 57))
        goto done;
    const float nx = at_narrow(vectors[3]->x), ny = at_narrow(vectors[3]->y),
                nz = at_narrow(vectors[3]->z);
    if (!graphics(error,
                  mc_client_nw_normal(nw, face->c ? -nx : nx, face->c ? -ny : ny,
                                      face->c ? -nz : nz, &error->graphics),
                  "tz", "a", face->c ? 89u : 114u))
        goto done;
    for (size_t i = 0; i < 4; ++i) {
        mc_client_model_vertex *vertex = face->a[i];
        if (vertex == NULL || vertex->a == NULL) {
            null_reference(error, "actual model vertex position", "tz", "a",
                           vertex == NULL ? 138u : 141u);
            goto done;
        }
        if (!fp(error, "tz", "a", 144) ||
            !graphics(error,
                      mc_client_nw_vertex_uv(nw, (double)at_fmul(at_narrow(vertex->a->x), scale),
                                             (double)at_fmul(at_narrow(vertex->a->y), scale),
                                             (double)at_fmul(at_narrow(vertex->a->z), scale),
                                             (double)vertex->b, (double)vertex->c,
                                             &error->graphics),
                      "tz", "a", 184))
            goto done;
        ++error->vertices;
    }
    ok = graphics(error, mc_client_nw_draw(nw, display, &error->graphics), "tz", "a", 194);
    if (ok)
        ++error->faces;
done:
    for (size_t i = 4; i != 0; --i)
        if (vectors[i - 1] != NULL)
            (void)mc_entity_vector_release(entities, vectors[i - 1]);
    return ok;
}

static bool compile(mc_client_model_part *part, mc_client_render_engine_classes *graphics_owner,
                    mc_entity_classes *entities, mc_client_display *display, float scale,
                    mc_client_model_draw_error *error) {
    int32_t list;
    if (!graphics(error,
                  mc_client_ge_generate_lists(graphics_owner, display, 1,
                                              (mc_client_graphics_site){"ps", "d", "(F)V", 2},
                                              &list, &error->graphics),
                  "ps", "d", 2))
        return false;
    part->o = list; /* Actual store5 survives every later failure. */
    error->owned_list = list;
    if (!gl(error, mc_client_display_lists_begin(display, part->o, &error->display), "d", 15))
        return false;
    error->pending_list = true;
    const void *identity = NULL;
    if (!mc_client_display_pending_list_identity(display, list, &identity))
        return fail(error, "model actual pending definition identity", "ps", "d", 15, true);
    error->pending_list_identity = (uintptr_t)identity;
    mc_client_tessellator *nw = NULL;
    if (!graphics(error,
                  mc_client_nw_active_use(graphics_owner,
                                          (mc_client_graphics_site){"ps", "d", "(F)V", 18}, &nw,
                                          &error->graphics),
                  "ps", "d", 18))
        return false;
    if (part->k == NULL)
        return null_reference(error, "actual model face array", "ps", "d", 29);
    for (size_t i = 0; i < 6; ++i)
        if (!quad(part->k[i], nw, entities, display, scale, error))
            return false;
    if (!gl(error, mc_client_display_lists_end(display, &error->display), "d", 50))
        return false;
    error->pending_list = false;
    error->pending_list_identity = 0;
    part->n = true; /* Actual store55, never at glNewList or a partial face. */
    error->compiled = true;
    return true;
}

static bool translate(mc_client_model_part *part, mc_client_display *display, float scale,
                      bool negative, unsigned int pc, mc_client_model_draw_error *error) {
    if (!fp(error, "ps", "a", pc))
        return false;
    return gl(error,
              mc_client_display_source_translate(
                  display, at_fmul(negative ? -part->a : part->a, scale),
                  at_fmul(negative ? -part->b : part->b, scale),
                  at_fmul(negative ? -part->c : part->c, scale), &error->display),
              "a", pc);
}

static bool draw(mc_client_model_part *part, mc_client_render_engine_classes *graphics_owner,
                 mc_entity_classes *entities, mc_client_display *display, float scale,
                 mc_client_model_draw_error *error) {
    if (part == NULL)
        return null_reference(error, "actual model part receiver", "ps", "a", 1);
    if (part->i || !part->h) {
        error->source_pc = part->i ? 7u : 15u;
        return true;
    }
    if (!part->n && !compile(part, graphics_owner, entities, display, scale, error))
        return false;
    if (part->d != 0 || part->e != 0 || part->f != 0) {
        if (!gl(error, mc_client_display_source_push(display, &error->display), "a", 55) ||
            !translate(part, display, scale, false, 76, error))
            return false;
        const float angles[3] = {part->f, part->e, part->d};
        static const unsigned int pcs[3] = {98, 120, 142};
        for (size_t i = 0; i < 3; ++i)
            if (angles[i] != 0 &&
                (!fp(error, "ps", "a", pcs[i]) ||
                 !gl(error,
                     mc_client_display_source_rotate(display, at_fmul(angles[i], 57.295776f),
                                                     i == 2 ? 1.0f : 0.0f, i == 1 ? 1.0f : 0.0f,
                                                     i == 0 ? 1.0f : 0.0f, &error->display),
                     "a", pcs[i])))
                return false;
        if (!gl(error, mc_client_display_list_call(display, part->o, &error->display), "a", 149) ||
            !gl(error, mc_client_display_source_pop(display, &error->display), "a", 152))
            return false;
    } else if (part->a != 0 || part->b != 0 || part->c != 0) {
        if (!translate(part, display, scale, false, 203, error) ||
            !gl(error, mc_client_display_list_call(display, part->o, &error->display), "a", 210) ||
            !translate(part, display, scale, true, 234, error))
            return false;
    } else if (!gl(error, mc_client_display_list_call(display, part->o, &error->display), "a", 244))
        return false;
    error->source_pc = 247;
    return true;
}

static bool abort_owned(mc_client_display *display, mc_client_model_draw_error *error,
                        mc_client_display_error *cleanup) {
    const void *identity = NULL;
    if (error->pending_list_identity == 0 ||
        !mc_client_display_pending_list_identity(display, error->owned_list, &identity) ||
        (uintptr_t)identity != error->pending_list_identity) {
        memset(cleanup, 0, sizeof(*cleanup));
        cleanup->status = MC_CLIENT_DISPLAY_BUSY;
        cleanup->operation = "Model pending definition changed; foreign right preserved";
        return false;
    }
    return mc_client_display_lists_abort(display, error->owned_list, cleanup);
}

bool mc_client_model_part_draw(mc_client_model_part *part,
                               mc_client_render_engine_classes *graphics_owner,
                               mc_entity_classes *entities, mc_client_display *display, float scale,
                               mc_client_model_draw_error *error) {
    if (error == NULL)
        return false;
    memset(error, 0, sizeof(*error));
    if (graphics_owner == NULL || entities == NULL || display == NULL)
        return fail(error, "model draw native namespaces", "ps", "a", 0, true);
    const bool ok = draw(part, graphics_owner, entities, display, scale, error);
    if (error->pending_list) {
        mc_client_display_error cleanup;
        if (abort_owned(display, error, &cleanup)) {
            error->pending_list = false;
            error->pending_list_identity = 0;
        } else
            error->graphics.cleanup_display = cleanup;
    }
    error->returned = ok;
    return ok;
}

static void part_at(mc_client_model_draw_error *error, const char *method, unsigned int pc) {
    error->site = (mc_client_graphics_site){"ps", method, "(F)V", (int32_t)pc};
    error->source_pc = pc;
}

static bool other_gate(mc_client_model_part *part, mc_client_render_engine_classes *graphics_owner,
                       mc_entity_classes *entities, mc_client_display *display, float scale,
                       const char *method, bool *early, mc_client_model_draw_error *error) {
    if (part == NULL)
        return null_reference(error, "actual model part receiver", "ps", method, 1);
    *early = part->i || !part->h;
    if (*early) {
        part_at(error, method, part->i ? 7u : 15u);
        return true;
    }
    if (!part->n) {
        error->caller = (mc_client_graphics_site){"ps", method, "(F)V", 25};
        if (!compile(part, graphics_owner, entities, display, scale, error))
            return false;
        error->caller = (mc_client_graphics_site){.pc = -1};
    }
    return true;
}

static bool other_translate(mc_client_model_part *part, mc_client_display *display, float scale,
                            const char *method, unsigned int arithmetic_pc, unsigned int call_pc,
                            mc_client_model_draw_error *error) {
    if (!fp(error, "ps", method, arithmetic_pc))
        return false;
    /* Separate Source fmul operations, evaluated X then Y then Z. */
    const float x = at_fmul(part->a, scale);
    const float y = at_fmul(part->b, scale);
    const float z = at_fmul(part->c, scale);
    return gl(error, mc_client_display_source_translate(display, x, y, z, &error->display), method,
              call_pc);
}

static bool other_rotate(float value, unsigned int axis, mc_client_display *display,
                         const char *method, unsigned int arithmetic_pc, unsigned int call_pc,
                         mc_client_model_draw_error *error) {
    if (!fp(error, "ps", method, arithmetic_pc))
        return false;
    const float angle = at_fmul(value, 57.295776f);
    return gl(error,
              mc_client_display_source_rotate(display, angle, axis == 0 ? 1.0f : 0.0f,
                                              axis == 1 ? 1.0f : 0.0f, axis == 2 ? 1.0f : 0.0f,
                                              &error->display),
              method, call_pc);
}

static bool rotated(mc_client_model_part *part, mc_client_render_engine_classes *graphics_owner,
                    mc_entity_classes *entities, mc_client_display *display, float scale,
                    mc_client_model_draw_error *error) {
    bool early = false;
    if (!other_gate(part, graphics_owner, entities, display, scale, "b", &early, error))
        return false;
    if (early)
        return true;
    if (!gl(error, mc_client_display_source_push(display, &error->display), "b", 28) ||
        !other_translate(part, display, scale, "b", 36, 49, error))
        return false;
    /* The tests and actual angle loads are distinct fresh reads. */
    if (part->e != 0.0f && !other_rotate(part->e, 1, display, "b", 67, 71, error))
        return false;
    if (part->d != 0.0f && !other_rotate(part->d, 0, display, "b", 89, 93, error))
        return false;
    if (part->f != 0.0f && !other_rotate(part->f, 2, display, "b", 111, 115, error))
        return false;
    if (!gl(error, mc_client_display_list_call(display, part->o, &error->display), "b", 122) ||
        !gl(error, mc_client_display_source_pop(display, &error->display), "b", 125))
        return false;
    part_at(error, "b", 128);
    return true;
}

static bool attachment(mc_client_model_part *part, mc_client_render_engine_classes *graphics_owner,
                       mc_entity_classes *entities, mc_client_display *display, float scale,
                       mc_client_model_draw_error *error) {
    bool early = false;
    if (!other_gate(part, graphics_owner, entities, display, scale, "c", &early, error))
        return false;
    if (early)
        return true;
    if (part->d != 0.0f || part->e != 0.0f || part->f != 0.0f) {
        if (!other_translate(part, display, scale, "c", 60, 73, error))
            return false;
        if (part->f != 0.0f && !other_rotate(part->f, 2, display, "c", 91, 95, error))
            return false;
        if (part->e != 0.0f && !other_rotate(part->e, 1, display, "c", 113, 117, error))
            return false;
        if (part->d != 0.0f && !other_rotate(part->d, 0, display, "c", 135, 139, error))
            return false;
    } else if (part->a != 0.0f || part->b != 0.0f || part->c != 0.0f) {
        if (!other_translate(part, display, scale, "c", 177, 190, error))
            return false;
    }
    part_at(error, "c", 193);
    return true;
}

static bool other_finish(bool ok, mc_client_display *display, mc_client_model_draw_error *error) {
    if (error->pending_list) {
        mc_client_display_error cleanup;
        if (abort_owned(display, error, &cleanup)) {
            error->pending_list = false;
            error->pending_list_identity = 0;
        } else
            error->graphics.cleanup_display = cleanup;
    }
    error->returned = ok;
    return ok;
}

bool mc_client_model_part_draw_rotated(mc_client_model_part *part,
                                       mc_client_render_engine_classes *graphics_owner,
                                       mc_entity_classes *entities, mc_client_display *display,
                                       float scale, mc_client_model_draw_error *error) {
    if (error == NULL)
        return false;
    memset(error, 0, sizeof(*error));
    error->caller.pc = -1;
    return other_finish(rotated(part, graphics_owner, entities, display, scale, error), display,
                        error);
}

bool mc_client_model_part_attach(mc_client_model_part *part,
                                 mc_client_render_engine_classes *graphics_owner,
                                 mc_entity_classes *entities, mc_client_display *display,
                                 float scale, mc_client_model_draw_error *error) {
    if (error == NULL)
        return false;
    memset(error, 0, sizeof(*error));
    error->caller.pc = -1;
    return other_finish(attachment(part, graphics_owner, entities, display, scale, error), display,
                        error);
}
