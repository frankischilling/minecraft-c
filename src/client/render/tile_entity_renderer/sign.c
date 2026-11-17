#include "draw_internal.h"

#include "../blocks/internal.h"
#include "../interpolation_internal.h"
#include "block/registry/registry_internal.h"
#include "block/registry/runtime_internal.h"
#include "client/display/camera_surface.h"
#include "client/display/fog_surface.h"
#include "client/display/source_surface.h"
#include "client/gui/bitmap_font.h"
#include "client/render_engine/texture_lookup.h"
#include "entity/render/internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/client/render/model_draw.h"
#include "mc/core/java/java.h"
#include "world/runtime/construction_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Only a reached PO body allocates this retained graph. The one query frame
 * can be reused after success; a failed query ends Source execution and keeps
 * the same producer/callback receipt until checked native retirement. */
struct tile_draw_sign_call {
    mc_cv_context query;
    mc_client_render_blocks query_fields;
    mc_client_render_global_error query_cleanup;
    mc_world_runtime *worlds[2];
    size_t world_count;
    bool exception_from_query;
    mc_client_render_engine *texture_engine;
    bool texture_pending_owned;
    mc_client_texture_lookup_error texture_error, texture_cleanup;
    mc_client_bitmap_font *font;
    mc_client_bitmap_font_error font_error, font_cleanup;
    mc_client_model_draw_error model_error;
    mc_tile_entity_name *line;
    uint16_t *decorated;
    size_t decorated_capacity;
};

static const char descriptor[] = "(Lyk;DDDF)V";

static void at(mc_client_tile_renderer_draw_call *c, int32_t pc) {
    tile_draw_at(c, "po", descriptor, pc);
}

static bool source_null(mc_client_tile_renderer_draw_call *c, const char *operation) {
    c->error.source_exception = true;
    c->error.exception_class = "java/lang/NullPointerException";
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE, operation, false);
}

static bool allocate_call(mc_client_tile_renderer_draw_call *c) {
    if (c->state.allocation_bytes > c->limits.max_bytes ||
        sizeof(*c->sign) > c->limits.max_bytes - c->state.allocation_bytes)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_LIMIT,
                              "PO reached native draw workspace limit", true);
    c->sign = calloc(1, sizeof(*c->sign));
    if (c->sign == NULL)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_ALLOCATION_FAILED,
                              "PO reached native draw workspace allocation", true);
    c->state.allocation_bytes += sizeof(*c->sign);
    return true;
}

static bool registry_result(mc_client_tile_renderer_draw_call *c, mc_registry_status status) {
    if (status == MC_REGISTRY_OK)
        return true;
    mc_entity_render_throwable throwable = MC_ENTITY_RENDER_NO_THROWABLE;
    mc_entity_render_registry_classification(c->dispatcher->classes->world_classes->registry,
                                             &c->error.registry, &throwable,
                                             &c->error.exception_class);
    c->error.source_exception = throwable != MC_ENTITY_RENDER_NO_THROWABLE;
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_BLOCKS_FAILED, c->error.registry.message,
                          !c->error.source_exception);
}

/* Exact inherited OW.f/e argument order. f captures Block.m first, then both
 * getters capture OW.d before each fresh coordinate read and the real World
 * invocation. This uses CV's existing bound-World query/receipt machinery;
 * it does not substitute an EW cache, Tile coordinates or a success fixture. */
static bool tile_query(mc_client_tile_renderer_draw_call *c, bool metadata, int32_t *out) {
    tile_draw_sign_call *s = c->sign;
    const char *method = metadata ? "e" : "f";
    const char *desc = metadata ? "()I" : "()Luu;";
    mc_tile_entity_snapshot tile;
    c->error.child = (mc_client_graphics_site){"ow", method, desc, metadata ? 1 : 4};
    if (!tile_draw_snapshot(c, &tile, c->error.child.pc))
        return false;
    mc_world_runtime *world = tile.world;
    if (world != NULL) {
        if (s->world_count == sizeof(s->worlds) / sizeof(*s->worlds))
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_LIMIT,
                                  "PO inherited getter World occurrence extent", true);
        if (!mc_world_runtime_binding_retain(world))
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_WORLD_FAILED,
                                  "PO inherited getter actual World occurrence", true);
        s->worlds[s->world_count++] = world;
    }
    int32_t xyz[3];
    for (size_t axis = 0; axis < 3; ++axis) {
        c->error.child.pc = (metadata ? 5 : 8) + (int32_t)axis * 4;
        if (!tile_draw_snapshot(c, &tile, c->error.child.pc))
            return false;
        xyz[axis] = axis == 0 ? tile.x : axis == 1 ? tile.y : tile.z;
    }
    c->error.child.pc = metadata ? 16 : 19;
    if (world == NULL)
        return source_null(c, "OW actual null World query receiver");
    s->query_fields.c = world;
    s->query = (mc_cv_context){.fields = &s->query_fields,
                               .world = world,
                               .code_classes = c->dispatcher->classes->world_classes,
                               .registry = c->dispatcher->classes->world_classes->registry};
    if (!mc_cv_push_source(&s->query, "ow", method, desc))
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_WORLD_FAILED,
                              "OW inherited getter native query frame", true);
    const bool ok = metadata
                        ? mc_cv_metadata_bound(&s->query, world, xyz[0], xyz[1], xyz[2], 16, out)
                        : mc_cv_block_bound(&s->query, world, xyz[0], xyz[1], xyz[2], 19, out);
    if (!ok) {
        c->error.blocks = &s->query.failure;
        c->error.blocks_world = &s->query.world_error;
        c->error.helper = s->query.failure.child;
        c->error.source_exception = !s->query.failure.native_policy;
        c->error.exception_class = s->query.exception_class;
        if (s->query.failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE)
            c->error.exception_class = "java/lang/NullPointerException";
        s->exception_from_query = s->query.exception_class != NULL;
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_WORLD_FAILED, s->query.failure.operation,
                              s->query.failure.native_policy);
    }
    mc_cv_pop(&s->query);
    c->error.child = (mc_client_graphics_site){0};
    return true;
}

static bool tile_block(mc_client_tile_renderer_draw_call *c, const mc_block **out) {
    at(c, 1);
    c->error.child = (mc_client_graphics_site){"ow", "f", "()Luu;", 0};
    mc_registry_classes *registry = c->dispatcher->classes->world_classes->registry;
    mc_registry_block_m_view array;
    if (!registry_result(c, mc_registry_block_m_get(registry, &array, &c->error.registry)))
        return false;
    int32_t id;
    if (!tile_query(c, false, &id))
        return false;
    c->error.child = (mc_client_graphics_site){"ow", "f", "()Luu;", 22};
    if (!registry_result(c, mc_registry_block_m_at(&array, id, out, &c->error.registry)))
        return false;
    c->state.sign_block_returned = true;
    c->error.child = (mc_client_graphics_site){0};
    return true;
}

static bool metadata(mc_client_tile_renderer_draw_call *c, int32_t pc) {
    at(c, pc);
    if (!tile_query(c, true, &c->state.sign_metadata))
        return false;
    c->state.sign_metadata_returned = true;
    return true;
}

static bool part(mc_client_tile_renderer_draw_call *c, mc_client_model_part *value, int32_t pc) {
    c->error.child = (mc_client_graphics_site){"rf", "a", "()V", pc};
    if (value == NULL)
        return source_null(c, "RF actual null PS draw receiver");
    if (!tile_draw_graphics(c))
        return false;
    const bool ok = mc_client_model_part_draw(value, c->services.graphics,
                                              c->dispatcher->classes->models->entities,
                                              c->services.display, 0.0625f, &c->sign->model_error);
    const mc_client_model_draw_error *e = &c->sign->model_error;
    c->state.model_faces += e->faces;
    c->state.model_vertices += e->vertices;
    c->state.model_compiled_parts += e->compiled ? 1u : 0u;
    if (!ok) {
        c->error.model = e;
        c->error.helper = e->site;
        c->error.source_exception = e->throwable == MC_CLIENT_MODEL_DRAW_SOURCE_EXCEPTION ||
                                    e->throwable == MC_CLIENT_MODEL_DRAW_SOURCE_ERROR;
        c->error.exception_class = e->exception_class;
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_MODEL_FAILED, e->operation,
                              e->native_policy);
    }
    ++c->state.model_parts;
    return true;
}

static bool model(mc_client_tile_renderer_draw_call *c) {
    at(c, 195);
    mc_client_sign_model *value = c->renderer->sign_model; /* Fresh PO.b192. */
    if (value == NULL)
        return source_null(c, "PO actual null RF draw receiver");
    if (!part(c, value->a, 6) || !part(c, value->b, 15))
        return false;
    c->state.sign_model_returned = true;
    c->error.child = (mc_client_graphics_site){0};
    return true;
}

static bool pole(mc_client_tile_renderer_draw_call *c, bool shown) {
    at(c, shown ? 71 : 164);
    mc_client_sign_model *value = c->renderer->sign_model;
    if (value == NULL)
        return source_null(c, "PO actual null RF pole field receiver");
    at(c, shown ? 75 : 168);
    if (value->b == NULL)
        return source_null(c, "PO actual null PS pole visibility receiver");
    value->b->h = shown;
    return true;
}

static bool font_result(mc_client_tile_renderer_draw_call *c, mc_client_bitmap_font_status status) {
    if (status == MC_CLIENT_BITMAP_FONT_OK)
        return true;
    mc_client_bitmap_font_error *e = &c->sign->font_error;
    c->error.font = e;
    c->error.child = (mc_client_graphics_site){"sj", e->method, e->descriptor, e->pc};
    c->error.source_exception = !e->native_policy;
    if (!e->native_policy) {
        if (status == MC_CLIENT_BITMAP_FONT_NULL_REFERENCE)
            c->error.exception_class = "java/lang/NullPointerException";
        else if (status == MC_CLIENT_BITMAP_FONT_ARRAY_INDEX)
            c->error.exception_class = "java/lang/ArrayIndexOutOfBoundsException";
        else if (status == MC_CLIENT_BITMAP_FONT_STRING_INDEX)
            c->error.exception_class = "java/lang/StringIndexOutOfBoundsException";
        else if (status == MC_CLIENT_BITMAP_FONT_SOURCE_ILLEGAL_ARGUMENT)
            c->error.exception_class = "java/lang/IllegalArgumentException";
        else if (status == MC_CLIENT_BITMAP_FONT_SOURCE_RUNTIME_EXCEPTION)
            c->error.exception_class = "java/lang/RuntimeException";
    }
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_FONT_FAILED, e->operation, e->native_policy);
}

static bool font(mc_client_tile_renderer_draw_call *c) {
    at(c, 202);
    c->error.child = (mc_client_graphics_site){"je", "a", "()Lsj;", 4};
    mc_client_tile_renderer_dispatcher *manager = c->renderer->manager;
    if (manager == NULL)
        return source_null(c, "JE actual null LL font field receiver");
    mc_client_bitmap_font *value = manager->fields.n;
    if (value != NULL) {
        c->error.owner.font_status = (int)mc_client_bitmap_font_retain(value);
        if (c->error.owner.font_status != MC_CLIENT_BITMAP_FONT_OK)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_FONT_FAILED,
                                  "JE actual Font local occurrence", true);
        c->sign->font = value;
    }
    c->state.sign_font_returned = true; /* A genuine null local is allowed here. */
    c->error.child = (mc_client_graphics_site){0};
    return true;
}

static bool line_count(mc_client_tile_renderer_draw_call *c, int32_t pc, int32_t *out) {
    at(c, pc);
    size_t count;
    c->error.owner.tile_entity = mc_tile_entity_sign_line_count(c->tile_entity, &count);
    if (c->error.owner.tile_entity != MC_TILE_ENTITY_OK)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                              "PO fresh actual Sign String array length", true);
    if (count > INT32_MAX)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_LIMIT,
                              "PO native Sign array Java length extent", true);
    *out = (int32_t)count;
    return true;
}

/* The selected-row StringBuilder appends the literal spaces and treats a
 * null String as the four units "null". The immutable local remains owned
 * through both real Font calls, including callbacks and retained failure. */
static bool decorated(mc_client_tile_renderer_draw_call *c, mc_player_string_view *view,
                      bool is_null) {
    static const uint16_t null_text[] = {'n', 'u', 'l', 'l'};
    const uint16_t *units = is_null ? null_text : view->units;
    const size_t length = is_null ? 4u : view->length;
    at(c, 308);
    if (length > INT32_MAX - 4u || length + 4u > SIZE_MAX / sizeof(uint16_t))
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_LIMIT,
                              "PO selected-row StringBuilder native extent", true);
    const size_t count = length + 4u;
    tile_draw_sign_call *s = c->sign;
    if (count > s->decorated_capacity) {
        const size_t bytes = count * sizeof(uint16_t);
        if (c->state.allocation_bytes > c->limits.max_bytes ||
            bytes > c->limits.max_bytes - c->state.allocation_bytes)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_LIMIT,
                                  "PO selected-row StringBuilder native bytes", true);
        uint16_t *fresh = malloc(bytes);
        if (fresh == NULL)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_ALLOCATION_FAILED,
                                  "PO selected-row StringBuilder native allocation", true);
        c->state.allocation_bytes += bytes;
        c->state.allocation_bytes -= s->decorated_capacity * sizeof(uint16_t);
        free(s->decorated);
        s->decorated = fresh;
        s->decorated_capacity = count;
    }
    s->decorated[0] = '>';
    s->decorated[1] = ' ';
    if (length != 0)
        memcpy(s->decorated + 2, units, length * sizeof(uint16_t));
    s->decorated[count - 2] = ' ';
    s->decorated[count - 1] = '<';
    *view = (mc_player_string_view){s->decorated, count};
    return true;
}

static void text_byte(uint64_t *digest, uint8_t value) {
    *digest = (*digest ^ value) * UINT64_C(1099511628211);
}

static uint64_t text_receipt(mc_client_tile_renderer_draw_call *c, int32_t row,
                             const mc_player_string_view *view, bool is_null, bool selected) {
    uint64_t total = c->state.sign_text_digest;
    uint64_t digest = UINT64_C(14695981039346656037);
    const uint32_t index = (uint32_t)row;
    const uint64_t length = is_null ? 0 : (uint64_t)view->length;
    for (unsigned int i = 0; i < 4; ++i) {
        text_byte(&total, (uint8_t)(index >> (i * 8u)));
        text_byte(&digest, (uint8_t)(index >> (i * 8u)));
    }
    text_byte(&total, is_null ? 1 : 0);
    text_byte(&digest, is_null ? 1 : 0);
    for (unsigned int i = 0; i < 8; ++i) {
        text_byte(&total, (uint8_t)(length >> (i * 8u)));
        text_byte(&digest, (uint8_t)(length >> (i * 8u)));
    }
    if (!is_null) {
        for (size_t i = 0; i < view->length; ++i) {
            text_byte(&total, (uint8_t)view->units[i]);
            text_byte(&digest, (uint8_t)view->units[i]);
            text_byte(&total, (uint8_t)(view->units[i] >> 8u));
            text_byte(&digest, (uint8_t)(view->units[i] >> 8u));
        }
    }
    if (row >= 0 && row < 4)
        c->state.sign_rows[row] =
            (mc_client_tile_renderer_sign_row_receipt){.captured = true,
                                                       .is_null = is_null,
                                                       .selected = selected,
                                                       .text_units = (size_t)length,
                                                       .text_digest = digest};
    return total;
}

static bool text(mc_client_tile_renderer_draw_call *c) {
    tile_draw_sign_call *s = c->sign;
    c->state.sign_text_digest = UINT64_C(14695981039346656037);
    for (int32_t row = 0;; row = mc_java_i32_add(row, 1)) {
        int32_t count;
        if (!line_count(c, 264, &count))
            return false;
        if (row >= count)
            return true;
        at(c, 274);
        mc_tile_entity_name_release(s->line);
        s->line = NULL;
        c->error.owner.tile_entity = mc_tile_entity_sign_get_line(c->tile_entity, row, &s->line);
        if (c->error.owner.tile_entity != MC_TILE_ENTITY_OK)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                                  "PO fresh actual Sign array element", true);
        int32_t selected;
        at(c, 280);
        c->error.owner.tile_entity = mc_tile_entity_sign_get_edit_row(c->tile_entity, &selected);
        if (c->error.owner.tile_entity != MC_TILE_ENTITY_OK)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                                  "PO fresh actual Sign edit row", true);
        mc_tile_entity_name_view name;
        if (mc_tile_entity_name_get_view(s->line, &name) != MC_TILE_ENTITY_OK)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                                  "PO held actual Sign line String", true);
        mc_player_string_view view = {name.text.units, name.text.length};
        bool is_null = name.is_null;
        const bool edit = row == selected;
        if (edit) {
            if (!decorated(c, &view, is_null))
                return false;
            is_null = false;
        }
        const uint64_t total_digest = text_receipt(c, row, &view, is_null, edit);
        mc_client_tile_renderer_sign_row_receipt *receipt =
            row >= 0 && row < 4 ? &c->state.sign_rows[row] : NULL;
        at(c, edit ? 321 : 356);
        if (receipt != NULL)
            receipt->measure_attempted = true;
        if (s->font == NULL)
            return source_null(c, "PO actual null Font measure receiver");
        int32_t width;
        if (!font_result(c, mc_client_bitmap_font_measure_view(s->font, is_null ? NULL : &view,
                                                               &width, &s->font_error)))
            return false;
        ++c->state.sign_rows_measured;
        if (receipt != NULL) {
            receipt->measure_returned = true;
            receipt->width = width;
        }
        const int32_t x = mc_java_i32_neg(width) / 2;
        if (!line_count(c, edit ? 336 : 371, &count))
            return false;
        const int32_t y = mc_java_i32_sub(mc_java_i32_mul(row, 10), mc_java_i32_mul(count, 5));
        at(c, edit ? 342 : 377);
        if (receipt != NULL) {
            receipt->x = x;
            receipt->y = y;
            receipt->draw_attempted = true;
        }
        if (!font_result(c, mc_client_bitmap_font_draw_view(s->font, is_null ? NULL : &view, x, y,
                                                            0, false, &s->font_error)))
            return false;
        c->state.sign_last_row = row;
        c->state.sign_last_width = width;
        c->state.sign_last_x = x;
        c->state.sign_last_y = y;
        c->state.sign_text_digest = total_digest;
        if (is_null)
            ++c->state.sign_null_rows;
        else
            c->state.sign_text_units += view.length;
        if (receipt != NULL)
            receipt->draw_returned = true;
        ++c->state.sign_rows_drawn;
    }
}

bool tile_draw_sign(mc_client_tile_renderer_draw_call *c) {
    tile_draw_at(c, "po", "(Low;DDDF)V", 2);
    if (c->tile_entity != NULL) {
        mc_tile_entity_snapshot tile;
        if (!tile_draw_snapshot(c, &tile, 2))
            return false;
        if (tile.kind != MC_TILE_ENTITY_SIGN) {
            c->error.source_exception = true;
            c->error.exception_class = "java/lang/ClassCastException";
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                                  "PO actual bridge Sign checkcast", false);
        }
    }
    c->state.bridge_cast_returned = true;
    at(c, 1);
    if (c->tile_entity == NULL)
        return source_null(c, "PO actual null OW Block getter receiver");
    if (!allocate_call(c))
        return false;
    const mc_block *block;
    if (!tile_block(c, &block))
        return false;
    at(c, 6);
    if (!tile_draw_display(c,
                           mc_client_display_source_push(c->services.display, &c->error.display)))
        return false;
    c->state.matrix_pushed = true;
    mc_registry_classes *registry = c->dispatcher->classes->world_classes->registry;
    const mc_block *standing;
    at(c, 15);
    if (!registry_result(
            c, mc_registry_block_static_get(registry, "aE", &standing, &c->error.registry)))
        return false;
    c->state.sign_standing = block == standing;
    const float scale = 0.6666667f;
    if (!c->state.sign_standing && !metadata(c, 82))
        return false;
    at(c, c->state.sign_standing ? 22 : 121);
    if (!tile_draw_fp(c, c->error.site.pc))
        return false;
    const float x = at_fadd(at_narrow(c->state.x), .5f);
    const float y = at_fadd(at_narrow(c->state.y), at_fmul(.75f, scale));
    const float z = at_fadd(at_narrow(c->state.z), .5f);
    at(c, c->state.sign_standing ? 41 : 140);
    if (!tile_draw_display(
            c, mc_client_display_source_translate(c->services.display, x, y, z, &c->error.display)))
        return false;
    float angle;
    if (c->state.sign_standing) {
        if (!metadata(c, 45) || !tile_draw_fp(c, 52))
            return false;
        angle = at_fdiv((float)mc_java_i32_mul(c->state.sign_metadata, 360), 16.0f);
    } else {
        const int32_t m = c->state.sign_metadata;
        angle = m == 2 ? 180.0f : m == 4 ? 90.0f : m == 5 ? -90.0f : 0.0f;
    }
    c->state.sign_angle = angle;
    at(c, c->state.sign_standing ? 64 : 149);
    if (!tile_draw_display(c, mc_client_display_source_rotate(c->services.display, at_fneg(angle),
                                                              0, 1, 0, &c->error.display)))
        return false;
    if (!c->state.sign_standing) {
        at(c, 157);
        if (!tile_draw_display(c, mc_client_display_source_translate(
                                      c->services.display, 0, -.3125f, -.4375f, &c->error.display)))
            return false;
    }
    if (!pole(c, c->state.sign_standing))
        return false;
    static const uint16_t texture[] = {'/', 'i', 't', 'e', 'm', '/', 's',
                                       'i', 'g', 'n', '.', 'p', 'n', 'g'};
    at(c, 174);
    if (!tile_draw_texture(c, texture, sizeof(texture) / sizeof(*texture), &c->sign->texture_engine,
                           &c->sign->texture_pending_owned, &c->sign->texture_error))
        return false;
    at(c, 177);
    if (!tile_draw_display(c,
                           mc_client_display_source_push(c->services.display, &c->error.display)))
        return false;
    c->state.sign_model_pushed = true;
    at(c, 188);
    if (!tile_draw_display(c, mc_client_display_source_scale(c->services.display, scale,
                                                             at_fneg(scale), at_fneg(scale),
                                                             &c->error.display)) ||
        !model(c))
        return false;
    at(c, 198);
    if (!tile_draw_display(c, mc_client_display_source_pop(c->services.display, &c->error.display)))
        return false;
    c->state.sign_model_popped = true;
    if (!font(c) || !tile_draw_fp(c, 211))
        return false;
    const float text_scale = at_fmul(.016666668f, scale);
    c->state.sign_text_scale = text_scale;
    at(c, 225);
    if (!tile_draw_display(
            c, mc_client_display_source_translate(c->services.display, 0, at_fmul(.5f, scale),
                                                  at_fmul(.07f, scale), &c->error.display)))
        return false;
    at(c, 235);
    if (!tile_draw_display(c, mc_client_display_source_scale(c->services.display, text_scale,
                                                             at_fneg(text_scale), text_scale,
                                                             &c->error.display)))
        return false;
    at(c, 245);
    if (!tile_draw_fp(c, 244) ||
        !tile_draw_display(c, mc_client_display_source_normal(c->services.display, 0, 0,
                                                              at_fmul(-1.0f, text_scale),
                                                              &c->error.display)))
        return false;
    at(c, 249);
    if (!tile_draw_display(
            c, mc_client_display_source_depth_mask(c->services.display, false, &c->error.display)))
        return false;
    c->state.sign_depth_disabled = true;
    if (!text(c))
        return false;
    at(c, 387);
    if (!tile_draw_display(
            c, mc_client_display_source_depth_mask(c->services.display, true, &c->error.display)))
        return false;
    c->state.sign_depth_restored = true;
    at(c, 394);
    if (!tile_draw_display(
            c, mc_client_display_source_color(c->services.display, 1, 1, 1, 1, &c->error.display)))
        return false;
    c->state.sign_color_restored = true;
    at(c, 397);
    if (!tile_draw_display(c, mc_client_display_source_pop(c->services.display, &c->error.display)))
        return false;
    c->state.matrix_popped = true;
    at(c, 400);
    return true;
}

bool tile_draw_sign_retire(mc_client_tile_renderer_draw_call *c,
                           mc_client_tile_renderer_draw_error *e) {
    tile_draw_sign_call *s = c->sign;
    if (s == NULL)
        return true;
    const bool retired = mc_cv_retire(&s->query, &s->query_cleanup);
    if (s->exception_from_query)
        c->error.exception_class = s->query.exception_class;
    if (!retired) {
        e->blocks = &s->query_cleanup;
        e->status = MC_CLIENT_TILE_RENDERER_WORLD_FAILED;
        e->operation = "PO inherited World query native retirement";
        e->native_policy = true;
        return false;
    }
    c->error.blocks = NULL;
    c->error.blocks_world = NULL;
    if (s->model_error.pending_list) {
        const void *identity = NULL;
        if (s->model_error.pending_list_identity == 0 ||
            !mc_client_display_pending_list_identity(c->services.display, s->model_error.owned_list,
                                                     &identity) ||
            (uintptr_t)identity != s->model_error.pending_list_identity ||
            !mc_client_display_lists_abort(c->services.display, s->model_error.owned_list,
                                           &e->display)) {
            e->status = MC_CLIENT_TILE_RENDERER_DISPLAY_FAILED;
            e->operation = "PO actual PS pending definition native retirement";
            e->native_policy = true;
            return false;
        }
        s->model_error.pending_list = false;
        s->model_error.pending_list_identity = 0;
    }
    if (s->texture_pending_owned) {
        const mc_client_texture_lookup_status status =
            mc_client_render_engine_retire_lookup(s->texture_engine, &s->texture_cleanup);
        s->texture_pending_owned = mc_client_texture_lookup_pending(s->texture_engine);
        if (status != MC_CLIENT_LOOKUP_OK) {
            e->texture = &s->texture_cleanup;
            e->status = MC_CLIENT_TILE_RENDERER_TEXTURE_FAILED;
            e->operation = "PO reached texture lookup retirement";
            e->native_policy = true;
            return false;
        }
    }
    if (s->font != NULL) {
        if (mc_client_bitmap_font_release(&s->font, &s->font_cleanup) != MC_CLIENT_BITMAP_FONT_OK) {
            e->font = &s->font_cleanup;
            e->status = MC_CLIENT_TILE_RENDERER_FONT_FAILED;
            e->operation = "PO reached Font occurrence retirement";
            e->native_policy = true;
            return false;
        }
        c->error.font = NULL;
    }
    if (s->texture_engine != NULL) {
        if (mc_client_render_engine_release(s->texture_engine) != MC_CLIENT_RENDER_ENGINE_OK) {
            e->status = MC_CLIENT_TILE_RENDERER_ENGINE_FAILED;
            e->operation = "PO reached Engine occurrence retirement";
            e->native_policy = true;
            return false;
        }
        s->texture_engine = NULL;
        c->error.texture = NULL;
    }
    return true;
}

void tile_draw_sign_free(mc_client_tile_renderer_draw_call *c) {
    if (c->sign != NULL) {
        /* These infallible releases wait until all outer fallible retirement
         * has completed; another refusal cannot strand borrowed World state. */
        while (c->sign->world_count != 0)
            mc_world_runtime_binding_release(c->sign->worlds[--c->sign->world_count]);
        mc_tile_entity_name_release(c->sign->line);
        free(c->sign->decorated);
        free(c->sign);
        c->sign = NULL;
    }
}
