#include "source_sign_drawing_internal.h"

#include "block/registry/runtime_internal.h"
#include "client/render/tile_entity_renderer/internal.h"
#include "entity/render/internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/client/render/model_draw.h"

#include <stdlib.h>
#include <string.h>

static const char descriptor[] = "(IIF)V";

static void at(mc_client_sign_draw_call *c, unsigned int pc) {
    c->state.source_pc = pc;
    c->error.caller = (mc_client_screen_site){"yc", "a", descriptor, pc};
}

bool mc_sign_draw_fail(mc_client_sign_draw_call *c, mc_client_screen_status status,
                       const char *operation, const char *owner, const char *method,
                       const char *desc, unsigned int pc, bool native) {
    c->error.screen = (mc_client_screen_error){.status = status,
                                               .operation = operation,
                                               .site = {owner, method, desc, pc},
                                               .native_policy = native};
    if (!native && status == MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE) {
        c->error.screen.source_exception = true;
        c->error.screen.exception_class = "java/lang/NullPointerException";
    }
    c->error.screen.parent_site = c->error.caller;
    return false;
}

bool mc_sign_draw_gl(mc_client_sign_draw_call *c, bool ok, const char *owner, const char *method,
                     const char *desc, unsigned int pc) {
    return ok || mc_sign_draw_fail(c, MC_CLIENT_SCREEN_TEXTURE_FAILED, c->error.display.operation,
                                   owner, method, desc, pc, true);
}

static const char *graphics_exception_class(mc_client_graphics_status status) {
    switch (status) {
        case MC_CLIENT_GRAPHICS_NULL_REFERENCE:
            return "java/lang/NullPointerException";
        case MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT:
            return "java/lang/IllegalArgumentException";
        case MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE:
            return "java/lang/IllegalStateException";
        case MC_CLIENT_GRAPHICS_ARRAY_INDEX:
            return "java/lang/ArrayIndexOutOfBoundsException";
        case MC_CLIENT_GRAPHICS_BUFFER_OVERFLOW:
            return "java/nio/BufferOverflowException";
        case MC_CLIENT_GRAPHICS_BUFFER_UNDERFLOW:
            return "java/nio/BufferUnderflowException";
        default:
            return NULL;
    }
}

static void graphics_throwable(mc_client_screen_error *out, const mc_client_graphics_error *e) {
    out->source_exception = false;
    out->source_error = false;
    out->exception_class = NULL;
    if (e->native_policy)
        return;
    if (e->status == MC_CLIENT_GRAPHICS_CLASS_FAILED) {
        const mc_client_graphics_cause *cause = &e->class_cause;
        if (cause->native_policy)
            return;
        if (cause->status == MC_CLIENT_GRAPHICS_CLASS_FAILED)
            cause = &e->dependency_class_cause;
        /* A cached native/unknown failure is not a fabricated Java Error.
         * Keep both complete producer cause records even when unclassified. */
        if (!cause->native_policy && graphics_exception_class(cause->status) != NULL) {
            out->source_error = true;
            out->exception_class = "java/lang/NoClassDefFoundError";
        }
        return;
    }
    const char *name = graphics_exception_class(e->status);
    if (name == NULL)
        return;
    if ((e->parent.method != NULL && strcmp(e->parent.method, "<clinit>") == 0) ||
        (e->site.method != NULL && strcmp(e->site.method, "<clinit>") == 0)) {
        out->source_error = true;
        out->exception_class = "java/lang/ExceptionInInitializerError";
    } else {
        out->source_exception = true;
        out->exception_class = name;
    }
}

bool mc_sign_draw_nw(mc_client_sign_draw_call *c, bool ok, const char *method, const char *desc,
                     unsigned int pc) {
    if (ok)
        return true;
    mc_client_graphics_error *e = &c->error.graphics;
    mc_sign_draw_fail(c,
                      e->status == MC_CLIENT_GRAPHICS_NULL_REFERENCE && !e->native_policy
                          ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                          : MC_CLIENT_SCREEN_TEXTURE_FAILED,
                      e->operation, e->site.class_name, e->site.method, e->site.descriptor,
                      e->site.pc < 0 ? 0u : (unsigned int)e->site.pc, e->native_policy);
    graphics_throwable(&c->error.screen, e);
    c->error.helper = (mc_client_screen_site){"ub", method, desc, pc};
    return false;
}

bool mc_sign_draw_fp(mc_client_sign_draw_call *c, const char *owner, const char *method,
                     const char *desc, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           mc_sign_draw_fail(c, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                             "YC reached strict native floating environment", owner, method, desc,
                             pc, true);
}

void *mc_sign_draw_allocate(mc_client_sign_draw_call *c, size_t bytes, const char *operation,
                            unsigned int pc) {
    if (c->state.allocation_bytes > c->limits.max_bytes ||
        bytes > c->limits.max_bytes - c->state.allocation_bytes) {
        mc_sign_draw_fail(c, MC_CLIENT_SCREEN_LIMIT, operation, "yc", "a", descriptor, pc, true);
        return NULL;
    }
    void *value = calloc(1, bytes);
    if (value == NULL)
        mc_sign_draw_fail(c, MC_CLIENT_SCREEN_ALLOCATION_FAILED, operation, "yc", "a", descriptor,
                          pc, true);
    else
        c->state.allocation_bytes += bytes;
    return value;
}

bool mc_sign_draw_base(mc_client_sign_draw_call *c, mc_client_screen_state *out) {
    return mc_client_screen_inspect(c->screen, out, &c->error.screen) == MC_CLIENT_SCREEN_OK;
}

bool mc_sign_draw_owner(mc_client_sign_draw_call *c, mc_client_sign_screen_state *out) {
    return mc_client_screen_sign_inspect(c->owner, out, &c->error.screen) == MC_CLIENT_SCREEN_OK;
}

static void font_throwable(mc_client_screen_error *out, const mc_client_bitmap_font_error *e) {
    out->source_exception = false;
    out->source_error = false;
    out->exception_class = NULL;
    if (!e->native_policy) {
        out->exception_class = e->status == MC_CLIENT_BITMAP_FONT_NULL_REFERENCE
                                   ? "java/lang/NullPointerException"
                               : e->status == MC_CLIENT_BITMAP_FONT_ARRAY_INDEX
                                   ? "java/lang/ArrayIndexOutOfBoundsException"
                               : e->status == MC_CLIENT_BITMAP_FONT_STRING_INDEX
                                   ? "java/lang/StringIndexOutOfBoundsException"
                               : e->status == MC_CLIENT_BITMAP_FONT_SOURCE_ILLEGAL_ARGUMENT
                                   ? "java/lang/IllegalArgumentException"
                               : e->status == MC_CLIENT_BITMAP_FONT_SOURCE_RUNTIME_EXCEPTION
                                   ? "java/lang/RuntimeException"
                                   : NULL;
        out->source_exception = out->exception_class != NULL;
        if (e->status == MC_CLIENT_BITMAP_FONT_RENDER_FAILED)
            graphics_throwable(out, &e->graphics);
    }
}

static bool font_result(mc_client_sign_draw_call *c, mc_client_bitmap_font_status status,
                        mc_client_screen_site helper) {
    if (status == MC_CLIENT_BITMAP_FONT_OK)
        return true;
    mc_client_bitmap_font_error *e = &c->error.font;
    mc_sign_draw_fail(c, MC_CLIENT_SCREEN_FONT_FAILED, e->operation, "sj", e->method, e->descriptor,
                      e->pc < 0 ? 0u : (unsigned int)e->pc, e->native_policy);
    c->error.helper = helper;
    font_throwable(&c->error.screen, e);
    return false;
}

static bool font_hold(mc_client_sign_draw_call *c, mc_client_bitmap_font *font,
                      mc_client_screen_site helper) {
    for (mc_sign_draw_font *f = c->fonts; f != NULL; f = f->next)
        if (f->font == font)
            return true;
    if (c->font_count == c->limits.max_fonts)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_LIMIT, "YC reached Font occurrence bound",
                                 helper.class_name, helper.method, helper.descriptor, helper.pc,
                                 true);
    mc_sign_draw_font *f =
        mc_sign_draw_allocate(c, sizeof(*f), "YC Font custody allocation", c->state.source_pc);
    if (f == NULL)
        return false;
    c->error.font.status = mc_client_bitmap_font_retain(font);
    if (c->error.font.status != MC_CLIENT_BITMAP_FONT_OK) {
        free(f);
        c->state.allocation_bytes -= sizeof(*f);
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_FONT_FAILED, "YC actual Font native hold",
                                 helper.class_name, helper.method, helper.descriptor, helper.pc,
                                 true);
    }
    f->font = font;
    f->next = c->fonts;
    c->fonts = f;
    ++c->font_count;
    return true;
}

bool mc_sign_draw_centered(mc_client_sign_draw_call *c, mc_client_bitmap_font *font,
                           const mc_player_string_view *text, int32_t x, int32_t y, uint32_t color,
                           mc_client_screen_site caller, int32_t *width_out, int32_t *x_out) {
    static const char center_desc[] = "(Lsj;Ljava/lang/String;III)V";
    c->error.helper = caller;
    if (font == NULL)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                 "UB actual centered Font measure null receiver", "ub", "a",
                                 center_desc, 5, false);
    const mc_client_screen_site measure = {"ub", "a", center_desc, 5};
    if (!font_hold(c, font, measure))
        return false;
    int32_t width;
    if (!font_result(c, mc_client_bitmap_font_measure_view(font, text, &width, &c->error.font),
                     measure))
        return false;
    ++c->state.font_measurements;
    const int32_t centered = mc_java_i32_sub(x, width / 2);
    if (width_out != NULL)
        *width_out = width;
    if (x_out != NULL)
        *x_out = centered;
    if (!font_result(
            c,
            mc_client_bitmap_font_draw_shadow_view(font, text, centered, y, color, &c->error.font),
            (mc_client_screen_site){"ub", "a", center_desc, 15}))
        return false;
    ++c->state.font_draws;
    return true;
}

static bool registry_result(mc_client_sign_draw_call *c, mc_registry_status status) {
    if (status == MC_REGISTRY_OK)
        return true;
    mc_entity_render_throwable throwable = MC_ENTITY_RENDER_NO_THROWABLE;
    const char *exception = NULL;
    mc_entity_render_registry_classification(c->services.registry, &c->error.registry, &throwable,
                                             &exception);
    mc_sign_draw_fail(c, MC_CLIENT_SCREEN_REGISTRY_FAILED, c->error.registry.message, "yc", "a",
                      descriptor, c->state.source_pc, throwable == MC_ENTITY_RENDER_NO_THROWABLE);
    c->error.screen.source_exception = throwable == MC_ENTITY_RENDER_SOURCE_EXCEPTION;
    c->error.screen.source_error = throwable == MC_ENTITY_RENDER_SOURCE_ERROR;
    c->error.screen.exception_class = exception;
    return false;
}

static bool preview_typed_throwable(mc_client_sign_draw_call *c) {
    const mc_client_tile_renderer_draw_error *e = &c->error.preview;
    if (e->model != NULL) {
        c->error.screen.source_exception =
            e->model->throwable == MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION;
        c->error.screen.source_error = e->model->throwable == MC_CLIENT_MODEL_DRAW_SOURCE_ERROR;
        c->error.screen.exception_class = e->model->exception_class;
        c->exception_from_preview = e->model->exception_class != NULL;
        return true;
    }
    if (e->font != NULL) {
        font_throwable(&c->error.screen, e->font);
        /* These translated names are our static literals, not a borrowed
         * lower name. They remain valid when partial retirement consumes the
         * Font receipt before a later Engine/graphics release refuses. */
        c->exception_from_preview = false;
        return true;
    }
    if (e->registry.status != MC_REGISTRY_OK) {
        mc_entity_render_throwable throwable = MC_ENTITY_RENDER_NO_THROWABLE;
        mc_entity_render_registry_classification(c->services.registry, &e->registry, &throwable,
                                                 &c->error.screen.exception_class);
        c->error.screen.source_exception = throwable == MC_ENTITY_RENDER_SOURCE_EXCEPTION;
        c->error.screen.source_error = throwable == MC_ENTITY_RENDER_SOURCE_ERROR;
        /* The existing Registry classifier returns the same static exact
         * class-name inventory; its value does not borrow the LL call. */
        c->exception_from_preview = false;
        return true;
    }
    return false;
}

static bool tile_query(mc_client_sign_draw_call *c, mc_tile_entity *receiver, bool metadata,
                       int32_t *out) {
    if (receiver == NULL)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                 "YC actual null Sign getter receiver", "yc", "a", descriptor,
                                 c->state.source_pc, false);
    mc_tile_entity_snapshot tile;
    c->error.tile = mc_tile_entity_get_snapshot(receiver, &tile);
    if (c->error.tile != MC_TILE_ENTITY_OK)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, "YC actual Sign native snapshot",
                                 "yc", "a", descriptor, c->state.source_pc, true);
    mc_world_runtime *world = tile.world;
    if (world != NULL) {
        if (c->world_count == sizeof(c->worlds) / sizeof(*c->worlds) ||
            !mc_world_runtime_binding_retain(world))
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_BUSY, "OW getter World native occurrence",
                                     "yc", "a", descriptor, c->state.source_pc, true);
        c->worlds[c->world_count++] = world;
    }
    int32_t xyz[3];
    for (size_t axis = 0; axis < 3; ++axis) {
        c->error.tile = mc_tile_entity_get_snapshot(receiver, &tile);
        if (c->error.tile != MC_TILE_ENTITY_OK)
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED,
                                     "OW fresh coordinate native snapshot", "ow",
                                     metadata ? "e" : "f", metadata ? "()I" : "()Luu;",
                                     (metadata ? 5u : 8u) + 4u * (unsigned int)axis, true);
        xyz[axis] = axis == 0 ? tile.x : axis == 1 ? tile.y : tile.z;
    }
    if (world == NULL)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                 "OW actual World query null receiver", "ow", metadata ? "e" : "f",
                                 metadata ? "()I" : "()Luu;", metadata ? 16u : 19u, false);
    c->query_fields.c = world; /* Native bound-World adapter, not a constructed CV Source claim. */
    c->query = (mc_cv_context){.fields = &c->query_fields,
                               .world = world,
                               .code_classes = c->services.world_classes,
                               .registry = c->services.registry};
    if (!mc_cv_push_source(&c->query, "ow", metadata ? "e" : "f", metadata ? "()I" : "()Luu;"))
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, "OW native query frame", "yc", "a",
                                 descriptor, c->state.source_pc, true);
    const bool ok = metadata
                        ? mc_cv_metadata_bound(&c->query, world, xyz[0], xyz[1], xyz[2], 16, out)
                        : mc_cv_block_bound(&c->query, world, xyz[0], xyz[1], xyz[2], 19, out);
    if (!ok) {
        c->error.query = &c->query.failure;
        c->error.query_world = &c->query.world_error;
        mc_sign_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, c->query.failure.operation, "ow",
                          metadata ? "e" : "f", metadata ? "()I" : "()Luu;", metadata ? 16u : 19u,
                          c->query.failure.native_policy);
        c->error.screen.source_exception = !c->query.failure.native_policy;
        c->error.screen.exception_class = c->query.exception_class;
        c->exception_from_query = c->query.exception_class != NULL;
        if (c->query.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE)
            c->error.screen.exception_class = "java/lang/NullPointerException";
        c->error.helper = (mc_client_screen_site){
            c->query.failure.child.class_name, c->query.failure.child.method,
            c->query.failure.child.descriptor,
            c->query.failure.child.pc < 0 ? 0u : (unsigned int)c->query.failure.child.pc};
        return false;
    }
    mc_cv_pop(&c->query);
    return true;
}

static bool body(mc_client_sign_draw_call *c) {
    mc_client_screen_state base;
    mc_client_sign_screen_state screen;
    at(c, 0);
    if (!mc_sign_draw_base(c, &base))
        return false;
    if (base.da.b == NULL)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                 "DA background App World field null receiver", "da", "a", "(I)V",
                                 4, false);
    if (base.da.b->active_world == NULL)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                                 "YC reached DA dirt background descendant", "da", "a", "(I)V", 32,
                                 true);
    if (!mc_sign_draw_gradient(c, base.da.c, base.da.d))
        return false;
    c->state.background_returned = true;
    at(c, 23);
    if (!mc_sign_draw_base(c, &base) || !mc_sign_draw_owner(c, &screen))
        return false;
    mc_player_string_view title;
    const mc_player_string_view *title_argument = NULL;
    if (screen.title != NULL) {
        if (!mc_player_string_read(screen.title, &title))
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_STRING_FAILED, "YC captured title String",
                                     "yc", "a", descriptor, 10, true);
        title_argument = &title;
    }
    if (!mc_sign_draw_centered(c, base.da.g, title_argument, base.da.c / 2, 40, UINT32_C(0xffffff),
                               (mc_client_screen_site){"yc", "a", descriptor, 23},
                               &c->state.title_width, &c->state.title_x))
        return false;
    c->state.title_returned = true;
    mc_client_display *d = c->services.draw.display;
#define GL(expr, pc)                                                                               \
    do {                                                                                           \
        at(c, pc);                                                                                 \
        if (!mc_sign_draw_gl(c, (expr), "yc", "a", descriptor, pc))                                \
            return false;                                                                          \
    } while (0)
    GL(mc_client_display_source_push(d, &c->error.display), 26);
    c->state.matrix_pushed = true;
    if (!mc_sign_draw_base(c, &base) || !mc_sign_draw_fp(c, "yc", "a", descriptor, 35))
        return false;
    GL(mc_client_display_source_translate(d, (float)(base.da.c / 2), 0, 50, &c->error.display), 39);
    GL(mc_client_display_source_scale(d, -93.75, -93.75, -93.75, &c->error.display), 55);
    GL(mc_client_display_source_rotate(d, 180, 0, 1, 0, &c->error.display), 63);
    at(c, 70);
    if (!mc_sign_draw_owner(c, &screen))
        return false;
    if (screen.tile == NULL)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                 "YC null Sign block receiver", "yc", "a", descriptor, 70, false);
    mc_registry_block_m_view array;
    if (!registry_result(c,
                         mc_registry_block_m_get(c->services.registry, &array, &c->error.registry)))
        return false;
    int32_t id;
    const mc_block *block, *standing;
    if (!tile_query(c, screen.tile, false, &id) ||
        !registry_result(c, mc_registry_block_m_at(&array, id, &block, &c->error.registry)))
        return false;
    c->state.block_returned = true;
    at(c, 77);
    if (!registry_result(c, mc_registry_block_static_get(c->services.registry, "aE", &standing,
                                                         &c->error.registry)))
        return false;
    c->state.standing = block == standing;
    at(c, c->state.standing ? 87u : 125u);
    if (!mc_sign_draw_owner(c, &screen) || !tile_query(c, screen.tile, true, &c->state.metadata))
        return false;
    c->state.metadata_returned = true;
    float angle = 0;
    if (c->state.standing) {
        if (!mc_sign_draw_fp(c, "yc", "a", descriptor, 94))
            return false;
        angle = at_fdiv((float)mc_java_i32_mul(c->state.metadata, 360), 16.0f);
    } else if (c->state.metadata == 2)
        angle = 180;
    else if (c->state.metadata == 4)
        angle = 90;
    else if (c->state.metadata == 5)
        angle = -90;
    c->state.angle = angle;
    const unsigned int rotate_pc = c->state.standing ? 105u : 165u;
    const unsigned int translate_pc = c->state.standing ? 112u : 172u;
    GL(mc_client_display_source_rotate(d, angle, 0, 1, 0, &c->error.display), rotate_pc);
    GL(mc_client_display_source_translate(d, 0, -1.0625f, 0, &c->error.display), translate_pc);
    if (!mc_sign_draw_owner(c, &screen))
        return false;
    c->state.tick = screen.tick;
    c->state.selected_row = screen.row;
    if ((screen.tick / 6) % 2 == 0) {
        at(c, 195);
        if (screen.tile == NULL)
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                     "YC null Sign marker field receiver", "yc", "a", descriptor,
                                     195, false);
        c->error.tile = mc_tile_entity_sign_set_edit_row(screen.tile, screen.row);
        if (c->error.tile != MC_TILE_ENTITY_OK)
            return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, "YC actual Sign marker store",
                                     "yc", "a", descriptor, 195, true);
        c->state.marker_stored = true;
    }
    at(c, 198);
    mc_client_tile_renderer_classes *classes = NULL;
    if (mc_client_tile_entity_renderer_classes_existing(
            c->services.models, c->services.registry, c->services.world_classes, &classes,
            &c->error.dispatcher) != MC_CLIENT_TILE_RENDERER_OK ||
        (classes == NULL &&
         mc_client_tile_renderer_classes_bind(
             c->services.models, c->services.world_classes, c->services.profile, NULL, &classes,
             &c->error.dispatcher) != MC_CLIENT_TILE_RENDERER_OK) ||
        mc_client_tile_renderer_static_get(classes, &c->dispatcher, &c->error.dispatcher) !=
            MC_CLIENT_TILE_RENDERER_OK)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, c->error.dispatcher.operation,
                                 "yc", "a", descriptor, 198, c->error.dispatcher.native_policy);
    if (c->dispatcher != NULL &&
        mc_client_tile_renderer_dispatcher_retain(c->dispatcher, &c->error.dispatcher) !=
            MC_CLIENT_TILE_RENDERER_OK) {
        c->dispatcher = NULL; /* No native occurrence was acquired. */
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, "YC actual LL native occurrence",
                                 "yc", "a", descriptor, 198, true);
    }
    at(c, 215);
    if (!mc_sign_draw_owner(c, &screen))
        return false;
    if (c->dispatcher == NULL)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                 "YC actual LL coordinates null receiver", "yc", "a", descriptor,
                                 215, false);
    const mc_client_tile_renderer_status preview = mc_client_tile_renderer_coordinates_draw_begin(
        c->dispatcher, screen.tile, -0.5, -0.75, -0.5, 0, &c->services.draw, &c->limits.preview,
        &c->preview, &c->error.preview);
    if (c->preview != NULL) {
        (void)mc_client_tile_renderer_draw_inspect(c->preview, &c->preview_state);
        c->state.preview = &c->preview_state;
    }
    if (preview != MC_CLIENT_TILE_RENDERER_OK) {
        const mc_client_tile_renderer_draw_error *e = &c->error.preview;
        mc_sign_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, e->operation, e->site.class_name,
                          e->site.method, e->site.descriptor,
                          e->site.pc < 0 ? 0u : (unsigned int)e->site.pc, e->native_policy);
        c->error.screen.source_exception = e->source_exception;
        c->error.screen.exception_class = e->exception_class;
        c->exception_from_preview = c->error.screen.exception_class != NULL;
        (void)preview_typed_throwable(c);
        return false;
    }
    c->state.preview_returned = true;
    at(c, 223);
    if (!mc_sign_draw_owner(c, &screen))
        return false;
    if (screen.tile == NULL)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                 "YC fresh null Sign marker-clear receiver", "yc", "a", descriptor,
                                 223, false);
    c->error.tile = mc_tile_entity_sign_set_edit_row(screen.tile, -1);
    if (c->error.tile != MC_TILE_ENTITY_OK)
        return mc_sign_draw_fail(c, MC_CLIENT_SCREEN_APP_FAILED, "YC actual marker clear store",
                                 "yc", "a", descriptor, 223, true);
    c->state.marker_cleared = true;
    GL(mc_client_display_source_pop(d, &c->error.display), 226);
    c->state.matrix_popped = true;
    at(c, 233);
    if (!mc_sign_draw_buttons(c))
        return false;
    c->state.buttons_returned = true;
    at(c, 236);
    c->error.screen.site = (mc_client_screen_site){"yc", "a", descriptor, 236};
    c->state.returned = c->error.screen.source_returned = true;
    return true;
#undef GL
}

mc_client_sign_draw_limits mc_client_source_sign_draw_default_limits(void) {
    return (mc_client_sign_draw_limits){1048576u, 4096u, 4096u,
                                        mc_client_tile_renderer_draw_default_limits()};
}

mc_client_screen_status
mc_client_source_sign_draw_begin(mc_client_source_yc *owner, mc_gui_mouse_position mouse,
                                 float partial, const mc_client_sign_draw_services *services,
                                 const mc_client_sign_draw_limits *limits,
                                 mc_client_sign_draw_call **out, mc_client_sign_draw_error *error) {
    mc_client_sign_draw_error e = {0};
    if (owner == NULL || services == NULL || out == NULL || *out != NULL) {
        e.screen.status = MC_CLIENT_SCREEN_INVALID_ARGUMENT;
        e.screen.native_policy = true;
        e.screen.operation = "YC draw native arguments";
        if (error != NULL)
            *error = e;
        return e.screen.status;
    }
    const mc_client_sign_draw_limits selected =
        limits == NULL ? mc_client_source_sign_draw_default_limits() : *limits;
    if (selected.max_bytes < sizeof(mc_client_sign_draw_call)) {
        e.screen.status = MC_CLIENT_SCREEN_LIMIT;
        e.screen.native_policy = true;
        e.screen.operation = "YC retained draw owner budget";
        if (error != NULL)
            *error = e;
        return e.screen.status;
    }
    mc_client_sign_draw_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        e.screen.status = MC_CLIENT_SCREEN_ALLOCATION_FAILED;
        e.screen.native_policy = true;
        e.screen.operation = "YC retained draw owner allocation";
        if (error != NULL)
            *error = e;
        return e.screen.status;
    }
    *out = c;
    c->owner = owner;
    c->services = *services;
    c->limits = selected;
    c->mouse = mouse;
    c->partial = partial;
    c->state.allocation_bytes = sizeof(*c);
    c->state.failure = &c->error;
    c->state.cleanup = &c->cleanup;
    mc_client_source_da *base = mc_client_screen_sign_base(owner);
    if (mc_client_screen_retain(base, &c->error.screen) == MC_CLIENT_SCREEN_OK) {
        c->screen = base;
        c->state.receiver = owner;
        mc_client_screen_state state;
        if (mc_sign_draw_base(c, &state)) {
            if (state.kind != MC_CLIENT_SCREEN_SIGN || state.phase != MC_CLIENT_SCREEN_READY)
                mc_sign_draw_fail(c, MC_CLIENT_SCREEN_INVALID_PHASE, "YC actual live draw owner",
                                  "yc", "a", descriptor, 0, true);
            else {
                c->state.active = true;
                (void)body(c);
                c->state.active = false;
            }
        }
    } else
        c->owner = NULL; /* No exported operand view without its actual native hold. */
    if (error != NULL)
        *error = c->error;
    return c->error.screen.status;
}

bool mc_client_source_sign_draw_inspect(const mc_client_sign_draw_call *c,
                                        mc_client_sign_draw_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

mc_client_screen_status mc_client_source_sign_draw_retire(mc_client_sign_draw_call **slot,
                                                          mc_client_sign_draw_error *error) {
    if (error != NULL)
        *error = (mc_client_sign_draw_error){0};
    if (slot == NULL)
        return MC_CLIENT_SCREEN_INVALID_ARGUMENT;
    mc_client_sign_draw_call *c = *slot;
    if (c == NULL)
        return MC_CLIENT_SCREEN_OK;
    if (c->state.active)
        return MC_CLIENT_SCREEN_BUSY;
    c->state.retiring = true;
    const mc_client_tile_renderer_status retired =
        mc_client_tile_renderer_draw_retire(&c->preview, &c->cleanup.preview);
    if (c->preview != NULL && mc_client_tile_renderer_draw_inspect(c->preview, &c->preview_state)) {
        c->state.preview = &c->preview_state;
        if (c->preview_state.failure != NULL)
            c->error.preview = *c->preview_state.failure;
        if (!preview_typed_throwable(c) && c->exception_from_preview)
            c->error.screen.exception_class = c->error.preview.exception_class;
    }
    if (retired != MC_CLIENT_TILE_RENDERER_OK) {
        c->cleanup.screen =
            (mc_client_screen_error){.status = MC_CLIENT_SCREEN_BUSY,
                                     .operation = "YC retained LL preview native retirement",
                                     .native_policy = true};
        goto busy;
    }
    c->state.preview = NULL;
    c->error.preview.font = NULL;
    c->error.preview.model = NULL;
    c->error.preview.blocks = NULL;
    c->error.preview.blocks_world = NULL;
    c->error.preview.brightness = NULL;
    c->error.preview.factory = NULL;
    c->error.preview.entity = NULL;
    c->error.preview.pose = NULL;
    c->error.preview.manager_owner = NULL;
    c->error.preview.manager = NULL;
    c->error.preview.texture = NULL;
    c->error.preview.lighting = NULL;
    c->error.preview.application_query = NULL;
    c->error.preview.exception_class = NULL; /* Consumed child name view; scalars/sites remain. */
    if (c->exception_from_preview) {
        c->error.screen.exception_class = NULL;
        c->exception_from_preview = false;
    }
    const bool query_retired = mc_cv_retire(&c->query, &c->query_cleanup);
    if (c->exception_from_query && c->query.capture_attempted &&
        c->query.captured_failure.receipt == NULL) {
        c->error.screen.exception_class = NULL;
        c->exception_from_query = false;
    }
    if (!query_retired) {
        c->cleanup.query = &c->query_cleanup;
        c->cleanup.screen =
            (mc_client_screen_error){.status = MC_CLIENT_SCREEN_BUSY,
                                     .operation = "YC inherited getter native receipt retirement",
                                     .native_policy = true};
        goto busy;
    }
    c->error.query = NULL;
    c->error.query_world = NULL;
    if (c->pending_lookup != NULL) {
        if (mc_client_render_engine_retire_lookup(c->pending_lookup, &c->texture_cleanup) !=
            MC_CLIENT_LOOKUP_OK) {
            c->cleanup.texture = &c->texture_cleanup;
            c->cleanup.screen =
                (mc_client_screen_error){.status = MC_CLIENT_SCREEN_BUSY,
                                         .operation = "YC retained JI lookup native retirement",
                                         .native_policy = true};
            goto busy;
        }
        c->pending_lookup = NULL;
        c->error.texture = NULL;
    }
    if (mc_client_tile_renderer_dispatcher_release(&c->dispatcher, &c->cleanup.dispatcher) !=
        MC_CLIENT_TILE_RENDERER_OK) {
        c->cleanup.screen =
            (mc_client_screen_error){.status = MC_CLIENT_SCREEN_BUSY,
                                     .operation = "YC retained LL native occurrence retirement",
                                     .native_policy = true};
        goto busy;
    }
    while (c->fonts != NULL) {
        mc_sign_draw_font *f = c->fonts;
        if (mc_client_bitmap_font_release(&f->font, &c->cleanup.font) != MC_CLIENT_BITMAP_FONT_OK) {
            c->cleanup.screen = (mc_client_screen_error){
                .status = MC_CLIENT_SCREEN_BUSY,
                .operation = "YC reached Font native occurrence retirement",
                .native_policy = true};
            goto busy;
        }
        c->fonts = f->next;
        free(f);
    }
    while (c->buttons != NULL) {
        mc_sign_draw_button *b = c->buttons;
        if (mc_client_screen_button_release(&b->button, &c->cleanup.screen) != MC_CLIENT_SCREEN_OK)
            goto busy;
        c->buttons = b->next;
        free(b);
    }
    while (c->world_count != 0)
        mc_world_runtime_binding_release(c->worlds[--c->world_count]);
    if (c->screen != NULL &&
        mc_client_screen_release(&c->screen, &c->cleanup.screen) != MC_CLIENT_SCREEN_OK)
        goto busy;
    free(c);
    *slot = NULL;
    return MC_CLIENT_SCREEN_OK;
busy:
    if (error != NULL)
        *error = c->cleanup;
    return MC_CLIENT_SCREEN_BUSY;
}
