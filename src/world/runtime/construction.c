#include "runtime_internal.h"

#include "construction_internal.h"
#include "lighting_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include "mc/world/dimension/biome_runtime.h"
#include "mc/world/dimension/dimension.h"
#include "mc/world/runtime/blocks.h"
#include "millis.h"
#include "provider_internal.h"

#include <limits.h>
#include <stdlib.h>

struct mc_world_active_chunks {
    void *entries;
    size_t count, capacity;
};

const mc_math_trig *mc_world_runtime_class_trig(const mc_world_runtime *runtime)
{
    return runtime->classes->trig;
}

static mc_world_storage_status result(mc_world_storage_error *error, mc_world_storage_status status,
                                      const char *operation)
{
    if (error != NULL)
        *error = (mc_world_storage_error){status, operation, 0};
    return status;
}

mc_world_runtime_construction_limits mc_world_runtime_default_construction_limits(void)
{
    return (mc_world_runtime_construction_limits){
        .max_direct_allocation_bytes = 16u * 1024u * 1024u,
        .max_scheduled_entries = 65536u,
        .max_provider_chunks = 512u,
        .max_provider_allocation_bytes = 8u * 1024u * 1024u,
        .saved_data = mc_world_saved_data_default_limits()};
}

mc_world_storage_status mc_world_runtime_classes_create(mc_world_info_side side,
                                                        mc_java_random_profile *profile,
                                                        const mc_math_trig *trig,
                                                        mc_world_runtime_classes **out,
                                                        mc_world_storage_error *error)
{
    if ((side != MC_WORLD_INFO_CLIENT && side != MC_WORLD_INFO_SERVER) || profile == NULL ||
        trig == NULL || out == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World class arguments");
    mc_world_runtime_classes *classes = calloc(1, sizeof(*classes));
    if (classes == NULL)
        return result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "World class owner");
    classes->side = side;
    classes->profile = profile;
    classes->trig = trig;
    *out = classes;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_classes_get(const mc_world_runtime_classes *classes,
                                                     mc_world_runtime_class_state *out)
{
    if (classes == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    *out = (mc_world_runtime_class_state){classes->side, classes->initialized,
                                          classes->enqueue_depth, classes->live_worlds};
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_classes_free(mc_world_runtime_classes *classes,
                                                      mc_world_storage_error *error)
{
    if (classes == NULL)
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    if (classes->live_worlds != 0)
        return result(error, MC_WORLD_STORAGE_BUSY, "World class has live instances");
    if (classes->registry != NULL) {
        mc_registry_error cause;
        mc_registry_status status = mc_registry_classes_release(classes->registry, &cause);
        if (status != MC_REGISTRY_OK)
            return result(error, mc_world_runtime_registry_status(status), cause.message);
    }
    free(classes);
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_registry_status mc_world_runtime_classes_bind_registry(mc_world_runtime_classes *classes,
                                                          mc_registry_classes *registry,
                                                          mc_registry_error *error)
{
    mc_registry_status status = MC_REGISTRY_INVALID_ARGUMENT;
    const char *message = "World registry binding arguments";
    if (classes != NULL && registry != NULL) {
        if (classes->registry == NULL && !classes->initialized && classes->live_worlds == 0) {
            status = mc_registry_classes_retain(
                registry,
                classes->side == MC_WORLD_INFO_CLIENT ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                classes->profile, error);
            if (status == MC_REGISTRY_OK)
                classes->registry = registry;
            return status;
        }
        status = MC_REGISTRY_BUSY;
        message = "World registry already bound or class used for allocation";
    }
    if (error != NULL)
        *error = (mc_registry_error){.status = status,
                                     .message = message,
                                     .initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return status;
}

mc_registry_classes *mc_world_runtime_registry(const mc_world_runtime *runtime)
{
    return runtime == NULL || runtime->classes == NULL ? NULL : runtime->classes->registry;
}

mc_world_storage_status
mc_world_runtime_allocate_world(mc_world_runtime_entry entry, mc_world_runtime_classes *classes,
                                const mc_world_runtime_construction_limits *limits,
                                mc_world_runtime **out, mc_world_storage_error *error)
{
    if (out == NULL || classes == NULL || entry < MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE ||
        entry > MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World allocation arguments");
    mc_world_runtime_construction_limits selected =
        limits == NULL ? mc_world_runtime_default_construction_limits() : *limits;
    if (selected.max_scheduled_entries > INT32_MAX || selected.max_provider_chunks > INT32_MAX ||
        (entry == MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE) !=
            (classes->side == MC_WORLD_INFO_SERVER))
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World construction limits");
    if (sizeof(mc_world_runtime) > selected.max_direct_allocation_bytes)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World raw owner limit");
    if (classes->live_worlds == SIZE_MAX)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World class instance limit");
    if (!classes->initialized) {
        classes->enqueue_depth = 0;
        classes->initialized = true;
    }
    mc_world_runtime *runtime = calloc(1, sizeof(*runtime));
    if (runtime == NULL)
        return result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED, "World raw owner");
    runtime->options.random_profile = classes->profile;
    runtime->classes = classes;
    runtime->construction_limits = selected;
    runtime->construction =
        (mc_world_runtime_construction){.mode = MC_WORLD_RUNTIME_GAME,
                                        .phase = MC_WORLD_RUNTIME_RAW,
                                        .entry = entry,
                                        .direct_allocation_bytes = sizeof(*runtime)};
    ++classes->live_worlds;
    *out = runtime;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_construction_get(const mc_world_runtime *runtime,
                                                          mc_world_runtime_construction *out)
{
    if (runtime == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    mc_world_runtime_construction state = runtime->construction;
    state.random_status = runtime->last_random_status;
    *out = state;
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_runtime_fields_get(const mc_world_runtime *runtime,
                                                    mc_world_runtime_fields *out)
{
    if (runtime == NULL || out == NULL)
        return MC_WORLD_STORAGE_INVALID_ARGUMENT;
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    *out = runtime->fields;
    return MC_WORLD_STORAGE_OK;
}

static void *world_allocate(mc_world_runtime *runtime, size_t bytes, unsigned int call_pc,
                            const char *operation)
{
    size_t used = runtime->construction.direct_allocation_bytes;
    if (bytes > runtime->construction_limits.max_direct_allocation_bytes - used) {
        runtime->construction.failed_call_pc = call_pc;
        result(&runtime->last_error, MC_WORLD_STORAGE_LIMIT, operation);
        return NULL;
    }
    void *allocation = calloc(1, bytes);
    if (allocation == NULL) {
        runtime->construction.failed_call_pc = call_pc;
        result(&runtime->last_error, MC_WORLD_STORAGE_ALLOCATION_FAILED, operation);
        return NULL;
    }
    runtime->construction.direct_allocation_bytes += bytes;
    return allocation;
}

static bool world_list(mc_world_runtime *runtime, mc_world_reference_list **field,
                       unsigned int call_pc, unsigned int store_pc)
{
    mc_world_reference_list *list =
        world_allocate(runtime, sizeof(*list), call_pc, "World ArrayList");
    if (list == NULL)
        return false;
    *field = list;
    runtime->construction.last_store_pc = store_pc;
    return true;
}

static bool world_random(mc_world_runtime *runtime, mc_java_random **out,
                         unsigned int allocation_pc, unsigned int call_pc)
{
    mc_java_random *random =
        world_allocate(runtime, sizeof(*random), allocation_pc, "World Random owner");
    if (random == NULL)
        return false;
    mc_java_random_profile_status status =
        mc_java_random_construct_default(runtime->options.random_profile, random);
    runtime->last_random_status = status;
    if (status != MC_JAVA_RANDOM_PROFILE_OK) {
        runtime->construction.failed_call_pc = call_pc;
        mc_world_storage_status mapped =
            status == MC_JAVA_RANDOM_PROFILE_INVALID_ARGUMENT ? MC_WORLD_STORAGE_INVALID_ARGUMENT
            : status == MC_JAVA_RANDOM_PROFILE_LIMIT          ? MC_WORLD_STORAGE_LIMIT
            : status == MC_JAVA_RANDOM_PROFILE_ALLOCATION_FAILED
                ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        result(&runtime->last_error, mapped, "World default Random construction");
        runtime->construction.direct_allocation_bytes -= sizeof(*random);
        free(random);
        return false;
    }
    *out = random;
    return true;
}

static bool world_prefix(mc_world_runtime *runtime)
{
    mc_world_runtime_fields *f = &runtime->fields;
    mc_world_runtime_construction *state = &runtime->construction;
    f->a = false;
    state->last_store_pc = 6;
    if (!world_list(runtime, &f->C, 14, 17) || !world_list(runtime, &f->b, 25, 28) ||
        !world_list(runtime, &f->D, 36, 39))
        return false;
    size_t remaining =
        runtime->construction_limits.max_direct_allocation_bytes - state->direct_allocation_bytes;
    mc_scheduled_ordered_status ordered = mc_scheduled_ordered_create(
        runtime->construction_limits.max_scheduled_entries, remaining, &f->E);
    if (ordered != MC_SCHEDULED_ORDERED_OK) {
        state->failed_call_pc = 47;
        result(&runtime->last_error,
               ordered == MC_SCHEDULED_ORDERED_ALLOCATION_FAILED
                   ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                   : MC_WORLD_STORAGE_LIMIT,
               "World scheduled ordered owner");
        return false;
    }
    size_t bytes = 0;
    (void)mc_scheduled_ordered_native_bytes(f->E, &bytes);
    state->direct_allocation_bytes += bytes;
    state->last_store_pc = 50;
    remaining =
        runtime->construction_limits.max_direct_allocation_bytes - state->direct_allocation_bytes;
    mc_scheduled_membership_status membership = mc_scheduled_membership_create(
        runtime->construction_limits.max_scheduled_entries, remaining, &f->F);
    if (membership != MC_SCHEDULED_MEMBERSHIP_OK) {
        state->failed_call_pc = 58;
        result(&runtime->last_error,
               membership == MC_SCHEDULED_MEMBERSHIP_ALLOCATION_FAILED
                   ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                   : MC_WORLD_STORAGE_LIMIT,
               "World scheduled membership owner");
        return false;
    }
    (void)mc_scheduled_membership_native_bytes(f->F, &bytes);
    state->direct_allocation_bytes += bytes;
    state->last_store_pc = 61;
    if (!world_list(runtime, &f->c, 69, 72) || !world_list(runtime, &f->G, 80, 83) ||
        !world_list(runtime, &f->d, 91, 94) || !world_list(runtime, &f->e, 102, 105))
        return false;
    f->H = INT64_C(16777215);
    state->last_store_pc = 112;
    f->f = 0;
    state->last_store_pc = 117;
    mc_java_random *temporary = NULL;
    if (!world_random(runtime, &temporary, 121, 125))
        return false;
    f->g = mc_java_random_next_i32(temporary);
    state->last_store_pc = 131;
    state->direct_allocation_bytes -= sizeof(*temporary);
    free(temporary);
    f->h = INT32_C(1013904223);
    state->last_store_pc = 137;
    f->m = 0;
    state->last_store_pc = 142;
    f->n = 0;
    state->last_store_pc = 147;
    f->o = false;
    state->last_store_pc = 152;
    int64_t millis = 0;
    mc_platform_millis_status clock = mc_platform_current_time_millis(&millis);
    state->clock_status = (int32_t)clock;
    if (clock != MC_PLATFORM_MILLIS_OK) {
        state->failed_call_pc = 156;
        result(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World wall clock");
        return false;
    }
    f->I = millis;
    state->last_store_pc = 159;
    f->p = 40;
    state->last_store_pc = 165;
    if (!world_random(runtime, &f->r, 169, 173))
        return false;
    state->last_store_pc = 176;
    f->s = false;
    state->last_store_pc = 181;
    if (!world_list(runtime, &f->u, 189, 192) || !world_list(runtime, &f->K, 200, 203))
        return false;
    f->M = 0;
    state->last_store_pc = 208;
    f->N = true;
    state->last_store_pc = 213;
    f->O = true;
    state->last_store_pc = 218;
    f->P = world_allocate(runtime, sizeof(*f->P), 226, "World active chunk set");
    if (f->P == NULL)
        return false;
    state->last_store_pc = 229;
    (void)mc_java_random_next_bounded(f->r, 12000, &f->Q);
    state->last_store_pc = 243;
    if (!world_list(runtime, &f->R, 251, 254))
        return false;
    f->B = false;
    state->last_store_pc = 259;
    return true;
}

static bool world_info_result(mc_world_runtime *runtime, mc_world_info_status status,
                              unsigned int call_pc)
{
    if (status == MC_WORLD_INFO_OK)
        return true;
    runtime->construction.failed_call_pc = call_pc;
    mc_world_storage_status mapped =
        status == MC_WORLD_INFO_ALLOCATION_FAILED  ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_WORLD_INFO_LIMIT            ? MC_WORLD_STORAGE_LIMIT
        : status == MC_WORLD_INFO_INVALID_ARGUMENT ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                   : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    result(&runtime->last_error, mapped, mc_world_info_status_string(status));
    return false;
}

static void world_bind_info_cells(mc_world_runtime *runtime)
{
    const mc_world_info_live *cells = mc_world_info_live_get(runtime->fields.x);
    runtime->live = (mc_world_runtime_state){.time = cells->time,
                                             .spawn_x = cells->spawn_x,
                                             .spawn_y = cells->spawn_y,
                                             .spawn_z = cells->spawn_z,
                                             .world_force_load = &runtime->fields.y,
                                             .provider_force_load = &runtime->provider_force_load,
                                             .saving_disabled = &runtime->saving_disabled};
}

static bool world_new_info(mc_world_runtime *runtime,
                           const mc_world_runtime_world_arguments *arguments,
                           const mc_world_info *copy, bool copying, unsigned int call_pc,
                           unsigned int store_pc)
{
    mc_world_info *info = NULL;
    mc_world_info_status status = mc_world_info_allocate(runtime->classes->side, &info);
    if (status == MC_WORLD_INFO_OK)
        status = copying ? mc_world_info_construct_copy(info, copy)
                         : mc_world_info_construct(info, arguments->seed, arguments->name);
    if (!world_info_result(runtime, status, call_pc)) {
        mc_world_info_release(info);
        return false;
    }
    runtime->fields.x = info;
    runtime->construction.last_store_pc = store_pc;
    world_bind_info_cells(runtime);
    return true;
}

static bool world_saved_data(mc_world_runtime *runtime, unsigned int call_pc, unsigned int store_pc)
{
    mc_world_saved_data *saved = NULL;
    mc_world_storage_status status = mc_world_saved_data_create(
        runtime->fields.w, &runtime->construction_limits.saved_data, &saved, &runtime->last_error);
    if (status != MC_WORLD_STORAGE_OK) {
        runtime->construction.failed_call_pc = call_pc;
        return false;
    }
    runtime->fields.z = saved;
    runtime->construction.last_store_pc = store_pc;
    return true;
}

static bool world_dimension_result(mc_world_runtime *runtime, mc_world_dimension_status status,
                                   unsigned int call_pc)
{
    if (status == MC_WORLD_DIMENSION_OK)
        return true;
    runtime->construction.failed_call_pc = call_pc;
    mc_world_storage_status mapped =
        status == MC_WORLD_DIMENSION_ALLOCATION_FAILED  ? MC_WORLD_STORAGE_ALLOCATION_FAILED
        : status == MC_WORLD_DIMENSION_LIMIT            ? MC_WORLD_STORAGE_LIMIT
        : status == MC_WORLD_DIMENSION_INVALID_ARGUMENT ? MC_WORLD_STORAGE_INVALID_ARGUMENT
                                                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    result(&runtime->last_error, mapped, mc_world_dimension_status_string(status));
    return false;
}

static bool world_set_dimension(mc_world_runtime *runtime, mc_world_dimension *dimension,
                                unsigned int store_pc)
{
    if (dimension != NULL &&
        !world_dimension_result(runtime, mc_world_dimension_retain(dimension), store_pc))
        return false;
    runtime->fields.t = dimension;
    runtime->construction.last_store_pc = store_pc;
    return true;
}

static bool world_metadata_tail(mc_world_runtime *runtime,
                                const mc_world_runtime_world_arguments *arguments, bool *new_world)
{
    mc_world_runtime_fields *f = &runtime->fields;
    mc_world_runtime_construction *state = &runtime->construction;
    *new_world = false;
    if (state->entry == MC_WORLD_RUNTIME_CLIENT_COPY) {
        const mc_world_runtime *parent = arguments->parent;
        if (parent == NULL || parent->construction.mode != MC_WORLD_RUNTIME_GAME ||
            parent->options.random_profile != runtime->options.random_profile) {
            state->failed_call_pc = 264;
            result(&runtime->last_error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World copy parent");
            return false;
        }
        f->I = parent->fields.I;
        state->last_store_pc = 267;
        f->w = parent->fields.w;
        runtime->options.store = f->w;
        state->last_store_pc = 275;
        if (!world_new_info(runtime, arguments, parent->fields.x, true, 287, 290) ||
            !world_saved_data(runtime, 302, 305))
            return false;
        return world_set_dimension(runtime, arguments->dimension, 310);
    }
    f->w = arguments->store;
    runtime->options.store = f->w;
    state->last_store_pc = 264;
    if (state->entry == MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA) {
        if (!world_new_info(runtime, arguments, NULL, false, 275, 278) ||
            !world_set_dimension(runtime, arguments->dimension, 283) ||
            !world_saved_data(runtime, 292, 295))
            return false;
        return true;
    }
    if (!world_saved_data(runtime, 273, 276))
        return false;
    mc_world_info *loaded = NULL;
    mc_world_storage_status status =
        mc_world_store_load_world_info(f->w, runtime->classes->side, &loaded, &runtime->last_error);
    if (status != MC_WORLD_STORAGE_OK && status != MC_WORLD_STORAGE_MISSING) {
        state->failed_call_pc = 281;
        return false;
    }
    f->x = loaded;
    state->last_store_pc = 286;
    f->s = loaded == NULL;
    state->last_store_pc = 302;
    if (arguments->dimension != NULL) {
        if (!world_set_dimension(runtime, arguments->dimension, 313))
            return false;
    } else {
        int32_t selected = 0;
        if (loaded != NULL) {
            mc_world_info_fields fields;
            (void)mc_world_info_fields_get(loaded, &fields);
            if (fields.value.player_dimension == -1)
                selected = -1;
        }
        mc_world_dimension *dimension = NULL;
        mc_world_dimension_status created =
            mc_world_dimension_create(selected, NULL, &dimension, NULL);
        if (!world_dimension_result(runtime, created, selected == -1 ? 339u : 350u))
            return false;
        f->t = dimension;
        state->last_store_pc = selected == -1 ? 342u : 353u;
    }
    if (loaded == NULL) {
        if (!world_new_info(runtime, arguments, NULL, false, 373, 376))
            return false;
        *new_world = true;
    } else {
        if (!world_info_result(runtime, mc_world_info_name_set(loaded, arguments->name), 390))
            return false;
        world_bind_info_cells(runtime);
    }
    return true;
}

static bool world_provider(mc_world_runtime *runtime, unsigned int call_pc)
{
    runtime->construction.failed_call_pc = call_pc;
    if (runtime->fields.w == NULL) {
        result(&runtime->last_error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World loader storage");
        return false;
    }
    /* The concrete normal store loader is a stable borrowed store/dimension
     * binding. Its region files are opened by the actual first chunk request.
     * Other selected provider/loader kinds remain explicit missing descendants. */
    mc_world_dimension_state dimension;
    if (mc_world_dimension_state_get(runtime->fields.t, &dimension) != MC_WORLD_DIMENSION_OK) {
        result(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World loader Dimension");
        return false;
    }
    runtime->loader =
        (mc_world_store_loader){.store = runtime->fields.w,
                                .dimension = dimension.kind == MC_WORLD_DIMENSION_NETHER ? -1 : 0};
    mc_world_provider *provider = NULL;
    mc_java_random_profile_status cause = runtime->last_random_status;
    mc_world_storage_status status = mc_world_provider_create(
        runtime, runtime->fields.t, runtime->classes->trig, runtime->classes->profile,
        runtime->construction_limits.max_provider_chunks,
        runtime->construction_limits.max_provider_allocation_bytes, &provider, &cause,
        &runtime->last_error);
    runtime->last_random_status = cause;
    if (status != MC_WORLD_STORAGE_OK)
        return false;
    runtime->fields.v = provider;
    runtime->construction.last_store_pc = call_pc + 3u;
    runtime->construction.failed_call_pc = 0;
    mc_world_provider_fields fields;
    status = mc_world_provider_bind_fields(provider, &fields, &runtime->last_error);
    if (status != MC_WORLD_STORAGE_OK)
        return false;
    runtime->options.cache = fields.cache;
    runtime->options.pending = fields.pending;
    runtime->options.dimension = 0;
    runtime->options.policy = MC_CHUNK_ACQUIRE_BASE;
    runtime->options.generator = fields.generator;
    runtime->options.provider = provider;
    status = mc_world_runtime_bind_contexts(runtime, &runtime->last_error);
    if (status != MC_WORLD_STORAGE_OK) {
        runtime->construction.failed_call_pc = call_pc;
        return false;
    }
    return true;
}

static bool world_spawn_draw(mc_world_runtime *runtime, int32_t *out)
{
    if (runtime->fields.r == NULL || !mc_java_random_next_bounded(runtime->fields.r, 64, out)) {
        result(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
               "World spawn Random receiver");
        return false;
    }
    return true;
}

static bool world_initial_spawn(mc_world_runtime *runtime)
{
    runtime->fields.y = true;
    int32_t x = 0, z = 0;
    for (;;) {
        bool accepted;
        mc_world_dimension_error cause;
        mc_world_dimension_status status =
            mc_world_dimension_can_spawn(runtime->fields.t, x, z, &accepted, &cause);
        if (status != MC_WORLD_DIMENSION_OK) {
            if (status == MC_WORLD_DIMENSION_REGISTRY_FAILED)
                (void)mc_world_runtime_registry_result(runtime, cause.registry.status,
                                                       &cause.registry, "World spawn registry",
                                                       &runtime->last_error);
            else if (status == MC_WORLD_DIMENSION_WORLD_FAILED)
                runtime->last_error = cause.world;
            else
                result(&runtime->last_error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "World spawn Dimension");
            return false;
        }
        if (accepted)
            break;
        /* Reload the actual World.r separately at every original draw. x is
         * completed before the z pair. The source has no rejection cap. */
        int32_t first, second;
        if (!world_spawn_draw(runtime, &first))
            return false;
        if (!world_spawn_draw(runtime, &second))
            return false;
        x = mc_java_i32_add(x, mc_java_i32_sub(first, second));
        if (!world_spawn_draw(runtime, &first))
            return false;
        if (!world_spawn_draw(runtime, &second))
            return false;
        z = mc_java_i32_add(z, mc_java_i32_sub(first, second));
    }
    if (!world_info_result(runtime, mc_world_info_spawn_set(runtime->fields.x, x, 64, z), 415))
        return false;
    runtime->fields.y = false;
    return true;
}

mc_world_storage_status
mc_world_runtime_construct_world(mc_world_runtime *runtime,
                                 const mc_world_runtime_world_arguments *arguments,
                                 mc_world_storage_error *error)
{
    if (runtime == NULL || arguments == NULL)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World constructor arguments");
    if (runtime->construction.mode != MC_WORLD_RUNTIME_GAME ||
        runtime->construction.phase != MC_WORLD_RUNTIME_RAW)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World constructor phase");
    if (runtime->active_calls == SIZE_MAX)
        return result(error, MC_WORLD_STORAGE_LIMIT, "World constructor call limit");
    ++runtime->active_calls;
    runtime->construction.phase = MC_WORLD_RUNTIME_CONSTRUCTING;
    bool completed = false;
    bool prefix = world_prefix(runtime);
    if (prefix) {
        bool new_world = false;
        if (world_metadata_tail(runtime, arguments, &new_world)) {
            unsigned int bind_pc =
                runtime->construction.entry == MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA ? 300u
                : runtime->construction.entry == MC_WORLD_RUNTIME_CLIENT_COPY          ? 315u
                                                                                       : 398u;
            mc_world_dimension_error dimension_cause = {0};
            mc_world_dimension_status bound;
            if (runtime->classes->registry != NULL) {
                const mc_world_dimension_registry_binding binding = {
                    .classes = runtime->classes->registry,
                    .side = runtime->classes->side == MC_WORLD_INFO_CLIENT ? MC_REGISTRY_CLIENT
                                                                           : MC_REGISTRY_SERVER,
                    .profile = runtime->classes->profile};
                bound = mc_world_dimension_bind_game(runtime->fields.t, runtime, &binding,
                                                     &dimension_cause);
            } else {
                bound = mc_world_dimension_bind(runtime->fields.t, runtime, &dimension_cause);
            }
            bool dimension_ready = false;
            if (bound != MC_WORLD_DIMENSION_OK &&
                dimension_cause.registry.status != MC_REGISTRY_OK) {
                runtime->construction.failed_call_pc = bind_pc;
                (void)mc_world_runtime_registry_result(runtime, dimension_cause.registry.status,
                                                       &dimension_cause.registry,
                                                       "World Dimension Biome registry", NULL);
            } else {
                dimension_ready = world_dimension_result(runtime, bound, bind_pc);
            }
            if (dimension_ready) {
                if (world_provider(runtime, bind_pc + 5u)) {
                    bool spawn_ready = !new_world || world_initial_spawn(runtime);
                    if (!spawn_ready)
                        runtime->construction.failed_call_pc = 415;
                    if (spawn_ready) {
                        unsigned int light_pc = bind_pc == 398u ? 419u : bind_pc + 12u;
                        if (mc_world_runtime_initialize_environment(
                                runtime, &runtime->last_error) == MC_WORLD_STORAGE_OK)
                            completed = true;
                        else
                            runtime->construction.failed_call_pc = light_pc;
                    }
                }
            }
        }
    }
    runtime->construction.phase =
        completed ? MC_WORLD_RUNTIME_READY : MC_WORLD_RUNTIME_FAILED_CONSTRUCTION;
    --runtime->active_calls;
    if (error != NULL)
        *error = runtime->last_error;
    return runtime->last_error.status;
}

mc_world_storage_status mc_world_runtime_game_free(mc_world_runtime *runtime,
                                                   mc_world_storage_error *error)
{
    if (runtime->construction.contexts_bound || runtime->construction.cache_registered)
        return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, "World context retirement");
    mc_world_runtime_fields *f = &runtime->fields;
    mc_world_runtime_lighting_free(runtime);
    mc_world_reference_list *lists[] = {f->C, f->b, f->D, f->c, f->G, f->d, f->e, f->u, f->K, f->R};
    for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); ++i) {
        if (lists[i] != NULL) {
            free(lists[i]->items);
            free(lists[i]);
        }
    }
    mc_scheduled_ordered_destroy(f->E);
    mc_scheduled_membership_destroy(f->F);
    free(f->r);
    if (f->P != NULL) {
        free(f->P->entries);
        free(f->P);
    }
    mc_world_provider_free(f->v);
    mc_world_info_release(f->x);
    mc_world_saved_data_free(f->z);
    mc_world_dimension_release(f->t);
    --runtime->classes->live_worlds;
    free(runtime);
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_runtime_retire_world(mc_world_runtime *runtime,
                                                      mc_world_storage_error *error)
{
    if (runtime == NULL || runtime->construction.mode != MC_WORLD_RUNTIME_GAME)
        return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT, "World retirement arguments");
    if (runtime->active_calls != 0 || runtime->local_leases != 0)
        return result(error, MC_WORLD_STORAGE_BUSY, "World call or lease during retirement");
    if (runtime->construction.phase == MC_WORLD_RUNTIME_RETIRED)
        return result(error, MC_WORLD_STORAGE_OK, NULL);
    mc_world_runtime_fields *f = &runtime->fields;
    mc_world_reference_list *lists[] = {f->C, f->b, f->D, f->c, f->G, f->d, f->e, f->u, f->K, f->R};
    /* C contains typed World-owned records, retired at final native free.
     * Other unimplemented collection descendants still require emptiness. */
    for (size_t i = 1; i < sizeof(lists) / sizeof(lists[0]); ++i) {
        if (lists[i] != NULL && lists[i]->count != 0)
            return result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                          "World owned collection retirement");
    }
    int32_t scheduled_count = 0;
    if (f->E != NULL) {
        if (mc_scheduled_ordered_size(f->E, &scheduled_count) != MC_SCHEDULED_ORDERED_OK)
            return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                          "World scheduled ordered owner");
        if (scheduled_count != 0)
            return result(error, MC_WORLD_STORAGE_BUSY, "World scheduled ordered owner nonempty");
    }
    if (f->F != NULL) {
        if (mc_scheduled_membership_size(f->F, &scheduled_count) != MC_SCHEDULED_MEMBERSHIP_OK)
            return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                          "World scheduled membership owner");
        if (scheduled_count != 0)
            return result(error, MC_WORLD_STORAGE_BUSY,
                          "World scheduled membership owner nonempty");
    }
    if (f->P != NULL && f->P->count != 0)
        return result(error, MC_WORLD_STORAGE_BUSY, "World active chunk set nonempty");
    if (runtime->construction.contexts_bound || runtime->construction.cache_registered) {
        if (!mc_chunk_cache_validate(runtime->options.cache) ||
            !mc_chunk_pending_validate(runtime->options.pending))
            return result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                          "World retirement cache bindings");
        if (runtime->options.cache->count != 0 || runtime->options.cache->insertion_count != 0 ||
            runtime->options.pending->count != 0)
            return result(error, MC_WORLD_STORAGE_BUSY, "World retirement cache nonempty");
        if (runtime->construction.cache_registered) {
            mc_world_storage_status status = mc_world_store_unregister_cache(
                runtime->options.store, runtime->options.cache, error);
            if (status != MC_WORLD_STORAGE_OK)
                return status;
            runtime->construction.cache_registered = false;
        }
        runtime->construction.contexts_bound = false;
    }
    if (mc_world_dimension_world(f->t) == runtime)
        mc_world_dimension_unbind(f->t);
    runtime->construction.phase = MC_WORLD_RUNTIME_RETIRED;
    return result(error, MC_WORLD_STORAGE_OK, NULL);
}
