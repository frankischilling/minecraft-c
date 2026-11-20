#include "rain_tick_internal.h"

#include "../../application_state.h"
#include "internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/math/math.h"
#include "mc/world/runtime/environment.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Renderer rain arithmetic requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Renderer rain arithmetic requires strict floating transformations"
#endif

struct mc_client_renderer_rain_call {
    mc_client_renderer_rain_state state;
    mc_client_renderer_rain_error failure;
    mc_client_entity_renderer_error owner_error, owner_cleanup;
    mc_world_storage_error world;
    mc_world_query_error query;
    mc_world_chunk_manager_error biome;
    mc_registry_error registry;
    mc_entity_snapshot camera;
    bool owner_held, camera_held, world_held, borrowed_parent;
};

static mc_client_renderer_rain_status fail(mc_client_renderer_rain_call *call,
                                           mc_client_renderer_rain_status status,
                                           const char *operation, unsigned int pc, bool native) {
    call->state.source_pc = pc;
    call->failure.status = status;
    call->failure.operation = operation;
    call->failure.source_pc = pc;
    call->failure.native_policy = native;
    return status;
}

static bool fp(mc_client_renderer_rain_call *call, unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u || fail(call, MC_CLIENT_RENDERER_RAIN_UNSUPPORTED_FP,
                                                       "renderer rain native floating environment",
                                                       pc, true) == MC_CLIENT_RENDERER_RAIN_OK;
}

static float multiply(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float add(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float int_float(int32_t value) {
    return _mm_cvtss_f32(_mm_cvtsi32_ss(_mm_setzero_ps(), value));
}

static bool random_int(mc_client_renderer_rain_call *call, int32_t bound, int32_t *out,
                       unsigned int pc) {
    mc_java_random *random = call->state.owner->fields.J;
    if (random == NULL) {
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_SOURCE_NULL, "actual rain Random receiver", pc,
                   false);
        return false;
    }
    if (!mc_java_random_next_bounded(random, bound, out)) {
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, "actual rain Random bound", pc,
                   true);
        return false;
    }
    return true;
}

static bool random_float(mc_client_renderer_rain_call *call, float *out, unsigned int pc) {
    mc_java_random *random = call->state.owner->fields.J;
    if (random == NULL) {
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_SOURCE_NULL, "actual rain Random receiver", pc,
                   false);
        return false;
    }
    *out = mc_java_random_next_float(random);
    return true;
}

static bool biome_rain(mc_client_renderer_rain_call *call, bool *out) {
    mc_world_runtime_fields world;
    if (mc_world_runtime_fields_get(call->state.world, &world) != MC_WORLD_STORAGE_OK) {
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, "rain actual fd.a fields", 219,
                   true);
        return false;
    }
    mc_world_provider_fields dimension;
    if (mc_world_provider_fields_get(world.t, &dimension) != MC_WORLD_PROVIDER_OK) {
        (void)fail(call,
                   world.t == NULL ? MC_CLIENT_RENDERER_RAIN_SOURCE_NULL
                                   : MC_CLIENT_RENDERER_RAIN_CHILD_FAILED,
                   "rain actual Dimension receiver", 219, world.t != NULL);
        return false;
    }
    mc_world_chunk_manager *manager = dimension.b;
    if (manager == NULL) {
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_SOURCE_NULL, "rain actual biome manager", 226,
                   false);
        return false;
    }
    if (mc_world_chunk_manager_retain(manager) != MC_WORLD_PROVIDER_OK) {
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, "rain native biome capture", 226,
                   true);
        return false;
    }
    const mc_biome *biome = NULL;
    const mc_world_provider_status status =
        mc_world_chunk_manager_point(manager, call->state.x, call->state.z, &biome, &call->biome);
    bool result = false;
    if (status != MC_WORLD_PROVIDER_OK) {
        call->failure.biome = &call->biome;
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, call->biome.operation, 226, true);
    } else {
        const mc_biome_fields *fields = mc_biome_fields_get(biome);
        if (fields == NULL)
            (void)fail(call, MC_CLIENT_RENDERER_RAIN_SOURCE_NULL, "rain actual Biome.d receiver",
                       229, false);
        else {
            *out = !fields->v && fields->w; /* Actual kd.d, no registered override. */
            result = true;
        }
    }
    mc_world_chunk_manager_release(manager);
    return result;
}

static const mc_block_fields *block_fields(mc_client_renderer_rain_call *call,
                                           unsigned int active_pc, unsigned int field_pc) {
    const mc_block *block = NULL;
    mc_registry_classes *registry = mc_world_runtime_registry(call->state.world);
    if (mc_registry_block_get(registry, call->state.block_id, &block, &call->registry) !=
        MC_REGISTRY_OK) {
        call->failure.registry = &call->registry;
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, "rain actual Block.m access",
                   active_pc, true);
        return NULL;
    }
    const mc_block_fields *fields = mc_block_fields_get(block);
    if (fields == NULL)
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_SOURCE_NULL, "rain actual Block receiver",
                   field_pc, false);
    return fields;
}

static bool camera_snapshot(mc_client_renderer_rain_call *call, unsigned int pc) {
    const mc_entity_status status = mc_entity_get_snapshot(call->state.camera, &call->camera);
    if (status == MC_ENTITY_OK)
        return true;
    call->failure.entity = status;
    (void)fail(call,
               call->state.camera == NULL ? MC_CLIENT_RENDERER_RAIN_SOURCE_NULL
                                          : MC_CLIENT_RENDERER_RAIN_CHILD_FAILED,
               "rain actual camera fields", pc, call->state.camera != NULL);
    return false;
}

static bool iteration(mc_client_renderer_rain_call *call) {
    int32_t first, second;
    if (!random_int(call, 10, &first, 134) || !random_int(call, 10, &second, 144))
        return false;
    call->state.x = mc_java_i32_sub(mc_java_i32_add(call->state.camera_x, first), second);
    if (!random_int(call, 10, &first, 158) || !random_int(call, 10, &second, 168))
        return false;
    call->state.z = mc_java_i32_sub(mc_java_i32_add(call->state.camera_z, first), second);
    if (mc_world_runtime_top_solid_liquid(call->state.world, call->state.x, call->state.z,
                                          &call->state.top, &call->query) != MC_WORLD_STORAGE_OK) {
        call->failure.query = &call->query;
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, call->query.operation, 179, true);
        return false;
    }
    if (mc_world_runtime_block_get(call->state.world, call->state.x,
                                   mc_java_i32_sub(call->state.top, 1), call->state.z,
                                   &call->state.block_id, &call->world) != MC_WORLD_STORAGE_OK) {
        call->failure.world = &call->world;
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, call->world.operation, 193, true);
        return false;
    }
    if (call->state.top > mc_java_i32_add(call->state.camera_y, 10) ||
        call->state.top < mc_java_i32_sub(call->state.camera_y, 10))
        return true;
    bool raining;
    if (!biome_rain(call, &raining))
        return false;
    if (!raining)
        return true;
    if (!random_float(call, &call->state.offset_x, 239) ||
        !random_float(call, &call->state.offset_z, 248))
        return false;
    if (call->state.block_id <= 0)
        return true;
    const mc_block_fields *fields = block_fields(call, 258, 264);
    if (fields == NULL)
        return false;
    mc_material *material = fields->bA, *lava = NULL;
    if (mc_registry_material_get(mc_world_runtime_registry(call->state.world), MC_MATERIAL_h, &lava,
                                 &call->registry) != MC_REGISTRY_OK) {
        call->failure.registry = &call->registry;
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, "rain actual lava Material.h", 267,
                   true);
        return false;
    }
    if (material == lava) {
        /* Source captures App.j/DN277 before NEW280. Its nullable receiver is
         * not dereferenced before the real missing concrete allocation. */
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_PARTICLE_DEPENDENCY,
                   "actual smoke particle xn allocation is not connected", 280, true);
        return false;
    }
    call->state.reservoir_count = mc_java_i32_add(call->state.reservoir_count, 1);
    if (!random_int(call, call->state.reservoir_count, &first, 337))
        return false;
    if (first == 0) {
        if (!fp(call, 348))
            return false;
        call->state.sound_x = (double)add(int_float(call->state.x), call->state.offset_x);
        const double top = (double)add(int_float(call->state.top), 0.1f);
        fields = block_fields(call, 359, 365);
        if (fields == NULL)
            return false;
        call->state.sound_y = _mm_cvtsd_f64(_mm_sub_sd(_mm_set_sd(top), _mm_set_sd(fields->bt)));
        call->state.sound_z = (double)add(int_float(call->state.z), call->state.offset_z);
    }
    (void)fail(call, MC_CLIENT_RENDERER_RAIN_PARTICLE_DEPENDENCY,
               "actual rain splash particle xd allocation is not connected", 387, true);
    return false;
}

static void body(mc_client_renderer_rain_call *call) {
    mc_client_source_px *owner = call->state.owner;
    if (owner == NULL) {
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_SOURCE_NULL, "actual rain PX receiver", 1, false);
        return;
    }
    call->state.app = owner->fields.j;
    if (call->state.app == NULL) {
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_SOURCE_NULL, "actual rain App receiver", 4, false);
        return;
    }
    mc_world_runtime *world = call->state.app->active_world;
    if (world == NULL) {
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_SOURCE_NULL, "actual rain World receiver", 8,
                   false);
        return;
    }
    if (mc_world_runtime_rain_strength(world, 1.0f, &call->state.strength, &call->world) !=
        MC_WORLD_STORAGE_OK) {
        call->failure.world = &call->world;
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, call->world.operation, 8, true);
        return;
    }
    call->state.strength_read = true;
    call->state.adjusted_strength = call->state.strength;
    mc_game_settings_state settings;
    if (!mc_game_settings_read_state(owner->fields.j->settings, &settings)) {
        (void)fail(call,
                   owner->fields.j->settings == NULL ? MC_CLIENT_RENDERER_RAIN_SOURCE_NULL
                                                     : MC_CLIENT_RENDERER_RAIN_CHILD_FAILED,
                   "actual rain Settings receiver", 19, owner->fields.j->settings != NULL);
        return;
    }
    call->state.fancy_read = true;
    call->state.fancy = settings.values.fancy_graphics;
    if (!call->state.fancy) {
        if (!fp(call, 27))
            return;
        call->state.adjusted_strength =
            _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(call->state.adjusted_strength), _mm_set_ss(2.0f)));
    }
    if (!fp(call, 31))
        return;
    if (call->state.adjusted_strength == 0.0f) {
        call->state.source_pc = 35;
        call->state.returned = true;
        return;
    }
    mc_java_random *random = owner->fields.J;
    call->state.seed = mc_java_i64_mul((int64_t)owner->fields.l, INT64_C(312987231));
    if (random == NULL) {
        (void)fail(call, MC_CLIENT_RENDERER_RAIN_SOURCE_NULL, "actual rain Random.setSeed", 49,
                   false);
        return;
    }
    mc_java_random_reseed(random, call->state.seed);
    call->state.reseeded = true;
    call->state.camera = owner->fields.j->i;
    call->state.world = owner->fields.j->active_world;
    if (call->state.camera != NULL) {
        const mc_entity_status status = mc_entity_retain(call->state.camera);
        if (status != MC_ENTITY_OK) {
            call->failure.entity = status;
            (void)fail(call, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, "rain native camera capture", 59,
                       true);
            return;
        }
        call->camera_held = true;
    }
    if (call->state.world != NULL) {
        if (!mc_world_runtime_binding_retain(call->state.world)) {
            (void)fail(call, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, "rain native World capture", 67,
                       true);
            return;
        }
        call->world_held = true;
    }
    if (!camera_snapshot(call, 69))
        return;
    call->state.camera_x = mc_math_floor_double(call->camera.base.x);
    call->state.camera_y = mc_math_floor_double(call->camera.base.y);
    call->state.camera_z = mc_math_floor_double(call->camera.base.z);
    for (;;) {
        call->state.source_pc = 114;
        const float squared = multiply(multiply(100.0f, call->state.adjusted_strength),
                                       call->state.adjusted_strength);
        call->state.loop_limit = mc_java_float_to_i32(squared);
        if (call->state.iteration >= call->state.loop_limit)
            break;
        if (!iteration(call))
            return;
        ++call->state.completed_iterations;
        call->state.iteration = mc_java_i32_add(call->state.iteration, 1);
    }
    /* Every positive reservoir selection reaches a real xn/xd allocation
     * before this tail. Until those concrete constructors exist, no reached
     * n6>0 attempt may be reported as a completed sound/tail branch. */
    call->state.source_pc = 553;
    call->state.returned = true;
}

static mc_client_renderer_rain_status admission(mc_client_renderer_rain_error *error,
                                                mc_client_renderer_rain_status status,
                                                const char *operation) {
    if (error != NULL)
        *error = (mc_client_renderer_rain_error){
            .status = status, .operation = operation, .native_policy = true};
    return status;
}

static mc_client_renderer_rain_status begin(mc_client_source_px *owner,
                                            mc_client_renderer_rain_call **out,
                                            mc_client_renderer_rain_error *error, bool entered) {
    if (out == NULL || *out != NULL)
        return admission(error, MC_CLIENT_RENDERER_RAIN_ARGUMENT, "rain owned output slot");
    if (owner != NULL && owner->active != entered)
        return admission(error, MC_CLIENT_RENDERER_RAIN_BUSY, "rain PX active guard");
    if (owner != NULL && (owner->phase == MC_CLIENT_ENTITY_RENDERER_CONSTRUCTING ||
                          owner->phase == MC_CLIENT_ENTITY_RENDERER_RETIRING))
        return admission(error, MC_CLIENT_RENDERER_RAIN_PHASE, "rain PX entered lifetime phase");
    if (entered && owner == NULL)
        return admission(error, MC_CLIENT_RENDERER_RAIN_ARGUMENT, "rain entered PX owner");
    mc_client_renderer_rain_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return admission(error, MC_CLIENT_RENDERER_RAIN_ALLOCATION,
                         "rain retained call allocation");
    call->state.owner = owner;
    call->state.failure = &call->failure;
    call->borrowed_parent = entered;
    if (!entered && owner != NULL) {
        if (mc_client_entity_renderer_retain(owner, &call->owner_error) !=
            MC_CLIENT_ENTITY_RENDERER_OK) {
            free(call);
            return admission(error, MC_CLIENT_RENDERER_RAIN_CHILD_FAILED, "rain PX occurrence");
        }
        call->owner_held = true;
        owner->active = true;
    }
    *out = call;
    call->state.active = true;
    body(call);
    call->state.active = false;
    if (!entered && owner != NULL)
        owner->active = false;
    if (error != NULL)
        *error = call->failure;
    return call->failure.status;
}

mc_client_renderer_rain_status
mc_client_entity_renderer_rain_tick_begin(mc_client_source_px *owner,
                                          mc_client_renderer_rain_call **out,
                                          mc_client_renderer_rain_error *error) {
    return begin(owner, out, error, false);
}

mc_client_renderer_rain_status
entity_renderer_rain_tick_entered(mc_client_source_px *owner, mc_client_renderer_rain_call **out,
                                  mc_client_renderer_rain_error *error) {
    return begin(owner, out, error, true);
}

bool mc_client_entity_renderer_rain_tick_inspect(const mc_client_renderer_rain_call *call,
                                                 mc_client_renderer_rain_state *out) {
    if (call == NULL || out == NULL || call->state.active)
        return false;
    *out = call->state;
    return true;
}

mc_client_renderer_rain_status
mc_client_entity_renderer_rain_tick_retire(mc_client_renderer_rain_call **slot,
                                           mc_client_renderer_rain_error *error) {
    if (slot == NULL)
        return admission(error, MC_CLIENT_RENDERER_RAIN_ARGUMENT, "rain retirement slot");
    mc_client_renderer_rain_call *call = *slot;
    if (call == NULL)
        return admission(error, MC_CLIENT_RENDERER_RAIN_OK, NULL);
    if (call->state.active ||
        (!call->borrowed_parent && call->state.owner != NULL && call->state.owner->active))
        return admission(error, MC_CLIENT_RENDERER_RAIN_BUSY, "rain active retirement");
    if (call->camera_held) {
        mc_entity_release(call->state.camera);
        call->camera_held = false;
        call->state.camera = NULL;
    }
    if (call->world_held) {
        mc_world_runtime_binding_release(call->state.world);
        call->world_held = false;
        call->state.world = NULL;
    }
    if (call->owner_held) {
        mc_client_source_px *owner = call->state.owner;
        if (mc_client_entity_renderer_release(&owner, &call->owner_cleanup) !=
            MC_CLIENT_ENTITY_RENDERER_OK)
            return admission(error, MC_CLIENT_RENDERER_RAIN_CLEANUP_FAILED,
                             "rain PX occurrence release");
        call->owner_held = false;
    }
    free(call);
    *slot = NULL;
    return admission(error, MC_CLIENT_RENDERER_RAIN_OK, NULL);
}
