#include "internal.h"

#include "../../../entity/player/tick_math_internal.h"
#include "../../application_state.h"
#include "../model_graph_internal.h"
#include "mc/client/world/entity_renderer.h"

static bool standard(mc_cv_context *c, const mc_block *b, int32_t x, int32_t y, int32_t z,
                     bool *out) {
    int32_t color;
    if (!mc_cv_block_color(c, b, x, y, z, 9, &color) || !mc_cv_fp(c, 26))
        return false;
    float red = at_fdiv((float)(((uint32_t)color >> 16u) & 255u), 255);
    float green = at_fdiv((float)(((uint32_t)color >> 8u) & 255u), 255);
    float blue = at_fdiv((float)((uint32_t)color & 255u), 255);
    if (mc_client_entity_renderer_class_active_use(c->models, NULL) != MC_CLIENT_ENTITY_RENDERER_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "actual PX defining class active use", 56, true);
    if (c->models->px_a) {
        const float r =
            at_fdiv(at_fadd(at_fadd(at_fmul(red, 30), at_fmul(green, 59)), at_fmul(blue, 11)), 100);
        const float g = at_fdiv(at_fadd(at_fmul(red, 30), at_fmul(green, 70)), 100);
        const float v = at_fdiv(at_fadd(at_fmul(red, 30), at_fmul(blue, 70)), 100);
        red = r;
        green = g;
        blue = v;
    }
    if (c->application == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "actual Minecraft defining class namespace", 128, true);
    bool ao;
    if (mc_client_application_ambient_occlusion(c->application->source_namespace, &ao,
                                                &c->application_query_error) !=
        MC_CLIENT_APPLICATION_QUERY_OK) {
        const mc_client_application_query_error *e = &c->application_query_error;
        c->failure.child = (mc_client_graphics_site){e->site.class_name, e->site.method,
                                                     e->site.descriptor, (int32_t)e->site.pc};
        return mc_cv_fail(c,
                          e->status == MC_CLIENT_APPLICATION_QUERY_SOURCE_NULL
                              ? MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE
                              : MC_CLIENT_RENDER_GLOBAL_GRAPHICS_FAILED,
                          e->operation, 128, e->native_policy);
    }
    mc_cv_pc(c, ao ? 146 : 162);
    if (!mc_cv_push(c, ao ? "a" : "b", "(Luu;IIIFFF)Z"))
        return false;
    if (ao ? !mc_source_cv_ambient_occlusion(c, b, x, y, z, red, green, blue, out)
           : !mc_cv_flat(c, b, x, y, z, red, green, blue, out))
        return false;
    mc_cv_pop(c);
    return true;
}

bool mc_cv_render(mc_cv_context *c, const mc_block *block, int32_t x, int32_t y, int32_t z,
                  bool *out) {
    if (c->failed)
        return false;
    c->returned = false;
    c->depth = 0;
    if (!mc_cv_push(c, "b", "(Luu;III)Z"))
        return false;
    if (block == NULL)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE,
                          "actual Block render-type receiver", 1, false);
    mc_block_render_description description;
    if (mc_block_render_describe(block, &description) != MC_REGISTRY_OK)
        return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_INVALID_PHASE,
                          "actual Block render-type dispatch", 1, true);
    /* Capture the virtual type before its SAME receiver's bound update. */
    const int32_t type = description.type;
    if (!mc_cv_update_bounds(c, block, x, y, z, 15))
        return false;
    bool rendered = false;
    if (type == 0) {
        mc_cv_pc(c, 29);
        if (!mc_cv_push(c, "l", "(Luu;III)Z") || !standard(c, block, x, y, z, &rendered))
            return false;
        mc_cv_pop(c);
    } else {
        static const unsigned int pc[] = {29,  78,  111, 127, 45, 143, 95,  177, 160,
                                          197, 214, 231, 248, 62, 265, 282, 300, 318};
        static const char *const method[] = {"l", "i", "c", "f", "k", "g", "j", "p", "h",
                                             "a", "o", "n", "e", "m", "q", "r", "b", "c"};
        if (type >= 1 && type <= 17) {
            c->failure.child = (mc_client_graphics_site){"cv", method[type],
                                                         type >= 16  ? "(Luu;IIIZ)Z"
                                                         : type == 9 ? "(Lpc;III)Z"
                                                                     : "(Luu;III)Z",
                                                         0};
            return mc_cv_fail(c, MC_CLIENT_RENDER_GLOBAL_UNIMPLEMENTED_DEPENDENCY,
                              "actual specialized RenderBlocks geometry child", pc[type], true);
        }
    }
    mc_cv_pc(c, type == 0 ? 32 : 323);
    c->returned = true;
    mc_cv_pop(c);
    *out = rendered;
    return true;
}
