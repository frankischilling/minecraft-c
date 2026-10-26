#include "mc/world/runtime/weather.h"

#include "../info/weather_source_internal.h"
#include "construction_internal.h"
#include "mc/core/java/java.h"
#include "runtime_internal.h"
#include "weather_math_internal.h"

#include <string.h>

void mc_world_weather_error_init(mc_world_weather_error *e) {
    if (e != NULL)
        memset(e, 0, sizeof(*e));
}

static bool fail(mc_world_weather_error *e, mc_world_storage_status status, const char *operation) {
    e->status = status;
    e->operation = operation;
    e->cause = (mc_world_storage_error){status, operation, 0};
    return false;
}

static bool info_admit(mc_world_info *info, unsigned int pc, const char *method,
                       mc_world_weather_error *e) {
    e->bytecode = pc;
    e->info_method = method;
    e->info_bytecode = 0; /* NULL never enters the actual ei method. */
    mc_world_info_state state;
    e->info = mc_world_info_state_get(info, &state);
    if (e->info != MC_WORLD_INFO_OK)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World weather null Info receiver");
    if (state.side != MC_WORLD_INFO_CLIENT) {
        e->info = MC_WORLD_INFO_UNSUPPORTED_OPERATION;
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World weather actual client Info namespace");
    }
    return true;
}

static bool info_result(mc_world_info_status status, unsigned int pc, mc_world_weather_error *e) {
    e->info = status;
    if (status != MC_WORLD_INFO_OK) {
        e->info_bytecode = pc;
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World weather actual Info scalar operation");
    }
    e->info_method = NULL;
    e->info_bytecode = 0;
    return true;
}

typedef mc_world_info_status (*timer_getter)(const mc_world_info *, int32_t *);
typedef mc_world_info_status (*timer_setter)(mc_world_info *, int32_t);
typedef mc_world_info_status (*flag_getter)(const mc_world_info *, bool *);
typedef mc_world_info_status (*flag_setter)(mc_world_info *, bool);

static bool get_timer(mc_world_info *info, unsigned int pc, const char *method, timer_getter get,
                      int32_t *out, mc_world_weather_error *e) {
    return info_admit(info, pc, method, e) && info_result(get(info, out), 1, e);
}

static bool set_timer(mc_world_info *info, unsigned int pc, const char *method, timer_setter set,
                      int32_t value, mc_world_weather_error *e) {
    return info_admit(info, pc, method, e) && info_result(set(info, value), 2, e);
}

static bool get_flag(mc_world_info *info, unsigned int pc, const char *method, flag_getter get,
                     bool *out, mc_world_weather_error *e) {
    return info_admit(info, pc, method, e) && info_result(get(info, out), 1, e);
}

static bool set_flag(mc_world_info *info, unsigned int pc, const char *method, flag_setter set,
                     bool value, mc_world_weather_error *e) {
    return info_admit(info, pc, method, e) && info_result(set(info, value), 2, e);
}

static bool draw(mc_world_runtime *world, unsigned int pc, int32_t bound, int32_t *out,
                 mc_world_weather_error *e) {
    e->bytecode = pc;
    mc_java_random *random = world->fields.r;
    if (random == NULL)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World weather null Random receiver");
    if (!mc_java_random_next_bounded(random, bound, out))
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World weather actual Random.nextInt");
    return true;
}

static bool fp(unsigned int pc, mc_world_weather_error *e) {
    e->bytecode = pc;
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World weather reached strict FP admission");
}

static bool body(mc_world_runtime *world, mc_world_weather_error *e) {
    e->bytecode = 4;
    mc_world_provider_fields dimension;
    e->provider = mc_world_provider_fields_get(world->fields.t, &dimension);
    if (e->provider != MC_WORLD_PROVIDER_OK)
        return fail(e, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                    "World weather null provider e receiver");
    if (dimension.e)
        return true; /* Actual fd.m return10, before all other work. */
    if (world->fields.m > 0)
        world->fields.m = mc_java_i32_sub(world->fields.m, 1);

    int32_t thunder;
    if (!get_timer(world->fields.x, 32, "n()I", mc_world_info_source_thunder_time_get, &thunder, e))
        return false;
    if (thunder <= 0) {
        bool thundering;
        if (!get_flag(world->fields.x, 44, "m()Z", mc_world_info_source_thundering_get, &thundering,
                      e))
            return false;
        mc_world_info *target = world->fields.x; /* Captured setter receiver51/75. */
        int32_t value;
        if (!draw(world, thundering ? 61u : 84u, thundering ? 12000 : 168000, &value, e))
            return false;
        value = mc_java_i32_add(value, thundering ? 3600 : 12000);
        if (!set_timer(target, thundering ? 68u : 91u, "e(I)V",
                       mc_world_info_source_thunder_time_set, value, e))
            return false;
    } else {
        thunder = mc_java_i32_sub(thunder, 1);
        if (!set_timer(world->fields.x, 105, "e(I)V", mc_world_info_source_thunder_time_set,
                       thunder, e))
            return false;
        if (thunder <= 0) {
            mc_world_info *target = world->fields.x; /* Before separate getter117. */
            bool thundering;
            if (!get_flag(world->fields.x, 120, "m()Z", mc_world_info_source_thundering_get,
                          &thundering, e) ||
                !set_flag(target, 131, "a(Z)V", mc_world_info_source_thundering_set, !thundering,
                          e))
                return false;
        }
    }

    int32_t rain;
    if (!get_timer(world->fields.x, 138, "p()I", mc_world_info_source_rain_time_get, &rain, e))
        return false;
    if (rain <= 0) {
        bool raining;
        if (!get_flag(world->fields.x, 150, "o()Z", mc_world_info_source_raining_get, &raining, e))
            return false;
        mc_world_info *target = world->fields.x;
        int32_t value;
        if (!draw(world, raining ? 167u : 190u, raining ? 12000 : 168000, &value, e))
            return false;
        if (!set_timer(target, raining ? 174u : 197u, "f(I)V", mc_world_info_source_rain_time_set,
                       mc_java_i32_add(value, 12000), e))
            return false;
    } else {
        rain = mc_java_i32_sub(rain, 1);
        if (!set_timer(world->fields.x, 211, "f(I)V", mc_world_info_source_rain_time_set, rain, e))
            return false;
        if (rain <= 0) {
            mc_world_info *target = world->fields.x;
            bool raining;
            if (!get_flag(world->fields.x, 226, "o()Z", mc_world_info_source_raining_get, &raining,
                          e) ||
                !set_flag(target, 237, "b(Z)V", mc_world_info_source_raining_set, !raining, e))
                return false;
        }
    }

    world->fields.i = world->fields.j; /* Store245 precedes getter252 and FP. */
    bool raining;
    if (!get_flag(world->fields.x, 252, "o()Z", mc_world_info_source_raining_get, &raining, e))
        return false;
    const float current_rain = world->fields.j;
    if (!fp(raining ? 263u : 280u, e))
        return false;
    world->fields.j = mc_weather_step(current_rain, raining);
    if (world->fields.j < 0.0f)
        world->fields.j = 0.0f;
    if (world->fields.j > 1.0f)
        world->fields.j = 1.0f;
    world->fields.k = world->fields.l;
    bool thundering;
    if (!get_flag(world->fields.x, 329, "m()Z", mc_world_info_source_thundering_get, &thundering,
                  e))
        return false;
    const float current_thunder = world->fields.l;
    if (!fp(thundering ? 340u : 357u, e))
        return false;
    world->fields.l = mc_weather_step(current_thunder, thundering);
    if (world->fields.l < 0.0f)
        world->fields.l = 0.0f;
    if (world->fields.l > 1.0f)
        world->fields.l = 1.0f;
    return true;
}

mc_world_storage_status mc_world_runtime_tick_weather(mc_world_runtime *world,
                                                      mc_world_weather_error *out) {
    mc_world_weather_error e;
    mc_world_weather_error_init(&e);
    bool entered = false;
    if (world == NULL)
        fail(&e, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World weather receiver");
    else if (world->classes == NULL || world->classes->side != MC_WORLD_INFO_CLIENT ||
             world->construction.mode != MC_WORLD_RUNTIME_GAME ||
             world->construction.phase == MC_WORLD_RUNTIME_RETIRED ||
             world->entity_containers_retired)
        fail(&e, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World weather CLIENT GAME native admission");
    else {
        e.status = mc_world_runtime_call_enter(world, &e.cause);
        if (e.status != MC_WORLD_STORAGE_OK)
            e.operation = e.cause.operation;
        else {
            entered = true;
            (void)body(world, &e);
        }
    }
    const mc_world_storage_status status = e.status;
    if (entered) {
        world->last_error = status == MC_WORLD_STORAGE_OK ? (mc_world_storage_error){0} : e.cause;
        mc_world_runtime_call_leave(world);
    }
    if (status == MC_WORLD_STORAGE_OK)
        mc_world_weather_error_init(&e);
    if (out != NULL)
        *out = e;
    return status;
}
