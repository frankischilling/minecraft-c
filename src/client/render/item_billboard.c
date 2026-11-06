#include "item_billboard.h"

#include "entity/player/tick_math_internal.h"
#include "mc/core/java/java.h"

static const char descriptor[] = "(Lhl;DDDFF)V";

static bool failure(mc_client_item_billboard_context *c, mc_client_graphics_status status,
                    const char *operation, unsigned int pc, bool native) {
    c->source_pc = pc;
    c->error = (mc_client_graphics_error){.status = status,
                                          .operation = operation,
                                          .site = {"bb", "a", descriptor, (int32_t)pc},
                                          .native_policy = native};
    return false;
}

static bool fp(mc_client_item_billboard_context *c, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           failure(c, MC_CLIENT_GRAPHICS_UNSUPPORTED_FP,
                   "Item billboard strict native floating environment", pc, true);
}

static void clear(mc_client_item_billboard_context *c) {
    c->source_pc = c->vertices = 0;
    c->source_exception = false;
    c->exception_class = NULL;
    c->error = (mc_client_graphics_error){0};
}

bool mc_client_item_billboard_uv_capture(mc_client_item_billboard_context *c, int32_t icon) {
    if (c == NULL)
        return false;
    clear(c);
    if (!fp(c, 421))
        return false;
    /* Java remainder/division truncate toward zero; both subsequent int
     * multiplications/additions wrap before the actual i2f conversions. */
    const int32_t column = mc_java_i32_mul(icon % 16, 16);
    c->uv[0] = at_fdiv((float)mc_java_i32_add(column, 0), 256.0f);
    c->uv[1] = at_fdiv((float)mc_java_i32_add(column, 16), 256.0f);
    const int32_t row = mc_java_i32_mul(icon / 16, 16);
    c->uv[2] = at_fdiv((float)mc_java_i32_add(row, 0), 256.0f);
    c->uv[3] = at_fdiv((float)mc_java_i32_add(row, 16), 256.0f);
    c->source_pc = 474;
    return true;
}

bool mc_client_item_billboard_entered(mc_client_item_billboard_context *c) {
    if (c == NULL)
        return false;
    clear(c);
    c->source_pc = 673;
    if (c->nw == NULL) {
        c->source_exception = true;
        c->exception_class = "java/lang/NullPointerException";
        return failure(c, MC_CLIENT_GRAPHICS_NULL_REFERENCE,
                       "Item billboard null captured Tessellator", 673, false);
    }
    if (!mc_client_nw_begin_quads(c->nw, &c->error))
        return false;
    c->source_pc = 681;
    if (!mc_client_nw_normal(c->nw, 0.0f, 1.0f, 0.0f, &c->error))
        return false;
    if (!fp(c, 689))
        return false;
    const float width = 1.0f, half_width = .5f, bottom = .25f;
#define VERTEX(pc, x, y, u, v)                                                                     \
    do {                                                                                           \
        c->source_pc = (pc);                                                                       \
        if (!mc_client_nw_vertex_uv(c->nw, at_widen(x), at_widen(y), 0.0, at_widen(u),             \
                                    at_widen(v), &c->error))                                       \
            return false;                                                                          \
        ++c->vertices;                                                                             \
    } while (0)
    VERTEX(703, at_fsub(0.0f, half_width), at_fsub(0.0f, bottom), c->uv[0], c->uv[3]);
    VERTEX(726, at_fsub(width, half_width), at_fsub(0.0f, bottom), c->uv[1], c->uv[3]);
    VERTEX(749, at_fsub(width, half_width), at_fsub(1.0f, bottom), c->uv[1], c->uv[2]);
    VERTEX(771, at_fsub(0.0f, half_width), at_fsub(1.0f, bottom), c->uv[0], c->uv[2]);
#undef VERTEX
    c->source_pc = 776;
    return true;
}
