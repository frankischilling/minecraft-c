#include "prepare_world.h"
#include "../../entity/entity_internal.h"
#include "../../world/runtime/chunk_provider_internal.h"
#include "../application_state.h"
#include "../startup/application_owner_internal.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/blocks.h"
#include "mc/world/runtime/maintenance.h"

#include <stdlib.h>
#include <string.h>
#include <threads.h>

typedef struct captured_loading {
    mc_client_loading_screen *owner;
    bool owned_call;
} captured_loading;

struct mc_client_world_prepare_call {
    mc_client_world_prepare_state state;
    client_application *app;
    mc_player_string *text;
    mc_player_coordinates *spawn;
    bool active;
    captured_loading loading[292]; /* start + two messages + 289 actual progress calls. */
    mc_client_world_prepare_sample samples[289];
    mc_client_loading_error loading_error, loading_cleanup;
    mc_world_storage_error world;
    mc_world_spawn_error spawn_error;
    mc_world_runtime_acquisition_error acquisition;
    mc_world_light_operation_error light;
    mc_world_runtime_lifecycle_error maintenance;
};

static bool fail(mc_client_world_prepare_call *call, mc_client_world_prepare_status status,
                 const char *operation, unsigned int pc) {
    call->state.status = status;
    call->state.operation = operation;
    call->state.source_pc = pc;
    call->state.phase = MC_CLIENT_WORLD_PREPARE_FAILED;
    return false;
}

static mc_client_world_prepare_status loading_status(mc_client_loading_status status) {
    if (status == MC_CLIENT_LOADING_BUSY)
        return MC_CLIENT_WORLD_PREPARE_BUSY;
    if (status == MC_CLIENT_LOADING_NULL_REFERENCE)
        return MC_CLIENT_WORLD_PREPARE_NULL_REFERENCE;
    if (status == MC_CLIENT_LOADING_LIMIT)
        return MC_CLIENT_WORLD_PREPARE_LIMIT;
    if (status == MC_CLIENT_LOADING_ALLOCATION_FAILED)
        return MC_CLIENT_WORLD_PREPARE_ALLOCATION_FAILED;
    return MC_CLIENT_WORLD_PREPARE_LOADING_FAILED;
}

static bool literal(mc_client_world_prepare_call *call, bool simulation, mc_player_string **out,
                    unsigned int pc) {
    call->state.source_pc = pc;
    client_application *app = call->app;
    if (app->native_lifetime_phase != MC_CLIENT_APPLICATION_LIFETIME_REGISTERED ||
        !app->source_namespace_borrowed || app->source_namespace == NULL)
        return fail(call, MC_CLIENT_WORLD_PREPARE_UNIMPLEMENTED,
                    "actual MC terrain-preparation literal namespace", pc);
    mc_client_application_classes *classes = app->source_namespace;
    const int locked = mtx_trylock(&classes->mutex);
    if (locked != thrd_success) {
        call->state.native_thread_error = locked;
        return fail(call, MC_CLIENT_WORLD_PREPARE_BUSY,
                    "terrain-preparation native class literal monitor", pc);
    }
    bool ok = true;
    if (classes->native_retiring || classes->phase != MC_CLIENT_APPLICATION_CLASS_READY)
        ok = fail(call, MC_CLIENT_WORLD_PREPARE_BUSY, "terrain-preparation active class namespace",
                  pc);
    else {
        mc_player_string **slot =
            simulation ? &classes->world_simulating : &classes->world_building_terrain;
        if (*slot == NULL) {
            const char *ascii = simulation ? "Simulating world for a bit" : "Building terrain";
            uint16_t units[32];
            const size_t length = strlen(ascii);
            for (size_t i = 0; i < length; ++i)
                units[i] = (uint8_t)ascii[i];
            call->state.string_status = mc_player_string_create(units, length, slot);
            if (call->state.string_status != MC_PLAYER_OK)
                ok = fail(call, MC_CLIENT_WORLD_PREPARE_STRING_FAILED,
                          "actual MC terrain-preparation literal resolution", pc);
        }
        if (ok)
            *out = *slot;
    }
    /* Same selected live-class mutex contract as existing startup literals. */
    (void)mtx_unlock(&classes->mutex);
    return ok;
}

typedef enum loading_method { START, MESSAGE, PROGRESS } loading_method;

static bool loading(mc_client_world_prepare_call *call, mc_client_loading_screen *owner,
                    loading_method method, mc_player_string *text, int32_t progress,
                    unsigned int pc) {
    call->state.source_pc = pc;
    if (call->state.loading_calls >= sizeof(call->loading) / sizeof(call->loading[0]))
        return fail(call, MC_CLIENT_WORLD_PREPARE_LIMIT,
                    "terrain-preparation derived Loading occurrence bound", pc);
    captured_loading *capture = &call->loading[call->state.loading_calls++];
    if (owner != NULL) {
        const mc_client_loading_status held = mc_client_loading_screen_retain(owner);
        if (held != MC_CLIENT_LOADING_OK) {
            call->loading_error = (mc_client_loading_error){.status = held};
            return fail(call, loading_status(held), "captured actual Loading native occurrence",
                        pc);
        }
        capture->owner = owner;
    }
    mc_client_loading_call_state before = {0};
    (void)mc_client_loading_screen_call_inspect(owner, &before);
    mc_client_loading_status status;
    if (method == START)
        status = mc_client_loading_screen_start(owner, text, &call->loading_error);
    else if (method == MESSAGE)
        status = mc_client_loading_screen_message(owner, text, &call->loading_error);
    else
        status = mc_client_loading_screen_progress(owner, progress, &call->loading_error);
    mc_client_loading_call_state after = {0};
    if (mc_client_loading_screen_call_inspect(owner, &after))
        capture->owned_call = !before.retained && after.retained;
    /* A pre-existing foreign retained call remains its original caller's right.
     * This record only retires lower work actually created by this invocation. */
    return status == MC_CLIENT_LOADING_OK ||
           fail(call, loading_status(status), "actual terrain-preparation Loading child", pc);
}

static bool world_receiver(mc_client_world_prepare_call *call, mc_world_runtime **out,
                           unsigned int pc) {
    call->state.source_pc = pc;
    *out = call->app->active_world; /* Fresh actual App.f at each reached invocation. */
    call->state.last_world = *out;
    if (*out == NULL) {
        call->world = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                               "actual terrain-preparation null World receiver", 0};
        return fail(call, MC_CLIENT_WORLD_PREPARE_NULL_REFERENCE,
                    "actual terrain-preparation null World receiver", pc);
    }
    return true;
}

static bool provider_and_spawn(mc_client_world_prepare_call *call) {
    mc_world_runtime *world;
    if (!world_receiver(call, &world, 44))
        return false;
    call->state.provider_world = world;
    mc_world_runtime_fields fields;
    call->world.status = mc_world_runtime_fields_get(world, &fields);
    if (call->world.status != MC_WORLD_STORAGE_OK) {
        call->world.operation = "actual fd.w World fields";
        return fail(call, MC_CLIENT_WORLD_PREPARE_WORLD_FAILED, call->world.operation, 44);
    }
    call->state.provider = fields.v; /* Actual fd.w() is getfield v1 / areturn4. */
    if (!world_receiver(call, &world, 53))
        return false;
    call->state.spawn_world = world;
    call->spawn = calloc(1, sizeof(*call->spawn)); /* Actual fd.u NEW br0 before arguments. */
    if (call->spawn == NULL)
        return fail(call, MC_CLIENT_WORLD_PREPARE_ALLOCATION_FAILED, "actual fd.u new br owner",
                    53);
    int32_t values[3];
    if (!mc_world_source_spawn_values(world, values, &call->spawn_error))
        return fail(call,
                    call->spawn_error.info_was_null ? MC_CLIENT_WORLD_PREPARE_NULL_REFERENCE
                                                    : MC_CLIENT_WORLD_PREPARE_WORLD_FAILED,
                    "actual fd.u Info argument getter", 53);
    call->spawn->x = values[0]; /* Actual br constructor stores6/11/16. */
    call->spawn->y = values[1];
    call->spawn->z = values[2];
    call->state.spawn_constructed = true;
    if (call->app->h != NULL) { /* Actual h62 gate, then independent h68/h81 reads. */
        mc_entity *player = call->app->h;
        if (player == NULL)
            return fail(call, MC_CLIENT_WORLD_PREPARE_NULL_REFERENCE, "actual Player.aM receiver",
                        71);
        if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
            return fail(call, MC_CLIENT_WORLD_PREPARE_UNIMPLEMENTED,
                        "terrain preparation actual dc Player owner", 71);
        call->spawn->x = mc_java_double_to_i32(mc_entity_runtime_fields(player)->x);
        call->state.player_x_stored = true; /* Actual br.a75; truncation, never floor. */
        player = call->app->h;
        if (player == NULL)
            return fail(call, MC_CLIENT_WORLD_PREPARE_NULL_REFERENCE, "actual Player.aO receiver",
                        84);
        if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER))
            return fail(call, MC_CLIENT_WORLD_PREPARE_UNIMPLEMENTED,
                        "terrain preparation actual dc Player owner", 84);
        call->spawn->z = mc_java_double_to_i32(mc_entity_runtime_fields(player)->z);
        call->state.player_z_stored = true; /* Actual br.c88; Y remains source spawn Y. */
    }
    call->state.source_pc = 93;
    const mc_chunk_provider_concrete_kind kind =
        mc_chunk_provider_concrete_kind_of(call->state.provider);
    if (kind != MC_CHUNK_PROVIDER_NULL && kind != MC_CHUNK_PROVIDER_MAP_CACHE)
        return fail(call, MC_CLIENT_WORLD_PREPARE_UNIMPLEMENTED,
                    "actual kx centered-provider method dependency", 122);
    /* NULL instanceof is false; genuine ok/jb is unrelated to kx. Neither
     * source branch is selected by native cache policy or its grid arrays. */
    return true;
}

static bool execute(mc_client_world_prepare_call *call) {
    client_application *app = call->app;
    if (!loading(call, app->loading_screen, START, call->text, 0, 5))
        return false;
    mc_client_loading_screen *receiver = app->loading_screen; /* Actual fresh s9 before ldc12. */
    mc_player_string *message = NULL;
    if (!literal(call, false, &message, 12) || !loading(call, receiver, MESSAGE, message, 0, 14))
        return false;
    call->state.radius = 128;
    call->state.count = 0;
    int32_t side;
    if (!mc_java_i32_div(mc_java_i32_mul(call->state.radius, 2), 16, &side))
        return fail(call, MC_CLIENT_WORLD_PREPARE_UNIMPLEMENTED, "fixed terrain-grid division", 28);
    side = mc_java_i32_add(side, 1);
    call->state.total = mc_java_i32_mul(side, side); /* Actual local stores20/22/38. */
    if (!provider_and_spawn(call))
        return false;
    for (call->state.dx = -128; call->state.dx <= 128;
         call->state.dx = mc_java_i32_add(call->state.dx, 16)) {
        for (call->state.dz = -128; call->state.dz <= 128;
             call->state.dz = mc_java_i32_add(call->state.dz, 16)) {
            mc_client_world_prepare_sample *sample = &call->samples[call->state.samples_started++];
            sample->dx = call->state.dx;
            sample->dz = call->state.dz;
            receiver = app->loading_screen; /* Actual s146 capture BEFORE count++150. */
            const int32_t previous = call->state.count;
            call->state.count = mc_java_i32_add(previous, 1);
            if (!mc_java_i32_div(mc_java_i32_mul(previous, 100), call->state.total,
                                 &sample->progress))
                return fail(call, MC_CLIENT_WORLD_PREPARE_UNIMPLEMENTED,
                            "terrain progress division", 158);
            call->state.last_progress = sample->progress;
            sample->progress_called = true;
            ++call->state.progress_calls;
            const bool returned = loading(call, receiver, PROGRESS, NULL, sample->progress, 159);
            sample->progress_returned = call->loading_error.source_returned;
            if (sample->progress_returned)
                ++call->state.progress_returns;
            if (!returned)
                return false;
            sample->x = mc_java_i32_add(call->spawn->x, sample->dx);
            sample->z = mc_java_i32_add(call->spawn->z, sample->dz);
            sample->block_called = true;
            mc_world_runtime *world;
            if (!world_receiver(call, &world, 184))
                return false;
            sample->block_world = world;
            int32_t ignored;
            call->world.status = mc_world_runtime_block_get_detailed(
                world, sample->x, 64, sample->z, &ignored, &call->world, &call->acquisition);
            if (call->world.status != MC_WORLD_STORAGE_OK)
                return fail(call, MC_CLIENT_WORLD_PREPARE_WORLD_FAILED,
                            "actual terrain-preparation blockID child", 184);
            sample->block_returned = true;
            ++call->state.block_samples;
            bool more;
            do {
                if (!world_receiver(call, &world, 192))
                    return false;
                ++sample->light_calls;
                ++call->state.light_calls;
                call->light.status =
                    mc_world_runtime_light_drain(world, SIZE_MAX, &more, &call->light);
                if (call->light.status != MC_WORLD_STORAGE_OK)
                    return fail(call, MC_CLIENT_WORLD_PREPARE_WORLD_FAILED,
                                "actual terrain-preparation fd.j light drain", 192);
                if (more) {
                    ++sample->light_true_returns;
                    ++call->state.light_true_returns;
                }
            } while (more); /* Source repeats to false, with a fresh App.f each time. */
        }
    }
    receiver = app->loading_screen; /* Actual fresh s214 before ldc217. */
    if (!literal(call, true, &message, 217) || !loading(call, receiver, MESSAGE, message, 0, 219))
        return false;
    call->state.unused_simulation_count = 2000; /* Dead local225, no ticks. */
    mc_world_runtime *world;
    if (!world_receiver(call, &world, 231))
        return false;
    call->maintenance.status = mc_world_runtime_finish_maintenance(world, &call->maintenance);
    if (call->maintenance.status != MC_CHUNK_LIFECYCLE_OK)
        return fail(call, MC_CLIENT_WORLD_PREPARE_WORLD_FAILED,
                    "actual terrain-preparation fd.p maintenance", 231);
    call->state.status = MC_CLIENT_WORLD_PREPARE_OK;
    call->state.phase = MC_CLIENT_WORLD_PREPARE_RETURNED;
    call->state.source_returned = true;
    call->state.source_pc = 234;
    return true;
}

mc_client_world_prepare_status mc_client_world_prepare_begin(client_application *app,
                                                             mc_player_string *text,
                                                             mc_client_world_prepare_call **out) {
    if (app == NULL || out == NULL || *out != NULL)
        return MC_CLIENT_WORLD_PREPARE_INVALID_ARGUMENT;
    mc_client_world_prepare_call *call = calloc(1, sizeof(*call));
    if (call == NULL)
        return MC_CLIENT_WORLD_PREPARE_ALLOCATION_FAILED;
    call->app = app;
    call->active = true;
    call->state.phase = MC_CLIENT_WORLD_PREPARE_ACTIVE;
    *out = call;
    if (text != NULL) {
        call->state.string_status = mc_player_string_retain(text);
        if (call->state.string_status != MC_PLAYER_OK) {
            (void)fail(call, MC_CLIENT_WORLD_PREPARE_STRING_FAILED,
                       "terrain-preparation native argument String hold", 0);
            call->active = false;
            return call->state.status;
        }
        call->text = text;
    }
    (void)execute(call);
    call->active = false;
    return call->state.status;
}

bool mc_client_world_prepare_inspect(const mc_client_world_prepare_call *call,
                                     mc_client_world_prepare_state *out) {
    if (call == NULL || out == NULL)
        return false;
    *out = call->state;
    out->app = call->app;
    out->text = call->text;
    out->spawn = call->spawn;
    out->samples = call->samples;
    out->loading = &call->loading_error;
    out->loading_cleanup = &call->loading_cleanup;
    out->world = &call->world;
    out->spawn_error = &call->spawn_error;
    out->acquisition = &call->acquisition;
    out->light = &call->light;
    out->maintenance = &call->maintenance;
    return true;
}

static void clear_loading_views(mc_client_loading_error *error) {
    error->lookup = NULL;
    error->update = NULL;
    error->cleanup = NULL;
}

mc_client_world_prepare_status mc_client_world_prepare_retire(mc_client_world_prepare_call **slot) {
    if (slot == NULL)
        return MC_CLIENT_WORLD_PREPARE_INVALID_ARGUMENT;
    mc_client_world_prepare_call *call = *slot;
    if (call == NULL)
        return MC_CLIENT_WORLD_PREPARE_OK;
    if (call->active)
        return MC_CLIENT_WORLD_PREPARE_BUSY;
    call->active = true;
    call->state.phase = MC_CLIENT_WORLD_PREPARE_RETIRING;
    for (size_t i = 0; i < call->state.loading_calls; ++i) {
        captured_loading *capture = &call->loading[i];
        if (!capture->owned_call)
            continue;
        const mc_client_loading_status status =
            mc_client_loading_screen_retire_call(capture->owner, &call->loading_cleanup);
        if (status != MC_CLIENT_LOADING_OK) {
            call->state.cleanup_status = status == MC_CLIENT_LOADING_BUSY
                                             ? MC_CLIENT_WORLD_PREPARE_BUSY
                                             : MC_CLIENT_WORLD_PREPARE_CLEANUP_FAILED;
            call->state.cleanup_operation = "terrain-preparation retained Loading call retirement";
            call->active = false;
            return call->state.cleanup_status;
        }
        capture->owned_call = false;
        clear_loading_views(&call->loading_error);
        clear_loading_views(&call->loading_cleanup);
    }
    for (size_t i = 0; i < call->state.loading_calls; ++i) {
        captured_loading *capture = &call->loading[i];
        if (capture->owner == NULL)
            continue;
        const mc_client_loading_status status = mc_client_loading_screen_release(capture->owner);
        if (status != MC_CLIENT_LOADING_OK) {
            call->loading_cleanup = (mc_client_loading_error){.status = status};
            call->state.cleanup_status = status == MC_CLIENT_LOADING_BUSY
                                             ? MC_CLIENT_WORLD_PREPARE_BUSY
                                             : MC_CLIENT_WORLD_PREPARE_CLEANUP_FAILED;
            call->state.cleanup_operation =
                "terrain-preparation captured Loading occurrence release";
            call->active = false;
            return call->state.cleanup_status;
        }
        capture->owner = NULL;
    }
    mc_player_string_release(call->text);
    free(call->spawn);
    free(call);
    *slot = NULL;
    return MC_CLIENT_WORLD_PREPARE_OK;
}
