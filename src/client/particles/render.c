#include "render_internal.h"

#include "../../entity/entity_internal.h"
#include "../display/source_surface.h"
#include "../render/interpolation_internal.h"
#include "../render_engine/internal.h"
#include "../render_engine/nw.h"
#include "mc/core/java/java.h"
#include "mc/core/math/math.h"

#include <limits.h>
#include <stdlib.h>

static const char draw_descriptor[] = "(Lsn;F)V";

bool mc_effect_draw_fail(mc_client_effect_draw_call *c, mc_effect_draw_status status,
                         const char *operation, unsigned int pc, bool native) {
    c->state.source_pc = pc;
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site =
        (mc_client_graphics_site){"dn", c->state.lit ? "b" : "a", draw_descriptor, (int32_t)pc};
    c->failure.native_policy = native;
    return false;
}

bool mc_effect_draw_fp(mc_client_effect_draw_call *c, const char *source, unsigned int pc) {
    if ((_mm_getcsr() & 0xffc0u) == 0x1f80u)
        return true;
    mc_effect_draw_fail(c, MC_EFFECT_DRAW_UNSUPPORTED_FP,
                        "particle draw reached strict floating controls", pc, true);
    c->failure.site.class_name = source;
    c->failure.site.method = "a";
    return false;
}

static bool services(mc_client_effect_draw_call *c, const mc_effect_draw_services *supplied,
                     unsigned int pc) {
    if (c->services_captured)
        return true;
    if (supplied == NULL)
        return mc_effect_draw_fail(c, MC_EFFECT_DRAW_DEPENDENCY,
                                   "particle draw reached native namespace services", pc, true);
    c->services = *supplied;
    c->services_captured = true;
    return true;
}

bool mc_effect_draw_math(mc_client_effect_draw_call *c, unsigned int pc) {
    if (c->math == NULL) {
        if (c->services.math == NULL)
            return mc_effect_draw_fail(c, MC_EFFECT_DRAW_DEPENDENCY,
                                       "particle draw reached canonical Math namespace", pc, true);
        if (mc_math_classes_retain(c->services.math, &c->failure.math) != MC_MATH_CLASS_OK)
            return mc_effect_draw_fail(c, MC_EFFECT_DRAW_BUSY,
                                       "particle draw actual Math occurrence", pc, true);
        c->math = c->services.math;
    }
    if (mc_math_classes_ensure(c->math, &c->failure.math) == MC_MATH_CLASS_OK)
        return true;
    mc_effect_draw_fail(c, MC_EFFECT_DRAW_CHILD_FAILED, c->failure.math.operation, pc, true);
    c->failure.child =
        (mc_client_graphics_site){"in", "<clinit>", "()V", c->failure.math.source_pc};
    return false;
}

static mc_effect_particle_list *list_at(mc_client_effect_draw_call *c, unsigned int array_pc,
                                        unsigned int list_pc) {
    mc_client_effect_renderer *r = c->state.owner;
    c->state.source_pc = array_pc;
    if (r->layers == NULL) {
        mc_effect_draw_fail(c, MC_EFFECT_DRAW_SOURCE_NULL, "actual DN particle List array receiver",
                            array_pc, false);
        return NULL;
    }
    mc_effect_particle_list *list = r->layers[(size_t)c->state.layer];
    c->state.source_pc = list_pc;
    if (list == NULL) {
        mc_effect_draw_fail(c, MC_EFFECT_DRAW_SOURCE_NULL, "actual DN particle List receiver",
                            list_pc, false);
        return NULL;
    }
    if (list->size > list->capacity || list->capacity > (size_t)INT32_MAX ||
        list->capacity > SIZE_MAX / sizeof(*list->elements) ||
        list->capacity * sizeof(*list->elements) > r->allocation_bytes ||
        ((list->capacity == 0) != (list->elements == NULL))) {
        mc_effect_draw_fail(c, MC_EFFECT_DRAW_LIMIT, "particle List native owning storage bounds",
                            list_pc, true);
        return NULL;
    }
    return list;
}

static bool count(mc_client_effect_draw_call *c, unsigned int array_pc, unsigned int size_pc,
                  int32_t *out) {
    const mc_effect_particle_list *list = list_at(c, array_pc, size_pc);
    if (list == NULL)
        return false;
    ++c->state.size_reads;
    *out = (int32_t)list->size;
    return true;
}

static void release_current(mc_client_effect_draw_call *c) {
    mc_entity_release(c->current);
    c->state.current = c->current = NULL;
}

static bool get(mc_client_effect_draw_call *c, unsigned int array_pc, unsigned int get_pc) {
    mc_effect_particle_list *list = list_at(c, array_pc, get_pc);
    if (list == NULL)
        return false;
    if (c->state.index < 0 || (size_t)c->state.index >= list->size)
        return mc_effect_draw_fail(c, MC_EFFECT_DRAW_SOURCE_INDEX,
                                   "actual DN particle List.get index", get_pc, false);
    mc_entity *particle = list->elements[(size_t)c->state.index];
    if (particle != NULL) {
        if (!mc_entity_is_kind(particle, MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE) ||
            particle->classes != c->state.owner->classes || particle->particle == NULL)
            return mc_effect_draw_fail(c, MC_EFFECT_DRAW_DEPENDENCY,
                                       "particle List reached unsupported concrete owning row",
                                       c->state.lit ? 54 : 272, true);
        c->failure.entity.status = mc_entity_retain(particle);
        if (c->failure.entity.status != MC_ENTITY_OK)
            return mc_effect_draw_fail(c, MC_EFFECT_DRAW_BUSY,
                                       "particle draw current Entity occurrence", get_pc, true);
    }
    c->state.current = c->current = particle;
    ++c->state.visited;
    return true;
}

static bool nw(mc_client_effect_draw_call *c, const mc_effect_draw_services *supplied,
               unsigned int pc) {
    if (!services(c, supplied, pc))
        return false;
    if (c->graphics == NULL) {
        mc_client_render_engine_classes *classes = c->services.graphics;
        if (classes == NULL || classes->resource_retirement_started)
            return mc_effect_draw_fail(c, MC_EFFECT_DRAW_DEPENDENCY,
                                       "particle draw reached actual graphics namespace", pc, true);
        if (classes->scene_lease_count == SIZE_MAX ||
            mc_client_render_engine_classes_retain(classes) != MC_CLIENT_RENDER_ENGINE_OK)
            return mc_effect_draw_fail(c, MC_EFFECT_DRAW_LIMIT, "particle draw graphics occurrence",
                                       pc, true);
        ++classes
              ->scene_lease_count; /* This actual retained draw owns one native resource lease. */
        c->graphics_lease = true;
        c->graphics = classes;
    }
    const mc_client_graphics_site caller = {"dn", c->state.lit ? "b" : "a", draw_descriptor,
                                            (int32_t)pc};
    if (!mc_client_nw_active_use(c->graphics, caller, &c->nw, &c->failure.graphics)) {
        mc_effect_draw_fail(c, MC_EFFECT_DRAW_CHILD_FAILED, c->failure.graphics.operation, pc,
                            c->failure.graphics.native_policy);
        c->failure.child = c->failure.graphics.site;
        return false;
    }
    c->state.source_pc = pc;
    c->state.nw = c->nw;
    c->state.nw_captured = true;
    return true;
}

static bool lit(mc_client_effect_draw_call *c, const mc_effect_draw_services *supplied) {
    c->state.layer = 3;
    int32_t size;
    if (!count(c, 7, 8, &size))
        return false;
    if (size == 0) {
        c->state.empty = true;
        c->state.source_pc = 16;
        c->state.returned = true;
        return true;
    }
    if (!nw(c, supplied, 17))
        return false;
    for (c->state.index = 0;; c->state.index = mc_java_i32_add(c->state.index, 1)) {
        if (!count(c, 32, 33, &size))
            return false;
        if (c->state.index >= size)
            break;
        if (!get(c, 46, 49) || !mc_effect_draw_particle(c, 69))
            return false;
        release_current(c);
    }
    c->state.source_pc = 78;
    c->state.returned = true;
    return true;
}

static bool angle(mc_client_effect_draw_call *c, const mc_effect_draw_services *supplied,
                  bool pitch, bool cosine, unsigned int field_pc, unsigned int mul_pc,
                  unsigned int div_pc, unsigned int invoke_pc, float *out) {
    if (c->state.camera == NULL)
        return mc_effect_draw_fail(c, MC_EFFECT_DRAW_SOURCE_NULL,
                                   "actual DN camera orientation receiver", field_pc, false);
    const mc_entity_base_fields *fields = mc_entity_runtime_fields((mc_entity *)c->state.camera);
    const float degrees = pitch ? fields->aT : fields->yaw;
    if (!mc_effect_draw_fp(c, "dn", mul_pc))
        return false;
    const float product = mc_client_interpolation_fmul(degrees, 3.1415927f);
    c->state.source_pc = div_pc;
    const float radians = at_fdiv(product, 180.0f);
    if (!services(c, supplied, invoke_pc) || !mc_effect_draw_math(c, invoke_pc))
        return false;
    *out = cosine ? mc_math_cos(mc_math_classes_table(c->math), radians)
                  : mc_math_sin(mc_math_classes_table(c->math), radians);
    ++c->state.orientation_values;
    return true;
}

static bool origins(mc_client_effect_draw_call *c, const mc_effect_draw_services *supplied) {
    if (!angle(c, supplied, false, true, 1, 6, 9, 10, &c->state.cos_yaw) ||
        !angle(c, supplied, false, false, 15, 20, 23, 24, &c->state.sin_yaw))
        return false;
    /* The fneg31 precedes the next actual camera pitch read33. */
    const float negative_sine = at_fneg(c->state.sin_yaw);
    float sine_pitch;
    if (!angle(c, supplied, true, false, 33, 38, 41, 42, &sine_pitch))
        return false;
    c->state.horizontal_y = mc_client_interpolation_fmul(negative_sine, sine_pitch);
    if (!angle(c, supplied, true, false, 50, 55, 58, 59, &sine_pitch))
        return false;
    c->state.horizontal_x = mc_client_interpolation_fmul(c->state.cos_yaw, sine_pitch);
    if (!angle(c, supplied, true, true, 66, 71, 74, 75, &c->state.cos_pitch))
        return false;
    static const unsigned int first[] = {81, 101, 121};
    static const int32_t sub[] = {92, 112, 132}, widen[] = {94, 114, 134};
    static const int32_t mul[] = {95, 115, 135}, add[] = {96, 116, 136};
    static const unsigned int put[] = {97, 117, 137};
    for (unsigned int axis = 0; axis < 3; ++axis) {
        if (c->state.camera == NULL)
            return mc_effect_draw_fail(c, MC_EFFECT_DRAW_SOURCE_NULL,
                                       "actual DN camera coordinate receiver", first[axis], false);
        const mc_entity_base_fields *fields =
            mc_entity_runtime_fields((mc_entity *)c->state.camera);
        const double previous = axis == 0 ? fields->bl : axis == 1 ? fields->bm : fields->bn;
        const double current = axis == 0 ? fields->x : axis == 1 ? fields->y : fields->z;
        mc_client_interpolation_error arithmetic;
        double value;
        if (!mc_client_interpolate_double(previous, current, c->state.partial, sub[axis],
                                          widen[axis], mul[axis], add[axis], &value, &arithmetic))
            return mc_effect_draw_fail(c,
                                       arithmetic.status == MC_CLIENT_INTERPOLATION_UNSUPPORTED_FP
                                           ? MC_EFFECT_DRAW_UNSUPPORTED_FP
                                           : MC_EFFECT_DRAW_UNSUPPORTED_NUMERIC,
                                       "DN origin reached interpolation admission",
                                       (unsigned int)arithmetic.pc, true);
        if (mc_entity_particle_origin_store(c->state.owner->classes, axis, value,
                                            &c->failure.entity) != MC_ENTITY_OK)
            return mc_effect_draw_fail(c, MC_EFFECT_DRAW_CHILD_FAILED, c->failure.entity.operation,
                                       put[axis], true);
        ++c->state.origins_stored;
        c->state.last_store_pc = put[axis];
        (void)mc_entity_particle_origin_read(c->state.owner->classes, &c->state.origin, NULL);
    }
    return true;
}

static bool texture(mc_client_effect_draw_call *c) {
    static const uint16_t layer0[] = {'/', 'p', 'a', 'r', 't', 'i', 'c',
                                      'l', 'e', 's', '.', 'p', 'n', 'g'};
    static const uint16_t layer1[] = {'/', 't', 'e', 'r', 'r', 'a', 'i', 'n', '.', 'p', 'n', 'g'};
    static const uint16_t layer2[] = {'/', 'g', 'u', 'i', '/', 'i', 't',
                                      'e', 'm', 's', '.', 'p', 'n', 'g'};
    static const unsigned int invoke[] = {181, 198, 215};
    mc_client_render_engine *engine = c->state.owner->render_engine;
    const size_t layer = (size_t)c->state.layer;
    if (engine == NULL)
        return mc_effect_draw_fail(c, MC_EFFECT_DRAW_SOURCE_NULL,
                                   "actual DN texture Engine receiver", invoke[layer], false);
    size_t index = 0;
    while (index < c->engine_count && c->engines[index] != engine)
        ++index;
    if (index == c->engine_count) {
        if (index == 3 || mc_client_render_engine_retain(engine) != MC_CLIENT_RENDER_ENGINE_OK)
            return mc_effect_draw_fail(c, MC_EFFECT_DRAW_BUSY,
                                       "DN captured texture Engine occurrence", invoke[layer],
                                       true);
        c->engines[c->engine_count++] = engine;
    }
    const mc_texture_pack_text key =
        layer == 0   ? (mc_texture_pack_text){layer0, sizeof(layer0) / sizeof(*layer0), false}
        : layer == 1 ? (mc_texture_pack_text){layer1, sizeof(layer1) / sizeof(*layer1), false}
                     : (mc_texture_pack_text){layer2, sizeof(layer2) / sizeof(*layer2), false};
    mc_client_texture id;
    const bool prior_pending = mc_client_texture_lookup_pending(engine);
    const mc_client_texture_lookup_status status =
        mc_client_render_engine_texture(engine, c->services.display, NULL, key, &id, &c->texture);
    if (!prior_pending && mc_client_texture_lookup_pending(engine))
        c->pending_lookup = engine; /* Only this invocation's actual new owning prefix. */
    if (status != MC_CLIENT_LOOKUP_OK) {
        mc_effect_draw_fail(c, MC_EFFECT_DRAW_CHILD_FAILED, c->texture.operation, invoke[layer],
                            c->texture.native_policy);
        c->failure.texture = &c->texture;
        c->failure.child = c->texture.site;
        return false;
    }
    if (!mc_client_display_source_bind_id(c->services.display, id, &c->failure.display))
        return mc_effect_draw_fail(c, MC_EFFECT_DRAW_CHILD_FAILED, "DN actual glBindTexture child",
                                   225, true);
    if (c->pending_lookup != NULL &&
        mc_client_render_engine_retire_lookup(engine, &c->texture_cleanup) != MC_CLIENT_LOOKUP_OK) {
        c->failure.texture = &c->texture_cleanup;
        return mc_effect_draw_fail(c, MC_EFFECT_DRAW_CLEANUP_FAILED,
                                   "DN completed texture lookup retirement", 225, true);
    }
    c->pending_lookup = NULL;
    return true;
}

static bool sprites(mc_client_effect_draw_call *c, const mc_effect_draw_services *supplied) {
    if (!origins(c, supplied))
        return false;
    for (c->state.layer = 0; c->state.layer < 3;
         c->state.layer = mc_java_i32_add(c->state.layer, 1)) {
        int32_t size;
        if (!count(c, 155, 156, &size))
            return false;
        if (size == 0)
            continue;
        if (!texture(c) || !nw(c, supplied, 228))
            return false;
        if (!mc_client_nw_begin_quads(c->nw, &c->failure.graphics)) {
            mc_effect_draw_fail(c, MC_EFFECT_DRAW_CHILD_FAILED, c->failure.graphics.operation, 235,
                                c->failure.graphics.native_policy);
            c->failure.child = c->failure.graphics.site;
            return false;
        }
        for (c->state.index = 0;; c->state.index = mc_java_i32_add(c->state.index, 1)) {
            if (!count(c, 249, 250, &size))
                return false;
            if (c->state.index >= size)
                break;
            if (!get(c, 264, 267) || !mc_effect_draw_particle(c, 291))
                return false;
            release_current(c);
        }
        if (!mc_client_nw_draw(c->nw, c->services.display, &c->failure.graphics)) {
            mc_effect_draw_fail(c, MC_EFFECT_DRAW_CHILD_FAILED, c->failure.graphics.operation, 302,
                                c->failure.graphics.native_policy);
            c->failure.child = c->failure.graphics.site;
            return false;
        }
    }
    c->state.empty = c->state.visited == 0;
    c->state.source_pc = 311;
    c->state.returned = true;
    return true;
}

static mc_effect_draw_status begin(mc_client_effect_renderer *r, mc_entity *camera, float partial,
                                   bool is_lit, const mc_effect_draw_services *supplied,
                                   mc_client_effect_draw_call **out, mc_effect_draw_error *error) {
    mc_effect_draw_error local = {0};
    if (error == NULL)
        error = &local;
    *error = (mc_effect_draw_error){0};
    if (r == NULL || out == NULL || *out != NULL) {
        error->status = MC_EFFECT_DRAW_ARGUMENT;
        error->operation = "particle draw native owner/output";
        error->native_policy = true;
        return error->status;
    }
    if (r->entered || r->draw_call != NULL || r->phase != MC_EFFECT_RENDERER_READY) {
        error->status = MC_EFFECT_DRAW_BUSY;
        error->operation = "particle draw native pending/entered/constructed owner";
        error->native_policy = true;
        return error->status;
    }
    mc_client_effect_draw_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        error->status = MC_EFFECT_DRAW_ALLOCATION;
        error->operation = "particle draw native call allocation";
        error->native_policy = true;
        return error->status;
    }
    error->owner = mc_client_effect_renderer_retain(r);
    if (error->owner != MC_EFFECT_RENDERER_OK) {
        free(c);
        error->status = MC_EFFECT_DRAW_BUSY;
        error->operation = "particle draw native manager occurrence";
        error->native_policy = true;
        return error->status;
    }
    c->state = (mc_effect_draw_state){.lit = is_lit,
                                      .owner = r,
                                      .camera = camera,
                                      .partial = partial,
                                      .failure = &c->failure,
                                      .cleanup = &c->cleanup};
    r->draw_call = c;
    *out = c;
    c->state.active = r->entered = true;
    (void)(is_lit ? lit(c, supplied) : sprites(c, supplied));
    c->state.active = r->entered = false;
    *error = c->failure;
    return error->status;
}

mc_effect_draw_status mc_client_effect_renderer_lit_begin(mc_client_effect_renderer *r,
                                                          mc_entity *camera, float partial,
                                                          const mc_effect_draw_services *supplied,
                                                          mc_client_effect_draw_call **out,
                                                          mc_effect_draw_error *error) {
    return begin(r, camera, partial, true, supplied, out, error);
}

mc_effect_draw_status
mc_client_effect_renderer_sprites_begin(mc_client_effect_renderer *r, mc_entity *camera,
                                        float partial, const mc_effect_draw_services *supplied,
                                        mc_client_effect_draw_call **out,
                                        mc_effect_draw_error *error) {
    return begin(r, camera, partial, false, supplied, out, error);
}

bool mc_client_effect_renderer_draw_inspect(const mc_client_effect_draw_call *c,
                                            mc_effect_draw_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

mc_effect_draw_status mc_client_effect_renderer_draw_retire(mc_client_effect_draw_call **slot,
                                                            mc_effect_draw_error *error) {
    mc_effect_draw_error local = {0};
    if (error == NULL)
        error = &local;
    *error = (mc_effect_draw_error){0};
    if (slot == NULL) {
        error->status = MC_EFFECT_DRAW_ARGUMENT;
        error->operation = "particle draw retirement slot";
        error->native_policy = true;
        return error->status;
    }
    mc_client_effect_draw_call *c = *slot;
    if (c == NULL)
        return MC_EFFECT_DRAW_OK;
    if (c->state.active || c->state.owner->entered || c->state.owner->draw_call != c) {
        error->status = MC_EFFECT_DRAW_BUSY;
        error->operation = "particle draw active/changed retirement";
        error->native_policy = true;
        return error->status;
    }
    c->state.retiring = true;
    if (c->manager != NULL &&
        mc_client_render_manager_release(&c->manager, &c->cleanup.manager) != MC_CLIENT_MANAGER_OK)
        goto refused;
    if (c->world_binding != NULL) {
        mc_world_runtime_binding_release(c->world_binding);
        c->world_binding = NULL;
    }
    release_current(c);
    if (c->pending_lookup != NULL) {
        const mc_client_texture_lookup_status status =
            mc_client_render_engine_retire_lookup(c->pending_lookup, &c->texture_cleanup);
        if (!mc_client_texture_lookup_pending(c->pending_lookup))
            c->pending_lookup = NULL;
        if (status != MC_CLIENT_LOOKUP_OK) {
            c->cleanup.texture = &c->texture_cleanup;
            goto refused;
        }
    }
    while (c->engine_count != 0) {
        const size_t i = c->engine_count - 1;
        if (mc_client_render_engine_release(c->engines[i]) != MC_CLIENT_RENDER_ENGINE_OK)
            goto refused;
        c->engines[i] = NULL;
        --c->engine_count;
    }
    if (c->math != NULL && mc_math_classes_release(&c->math, &c->cleanup.math) != MC_MATH_CLASS_OK)
        goto refused;
    if (c->graphics != NULL) {
        if (c->graphics_lease) {
            if (c->graphics->scene_lease_count == 0)
                goto refused;
            --c->graphics->scene_lease_count;
            c->graphics_lease = false;
            c->state.nw = c->nw = NULL;
        }
        if (mc_client_render_engine_classes_release(c->graphics) != MC_CLIENT_RENDER_ENGINE_OK)
            goto refused;
        c->graphics = NULL;
        c->state.nw = c->nw = NULL;
    }
    if (mc_client_effect_renderer_release(c->state.owner) != MC_EFFECT_RENDERER_OK)
        goto refused;
    c->state.owner->draw_call = NULL;
    free(c);
    *slot = NULL;
    return MC_EFFECT_DRAW_OK;
refused:
    c->cleanup.status = MC_EFFECT_DRAW_CLEANUP_FAILED;
    c->cleanup.operation = "particle draw checked remaining owner retirement";
    c->cleanup.native_policy = true;
    *error = c->cleanup;
    return error->status;
}
