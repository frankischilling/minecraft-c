#include "draw_internal.h"

#include "../interpolation_internal.h"
#include "../living_draw_internal.h"
#include "client/display/source_surface.h"
#include "client/render_engine/texture_lookup.h"
#include "mc/core/java/java.h"
#include "world/runtime/runtime_internal.h"

#include <stdlib.h>
#include <string.h>

static const char ordinary_descriptor[] = "(Low;F)V";
static const char coordinates_descriptor[] = "(Low;DDDF)V";

void tile_draw_at(mc_client_tile_renderer_draw_call *c, const char *source, const char *descriptor,
                  int32_t pc) {
    c->error.site = (mc_client_graphics_site){source, "a", descriptor, pc};
    c->state.source_pc = (unsigned int)pc;
}

bool tile_draw_fail(mc_client_tile_renderer_draw_call *c, mc_client_tile_renderer_status status,
                    const char *operation, bool native) {
    c->error.status = status;
    c->error.operation = operation;
    c->error.native_policy = native;
    return false;
}

bool tile_draw_fp(mc_client_tile_renderer_draw_call *c, int32_t pc) {
    mc_client_interpolation_error error = {0};
    if (mc_client_interpolation_fp(&error, pc))
        return true;
    c->error.site.pc = pc;
    c->state.source_pc = (unsigned int)pc;
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_UNSUPPORTED_FP,
                          "TileEntity renderer strict floating environment", true);
}

bool tile_draw_snapshot(mc_client_tile_renderer_draw_call *c, mc_tile_entity_snapshot *out,
                        int32_t pc) {
    if (c->tile_entity == NULL) {
        c->error.site.pc = pc;
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/NullPointerException";
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE,
                              "actual null TileEntity receiver", false);
    }
    c->error.owner.tile_entity = mc_tile_entity_get_snapshot(c->tile_entity, out);
    if (c->error.owner.tile_entity == MC_TILE_ENTITY_OK)
        return true;
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                          "actual TileEntity field access", true);
}

bool tile_draw_graphics(mc_client_tile_renderer_draw_call *c) {
    if (c->graphics != NULL || c->services.graphics == NULL)
        return true;
    c->error.owner.engine_status = mc_client_render_engine_classes_retain(c->services.graphics);
    if (c->error.owner.engine_status != MC_CLIENT_RENDER_ENGINE_OK)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_ENGINE_FAILED,
                              "TileEntity draw reached graphics namespace hold", true);
    c->graphics = c->services.graphics;
    return true;
}

bool tile_draw_display(mc_client_tile_renderer_draw_call *c, bool returned) {
    ++c->state.gl_calls;
    return returned || tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_DISPLAY_FAILED,
                                      c->error.display.operation, true);
}

bool tile_draw_texture(mc_client_tile_renderer_draw_call *c, const uint16_t *key, size_t length,
                       mc_client_render_engine **held, bool *pending,
                       mc_client_texture_lookup_error *detail) {
    c->error.child = (mc_client_graphics_site){"je", "a", "(Ljava/lang/String;)V", 4};
    mc_client_tile_renderer_dispatcher *manager = c->renderer->manager;
    mc_client_render_engine *engine = manager == NULL ? NULL : manager->fields.e;
    if (manager == NULL || engine == NULL) {
        if (manager != NULL)
            c->error.child.pc = 11;
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/NullPointerException";
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE,
                              manager == NULL ? "JE actual null LL texture field receiver"
                                              : "JE actual null JI texture lookup receiver",
                              false);
    }
    c->error.child.pc = 11;
    c->error.owner.engine_status = mc_client_render_engine_retain(engine);
    if (c->error.owner.engine_status != MC_CLIENT_RENDER_ENGINE_OK)
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_ENGINE_FAILED,
                              "JE reached actual Engine occurrence", true);
    *held = engine;
    const bool foreign_pending = mc_client_texture_lookup_pending(engine);
    mc_client_texture id;
    c->error.texture = detail;
    const mc_client_texture_lookup_status status = mc_client_render_engine_texture(
        engine, c->services.display, NULL, (mc_texture_pack_text){key, length, false}, &id, detail);
    *pending = !foreign_pending && mc_client_texture_lookup_pending(engine);
    if (status != MC_CLIENT_LOOKUP_OK) {
        c->error.child = detail->site;
        c->error.source_exception = !detail->native_policy;
        if (status == MC_CLIENT_LOOKUP_NULL_REFERENCE)
            c->error.exception_class = "java/lang/NullPointerException";
        else if (status == MC_CLIENT_LOOKUP_SOURCE_ILLEGAL_ARGUMENT)
            c->error.exception_class = "java/lang/IllegalArgumentException";
        else if (status == MC_CLIENT_LOOKUP_SOURCE_NEGATIVE_SIZE)
            c->error.exception_class = "java/lang/NegativeArraySizeException";
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TEXTURE_FAILED, detail->operation,
                              detail->native_policy);
    }
    c->state.texture_returned = true;
    c->error.child = (mc_client_graphics_site){"je", "a", "(Ljava/lang/String;)V", 14};
    if (mc_java_i32_narrow((int64_t)id) >= 0) {
        if (!tile_draw_display(
                c, mc_client_display_source_bind_id(c->services.display, id, &c->error.display)))
            return false;
        c->state.texture_bound = true;
    }
    c->error.child = (mc_client_graphics_site){0};
    return true;
}

static bool numeric(mc_client_tile_renderer_draw_call *c, double a, double b, int32_t pc) {
    mc_client_interpolation_error error = {0};
    if (!tile_draw_fp(c, pc))
        return false;
    if (mc_client_interpolation_double_nan_admit(a, b, pc, &error))
        return true;
    c->error.site.pc = pc;
    c->state.source_pc = (unsigned int)pc;
    return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_UNSUPPORTED_NUMERIC,
                          "TileEntity renderer unmeasured signaling NaN pair", true);
}

/* Actual OW.a(DDD)D, preserving each reached field read and JVM operation.
 * The enclosing LL frame stays separate from this child's exact source PCs. */
static bool distance(mc_client_tile_renderer_draw_call *c) {
    tile_draw_at(c, "ll", ordinary_descriptor, 13);
    const mc_client_graphics_site parent = c->error.site;
    const double view[3] = {c->dispatcher->fields.j, c->dispatcher->fields.k,
                            c->dispatcher->fields.l};
    /* The invocation receiver is checked only after all three LL fields were
     * evaluated, but before OW.a enters or reads OW.e. */
    if (c->tile_entity == NULL) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/NullPointerException";
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE,
                              "actual LL null OW.a invocation receiver", false);
    }
    static const int32_t field_pc[3] = {1, 14, 27};
    static const int32_t add_pc[3] = {8, 21, 34};
    static const int32_t sub_pc[3] = {10, 23, 37};
    double delta[3];
    c->error.parent = parent;
    for (size_t i = 0; i < 3; ++i) {
        mc_tile_entity_snapshot fields;
        c->error.site = (mc_client_graphics_site){"ow", "a", "(DDD)D", field_pc[i]};
        if (!tile_draw_snapshot(c, &fields, field_pc[i]))
            return false;
        const int32_t coordinate = i == 0 ? fields.x : i == 1 ? fields.y : fields.z;
        if (!numeric(c, (double)coordinate, 0.5, add_pc[i]))
            return false;
        const double center = mc_client_interpolation_dadd((double)coordinate, 0.5);
        if (!numeric(c, center, view[i], sub_pc[i]))
            return false;
        delta[i] = mc_client_interpolation_dsub(center, view[i]);
    }
    if (!numeric(c, delta[0], delta[0], 44))
        return false;
    const double xx = mc_client_interpolation_dmul(delta[0], delta[0]);
    if (!numeric(c, delta[1], delta[1], 49))
        return false;
    const double yy = mc_client_interpolation_dmul(delta[1], delta[1]);
    if (!numeric(c, xx, yy, 50))
        return false;
    const double xy = mc_client_interpolation_dadd(xx, yy);
    if (!numeric(c, delta[2], delta[2], 55))
        return false;
    const double zz = mc_client_interpolation_dmul(delta[2], delta[2]);
    if (!numeric(c, xy, zz, 56))
        return false;
    c->state.distance = mc_client_interpolation_dadd(xy, zz);
    c->state.distance_returned = true;
    c->error.parent = (mc_client_graphics_site){0};
    tile_draw_at(c, "ll", ordinary_descriptor, 20);
    /* dcmpg makes NaN compare +1: Java takes the cull branch. */
    c->state.distance_culled = !(c->state.distance < 4096.0);
    return true;
}

static bool brightness(mc_client_tile_renderer_draw_call *c) {
    tile_draw_at(c, "ll", ordinary_descriptor, 39);
    mc_world_runtime *world = c->dispatcher->fields.f; /* Actual receiver24. */
    mc_tile_entity_snapshot fields;
    if (!tile_draw_snapshot(c, &fields, 28))
        return false;
    const int32_t x = fields.x;
    if (!tile_draw_snapshot(c, &fields, 32))
        return false;
    const int32_t y = fields.y;
    if (!tile_draw_snapshot(c, &fields, 36))
        return false;
    const int32_t z = fields.z;
    if (world == NULL) {
        c->error.source_exception = true;
        c->error.exception_class = "java/lang/NullPointerException";
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_NULL_REFERENCE,
                              "actual LL brightness null World receiver", false);
    }
    if (!mc_world_runtime_binding_retain(world))
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_WORLD_FAILED,
                              "LL brightness actual World occurrence", true);
    c->brightness_world = world;
    const mc_world_storage_status status =
        mc_world_runtime_brightness(world, x, y, z, &c->state.brightness, &c->brightness);
    c->error.brightness = &c->brightness;
    if (c->brightness.query.source_chunk != NULL) {
        c->source_chunk = *c->brightness.query.source_chunk;
        c->brightness.query.source_chunk = &c->source_chunk;
    }
    if (status != MC_WORLD_STORAGE_OK) {
        c->error.child =
            (mc_client_graphics_site){"fd", "c", "(III)F", (int32_t)c->brightness.bytecode};
        c->error.source_exception =
            c->brightness.source_exception != MC_WORLD_BRIGHTNESS_SOURCE_NONE ||
            c->brightness.query.source_null;
        if (c->error.source_exception)
            c->error.exception_class =
                c->brightness.source_exception == MC_WORLD_BRIGHTNESS_SOURCE_INDEX
                    ? "java/lang/ArrayIndexOutOfBoundsException"
                    : "java/lang/NullPointerException";
        else {
            mc_entity_render_throwable throwable = MC_ENTITY_RENDER_NO_THROWABLE;
            mc_entity_render_registry_classification(mc_world_runtime_registry(world),
                                                     &c->brightness.query.registry, &throwable,
                                                     &c->error.exception_class);
            c->error.source_exception = throwable != MC_ENTITY_RENDER_NO_THROWABLE;
        }
        return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_WORLD_FAILED, c->brightness.operation,
                              c->brightness.native_policy);
    }
    c->state.brightness_returned = true;
    tile_draw_at(c, "ll", ordinary_descriptor, 46);
    if (!tile_draw_graphics(c) ||
        !tile_draw_display(c, mc_client_display_source_color(
                                  c->services.display, c->state.brightness, c->state.brightness,
                                  c->state.brightness, 1.0f, &c->error.display)))
        return false;
    c->state.color_returned = true;
    return true;
}

static bool coordinates(mc_client_tile_renderer_draw_call *c) {
    tile_draw_at(c, "ll", coordinates_descriptor, 2);
    mc_client_tile_renderer *renderer = NULL;
    const mc_client_tile_renderer_status status =
        tile_renderer_lookup_tile_entered(c->dispatcher, c->tile_entity, &renderer, &c->error.owner)
            ? MC_CLIENT_TILE_RENDERER_OK
            : c->error.owner.status;
    if (status != MC_CLIENT_TILE_RENDERER_OK)
        return tile_draw_fail(c, status, c->error.owner.operation, c->error.owner.native_policy);
    c->state.lookup_returned = true;
    c->renderer = renderer;
    c->state.renderer_identity = (uintptr_t)renderer;
    if (renderer != NULL) {
        c->state.renderer_class = renderer->source_class;
        tile_draw_at(c, "ll", coordinates_descriptor, 22);
        c->error.parent = c->error.site;
        if (strcmp(renderer->source_class, "ag") == 0) {
            if (!tile_draw_spawner(c))
                return false;
        } else if (strcmp(renderer->source_class, "hy") == 0) {
            if (!tile_draw_piston(c))
                return false;
        } else if (strcmp(renderer->source_class, "po") == 0) {
            if (!tile_draw_sign(c))
                return false;
        } else
            return tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_UNIMPLEMENTED_DEPENDENCY,
                                  "reached TileEntity renderer draw descendant", true);
        c->state.renderer_returned = true;
    }
    c->error.parent = (mc_client_graphics_site){0};
    tile_draw_at(c, "ll", coordinates_descriptor, 25);
    return true;
}

static bool ordinary(mc_client_tile_renderer_draw_call *c) {
    if (!distance(c))
        return false;
    if (c->state.distance_culled) {
        tile_draw_at(c, "ll", ordinary_descriptor, 82);
        return true;
    }
    if (!brightness(c))
        return false;
    static const int32_t field_pc[3] = {52, 61, 70}, sub_pc[3] = {59, 68, 77};
    double *const outputs[3] = {&c->state.x, &c->state.y, &c->state.z};
    for (size_t i = 0; i < 3; ++i) {
        mc_tile_entity_snapshot fields;
        tile_draw_at(c, "ll", ordinary_descriptor, field_pc[i]);
        if (!tile_draw_snapshot(c, &fields, field_pc[i]))
            return false;
        const double value = (double)(i == 0 ? fields.x : i == 1 ? fields.y : fields.z);
        const double origin = i == 0   ? c->dispatcher->classes->b
                              : i == 1 ? c->dispatcher->classes->c
                                       : c->dispatcher->classes->d;
        if (!numeric(c, value, origin, sub_pc[i]))
            return false;
        *outputs[i] = mc_client_interpolation_dsub(value, origin);
    }
    tile_draw_at(c, "ll", ordinary_descriptor, 79);
    if (!coordinates(c))
        return false;
    tile_draw_at(c, "ll", ordinary_descriptor, 82);
    return true;
}

mc_client_tile_renderer_draw_limits mc_client_tile_renderer_draw_default_limits(void) {
    return (mc_client_tile_renderer_draw_limits){4u * 1024u * 1024u,
                                                 mc_client_render_manager_draw_default_limits()};
}

static mc_client_tile_renderer_status
begin(mc_client_tile_renderer_dispatcher *m, mc_tile_entity *tile, double x, double y, double z,
      float partial, bool coordinate_only, const mc_client_manager_draw_services *services,
      const mc_client_tile_renderer_draw_limits *limits, mc_client_tile_renderer_draw_call **out,
      mc_client_tile_renderer_draw_error *error) {
    mc_client_tile_renderer_draw_error e = {0};
    tile_renderer_error_init(&e.owner);
    if (out == NULL || *out != NULL) {
        e.status = MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT;
        e.operation = "LL draw exclusive fresh output";
        e.native_policy = true;
    } else if (!tile_renderer_passive_admit(m, &e.owner)) {
        e.status = e.owner.status;
        e.operation = e.owner.operation;
        e.native_policy = true;
    } else if (m->classes->phase != MC_CLIENT_TILE_RENDERER_READY || m->classes->a != m) {
        e.status = MC_CLIENT_TILE_RENDERER_INVALID_PHASE;
        e.operation = "LL draw canonical initialized receiver";
        e.native_policy = true;
    } else {
        const mc_client_tile_renderer_draw_limits policy =
            limits == NULL ? mc_client_tile_renderer_draw_default_limits() : *limits;
        mc_client_tile_renderer_draw_call *c = NULL;
        if (sizeof(*c) > policy.max_bytes || m->holds == SIZE_MAX) {
            e.status = MC_CLIENT_TILE_RENDERER_LIMIT;
            e.operation = "LL retained draw header/occurrence limit";
            e.native_policy = true;
        } else if ((c = calloc(1, sizeof(*c))) == NULL) {
            e.status = MC_CLIENT_TILE_RENDERER_ALLOCATION_FAILED;
            e.operation = "LL retained draw header allocation";
            e.native_policy = true;
        } else {
            c->limits = policy;
            if (services != NULL)
                c->services = *services;
            c->dispatcher = m;
            c->error = e;
            c->state =
                (mc_client_tile_renderer_draw_state){.active = true,
                                                     .coordinates_only = coordinate_only,
                                                     .x = x,
                                                     .y = y,
                                                     .z = z,
                                                     .partial = partial,
                                                     .allocation_bytes = sizeof(*c),
                                                     .receiver_identity = (uintptr_t)m,
                                                     .tile_entity_identity = (uintptr_t)tile};
            ++m->holds;
            m->draw_call = c;
            *out = c;
            if (tile != NULL && mc_tile_entity_retain(tile) != MC_TILE_ENTITY_OK)
                tile_draw_fail(c, MC_CLIENT_TILE_RENDERER_TILE_ENTITY_FAILED,
                               "LL actual TileEntity draw occurrence", true);
            else {
                c->tile_entity = tile;
                m->active = m->classes->active = true;
                c->state.returned = coordinate_only ? coordinates(c) : ordinary(c);
                m->active = m->classes->active = false;
            }
            c->state.active = false;
            c->state.failure = c->error.status == MC_CLIENT_TILE_RENDERER_OK ? NULL : &c->error;
            e = c->error;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_draw_begin(mc_client_tile_renderer_dispatcher *m, mc_tile_entity *tile,
                                   float partial, const mc_client_manager_draw_services *services,
                                   const mc_client_tile_renderer_draw_limits *limits,
                                   mc_client_tile_renderer_draw_call **out,
                                   mc_client_tile_renderer_draw_error *error) {
    return begin(m, tile, 0, 0, 0, partial, false, services, limits, out, error);
}

mc_client_tile_renderer_status mc_client_tile_renderer_coordinates_draw_begin(
    mc_client_tile_renderer_dispatcher *m, mc_tile_entity *tile, double x, double y, double z,
    float partial, const mc_client_manager_draw_services *services,
    const mc_client_tile_renderer_draw_limits *limits, mc_client_tile_renderer_draw_call **out,
    mc_client_tile_renderer_draw_error *error) {
    return begin(m, tile, x, y, z, partial, true, services, limits, out, error);
}

void tile_draw_manager_receipt(mc_client_tile_renderer_draw_call *c) {
    mc_client_manager_draw_state state;
    if (c->manager_call == NULL || !mc_client_render_manager_draw_inspect(c->manager_call, &state))
        return;
    c->state.manager_returned = state.returned;
    if (state.living != NULL) {
        c->state.model_parts = state.living->primary_render.part_returns;
        c->state.model_faces = state.living->primary_render.faces;
        c->state.model_vertices = state.living->primary_render.vertices;
        c->state.model_compiled_parts = state.living->primary_render.compiled_parts;
    }
}

bool mc_client_tile_renderer_draw_inspect(const mc_client_tile_renderer_draw_call *c,
                                          mc_client_tile_renderer_draw_state *out) {
    if (c == NULL || out == NULL || c->state.active)
        return false;
    *out = c->state;
    return true;
}

mc_client_tile_renderer_status
mc_client_tile_renderer_draw_retire(mc_client_tile_renderer_draw_call **slot,
                                    mc_client_tile_renderer_draw_error *error) {
    mc_client_tile_renderer_draw_error e = {0};
    if (slot == NULL) {
        e.status = MC_CLIENT_TILE_RENDERER_INVALID_ARGUMENT;
        e.operation = "LL draw retirement slot";
        e.native_policy = true;
    } else if (*slot != NULL) {
        mc_client_tile_renderer_draw_call *c = *slot;
        mc_client_tile_renderer_dispatcher *m = c->dispatcher;
        if (c->state.active || m->active || m->classes->active || m->draw_call != c ||
            m->holds < 2) {
            e.status = MC_CLIENT_TILE_RENDERER_BUSY;
            e.operation = "LL draw active/occurrence retirement";
            e.native_policy = true;
        } else {
            c->state.retiring = true;
            if (!tile_draw_piston_retire(c, &e))
                goto retained;
            if (!tile_draw_sign_retire(c, &e))
                goto retained;
            /* Fallible graphics release precedes child diagnostic expiry. */
            if (c->graphics != NULL) {
                if (mc_client_render_engine_classes_release(c->graphics) !=
                    MC_CLIENT_RENDER_ENGINE_OK) {
                    e.status = MC_CLIENT_TILE_RENDERER_ENGINE_FAILED;
                    e.operation = "LL reached graphics namespace retirement";
                    e.native_policy = true;
                    goto retained;
                }
                c->graphics = NULL;
            }
            if (c->manager_call != NULL &&
                mc_client_render_manager_draw_retire(&c->manager_call, &c->manager_cleanup) !=
                    MC_CLIENT_MANAGER_OK) {
                e.status = MC_CLIENT_TILE_RENDERER_MANAGER_FAILED;
                e.operation = "LL retained TH draw retirement";
                e.native_policy = true;
                e.manager = &c->manager_cleanup;
                goto retained;
            }
            /* Only these child borrows end on successful checked retirement;
             * keep the completed source status/sites/Throwable scalars. */
            c->manager_error.entity = NULL;
            c->manager_error.living = NULL;
            if (c->manager_consumer.attached &&
                mc_client_render_manager_consumer_detach(
                    &c->manager_consumer, &c->manager_owner_cleanup) != MC_CLIENT_MANAGER_OK) {
                e.status = MC_CLIENT_TILE_RENDERER_MANAGER_FAILED;
                e.operation = "LL reached TH namespace retirement";
                e.native_policy = true;
                e.manager_owner = &c->manager_owner_cleanup;
                goto retained;
            }
            c->manager_owner.font = NULL;
            c->manager_owner.acquisition = NULL;
            mc_entity_release(c->preview);
            mc_entity_release(c->factory_result);
            mc_entity_release(c->map_value_pending);
            for (size_t i = 0; i < 3; ++i)
                mc_tile_entity_name_release(c->names[i]);
            if (c->brightness_world != NULL)
                mc_world_runtime_binding_release(c->brightness_world);
            mc_tile_entity_release(c->tile_entity);
            tile_draw_piston_free(c);
            tile_draw_sign_free(c);
            --m->holds;
            m->draw_call = NULL;
            free(c);
            *slot = NULL;
        retained:
            if (*slot != NULL) {
                c->cleanup = e;
                c->state.cleanup = &c->cleanup;
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}
