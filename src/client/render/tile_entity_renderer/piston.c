#include "draw_internal.h"

#include "../blocks/internal.h"
#include "../interpolation_internal.h"
#include "block/registry/registry_internal.h"
#include "block/registry/runtime_internal.h"
#include "client/display/fog_surface.h"
#include "client/display/source_surface.h"
#include "client/display/world_frame_surface.h"
#include "client/render_engine/lighting.h"
#include "client/render_engine/texture_lookup.h"
#include "entity/render/internal.h"
#include "mc/block/registry/runtime.h"
#include "world/runtime/construction_internal.h"
#include "world/runtime/tile_entities/piston_list_internal.h"
#include "world/runtime/tile_entities/piston_math_internal.h"

#include <stdlib.h>
#include <string.h>

/* Reached HY-only native storage. Culled Pistons and all Spawner/Sign draws
 * allocate none of this CV/query/texture diagnostic graph. */
struct tile_draw_piston_call {
    mc_client_tile_renderer_cv *cv;
    mc_cv_context blocks;
    mc_client_render_global_error blocks_cleanup;
    mc_client_render_engine *texture_engine;
    bool texture_pending_owned;
    bool exception_from_blocks;
    mc_client_texture_lookup_error texture_error, texture_cleanup;
    mc_client_lighting_error lighting_error;
    mc_client_application_query_error application_query_error;
};

static bool allocate_call(mc_client_tile_renderer_draw_call *c) {
    if (c->state.allocation_bytes > c->limits.max_bytes ||
        sizeof(*c->piston) > c->limits.max_bytes - c->state.allocation_bytes)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_LIMIT,
                              "HY reached native draw workspace limit", true);
    c->piston = calloc(1, sizeof(*c->piston));
    if (c->piston == NULL)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_ALLOCATION_FAILED,
                              "HY reached native draw workspace allocation", true);
    c->state.allocation_bytes += sizeof(*c->piston);
    return true;
}

bool tile_draw_piston_retire(mc_client_tile_renderer_draw_call *c,
                             mc_client_tile_renderer_draw_error *e) {
    tile_draw_piston_call *p = c->piston;
    if (p == NULL)
        return true;
    const bool blocks_retired = mc_cv_retire(&p->blocks, &p->blocks_cleanup);
    if (p->exception_from_blocks)
        c->error.exception_class = p->blocks.exception_class;
    if (!blocks_retired) {
        e->status = MC_CLIENT_TILE_RENDERER_BLOCKS_FAILED;
        e->operation = "HY reached CV child retirement";
        e->native_policy = true;
        e->blocks = &p->blocks_cleanup;
        return false;
    }
    if (p->texture_pending_owned) {
        const mc_client_texture_lookup_status status =
            mc_client_render_engine_retire_lookup(p->texture_engine, &p->texture_cleanup);
        p->texture_pending_owned = mc_client_texture_lookup_pending(p->texture_engine);
        if (status != MC_CLIENT_LOOKUP_OK) {
            e->texture = &p->texture_cleanup;
            e->status = MC_CLIENT_TILE_RENDERER_TEXTURE_FAILED;
            e->operation = "HY reached texture lookup retirement";
            e->native_policy = true;
            return false;
        }
    }
    if (p->texture_engine != NULL) {
        if (mc_client_render_engine_release(p->texture_engine) != MC_CLIENT_RENDER_ENGINE_OK) {
            e->status = MC_CLIENT_TILE_RENDERER_ENGINE_FAILED;
            e->operation = "HY reached texture Engine retirement";
            e->native_policy = true;
            return false;
        }
        p->texture_engine = NULL;
    }
    if (p->cv != NULL && !tile_renderer_cv_release_draw(&p->cv, c, &e->owner)) {
        e->status = e->owner.status;
        e->operation = "HY reached CV occurrence retirement";
        e->native_policy = true;
        return false;
    }
    return true;
}

void tile_draw_piston_free(mc_client_tile_renderer_draw_call *c) {
    free(c->piston);
    c->piston = NULL;
}

static const char descriptor[] = "(Luk;DDDF)V";

static void at(mc_client_tile_renderer_draw_call *c, int32_t pc) {
    tile_draw_at(c, "hy", descriptor, pc);
}

static bool source_null(mc_client_tile_renderer_draw_call *c, const char *operation) {
    c->error.source_exception = true;
    c->error.exception_class = "java/lang/NullPointerException";
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE, operation, false);
}

static bool registry_result(mc_client_tile_renderer_draw_call *c, mc_registry_classes *classes,
                            mc_registry_status status) {
    if (status == MC_REGISTRY_OK)
        return true;
    mc_entity_render_throwable throwable = MC_ENTITY_RENDER_NO_THROWABLE;
    mc_entity_render_registry_classification(classes, &c->error.registry, &throwable,
                                             &c->error.exception_class);
    c->error.source_exception = throwable != MC_ENTITY_RENDER_NO_THROWABLE;
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_BLOCKS_FAILED, c->error.registry.message,
                          !c->error.source_exception);
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

static void graphics_exception(mc_client_tile_renderer_draw_call *c,
                               const mc_client_graphics_error *error) {
    const char *name = error->native_policy ? NULL : graphics_exception_class(error->status);
    if (error->status == MC_CLIENT_GRAPHICS_CLASS_FAILED && !error->native_policy) {
        const mc_client_graphics_cause *cause = &error->class_cause;
        if (cause->status == MC_CLIENT_GRAPHICS_CLASS_FAILED)
            cause = &error->dependency_class_cause;
        if (!cause->native_policy && graphics_exception_class(cause->status) != NULL)
            name = "java/lang/NoClassDefFoundError";
    }
    c->error.source_exception = name != NULL;
    c->error.exception_class = name;
}

static bool graphics(mc_client_tile_renderer_draw_call *c, bool ok) {
    if (ok)
        return true;
    c->error.child = c->error.graphics.site;
    graphics_exception(c, &c->error.graphics);
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_GRAPHICS_FAILED, c->error.graphics.operation,
                          !c->error.source_exception);
}

static uint32_t bits(float value) {
    uint32_t out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static float value(uint32_t raw) {
    float out;
    memcpy(&out, &raw, sizeof(out));
    return out;
}

static bool progress(mc_client_tile_renderer_draw_call *c, int32_t pc, float *out) {
    at(c, pc);
    uint32_t raw;
    c->error.owner.tile_entity = mc_tile_entity_piston_progress(
        c->tile_entity, bits(c->state.partial), &raw, &c->error.piston);
    if (c->error.owner.tile_entity != MC_TILE_ENTITY_OK) {
        c->error.child =
            (mc_client_graphics_site){"uk", "a", "(F)F", (int32_t)c->error.piston.getter_bytecode};
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                              "HY actual UK progress getter", true);
    }
    *out = value(raw);
    return true;
}

static bool narrow(mc_client_tile_renderer_draw_call *c, double input, int32_t pc, float *out) {
    at(c, pc);
    if (!tile_draw_fp(c, pc))
        return false;
    *out = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), _mm_set_sd(input)));
    return true;
}

/* Inherited JE.a(String) keeps the same HY invocation site and retained
 * actual Engine/lookup occurrences as the shared entered helper. */
static bool tile_draw_piston_texture(mc_client_tile_renderer_draw_call *c) {
    static const uint16_t key[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '.', 'p', 'n', 'g'};
    at(c, 34);
    return tile_draw_texture(c, key, sizeof(key) / sizeof(*key), &c->piston->texture_engine,
                             &c->piston->texture_pending_owned, &c->piston->texture_error);
}

/* Each axis is a fresh actual UK method invocation. Its Direction getstatic
 * precedes the fresh raw facing field and reached iaload; malformed facing is
 * never normalized into a valid direction. */
static bool offset(mc_client_tile_renderer_draw_call *c, size_t axis, int32_t pc, float *out) {
    at(c, pc);
    static const char *const methods[] = {"b", "c", "d"};
    static const mc_registry_direction_field fields[] = {
        MC_REGISTRY_DIRECTION_B, MC_REGISTRY_DIRECTION_C, MC_REGISTRY_DIRECTION_D};
    mc_tile_entity_snapshot tile;
    if (!tile_draw_snapshot(c, &tile, pc))
        return false;
    const bool extending = tile.piston_extending;
    uint32_t raw;
    c->error.child = (mc_client_graphics_site){"uk", methods[axis], "(F)F", extending ? 9 : 28};
    c->error.owner.tile_entity = mc_tile_entity_piston_progress(
        c->tile_entity, bits(c->state.partial), &raw, &c->error.piston);
    if (c->error.owner.tile_entity != MC_TILE_ENTITY_OK) {
        c->error.helper =
            (mc_client_graphics_site){"uk", "a", "(F)F", (int32_t)c->error.piston.getter_bytecode};
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                              "UK offset actual progress invocation", true);
    }
    c->error.child.pc = extending ? 13 : 31;
    if (!mc_piston_math_admitted())
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_UNSUPPORTED_FP,
                              "UK offset strict floating environment", true);
    const float amount = extending ? mc_piston_float_subtract(value(raw), 1.0f)
                                   : mc_piston_float_subtract(1.0f, value(raw));
    mc_world_runtime_classes *code = mc_tile_entity_piston_code_classes_held(c->tile_entity);
    if (code == NULL)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_INVALID_PHASE,
                              "UK offset defining code namespace", true);
    mc_registry_direction_view direction;
    c->error.child.pc = extending ? 14 : 32;
    if (!registry_result(c, code->registry,
                         mc_registry_direction_get(code->registry, fields[axis], &direction,
                                                   &c->error.registry)))
        return false;
    if (!tile_draw_snapshot(c, &tile, extending ? 18 : 36))
        return false;
    int32_t component;
    c->error.child.pc = extending ? 21 : 39;
    if (!registry_result(c, code->registry,
                         mc_registry_direction_at(&direction, tile.piston_facing, &component,
                                                  &c->error.registry)))
        return false;
    c->error.child.pc = extending ? 23 : 41;
    if (!mc_piston_math_admitted())
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_UNSUPPORTED_FP,
                              "UK offset strict floating environment", true);
    *out = mc_piston_float_multiply(amount, (float)component);
    c->error.child = (mc_client_graphics_site){0};
    return true;
}

static bool translation(mc_client_tile_renderer_draw_call *c, mc_client_tessellator *t,
                        bool moving) {
    static const int32_t narrow_pc[2][3] = {{271, 281, 291}, {90, 107, 124}};
    static const int32_t field_pc[2][3] = {{273, 283, 293}, {92, 109, 126}};
    static const int32_t sub_pc[2][3] = {{277, 287, 297}, {96, 113, 130}};
    static const int32_t offset_pc[3] = {100, 117, 134}, add_pc[3] = {103, 120, 137};
    const double coordinates[3] = {c->state.x, c->state.y, c->state.z};
    double result[3];
    for (size_t axis = 0; axis < 3; ++axis) {
        float coordinate;
        mc_tile_entity_snapshot tile;
        if (!narrow(c, coordinates[axis], narrow_pc[moving][axis], &coordinate) ||
            !tile_draw_snapshot(c, &tile, field_pc[moving][axis]) ||
            !tile_draw_fp(c, sub_pc[moving][axis]))
            return false;
        const int32_t integer = axis == 0 ? tile.x : axis == 1 ? tile.y : tile.z;
        float translated = mc_piston_float_subtract(coordinate, (float)integer);
        if (moving) {
            float delta;
            if (!offset(c, axis, offset_pc[axis], &delta) || !tile_draw_fp(c, add_pc[axis]))
                return false;
            translated = mc_piston_float_add(translated, delta);
        }
        result[axis] = mc_client_interpolation_widen(translated);
    }
    at(c, moving ? 139 : 299);
    if (!graphics(c,
                  mc_client_nw_translation(t, result[0], result[1], result[2], &c->error.graphics)))
        return false;
    memcpy(c->state.piston_translation, result, sizeof(result));
    return true;
}

static bool blocks(mc_client_tile_renderer_draw_call *c, const mc_block *block, int32_t pc,
                   int branch, bool full_rod) {
    at(c, pc);
    if (c->piston->cv == NULL) {
        if (!tile_renderer_cv_capture_entered(c->renderer, &c->piston->cv, &c->error.owner))
            return tile_draw_fail(c, c->error.owner.status, c->error.owner.operation, true);
        if (c->piston->cv == NULL)
            return source_null(c, "HY actual null CV invocation receiver");
        c->piston->blocks =
            (mc_cv_context){.fields = &c->piston->cv->fields,
                            .world = c->piston->cv->world_occurrence,
                            .code_classes = c->dispatcher->classes->world_classes,
                            .registry = c->dispatcher->classes->world_classes->registry,
                            .models = c->dispatcher->classes->models,
                            .graphics = c->services.graphics,
                            .display = c->services.display,
                            .application_classes = c->services.application_classes};
        c->state.piston_cv_identity = (uintptr_t)&c->piston->cv->fields;
    }
    mc_tile_entity_snapshot tile;
    if (!tile_draw_snapshot(c, &tile, pc))
        return false;
    mc_cv_context *cv = &c->piston->blocks;
    const bool ok = branch == 0 ? mc_cv_piston_head_all(cv, block, tile.x, tile.y, tile.z, full_rod)
                    : branch == 1 ? mc_cv_piston_base_all(cv, block, tile.x, tile.y, tile.z)
                                  : mc_cv_render_all(cv, block, tile.x, tile.y, tile.z);
    c->state.blocks_capture_attempted = cv->capture_attempted;
    c->state.blocks_capture_status = cv->capture_status;
    if (!ok) {
        c->error.blocks = &cv->failure;
        c->error.blocks_world = &cv->world_error;
        c->error.child = (mc_client_graphics_site){
            cv->failure.source_class, cv->failure.source_method, cv->failure.source_descriptor,
            (int32_t)cv->failure.source_pc};
        c->error.source_exception = !cv->failure.native_policy;
        c->error.exception_class = cv->exception_class;
        c->piston->exception_from_blocks = cv->exception_class != NULL;
        if (cv->failure.status == MC_CLIENT_RENDER_GLOBAL_NULL_REFERENCE)
            c->error.exception_class = "java/lang/NullPointerException";
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_BLOCKS_FAILED, cv->failure.operation,
                              cv->failure.native_policy);
    }
    if (branch == 0)
        c->state.piston_head_returned = true;
    else if (branch == 1)
        c->state.piston_base_returned = true;
    else
        c->state.piston_block_returned = true;
    return true;
}

bool tile_draw_piston(mc_client_tile_renderer_draw_call *c) {
    tile_draw_at(c, "hy", "(Low;DDDF)V", 2);
    mc_tile_entity_snapshot tile;
    if (!tile_draw_snapshot(c, &tile, 2))
        return false;
    if (tile.kind != MC_TILE_ENTITY_PISTON) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/ClassCastException";
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                              "HY actual bridge UK checkcast", false);
    }
    c->state.bridge_cast_returned = true;
    mc_registry_classes *registry = c->dispatcher->classes->world_classes->registry;
    mc_registry_block_m_view array;
    at(c, 0);
    if (!registry_result(c, registry,
                         mc_registry_block_m_get(registry, &array, &c->error.registry)))
        return false;
    int32_t id;
    at(c, 4);
    c->error.owner.tile_entity = mc_tile_entity_piston_block_id(c->tile_entity, &id);
    if (c->error.owner.tile_entity != MC_TILE_ENTITY_OK)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                              "HY actual UK carried Block getter", true);
    const mc_block *block;
    at(c, 7);
    if (!registry_result(c, registry,
                         mc_registry_block_m_at(&array, id, &block, &c->error.registry)))
        return false;
    if (block == NULL) {
        c->state.piston_skipped = true;
        at(c, 363);
        return true;
    }
    if (!progress(c, 18, &c->state.piston_progress))
        return false;
    c->state.piston_progress_returned = true;
    /* fcmpg makes every NaN take the >=1 return branch. */
    if (!(c->state.piston_progress < 1.0f)) {
        c->state.piston_skipped = true;
        at(c, 363);
        return true;
    }
    at(c, 26);
    if (!allocate_call(c))
        return false;
    mc_client_tessellator *t;
    if (!tile_draw_graphics(c) ||
        !graphics(c, mc_client_nw_active_use(c->services.graphics, c->error.site, &t,
                                             &c->error.graphics)))
        return false;
    /* The inherited JE texture body is entered before U and every GL state
     * mutation. Its actual Engine occurrence/pending attempt is held by c. */
    if (!tile_draw_piston_texture(c))
        return false;
    at(c, 37);
    if (!graphics(c, mc_client_lighting_disable(c->services.graphics, c->services.display,
                                                c->error.site, &c->error.graphics)))
        return false;
    at(c, 46);
    if (!tile_draw_display(c, mc_client_display_source_blend_function(c->services.display, 770, 771,
                                                                      &c->error.display)))
        return false;
    at(c, 52);
    if (!tile_draw_display(
            c, mc_client_display_source_blend_enable(c->services.display, true, &c->error.display)))
        return false;
    at(c, 58);
    if (!tile_draw_display(
            c, mc_client_display_source_cull_test(c->services.display, false, &c->error.display)))
        return false;
    at(c, 61);
    mc_client_application_query_error *query = &c->piston->application_query_error;
    c->error.application_query = query;
    bool ao;
    if (mc_client_application_ambient_occlusion(c->services.application_classes, &ao, query) !=
        MC_CLIENT_APPLICATION_QUERY_OK) {
        c->error.child = (mc_client_graphics_site){query->site.class_name, query->site.method,
                                                   query->site.descriptor, (int32_t)query->site.pc};
        c->error.source_exception = query->status == MC_CLIENT_APPLICATION_QUERY_SOURCE_NULL;
        if (c->error.source_exception)
            c->error.exception_class = "java/lang/NullPointerException";
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_INVALID_PHASE, query->operation,
                              query->native_policy);
    }
    at(c, ao ? 70 : 79);
    if (!tile_draw_display(c, mc_client_display_source_shade_model(
                                  c->services.display, ao ? 7425 : 7424, &c->error.display)))
        return false;
    at(c, 84);
    if (t == NULL)
        return source_null(c, "HY actual null NW begin receiver");
    if (!graphics(c, mc_client_nw_begin_quads(t, &c->error.graphics)))
        return false;
    c->state.tessellator_begun = true;
    if (!translation(c, t, true))
        return false;
    at(c, 147);
    if (!graphics(c, mc_client_nw_color_rgb(t, 1, 1, 1, &c->error.graphics)))
        return false;
    const mc_block *head;
    at(c, 152);
    if (!registry_result(c, registry,
                         mc_registry_block_static_get(registry, "ab", &head, &c->error.registry)))
        return false;
    float p;
    bool early_head = false;
    if (block == head) {
        if (!progress(c, 161, &p))
            return false;
        early_head = p < .5f;
    }
    if (early_head) {
        if (!blocks(c, block, 189, 0, false))
            return false;
    } else {
        at(c, 196);
        bool render_head, extending = false;
        if (mc_tile_entity_piston_render_head(c->tile_entity, &render_head) != MC_TILE_ENTITY_OK)
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                                  "HY actual UK render-head getter", true);
        if (render_head) {
            at(c, 203);
            if (mc_tile_entity_piston_extending(c->tile_entity, &extending) != MC_TILE_ENTITY_OK)
                return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                                      "HY actual UK extending getter", true);
        }
        if (render_head && !extending) {
            at(c, 209);
            if (!registry_result(
                    c, registry,
                    mc_registry_block_static_get(registry, "ab", &head, &c->error.registry)))
                return false;
            at(c, 214);
            if (block->kind != MC_BLOCK_PISTON_BASE) {
                c->error.source_exception = true;
                c->error.exception_class = "java/lang/ClassCastException";
                return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_BLOCKS_FAILED,
                                      "HY actual JQ checkcast", false);
            }
            int32_t texture;
            at(c, 217);
            if (!registry_result(c, registry, mc_block_render_piston_base_texture(block, &texture)))
                return false;
            at(c, 220);
            if (head == NULL)
                return source_null(c, "HY actual null H texture setter receiver");
            if (!registry_result(
                    c, registry,
                    mc_block_render_piston_head_texture_set((mc_block *)head, texture)))
                return false;
            c->state.piston_override_set = true;
            /* Fresh UU.ab227 is evaluated independently of the setter's receiver. */
            at(c, 227);
            if (!registry_result(
                    c, registry,
                    mc_registry_block_static_get(registry, "ab", &head, &c->error.registry)) ||
                !progress(c, 245, &p) || !blocks(c, head, 259, 0, p < .5f))
                return false;
            at(c, 262);
            if (!registry_result(
                    c, registry,
                    mc_registry_block_static_get(registry, "ab", &head, &c->error.registry)))
                return false;
            at(c, 265);
            if (head == NULL)
                return source_null(c, "HY actual null H texture reset receiver");
            if (!registry_result(c, registry,
                                 mc_block_render_piston_head_texture_reset((mc_block *)head)))
                return false;
            c->state.piston_override_reset = true;
            if (!translation(c, t, false) || !blocks(c, block, 320, 1, false))
                return false;
        } else if (!blocks(c, block, 344, 2, false))
            return false;
    }
    at(c, 352);
    if (!graphics(c, mc_client_nw_translation(t, 0, 0, 0, &c->error.graphics)))
        return false;
    /* Observe the last completed Source translation, including this tail
     * reset. Geometry already contains the earlier translated coordinates. */
    for (size_t axis = 0; axis < 3; ++axis)
        c->state.piston_translation[axis] = 0.0;
    mc_client_nw_fields fields;
    if (mc_client_nw_fields_get(t, &fields))
        c->state.piston_vertices = (size_t)fields.h;
    at(c, 357);
    if (!graphics(c, mc_client_nw_draw(t, c->services.display, &c->error.graphics)))
        return false;
    c->state.tessellator_drawn = true;
    at(c, 360);
    c->error.lighting = &c->piston->lighting_error;
    if (!mc_client_lighting_enable(c->services.graphics, c->dispatcher->classes->models->entities,
                                   c->services.math, c->services.display, c->error.site,
                                   &c->piston->lighting_error)) {
        c->error.child = c->piston->lighting_error.site;
        c->error.helper = c->piston->lighting_error.child;
        if (c->piston->lighting_error.status == MC_CLIENT_LIGHTING_GRAPHICS_FAILED) {
            c->error.graphics = c->piston->lighting_error.graphics;
            graphics_exception(c, &c->error.graphics);
        }
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_LIGHTING_FAILED,
                              c->piston->lighting_error.operation, !c->error.source_exception);
    }
    at(c, 363);
    return true;
}
