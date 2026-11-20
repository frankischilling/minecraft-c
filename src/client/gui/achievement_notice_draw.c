#include "achievement_notice_draw.h"
#include "../application_state.h"
#include "../display/camera_surface.h"
#include "../display/fog_surface.h"
#include "../display/world_frame_surface.h"
#include "../render/model_graph_internal.h"
#include "../render_engine/internal.h"
#include "../startup/application_owner_internal.h"
#include "mc/core/java/java.h"
#include "millis.h"
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

struct mc_client_notice_draw_call {
    mc_client_notice_draw_state state;
    mc_client_notice_draw_error failure, cleanup;
    mc_client_achievement_notice *owner;
    client_application *application;
    mc_client_render_engine_classes *graphics;
    mc_math_classes *math;
    mc_game_settings *settings;
    mc_client_bitmap_font *fonts[6];
    size_t font_count;
    mc_client_render_engine *engine;
    mc_client_tessellator *nw;
    mc_client_item_gui_call *item;
    mc_client_item_gui_error item_error, item_cleanup;
    mc_client_bitmap_font_error font_cleanup;
    bool graphics_held, graphics_lease, math_held, engine_held, lookup_owned;
};

static mc_client_graphics_site site(int32_t pc) {
    return (mc_client_graphics_site){"vu", "a", "()V", pc};
}

static mc_client_notice_draw_status admission(mc_client_notice_draw_error *out,
                                              mc_client_notice_draw_status status,
                                              const char *operation) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->status = status;
        out->operation = operation;
        out->native_policy = status != MC_CLIENT_NOTICE_DRAW_OK;
        out->site.pc = out->child.pc = -1;
        out->font.recursive_pc = -1;
    }
    return status;
}

static bool fail(mc_client_notice_draw_call *c, mc_client_notice_draw_status status,
                 const char *operation, int32_t pc, bool native) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site = site(pc);
    c->failure.native_policy = native;
    c->state.source_pc = pc;
    return false;
}

static bool null_reference(mc_client_notice_draw_call *c, const char *operation, int32_t pc) {
    c->failure.source_exception = true;
    c->failure.throwable = MC_ITEM_RENDER_SOURCE_EXCEPTION;
    c->failure.exception_class = "java/lang/NullPointerException";
    return fail(c, MC_CLIENT_NOTICE_DRAW_SOURCE_NULL, operation, pc, false);
}

static bool fp(mc_client_notice_draw_call *c, int32_t pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u || fail(c, MC_CLIENT_NOTICE_DRAW_UNSUPPORTED_FP,
                                                       "Notice strict floating controls", pc, true);
}

static bool gl(mc_client_notice_draw_call *c, bool ok, int32_t pc) {
    c->state.source_pc = pc;
    if (!ok)
        return fail(c, MC_CLIENT_NOTICE_DRAW_CHILD_FAILED, c->failure.display.operation, pc, true);
    ++c->state.gl_calls;
    return true;
}

static client_application *app(mc_client_notice_draw_call *c, int32_t pc) {
    client_application *a = c->owner->fields.a;
    if (a == NULL) {
        null_reference(c, "actual Notice null App receiver", pc);
        return NULL;
    }
    /* Association changes are not a source App replacement. Never borrow one
     * registered object and silently operate on another after a callback. */
    if (a != c->application) {
        fail(c, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice App association changed", pc, true);
        return NULL;
    }
    return a;
}

static bool hold_graphics(mc_client_notice_draw_call *c, int32_t pc) {
    if (c->graphics_held)
        return true;
    if (c->graphics == NULL || c->graphics->resource_retirement_started ||
        c->graphics->scene_lease_count == SIZE_MAX)
        return fail(c, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice graphics occurrence", pc, true);
    c->failure.graphics_owner = mc_client_render_engine_classes_retain(c->graphics);
    if (c->failure.graphics_owner != MC_CLIENT_RENDER_ENGINE_OK)
        return fail(c, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice graphics occurrence", pc, true);
    ++c->graphics->scene_lease_count;
    c->graphics_held = c->graphics_lease = true;
    return true;
}

static bool graphics(mc_client_notice_draw_call *c, bool ok, int32_t pc) {
    if (ok)
        return true;
    c->failure.child = c->failure.graphics.site;
    return fail(c, MC_CLIENT_NOTICE_DRAW_CHILD_FAILED, c->failure.graphics.operation, pc,
                c->failure.graphics.native_policy);
}

/* VU.b uses rounded QQ ints, unlike PX.b's exact double dimensions. The
 * viewport/matrix prefix and the physical b/c stores precede QQ construction. */
static bool projection(mc_client_notice_draw_call *c, int32_t caller) {
    mc_client_display *d = c->state.display;
    client_application *a = app(c, 6);
    if (a == NULL)
        goto failed;
    const int32_t width = a->d;
    a = app(c, 13);
    if (a == NULL)
        goto failed;
    const int32_t height = a->e;
    c->state.physical_width = width;
    c->state.physical_height = height;
#define PROJECTION_GL(PC, EXPR)                                                                    \
    do {                                                                                           \
        c->state.projection_pc = (PC);                                                             \
        if (!gl(c, (EXPR), (PC)))                                                                  \
            goto failed;                                                                           \
    } while (0)
    PROJECTION_GL(16, mc_client_display_source_viewport(d, width, height, &c->failure.display));
    PROJECTION_GL(22, mc_client_display_source_matrix_mode(d, 5889, &c->failure.display));
    PROJECTION_GL(25, mc_client_display_source_load_identity(d, &c->failure.display));
    PROJECTION_GL(31, mc_client_display_source_matrix_mode(d, 5888, &c->failure.display));
    PROJECTION_GL(34, mc_client_display_source_load_identity(d, &c->failure.display));
    a = app(c, 42);
    if (a == NULL)
        goto failed;
    c->owner->fields.b = a->d;
    c->state.projection_pc = 45;
    a = app(c, 53);
    if (a == NULL)
        goto failed;
    c->owner->fields.c = a->e;
    c->state.projection_pc = 56;
    a = app(c, 67);
    if (a == NULL)
        goto failed;
    mc_game_settings *settings = a->settings;
    a = app(c, 74);
    if (a == NULL)
        goto failed;
    const int32_t qw = a->d;
    a = app(c, 81);
    if (a == NULL)
        goto failed;
    const int32_t qh = a->e;
    c->state.projection_pc = 84;
    if (settings == NULL) {
        null_reference(c, "actual QQ null Settings receiver", 84);
        c->failure.child = (mc_client_graphics_site){"qq", "<init>", "(Lkv;II)V", 20};
        goto failed;
    }
    if (c->settings == NULL) {
        if (mc_game_settings_borrow(settings, &c->failure.settings) != MC_GAME_SETTINGS_OK) {
            fail(c, MC_CLIENT_NOTICE_DRAW_CHILD_FAILED, "Notice reached Settings occurrence", 84,
                 true);
            goto failed;
        }
        c->settings = settings;
    } else if (c->settings != settings) {
        fail(c, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice Settings association changed", 84, true);
        goto failed;
    }
    mc_game_settings_state options;
    if (!mc_game_settings_read_state(settings, &options) || !fp(c, 84) ||
        !mc_scaled_resolution_init(qw, qh, options.values.gui_scale, &c->state.resolution)) {
        if (c->failure.status == MC_CLIENT_NOTICE_DRAW_OK)
            fail(c, MC_CLIENT_NOTICE_DRAW_DEPENDENCY, "Notice QQ native dimensions/state", 84,
                 true);
        goto failed;
    }
    c->owner->fields.b = c->state.resolution.width;
    c->state.projection_pc = 93;
    c->owner->fields.c = c->state.resolution.height;
    c->state.projection_pc = 101;
    PROJECTION_GL(107, mc_client_display_source_clear(d, 256, &c->failure.display));
    PROJECTION_GL(113, mc_client_display_source_matrix_mode(d, 5889, &c->failure.display));
    PROJECTION_GL(116, mc_client_display_source_load_identity(d, &c->failure.display));
    PROJECTION_GL(137, mc_client_display_source_ortho(d, 0, (double)c->owner->fields.b,
                                                      (double)c->owner->fields.c, 0, 1000, 3000,
                                                      &c->failure.display));
    PROJECTION_GL(143, mc_client_display_source_matrix_mode(d, 5888, &c->failure.display));
    PROJECTION_GL(146, mc_client_display_source_load_identity(d, &c->failure.display));
    PROJECTION_GL(153, mc_client_display_source_translate(d, 0, 0, -2000, &c->failure.display));
#undef PROJECTION_GL
    c->state.projection_pc = 156;
    c->state.projection_returned = true;
    ++c->state.projection_returns;
    c->state.source_pc = caller;
    return true;
failed:
    /* Child belongs to VU.b unless a deeper QQ child has already been stored. */
    if (c->failure.child.class_name == NULL)
        c->failure.child = (mc_client_graphics_site){"vu", "b", "()V", c->failure.site.pc};
    c->state.projection_pc = c->failure.site.pc;
    c->failure.site = site(caller);
    c->state.source_pc = caller;
    return false;
}

static mc_client_bitmap_font *font(mc_client_notice_draw_call *c, int32_t get, int32_t invoke) {
    client_application *a = app(c, get);
    if (a == NULL)
        return NULL;
    mc_client_bitmap_font *f = a->font_renderer;
    if (f == NULL) {
        null_reference(c, "actual Notice null Font receiver", invoke);
        return NULL;
    }
    for (size_t i = 0; i < c->font_count; ++i)
        if (c->fonts[i] == f)
            return f;
    if (c->font_count == sizeof(c->fonts) / sizeof(*c->fonts)) {
        fail(c, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice reached Font occurrence", invoke, true);
        return NULL;
    }
    c->failure.font_owner = mc_client_bitmap_font_retain(f);
    if (c->failure.font_owner != MC_CLIENT_BITMAP_FONT_OK) {
        fail(c, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice reached Font occurrence", invoke, true);
        return NULL;
    }
    c->fonts[c->font_count++] = f;
    return f;
}

static bool text_result(mc_client_notice_draw_call *c, mc_client_bitmap_font_status status,
                        int32_t pc) {
    if (status == MC_CLIENT_BITMAP_FONT_OK) {
        ++c->state.text_calls;
        c->state.source_pc = pc;
        return true;
    }
    c->failure.child =
        (mc_client_graphics_site){"sj", c->failure.font.font.method,
                                  c->failure.font.font.descriptor, c->failure.font.font.pc};
    if (!c->failure.font.font.native_policy) {
        switch (status) {
            case MC_CLIENT_BITMAP_FONT_NULL_REFERENCE:
                c->failure.exception_class = "java/lang/NullPointerException";
                break;
            case MC_CLIENT_BITMAP_FONT_ARRAY_INDEX:
                c->failure.exception_class = "java/lang/ArrayIndexOutOfBoundsException";
                break;
            case MC_CLIENT_BITMAP_FONT_STRING_INDEX:
                c->failure.exception_class = "java/lang/StringIndexOutOfBoundsException";
                break;
            case MC_CLIENT_BITMAP_FONT_SOURCE_ILLEGAL_ARGUMENT:
                c->failure.exception_class = "java/lang/IllegalArgumentException";
                break;
            case MC_CLIENT_BITMAP_FONT_SOURCE_RUNTIME_EXCEPTION:
                c->failure.exception_class = "java/lang/RuntimeException";
                break;
            default:
                break;
        }
        c->failure.source_exception = c->failure.exception_class != NULL;
        if (c->failure.source_exception)
            c->failure.throwable = MC_ITEM_RENDER_SOURCE_EXCEPTION;
    }
    return fail(c, MC_CLIENT_NOTICE_DRAW_CHILD_FAILED, c->failure.font.font.operation, pc,
                c->failure.font.font.native_policy);
}

static bool literal(mc_client_notice_draw_call *c, const char *s, int32_t get, int32_t pc,
                    int32_t y) {
    mc_client_bitmap_font *f = font(c, get, pc);
    if (f == NULL)
        return false;
    uint16_t units[64];
    const size_t length = strlen(s);
    if (length > sizeof(units) / sizeof(*units))
        return fail(c, MC_CLIENT_NOTICE_DRAW_DEPENDENCY, "Notice source literal bound", pc, true);
    for (size_t i = 0; i < length; ++i)
        units[i] = (unsigned char)s[i];
    const mc_player_string_view text = {units, length};
    return text_result(
        c, mc_client_bitmap_font_draw_shadow_view(f, &text, 2, y, 0xffffffu, &c->failure.font.font),
        pc);
}

static bool caption(mc_client_notice_draw_call *c, bool wrapped, bool title, int32_t get,
                    int32_t pc, int32_t y, uint32_t color) {
    mc_client_bitmap_font *f = font(c, get, pc);
    if (f == NULL)
        return false;
    const mc_registry_string *s = title ? c->owner->fields.d : c->owner->fields.e;
    mc_registry_string_view v;
    mc_player_string_view text;
    const mc_player_string_view *nullable = NULL;
    if (s != NULL) {
        if (!mc_registry_string_read(s, &v))
            return fail(c, MC_CLIENT_NOTICE_DRAW_DEPENDENCY, "Notice actual Registry String", pc,
                        true);
        text = (mc_player_string_view){v.units, v.length};
        nullable = &text;
    }
    const int32_t x = mc_java_i32_add(c->state.x, 30);
    if (wrapped)
        return text_result(c,
                           mc_client_bitmap_font_draw_wrapped_view(f, nullable, x, y, 120, color,
                                                                   &c->failure.font),
                           pc);
    const mc_client_bitmap_font_status status =
        mc_client_bitmap_font_draw_view(f, nullable, x, y, color, false, &c->failure.font.font);
    if (status != MC_CLIENT_BITMAP_FONT_OK) {
        /* Actual SJ.b wrapper's sole child is a(String,IIIZ) at7. */
        c->failure.font.font.parent_method = "b";
        c->failure.font.font.parent_descriptor = "(Ljava/lang/String;III)V";
        c->failure.font.font.parent_pc = 7;
    }
    return text_result(c, status, pc);
}

static bool texture(mc_client_notice_draw_call *c) {
    client_application *a = app(c, 239);
    if (a == NULL)
        return false;
    mc_client_render_engine *e = a->render_engine;
    if (e == NULL)
        return null_reference(c, "actual Notice null Engine lookup receiver", 244);
    if (e->classes != c->graphics)
        return fail(c, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice same-namespace Engine occurrence", 244,
                    true);
    c->failure.engine_owner = mc_client_render_engine_retain(e);
    if (c->failure.engine_owner != MC_CLIENT_RENDER_ENGINE_OK)
        return fail(c, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice Engine occurrence", 244, true);
    c->engine = e;
    c->engine_held = true;
    static const uint16_t name[] = {'/', 'a', 'c', 'h', 'i', 'e', 'v', 'e', 'm', 'e',
                                    'n', 't', '/', 'b', 'g', '.', 'p', 'n', 'g'};
    const mc_texture_pack_text key = {name, sizeof(name) / sizeof(*name), false};
    bool found = false;
    mc_client_texture prior_id;
    (void)mc_client_render_engine_cached_texture(e, key, &found, &prior_id);
    const bool foreign = mc_client_texture_lookup_pending(e);
    mc_client_texture_lookup_status status = mc_client_render_engine_texture(
        e, c->state.display, NULL, key, &c->state.texture_id, &c->failure.texture);
    c->lookup_owned = c->state.pending_lookup_owned =
        !foreign && mc_client_texture_lookup_pending(e);
    if (status != MC_CLIENT_LOOKUP_OK) {
        c->failure.child = c->failure.texture.site;
        return fail(c, MC_CLIENT_NOTICE_DRAW_CHILD_FAILED, c->failure.texture.operation, 244,
                    c->failure.texture.native_policy);
    }
    c->state.texture_lookup_returned = true;
    c->state.texture_cache_hit = found;
    return true;
}

static bool background(mc_client_notice_draw_call *c) {
    if (!hold_graphics(c, 291))
        return false;
    c->state.background_pc = 8;
    mc_client_graphics_site caller = {"ub", "b", "(IIIIII)V", 8};
    if (!graphics(c, mc_client_nw_active_use(c->graphics, caller, &c->nw, &c->failure.graphics),
                  291))
        return false;
    c->state.background_pc = 15;
    if (!graphics(c, mc_client_nw_begin_quads(c->nw, &c->failure.graphics), 291))
        return false;
    static const int32_t pc[4] = {52, 91, 128, 163};
    for (size_t i = 0; i < 4; ++i) {
        if (!fp(c, 291))
            return false;
        const int32_t u = mc_java_i32_add(96, i == 1 || i == 2 ? 160 : 0);
        const int32_t v = mc_java_i32_add(202, i < 2 ? 32 : 0);
        volatile float uf = (float)u * 0.00390625f;
        volatile float vf = (float)v * 0.00390625f;
        double *p = c->state.vertices[i];
        p[0] = (double)mc_java_i32_add(c->state.x, i == 1 || i == 2 ? 160 : 0);
        p[1] = (double)mc_java_i32_add(c->state.y, i < 2 ? 32 : 0);
        p[2] = (double)c->owner->fields.k;
        p[3] = (double)uf;
        p[4] = (double)vf;
        c->state.background_pc = pc[i];
        if (!graphics(
                c,
                mc_client_nw_vertex_uv(c->nw, p[0], p[1], p[2], p[3], p[4], &c->failure.graphics),
                291))
            return false;
        ++c->state.background_vertices;
    }
    c->state.background_pc = 168;
    if (!graphics(c, mc_client_nw_draw(c->nw, c->state.display, &c->failure.graphics), 291))
        return false;
    c->state.background_pc = 171;
    c->state.background_drawn = true;
    return true;
}

static bool body(mc_client_notice_draw_call *c) {
    mc_client_achievement_notice *p = c->owner;
    client_application *a = p->fields.a;
    if (a == NULL || !a->source_namespace_borrowed || a->source_namespace == NULL ||
        mc_client_application_render_borrow(a, &c->failure.application) !=
            MC_CLIENT_APPLICATION_OWNER_OK)
        return fail(c, MC_CLIENT_NOTICE_DRAW_DEPENDENCY, "Notice real bound App namespace", 0,
                    true);
    c->application = a;
    c->state.application = a;
    mc_client_application_classes *classes = a->source_namespace;
    if (mtx_trylock(&classes->mutex) != thrd_success)
        return fail(c, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice actual App class monitor", 0, true);
    if (classes->phase != MC_CLIENT_APPLICATION_CLASS_READY || classes->native_retiring) {
        (void)mtx_unlock(&classes->mutex);
        return fail(c, MC_CLIENT_NOTICE_DRAW_DEPENDENCY, "Notice actual App class phase", 0, true);
    }
    c->state.watermark_value = classes->H;
    (void)mtx_unlock(&classes->mutex);
    mc_client_display *d = c->state.display;
#define GL(PC, EXPR)                                                                               \
    do {                                                                                           \
        if (!gl(c, (EXPR), (PC)))                                                                  \
            return false;                                                                          \
    } while (0)
    if (c->state.watermark_value > 0) {
        c->state.watermark = true;
        GL(11, mc_client_display_source_depth_test(d, false, &c->failure.display));
        GL(15, mc_client_display_source_depth_mask(d, false, &c->failure.display));
        if (!hold_graphics(c, 18) ||
            !graphics(c, mc_client_lighting_disable(c->graphics, d, site(18), &c->failure.graphics),
                      18) ||
            !projection(c, 22) ||
            !literal(c, "Minecraft Beta 1.7.3   Unlicensed Copy :(", 38, 46, 2) ||
            !literal(c, "(Or logged in from another location)", 53, 62, 11) ||
            !literal(c, "Purchase at minecraft.net", 69, 78, 20))
            return false;
        GL(82, mc_client_display_source_depth_mask(d, true, &c->failure.display));
        GL(88, mc_client_display_source_depth_test(d, true, &c->failure.display));
    }
    if (p->fields.f == NULL || p->fields.g == 0) {
        c->state.inactive = c->state.returned = true;
        c->state.source_pc = 107;
        return true;
    }
    c->failure.clock = mc_platform_current_time_millis(&c->state.now);
    if (c->failure.clock != MC_PLATFORM_MILLIS_OK)
        return fail(c, MC_CLIENT_NOTICE_DRAW_CLOCK, "actual System.currentTimeMillis", 108, true);
    c->state.clock_read = true;
    c->state.g = p->fields.g;
    if (!fp(c, 116))
        return false;
    volatile double elapsed = (double)mc_java_i64_sub(c->state.now, c->state.g) / 3000.0;
    c->state.elapsed = elapsed;
    if (!p->fields.i && (elapsed < 0 || elapsed > 1)) {
        p->fields.g = 0;
        c->state.expired = c->state.returned = true;
        c->state.source_pc = 156;
        return true;
    }
    if (!projection(c, 158))
        return false;
    GL(164, mc_client_display_source_depth_test(d, false, &c->failure.display));
    GL(168, mc_client_display_source_depth_mask(d, false, &c->failure.display));
    if (!fp(c, 175))
        return false;
    volatile double easing = elapsed * 2.0;
    if (easing > 1.0)
        easing = 2.0 - easing;
    easing = easing * 4.0;
    easing = 1.0 - easing;
    if (easing < 0.0)
        easing = 0.0;
    easing = easing * easing;
    easing = easing * easing;
    c->state.easing = easing;
    c->state.x = mc_java_i32_sub(p->fields.b, 160);
    volatile double offset = easing * 36.0;
    c->state.y = mc_java_i32_neg(mc_java_double_to_i32(offset));
    if (!texture(c))
        return false;
    GL(253, mc_client_display_source_color(d, 1, 1, 1, 1, &c->failure.display));
    GL(259, mc_client_display_source_texture_enable(d, true, &c->failure.display));
    GL(267, mc_client_display_source_bind_id(d, c->state.texture_id, &c->failure.display));
    GL(273, mc_client_display_source_lighting_enable(d, false, &c->failure.display));
    if (!background(c))
        return false;
    c->state.tutorial = p->fields.i;
    if (p->fields.i) {
        if (!caption(c, true, false, 305, 325, mc_java_i32_add(c->state.y, 7), UINT32_MAX))
            return false;
    } else if (!caption(c, false, true, 335, 355, mc_java_i32_add(c->state.y, 7),
                        UINT32_C(0xffffff00)) ||
               !caption(c, false, false, 362, 380, mc_java_i32_add(c->state.y, 18), UINT32_MAX))
        return false;
    GL(383, mc_client_display_source_push(d, &c->failure.display));
    GL(391, mc_client_display_source_rotate(d, 180, 1, 0, 0, &c->failure.display));
    if (!hold_graphics(c, 394))
        return false;
    if (mc_math_classes_retain(c->math, &c->failure.math) != MC_MATH_CLASS_OK)
        return fail(c, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice reached Math occurrence", 394, true);
    c->math_held = true;
    if (!mc_client_lighting_enable(c->graphics, p->models->entities, c->math, d, site(394),
                                   &c->failure.lighting)) {
        c->failure.child = c->failure.lighting.site;
        return fail(c, MC_CLIENT_NOTICE_DRAW_CHILD_FAILED, c->failure.lighting.operation, 394,
                    c->failure.lighting.native_policy);
    }
    GL(397, mc_client_display_source_pop(d, &c->failure.display));
    GL(403, mc_client_display_source_lighting_enable(d, false, &c->failure.display));
    GL(408, mc_client_display_source_rescale_normal(d, true, &c->failure.display));
    GL(414, mc_client_display_source_color_material_enable(d, true, &c->failure.display));
    GL(420, mc_client_display_source_lighting_enable(d, true, &c->failure.display));
    mc_client_item_renderer *renderer = p->fields.h;
    a = app(c, 431);
    if (a == NULL)
        return false;
    mc_client_bitmap_font *unused = a->font_renderer;
    a = app(c, 438);
    if (a == NULL)
        return false;
    mc_client_render_engine *engine = a->render_engine;
    const mc_statistic *achievement = p->fields.f;
    if (achievement == NULL)
        return null_reference(c, "actual Notice null Achievement icon field", 445);
    mc_item_stack *stack = (mc_item_stack *)mc_statistic_achievement_fields_get(achievement)->d;
    if (renderer == NULL)
        return null_reference(c, "actual Notice null ItemRenderer invocation", 458);
    mc_client_item_gui_status status = mc_client_item_gui_begin(
        renderer, c->graphics, p->registry, d, unused, engine, stack,
        mc_java_i32_add(c->state.x, 8), mc_java_i32_add(c->state.y, 8), &c->item, &c->item_error);
    if (c->item != NULL)
        (void)mc_client_item_gui_inspect(c->item, &c->state.item);
    if (status != MC_CLIENT_ITEM_GUI_OK) {
        c->failure.item = &c->item_error;
        c->failure.child = c->item_error.site;
        c->failure.source_exception = c->item_error.source_exception;
        c->failure.throwable = c->item_error.throwable;
        c->failure.exception_class = c->item_error.exception_class;
        return fail(c, MC_CLIENT_NOTICE_DRAW_CHILD_FAILED, c->item_error.operation, 458,
                    c->item_error.native_policy);
    }
    c->state.item_returned = true;
    GL(464, mc_client_display_source_lighting_enable(d, false, &c->failure.display));
    GL(468, mc_client_display_source_depth_mask(d, true, &c->failure.display));
    GL(474, mc_client_display_source_depth_test(d, true, &c->failure.display));
#undef GL
    c->state.source_pc = 477;
    c->state.returned = true;
    return true;
}

mc_client_notice_draw_status mc_client_notice_draw_begin(mc_client_achievement_notice *owner,
                                                         mc_client_render_engine_classes *graphics,
                                                         mc_math_classes *math,
                                                         mc_client_display *display,
                                                         mc_client_notice_draw_call **out,
                                                         mc_client_notice_draw_error *error) {
    if (owner == NULL || out == NULL || *out != NULL || error == NULL)
        return admission(error, MC_CLIENT_NOTICE_DRAW_ARGUMENT, "Notice native owner/output");
    if (owner->phase != MC_CLIENT_MODEL_READY || owner->draw_active || owner->draw_call != NULL)
        return admission(error, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice active/retained source call");
    mc_client_notice_draw_call *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return admission(error, MC_CLIENT_NOTICE_DRAW_ALLOCATION, "Notice retained draw storage");
    admission(&c->failure, MC_CLIENT_NOTICE_DRAW_OK, NULL);
    c->owner = owner;
    c->graphics = graphics;
    c->math = math;
    c->state = (mc_client_notice_draw_state){.owner = owner,
                                             .graphics = graphics,
                                             .display = display,
                                             .active = true,
                                             .source_pc = 0,
                                             .projection_pc = -1,
                                             .background_pc = -1,
                                             .failure = &c->failure,
                                             .cleanup = &c->cleanup};
    owner->draw_active = true;
    owner->draw_call = c;
    *out = c;
    (void)body(c);
    owner->draw_active = c->state.active = false;
    *error = c->failure;
    return error->status;
}

bool mc_client_notice_draw_inspect(const mc_client_notice_draw_call *c,
                                   mc_client_notice_draw_state *out) {
    if (c == NULL || out == NULL)
        return false;
    *out = c->state;
    return true;
}

mc_client_notice_draw_status mc_client_notice_draw_retire(mc_client_notice_draw_call **slot,
                                                          mc_client_notice_draw_error *error) {
    if (slot == NULL || error == NULL)
        return admission(error, MC_CLIENT_NOTICE_DRAW_ARGUMENT, "Notice retirement slot");
    mc_client_notice_draw_call *c = *slot;
    if (c == NULL)
        return admission(error, MC_CLIENT_NOTICE_DRAW_OK, NULL);
    if (c->state.active || c->owner->draw_active || c->owner->draw_call != c)
        return admission(error, MC_CLIENT_NOTICE_DRAW_BUSY, "Notice active/changed retirement");
    c->state.retiring = true;
    c->owner->draw_active = c->state.active = true;
    if (mc_client_item_gui_retire(&c->item, &c->item_cleanup) != MC_CLIENT_ITEM_GUI_OK) {
        c->cleanup.item = &c->item_cleanup;
        goto refused;
    }
    if (c->lookup_owned) {
        mc_client_texture_lookup_status status =
            mc_client_render_engine_retire_lookup(c->engine, &c->cleanup.texture);
        c->lookup_owned = c->state.pending_lookup_owned =
            mc_client_texture_lookup_pending(c->engine);
        if (status != MC_CLIENT_LOOKUP_OK)
            goto refused;
    }
    if (c->engine_held) {
        c->cleanup.engine_owner = mc_client_render_engine_release(c->engine);
        if (c->cleanup.engine_owner != MC_CLIENT_RENDER_ENGINE_OK)
            goto refused;
        c->engine_held = false;
    }
    while (c->font_count != 0) {
        if (mc_client_bitmap_font_release(&c->fonts[c->font_count - 1], &c->font_cleanup) !=
            MC_CLIENT_BITMAP_FONT_OK) {
            c->cleanup.font.font = c->font_cleanup;
            goto refused;
        }
        --c->font_count;
    }
    if (c->settings != NULL &&
        mc_game_settings_release_borrow(&c->settings, &c->cleanup.settings) != MC_GAME_SETTINGS_OK)
        goto refused;
    if (c->math_held) {
        if (mc_math_classes_release(&c->math, &c->cleanup.math) != MC_MATH_CLASS_OK)
            goto refused;
        c->math_held = false;
    }
    if (c->graphics_lease) {
        if (c->graphics->scene_lease_count == 0)
            goto refused;
        --c->graphics->scene_lease_count;
        c->graphics_lease = false;
        c->nw = NULL;
    }
    if (c->graphics_held) {
        c->cleanup.graphics_owner = mc_client_render_engine_classes_release(c->graphics);
        if (c->cleanup.graphics_owner != MC_CLIENT_RENDER_ENGINE_OK)
            goto refused;
        c->graphics_held = false;
    }
    if (c->application != NULL &&
        mc_client_application_render_release(&c->application, &c->cleanup.application) !=
            MC_CLIENT_APPLICATION_OWNER_OK)
        goto refused;
    c->owner->draw_call = NULL;
    c->owner->draw_active = false;
    free(c);
    *slot = NULL;
    return admission(error, MC_CLIENT_NOTICE_DRAW_OK, NULL);
refused:
    c->owner->draw_active = c->state.active = false;
    c->cleanup.status = MC_CLIENT_NOTICE_DRAW_CLEANUP_FAILED;
    c->cleanup.operation = "Notice checked remaining native retirement";
    c->cleanup.native_policy = true;
    *error = c->cleanup;
    return error->status;
}
