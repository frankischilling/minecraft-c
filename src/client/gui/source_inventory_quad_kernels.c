#include "source_inventory_quad_kernels.h"

#include "../../entity/player/tick_math_internal.h"
#include "mc/core/java/java.h"

static bool fail(mc_inventory_quad_context *c, mc_client_graphics_status status,
                 const char *operation, const char *method, unsigned int pc, bool native) {
    c->source_pc = pc;
    mc_client_graphics_fail(&c->error, status, operation,
                            (mc_client_graphics_site){"ub", method, "(IIIIII)V", (int32_t)pc},
                            native);
    return false;
}

static bool null_reference(mc_inventory_quad_context *c, const char *method, unsigned int pc) {
    c->source_exception = true;
    c->exception_class = "java/lang/NullPointerException";
    return fail(c, MC_CLIENT_GRAPHICS_NULL_REFERENCE, "UB actual null source receiver", method, pc,
                false);
}

static bool fp(mc_inventory_quad_context *c, const char *method, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(c, MC_CLIENT_GRAPHICS_UNSUPPORTED_FP,
                "Inventory strict native floating environment", method, pc, true);
}

static void clear(mc_inventory_quad_context *c) {
    c->error = (mc_client_graphics_error){0};
    c->source_pc = c->vertices = 0;
    c->source_exception = false;
    c->exception_class = NULL;
}

bool mc_inventory_quad_gradient_colors_capture(mc_inventory_quad_context *c, uint32_t upper,
                                               uint32_t lower, mc_inventory_quad_colors *out) {
    if (c == NULL)
        return false;
    clear(c);
    if (out == NULL)
        return fail(c, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT, "UB native captured color output", "a",
                    0, true);
    if (!fp(c, "a", 9))
        return false;
    mc_inventory_quad_colors captured;
    /* Source order is alpha/red/green/blue for each packed argument. */
    captured.upper.alpha = at_fdiv((float)(upper >> 24), 255.0f);
    captured.upper.red = at_fdiv((float)((upper >> 16) & 255u), 255.0f);
    captured.upper.green = at_fdiv((float)((upper >> 8) & 255u), 255.0f);
    captured.upper.blue = at_fdiv((float)(upper & 255u), 255.0f);
    captured.lower.alpha = at_fdiv((float)(lower >> 24), 255.0f);
    captured.lower.red = at_fdiv((float)((lower >> 16) & 255u), 255.0f);
    captured.lower.green = at_fdiv((float)((lower >> 8) & 255u), 255.0f);
    captured.lower.blue = at_fdiv((float)(lower & 255u), 255.0f);
    *out = captured;
    c->source_pc = 114;
    return true;
}

static bool begin(mc_inventory_quad_context *c, const char *method, unsigned int pc) {
    c->source_pc = pc;
    return c->nw == NULL ? null_reference(c, method, pc)
                         : mc_client_nw_begin_quads(c->nw, &c->error);
}

bool mc_inventory_quad_gradient_entered(mc_inventory_quad_context *c,
                                        const mc_inventory_quad_colors *colors, int32_t left,
                                        int32_t top, int32_t right, int32_t bottom) {
    if (c == NULL)
        return false;
    clear(c);
    if (colors == NULL)
        return fail(c, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT, "UB native captured gradient colors",
                    "a", 0, true);
    if (!begin(c, "a", 154))
        return false;
#define NW(expr, pc)                                                                               \
    do {                                                                                           \
        c->source_pc = (pc);                                                                       \
        if (!(expr))                                                                               \
            return false;                                                                          \
    } while (0)
    NW(mc_client_nw_color_rgba_float(c->nw, colors->upper.red, colors->upper.green,
                                     colors->upper.blue, colors->upper.alpha, &c->error),
       167);
    NW(mc_client_nw_vertex(c->nw, right, top, 0, &c->error), 177);
    ++c->vertices;
    NW(mc_client_nw_vertex(c->nw, left, top, 0, &c->error), 187);
    ++c->vertices;
    NW(mc_client_nw_color_rgba_float(c->nw, colors->lower.red, colors->lower.green,
                                     colors->lower.blue, colors->lower.alpha, &c->error),
       200);
    NW(mc_client_nw_vertex(c->nw, left, bottom, 0, &c->error), 211);
    ++c->vertices;
    NW(mc_client_nw_vertex(c->nw, right, bottom, 0, &c->error), 222);
    ++c->vertices;
#undef NW
    c->source_pc = 227;
    return true;
}

bool mc_inventory_quad_tile_entered(mc_inventory_quad_context *c, const float *source_k, int32_t x,
                                    int32_t y, int32_t u, int32_t v, int32_t width,
                                    int32_t height) {
    if (c == NULL)
        return false;
    clear(c);
    if (!begin(c, "b", 15))
        return false;
    const unsigned int field_pcs[] = {30, 68, 106, 142};
    const unsigned int vertex_pcs[] = {52, 91, 128, 163};
    for (size_t i = 0; i < 4; ++i) {
        const bool right = i == 1 || i == 2;
        const bool bottom = i < 2;
        const int32_t vx = mc_java_i32_add(x, right ? width : 0);
        const int32_t vy = mc_java_i32_add(y, bottom ? height : 0);
        c->source_pc = field_pcs[i];
        if (source_k == NULL)
            return fail(c, MC_CLIENT_GRAPHICS_INVALID_ARGUMENT, "UB native live k field borrow",
                        "b", field_pcs[i], true);
        const float k = *source_k;
        /* The live field read precedes the first source f2d33/71/109/145. */
        if (!fp(c, "b", field_pcs[i] + 3))
            return false;
        const double z = at_widen(k);
        const double tu =
            at_widen(at_fmul((float)mc_java_i32_add(u, right ? width : 0), 0.00390625f));
        const double tv =
            at_widen(at_fmul((float)mc_java_i32_add(v, bottom ? height : 0), 0.00390625f));
        c->source_pc = vertex_pcs[i];
        if (!mc_client_nw_vertex_uv(c->nw, vx, vy, z, tu, tv, &c->error))
            return false;
        ++c->vertices;
    }
    c->source_pc = 168;
    return true;
}
