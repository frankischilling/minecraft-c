#include "mc/client/world/renderer_tick.h"

#include "../../../entity/player/tick_math_internal.h"
#include "../../application_state.h"
#include "../../view_slot.h"
#include "internal.h"
#include "rain_tick_internal.h"

#include <stdlib.h>
#include <string.h>

struct mc_client_entity_renderer_tick_call {
    mc_client_renderer_tick_state state;
    mc_client_renderer_tick_error failure;
    mc_client_entity_renderer_error owner_error;
    mc_math_class_error math_error;
    mc_math_classes *math;
    mc_world_brightness_error brightness;
    mc_client_held_item_tick_call *held_item;
    mc_client_held_item_tick_state held_state;
    mc_client_held_item_tick_error held_error;
    mc_client_renderer_rain_call *rain;
    mc_client_renderer_rain_state rain_state;
    mc_client_renderer_rain_error rain_error;
    mc_world_storage_error rain_world;
    mc_world_query_error rain_query;
    mc_world_chunk_manager_error rain_biome;
    mc_registry_error rain_registry;
    mc_client_entity_renderer_error rain_owner;
    bool owner_held;
};

static mc_client_renderer_tick_status result(mc_client_renderer_tick_error *out,
                                             mc_client_renderer_tick_status status,
                                             const char *operation) {
    if (out != NULL) {
        memset(out, 0, sizeof(*out));
        out->status = status;
        out->operation = operation;
        out->native_policy = status != MC_CLIENT_RENDERER_TICK_OK;
        out->site.pc = out->child.pc = -1;
    }
    return status;
}

static bool fail(mc_client_entity_renderer_tick_call *call, mc_client_renderer_tick_status status,
                 const char *operation, unsigned int pc, bool native) {
    call->failure.status = status;
    call->failure.operation = operation;
    call->failure.native_policy = native;
    call->failure.site = (mc_client_graphics_site){"px", "a", "()V", (int32_t)pc};
    call->state.source_pc = pc;
    return false;
}

static client_application *app_at(mc_client_entity_renderer_tick_call *call, unsigned int pc) {
    client_application *app = call->state.owner->fields.j;
    if (app == NULL) {
        fail(call, MC_CLIENT_RENDERER_TICK_SOURCE_NULL, "actual PX tick null App receiver", pc,
             false);
        return NULL;
    }
    call->state.app = app;
    return app;
}

static bool coordinate(mc_client_entity_renderer_tick_call *call, mc_math_classes *math,
                       size_t axis, unsigned int app_pc, unsigned int entity_pc,
                       unsigned int floor_pc, int32_t *out) {
    client_application *app = app_at(call, app_pc);
    if (app == NULL)
        return false;
    if (app->i == NULL)
        return fail(call, MC_CLIENT_RENDERER_TICK_SOURCE_NULL, "actual PX tick null view receiver",
                    entity_pc, false);
    mc_entity_snapshot entity;
    call->failure.entity = mc_entity_get_snapshot(app->i, &entity);
    if (call->failure.entity != MC_ENTITY_OK)
        return fail(call, MC_CLIENT_RENDERER_TICK_NATIVE_FAILED, "PX tick actual view fields",
                    entity_pc, true);
    if (call->math == NULL) {
        if (mc_math_classes_retain(math, &call->math_error) != MC_MATH_CLASS_OK) {
            call->failure.math = &call->math_error;
            return fail(call, MC_CLIENT_RENDERER_TICK_NATIVE_FAILED,
                        "PX tick SAME MathHelper occurrence", floor_pc, true);
        }
        call->math = math;
    }
    if (mc_math_classes_ensure(call->math, &call->math_error) != MC_MATH_CLASS_OK) {
        call->failure.math = &call->math_error;
        return fail(call, MC_CLIENT_RENDERER_TICK_CHILD_FAILED, "PX tick MathHelper active use",
                    floor_pc, true);
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(call, MC_CLIENT_RENDERER_TICK_UNSUPPORTED_FP,
                    "PX tick native coordinate floating environment", floor_pc, true);
    const double value = axis == 0 ? entity.base.x : axis == 1 ? entity.base.y : entity.base.z;
    *out = mc_math_floor_double(value);
    return true;
}

static bool body(mc_client_entity_renderer_tick_call *call, mc_math_classes *math) {
    mc_client_source_px *owner = call->state.owner;
    mc_client_entity_renderer_fields *f = &owner->fields;
    memcpy(&f->L, &f->M, sizeof(float));
    call->state.interpolation_stores = 1;
    memcpy(&f->u, &f->t, sizeof(float));
    ++call->state.interpolation_stores;
    for (size_t index = 0; index < 8; index += 2) {
        memcpy(&f->v_to_C[index + 1], &f->v_to_C[index], sizeof(float));
        ++call->state.interpolation_stores;
    }
    client_application *app = app_at(call, 52);
    if (app == NULL)
        return false;
    if (app->i == NULL) {
        call->failure.entity = mc_client_view_slot_store(app, app->h);
        if (call->failure.entity != MC_ENTITY_OK)
            return fail(call, MC_CLIENT_RENDERER_TICK_NATIVE_FAILED,
                        "PX tick App.i native occurrence", 69, true);
        call->state.view_stored = true;
    }
    /* JVM captures the World receiver before evaluating X/Y/Z operands. */
    app = app_at(call, 76);
    if (app == NULL)
        return false;
    mc_world_runtime *world = app->active_world;
    if (!coordinate(call, math, 0, 83, 86, 89, &call->state.x) ||
        !coordinate(call, math, 1, 96, 99, 102, &call->state.y) ||
        !coordinate(call, math, 2, 109, 112, 115, &call->state.z))
        return false;
    if (world == NULL)
        return fail(call, MC_CLIENT_RENDERER_TICK_SOURCE_NULL,
                    "actual PX tick null World brightness receiver", 118, false);
    if (mc_world_runtime_brightness(world, call->state.x, call->state.y, call->state.z,
                                    &call->state.brightness,
                                    &call->brightness) != MC_WORLD_STORAGE_OK) {
        call->failure.brightness = &call->brightness;
        call->failure.child =
            (mc_client_graphics_site){"fd", "c", "(III)F", (int32_t)call->brightness.bytecode};
        return fail(call, MC_CLIENT_RENDERER_TICK_CHILD_FAILED, call->brightness.operation, 118,
                    call->brightness.native_policy);
    }
    call->state.brightness_returned = true;
    app = app_at(call, 127);
    if (app == NULL)
        return false;
    if (app->settings == NULL)
        return fail(call, MC_CLIENT_RENDERER_TICK_SOURCE_NULL,
                    "actual PX tick null Settings receiver", 130, false);
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(app->settings, &settings))
        return fail(call, MC_CLIENT_RENDERER_TICK_NATIVE_FAILED, "PX tick actual Settings fields",
                    130, true);
    call->state.render_distance = settings.values.render_distance;
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return fail(call, MC_CLIENT_RENDERER_TICK_UNSUPPORTED_FP,
                    "PX tick native brightness floating environment", 134, true);
    const int32_t difference = mc_java_i32_sub(3, call->state.render_distance);
    const float converted = _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), difference));
    call->state.distance_blend = at_fdiv(converted, 3.0f);
    call->state.target =
        at_fadd(at_fmul(call->state.brightness, at_fsub(1.0f, call->state.distance_blend)),
                call->state.distance_blend);
    memcpy(&call->state.before_M, &f->M, sizeof(float));
    const float after = at_fadd(f->M, at_fmul(at_fsub(call->state.target, f->M), 0.1f));
    memcpy(&f->M, &after, sizeof(float));
    memcpy(&call->state.after_M, &after, sizeof(float));
    call->state.M_stored = true;
    f->l = mc_java_i32_add(f->l, 1);
    call->state.l_stored = true;
    if (f->c == NULL)
        return fail(call, MC_CLIENT_RENDERER_TICK_SOURCE_NULL,
                    "actual PX tick null held-item receiver", 179, false);
    if (mc_client_held_item_identity(owner->pending_ra) != f->c)
        return fail(call, MC_CLIENT_RENDERER_TICK_NATIVE_FAILED,
                    "PX tick unresolved actual RA occurrence", 179, true);
    call->state.held_item_called = true;
    const mc_client_held_item_tick_status held =
        mc_client_held_item_tick_begin(owner->pending_ra, &call->held_item, &call->held_error);
    if (call->held_item != NULL) {
        (void)mc_client_held_item_tick_inspect(call->held_item, &call->held_state);
        call->held_state.error = &call->held_error;
        call->state.held_item = &call->held_state;
    }
    if (held != MC_CLIENT_HELD_ITEM_TICK_OK) {
        call->failure.held_item = &call->held_error;
        call->failure.child = call->held_error.site;
        return fail(call, MC_CLIENT_RENDERER_TICK_CHILD_FAILED, call->held_error.operation, 179,
                    call->held_error.native_policy);
    }
    call->state.held_item_returned = true;
    call->state.rain_called = true;
    const mc_client_renderer_rain_status rain =
        entity_renderer_rain_tick_entered(owner, &call->rain, &call->rain_error);
    if (call->rain != NULL) {
        (void)mc_client_entity_renderer_rain_tick_inspect(call->rain, &call->rain_state);
        if (call->rain_error.world != NULL) {
            call->rain_world = *call->rain_error.world;
            call->rain_error.world = &call->rain_world;
        }
        if (call->rain_error.query != NULL) {
            call->rain_query = *call->rain_error.query;
            call->rain_error.query = &call->rain_query;
        }
        if (call->rain_error.biome != NULL) {
            call->rain_biome = *call->rain_error.biome;
            call->rain_error.biome = &call->rain_biome;
        }
        if (call->rain_error.registry != NULL) {
            call->rain_registry = *call->rain_error.registry;
            call->rain_error.registry = &call->rain_registry;
        }
        if (call->rain_error.owner != NULL) {
            call->rain_owner = *call->rain_error.owner;
            call->rain_error.owner = &call->rain_owner;
        }
        call->rain_state.failure = &call->rain_error;
        call->state.rain = &call->rain_state;
    }
    if (rain != MC_CLIENT_RENDERER_RAIN_OK) {
        call->failure.rain = &call->rain_error;
        call->failure.child =
            (mc_client_graphics_site){"px", "c", "()V", (int32_t)call->rain_error.source_pc};
        return fail(call, MC_CLIENT_RENDERER_TICK_CHILD_FAILED, call->rain_error.operation, 183,
                    call->rain_error.native_policy);
    }
    call->state.rain_returned = true;
    call->state.source_returned = true;
    call->state.source_pc = 186;
    return true;
}

mc_client_renderer_tick_status
mc_client_entity_renderer_tick_begin(mc_client_source_px *owner, mc_math_classes *math,
                                     mc_client_entity_renderer_tick_call **out,
                                     mc_client_renderer_tick_error *error) {
    if (out == NULL || *out != NULL)
        return result(error, MC_CLIENT_RENDERER_TICK_ARGUMENT, "PX tick empty output slot");
    if (owner != NULL && (owner->active || owner->tick_call != NULL ||
                          owner->phase == MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING ||
                          owner->phase == MC_CLIENT_ENTITY_RENDERER_RETIRING))
        return result(error, MC_CLIENT_RENDERER_TICK_BUSY, "PX tick active or pending occurrence");
    mc_client_entity_renderer_tick_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return result(error, MC_CLIENT_RENDERER_TICK_ALLOCATION, "PX tick diagnostic allocation");
    call->state.owner = owner;
    call->state.failure = &call->failure;
    call->state.active = true;
    *out = call;
    if (owner == NULL)
        fail(call, MC_CLIENT_RENDERER_TICK_SOURCE_NULL, "actual PX tick null receiver", 0, false);
    else if (mc_client_entity_renderer_retain(owner, &call->owner_error) !=
             MC_CLIENT_ENTITY_RENDERER_OK) {
        call->failure.owner = &call->owner_error;
        fail(call, MC_CLIENT_RENDERER_TICK_NATIVE_FAILED, "PX tick native occurrence", 0, true);
    } else {
        call->owner_held = true;
        owner->tick_call = call;
        owner->active = true;
        (void)body(call, math);
        owner->active = false;
    }
    call->state.active = false;
    if (error != NULL)
        *error = call->failure;
    return call->failure.status;
}

bool mc_client_entity_renderer_tick_inspect(const mc_client_entity_renderer_tick_call *call,
                                            mc_client_renderer_tick_state *out) {
    if (call == NULL || out == NULL || call->state.active)
        return false;
    *out = call->state;
    return true;
}

mc_client_renderer_tick_status
mc_client_entity_renderer_tick_retire(mc_client_entity_renderer_tick_call **slot,
                                      mc_client_renderer_tick_error *error) {
    if (slot == NULL)
        return result(error, MC_CLIENT_RENDERER_TICK_ARGUMENT, "PX tick retirement slot");
    mc_client_entity_renderer_tick_call *call = *slot;
    if (call == NULL)
        return result(error, MC_CLIENT_RENDERER_TICK_OK, NULL);
    if (call->state.active || (call->state.owner != NULL && call->state.owner->active))
        return result(error, MC_CLIENT_RENDERER_TICK_BUSY, "PX tick active retirement");
    mc_client_renderer_rain_error rain;
    if (mc_client_entity_renderer_rain_tick_retire(&call->rain, &rain) !=
        MC_CLIENT_RENDERER_RAIN_OK)
        return result(error, MC_CLIENT_RENDERER_TICK_BUSY, "PX tick rain child retirement");
    call->state.rain = NULL; /* Child-owned observations end even if later cleanup refuses. */
    mc_client_held_item_tick_error held;
    if (mc_client_held_item_tick_retire(&call->held_item, &held) != MC_CLIENT_HELD_ITEM_TICK_OK)
        return result(error, MC_CLIENT_RENDERER_TICK_BUSY, "PX tick held-item child retirement");
    call->state.held_item = NULL;
    if (mc_math_classes_release(&call->math, &call->math_error) != MC_MATH_CLASS_OK)
        return result(error, MC_CLIENT_RENDERER_TICK_BUSY, "PX tick MathHelper retirement");
    if (call->owner_held) {
        mc_client_source_px *owner = call->state.owner;
        owner->tick_call = NULL;
        if (mc_client_entity_renderer_release(&owner, &call->owner_error) !=
            MC_CLIENT_ENTITY_RENDERER_OK) {
            call->state.owner->tick_call = call;
            return result(error, MC_CLIENT_RENDERER_TICK_BUSY, "PX tick owner retirement");
        }
    }
    free(call);
    *slot = NULL;
    return result(error, MC_CLIENT_RENDERER_TICK_OK, NULL);
}
