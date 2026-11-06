#include "mc/client/sound/construction.h"
#include "mc/client/sound/sound_file_registration.h"

#include "pool_construction.h"

#include "../settings/sound_access.h"

#include <stdio.h>
#include <stdlib.h>

struct mc_sound_manager_shared {
    mc_sound_manager_backend calls;
    void *backend;
    size_t managers;
    bool initialized, active;
    mc_sound_status setup_failure;
    int32_t setup_failure_pc;
    mc_game_settings_status setup_settings_status;
    mc_game_settings_error setup_settings_error;
    mc_sound_error setup_backend_error;
};

struct mc_sound_manager {
    mc_sound_manager_shared *shared;
    mc_sound_pool *pools[MC_SOUND_MANAGER_POOL_COUNT];
    mc_java_random random;
    mc_game_settings *settings;
    int32_t counter, delay;
    bool active;
    mc_sound_manager_phase phase;
    mc_sound_pool *pending_pool;
    mc_sound_pool_constructor_prefix pool_prefixes[MC_SOUND_MANAGER_POOL_COUNT], pending_prefix;
    int32_t pending_pool_index, last_store_pc;
    unsigned published_pool_mask;
    bool counter_published, random_published, delay_published;
    mc_sound_manager_constructor_error constructor_failure;
};

static mc_sound_status result(mc_sound_error *error, mc_sound_status status, const char *operation,
                              int32_t pc) {
    if (error != NULL)
        *error = (mc_sound_error){.status = status, .operation = operation, .source_pc = pc};
    return status;
}

static mc_sound_status admit(mc_sound_manager *manager, mc_sound_error *error) {
    if (manager == NULL)
        return result(error, MC_SOUND_INVALID_ARGUMENT, "SoundManager receiver", -1);
    if (manager->active || manager->shared->active)
        return result(error, MC_SOUND_BUSY, "SoundManager active operation", -1);
    if (manager->phase != MC_SOUND_MANAGER_READY)
        return result(error, MC_SOUND_INVALID_ARGUMENT, "SoundManager unconstructed operation", -1);
    manager->active = true;
    manager->shared->active = true;
    return MC_SOUND_OK;
}

static mc_sound_status finish(mc_sound_manager *manager, const mc_sound_manager_report *report,
                              mc_sound_manager_report *out, mc_sound_error *error,
                              mc_sound_status status) {
    manager->active = false;
    manager->shared->active = false;
    if (out != NULL)
        *out = *report;
    if (status == MC_SOUND_OK)
        result(error, status, NULL, report->source_pc);
    return status;
}

static mc_sound_status source_failure(mc_sound_manager_report *report, mc_sound_error *error,
                                      mc_sound_manager_source_exception exception, int32_t pc) {
    report->source_pc = pc;
    report->exception = exception;
    return result(error, MC_SOUND_SOURCE_EXCEPTION,
                  exception == MC_SOUND_MANAGER_EXCEPTION_NULL_SETTINGS
                      ? "SoundManager null GameSettings"
                      : "SoundManager null SoundSystem",
                  pc);
}

static mc_sound_status backend_result(mc_sound_status status, mc_sound_manager_report *report,
                                      mc_sound_error *error, int32_t pc) {
    report->source_pc = pc;
    if (status == MC_SOUND_SOURCE_EXCEPTION)
        report->exception = MC_SOUND_MANAGER_EXCEPTION_BACKEND_THROWABLE;
    if (error != NULL && status != MC_SOUND_OK) {
        error->status = status;
        error->source_pc = pc;
    }
    return status;
}

static mc_sound_status command(mc_sound_manager *manager, mc_sound_manager_command request,
                               mc_sound_manager_report *report, mc_sound_error *error, int32_t pc) {
    mc_sound_manager_shared *shared = manager->shared;
    if (shared->backend == NULL)
        return source_failure(report, error, MC_SOUND_MANAGER_EXCEPTION_NULL_BACKEND, pc);
    mc_sound_error backend_error = {0};
    const mc_sound_status status =
        shared->calls.command(shared->calls.context, shared->backend, &request, &backend_error);
    if (error != NULL && status != MC_SOUND_OK)
        *error = backend_error;
    return backend_result(status, report, error, pc);
}

static mc_sound_status playing(mc_sound_manager *manager, const char *name, bool *out,
                               mc_sound_manager_report *report, mc_sound_error *error, int32_t pc) {
    mc_sound_manager_shared *shared = manager->shared;
    if (shared->backend == NULL)
        return source_failure(report, error, MC_SOUND_MANAGER_EXCEPTION_NULL_BACKEND, pc);
    mc_sound_error backend_error = {0};
    const mc_sound_status status =
        shared->calls.playing(shared->calls.context, shared->backend, name, out, &backend_error);
    if (error != NULL && status != MC_SOUND_OK)
        *error = backend_error;
    return backend_result(status, report, error, pc);
}

static mc_sound_status settings_result(mc_game_settings_status status,
                                       mc_sound_manager_setup_report *report, mc_sound_error *error,
                                       int32_t pc) {
    report->settings_status = status;
    report->source_pc = pc;
    if (status == MC_GAME_SETTINGS_OK)
        return MC_SOUND_OK;
    mc_sound_status native = MC_SOUND_IO_FAILED;
    if (status == MC_GAME_SETTINGS_BUSY)
        native = MC_SOUND_BUSY;
    else if (status == MC_GAME_SETTINGS_ALLOCATION_FAILED)
        native = MC_SOUND_ALLOCATION_FAILED;
    else if (status == MC_GAME_SETTINGS_LIMIT)
        native = MC_SOUND_LIMIT;
    else if (status == MC_GAME_SETTINGS_INVALID_ARGUMENT)
        native = MC_SOUND_INVALID_ARGUMENT;
    return result(error, native, "SoundManager GameSettings operation", pc);
}

static mc_sound_status setup_native_failure(mc_sound_manager_shared *shared,
                                            mc_sound_manager_setup_report *report,
                                            mc_sound_status status) {
    shared->setup_failure = status;
    shared->setup_failure_pc = report->source_pc;
    shared->setup_settings_status = report->settings_status;
    shared->setup_settings_error = report->settings_error;
    shared->setup_backend_error = report->backend_error;
    /* A callback's operation text need not outlive the original call. Retain
     * classifications/numeric details, with static text for later reports. */
    shared->setup_settings_error.operation = "SoundManager retained native settings failure";
    shared->setup_backend_error.operation = "SoundManager retained native backend failure";
    report->native_terminal = true;
    return status;
}

static mc_sound_status setup(mc_sound_manager *manager,
                             const mc_game_settings_diagnostics *diagnostics,
                             mc_sound_manager_report *report, mc_sound_error *error) {
    mc_sound_manager_setup_report *setup_report = &report->setup;
    *setup_report = (mc_sound_manager_setup_report){.entered = true};
    mc_sound_manager_shared *shared = manager->shared;
    if (shared->setup_failure != MC_SOUND_OK) {
        setup_report->source_pc = shared->setup_failure_pc;
        setup_report->native_terminal = true;
        setup_report->settings_status = shared->setup_settings_status;
        setup_report->settings_error = shared->setup_settings_error;
        setup_report->backend_error = shared->setup_backend_error;
        return result(error, shared->setup_failure, "SoundManager terminal native setup failure",
                      shared->setup_failure_pc);
    }
    mc_game_settings_state state;
    mc_sound_status status = MC_SOUND_OK;
    if (!mc_game_settings_read_state(manager->settings, &state)) {
        setup_report->source_pc = 4;
        setup_report->exception = MC_SOUND_MANAGER_EXCEPTION_NULL_SETTINGS;
        goto caught;
    }
    const float sound = state.values.sound_volume;
    const float music = state.values.music_volume;
    status =
        settings_result(mc_game_settings_sound_set_volumes(manager->settings, manager, 0.0f, 0.0f,
                                                           &setup_report->settings_error),
                        setup_report, error, 29);
    if (status != MC_SOUND_OK)
        return setup_native_failure(shared, setup_report, status);
    setup_report->volumes_zeroed = true;
    setup_report->first_save_reached = true;
    status = settings_result(mc_game_settings_sound_save(manager->settings, manager, diagnostics,
                                                         &setup_report->saves[0],
                                                         &setup_report->settings_error),
                             setup_report, error, 36);
    if (status != MC_SOUND_OK)
        return setup_native_failure(shared, setup_report, status);
    static const int32_t configuration_pcs[] = {41, 48, 55, 62};
    for (size_t i = 0; i < sizeof(configuration_pcs) / sizeof(configuration_pcs[0]); ++i) {
        setup_report->source_pc = configuration_pcs[i];
        status = shared->calls.configure(shared->calls.context, (mc_sound_manager_configuration)i,
                                         &setup_report->backend_error);
        if (status != MC_SOUND_OK)
            goto backend_failed;
    }
    setup_report->source_pc = 69;
    void *backend = NULL;
    status = shared->calls.construct(shared->calls.context, &backend, &setup_report->backend_error);
    if (status != MC_SOUND_OK)
        goto backend_failed;
    if (backend == NULL) {
        status = result(error, MC_SOUND_INVALID_ARGUMENT,
                        "SoundManager construct returned null backend", 69);
        return setup_native_failure(shared, setup_report, status);
    }
    shared->backend = backend;
    setup_report->backend_published = true;
    if (!mc_game_settings_read_state(manager->settings, &state)) {
        setup_report->source_pc = 80;
        status = result(error, MC_SOUND_INVALID_ARGUMENT, "SoundManager live GameSettings", 80);
        return setup_native_failure(shared, setup_report, status);
    }
    status =
        settings_result(mc_game_settings_sound_set_volumes(manager->settings, manager, music, sound,
                                                           &setup_report->settings_error),
                        setup_report, error, 88);
    if (status != MC_SOUND_OK)
        return setup_native_failure(shared, setup_report, status);
    setup_report->volumes_restored = true;
    setup_report->second_save_reached = true;
    status = settings_result(mc_game_settings_sound_save(manager->settings, manager, diagnostics,
                                                         &setup_report->saves[1],
                                                         &setup_report->settings_error),
                             setup_report, error, 95);
    if (status != MC_SOUND_OK)
        return setup_native_failure(shared, setup_report, status);
    shared->initialized = true;
    setup_report->source_pc = 118;
    return MC_SOUND_OK;
backend_failed:
    setup_report->backend_error.status = status;
    setup_report->backend_error.source_pc = setup_report->source_pc;
    if (status != MC_SOUND_SOURCE_EXCEPTION) {
        if (error != NULL)
            *error = setup_report->backend_error;
        return setup_native_failure(shared, setup_report, status);
    }
    setup_report->exception = MC_SOUND_MANAGER_EXCEPTION_BACKEND_THROWABLE;
caught:
    setup_report->throwable_caught = true;
    shared->initialized = true;
    return result(error, MC_SOUND_OK, NULL, 118);
}

mc_sound_status mc_sound_manager_shared_create(const mc_sound_manager_backend *backend,
                                               mc_sound_manager_shared **out,
                                               mc_sound_error *error) {
    if (backend == NULL || out == NULL || backend->configure == NULL ||
        backend->construct == NULL || backend->command == NULL || backend->playing == NULL ||
        backend->retire == NULL)
        return result(error, MC_SOUND_INVALID_ARGUMENT, "SoundManager backend interface", -1);
    mc_sound_manager_shared *shared = calloc(1, sizeof(*shared));
    if (shared == NULL)
        return result(error, MC_SOUND_ALLOCATION_FAILED, "SoundManager shared owner", -1);
    shared->calls = *backend;
    *out = shared;
    return result(error, MC_SOUND_OK, NULL, -1);
}

mc_sound_status mc_sound_manager_shared_retire(mc_sound_manager_shared **owner,
                                               mc_sound_error *error) {
    if (owner == NULL)
        return result(error, MC_SOUND_INVALID_ARGUMENT, "SoundManager shared owner pointer", -1);
    mc_sound_manager_shared *shared = *owner;
    if (shared == NULL)
        return result(error, MC_SOUND_OK, NULL, -1);
    if (shared->active || shared->managers != 0)
        return result(error, MC_SOUND_BUSY, "SoundManager shared owner in use", -1);
    if (shared->backend != NULL) {
        shared->active = true;
        const mc_sound_status status =
            shared->calls.retire(shared->calls.context, &shared->backend, error);
        shared->active = false;
        if (status != MC_SOUND_OK)
            return status;
        if (shared->backend != NULL)
            return result(error, MC_SOUND_INVALID_ARGUMENT,
                          "SoundManager backend retirement retained handle", -1);
    }
    free(shared);
    *owner = NULL;
    return result(error, MC_SOUND_OK, NULL, -1);
}

/* A separately compiled allocation-fault control can wrap the exact owner
 * allocation. Production has no runtime allocator callback or alternate body. */
#if defined(MC_SOUND_MANAGER_CALLOC)
extern void *MC_SOUND_MANAGER_CALLOC(size_t, size_t);
#else
#define MC_SOUND_MANAGER_CALLOC calloc
#endif

mc_sound_status mc_sound_manager_allocate(mc_sound_manager_shared *shared, mc_sound_manager **out,
                                          mc_sound_error *error) {
    if (shared == NULL || out == NULL)
        return result(error, MC_SOUND_INVALID_ARGUMENT, "SoundManager construction", -1);
    if (shared->active)
        return result(error, MC_SOUND_BUSY, "SoundManager shared owner active", -1);
    if (shared->managers == SIZE_MAX)
        return result(error, MC_SOUND_LIMIT, "SoundManager owner count", -1);
    mc_sound_manager *manager = MC_SOUND_MANAGER_CALLOC(1, sizeof(*manager));
    if (manager == NULL)
        return result(error, MC_SOUND_ALLOCATION_FAILED, "SoundManager owner", -1);
    manager->shared = shared;
    manager->pending_pool_index = -1;
    manager->last_store_pc = -1;
    manager->pending_prefix.last_store_pc = -1;
    manager->constructor_failure.pool_index = -1;
    for (size_t i = 0; i < MC_SOUND_MANAGER_POOL_COUNT; ++i)
        manager->pool_prefixes[i].last_store_pc = -1;
    ++shared->managers; /* Native lifetime includes RAW and retained failures. */
    *out = manager;
    return result(error, MC_SOUND_OK, NULL, -1);
}

static mc_sound_status construction_error(mc_sound_manager_constructor_error *out,
                                          mc_sound_status status, const char *operation,
                                          int32_t yo_pc, int32_t hr_pc, int32_t index,
                                          const mc_sound_error *cause) {
    if (out != NULL) {
        *out = (mc_sound_manager_constructor_error){
            .status = status, .operation = operation, .native_policy = true, .pool_index = index};
        if (yo_pc >= 0)
            out->frames[out->frame_count++] =
                (mc_sound_constructor_site){"yo", "<init>", "()V", yo_pc};
        if (hr_pc >= 0)
            out->frames[out->frame_count++] =
                (mc_sound_constructor_site){"hr", "<init>", "()V", hr_pc};
        if (cause != NULL)
            out->cause = *cause;
        else
            result(&out->cause, status, operation, -1);
    }
    return status;
}

mc_sound_status mc_sound_manager_construct(mc_sound_manager *manager,
                                           mc_java_random_profile *profile,
                                           size_t pool_allocation_bytes,
                                           mc_sound_manager_constructor_error *error) {
    if (manager == NULL || profile == NULL)
        return construction_error(error, MC_SOUND_INVALID_ARGUMENT,
                                  "SoundManager constructor arguments", -1, -1, -1, NULL);
    if (manager->active || manager->shared->active)
        return construction_error(error, MC_SOUND_BUSY, "SoundManager active constructor", -1, -1,
                                  -1, NULL);
    if (manager->phase != MC_SOUND_MANAGER_RAW)
        return construction_error(error, MC_SOUND_INVALID_ARGUMENT,
                                  "SoundManager constructor phase", -1, -1, -1, NULL);
    manager->active = true;
    manager->shared->active = true;
    manager->phase = MC_SOUND_MANAGER_CONSTRUCTING;
    static const int32_t new_pcs[MC_SOUND_MANAGER_POOL_COUNT] = {5, 16, 27};
    static const int32_t call_pcs[MC_SOUND_MANAGER_POOL_COUNT] = {9, 20, 31};
    static const int32_t store_pcs[MC_SOUND_MANAGER_POOL_COUNT] = {12, 23, 34};
    mc_sound_error cause = {0};
    mc_sound_status status = MC_SOUND_OK;
    for (size_t i = 0; i < MC_SOUND_MANAGER_POOL_COUNT; ++i) {
        manager->pending_pool_index = (int32_t)i;
        status = mc_sound_pool_allocate_raw(pool_allocation_bytes, &manager->pending_pool, &cause);
        if (status != MC_SOUND_OK) {
            construction_error(&manager->constructor_failure, status,
                               "SoundManager pool native allocation", new_pcs[i], -1, (int32_t)i,
                               &cause);
            goto failed;
        }
        status = mc_sound_pool_construct_raw(manager->pending_pool, profile,
                                             &manager->pending_prefix, &cause);
        if (status != MC_SOUND_OK) {
            construction_error(&manager->constructor_failure, status,
                               "SoundManager pool constructor", call_pcs[i], cause.source_pc,
                               (int32_t)i, &cause);
            goto failed;
        }
        manager->pools[i] = manager->pending_pool;
        manager->pool_prefixes[i] = manager->pending_prefix;
        manager->published_pool_mask |= 1u << i;
        manager->last_store_pc = store_pcs[i];
        manager->pending_pool = NULL;
        manager->pending_pool_index = -1;
        manager->pending_prefix = (mc_sound_pool_constructor_prefix){.last_store_pc = -1};
    }
    manager->counter = 0;
    manager->counter_published = true;
    manager->last_store_pc = 39;
    const mc_java_random_profile_status random_status =
        mc_java_random_construct_default(profile, &manager->random);
    if (random_status != MC_JAVA_RANDOM_PROFILE_OK) {
        status = result(&cause, MC_SOUND_RANDOM_FAILED, "SoundManager default Random", 47);
        cause.random_status = random_status;
        construction_error(&manager->constructor_failure, status,
                           "SoundManager fourth Random constructor", 47, -1, -1, &cause);
        goto failed;
    }
    manager->random_published = true;
    manager->last_store_pc = 50;
    if (!mc_java_random_next_bounded(&manager->random, 12000, &manager->delay)) {
        status = result(&cause, MC_SOUND_RANDOM_FAILED, "SoundManager initial music delay", 61);
        construction_error(&manager->constructor_failure, status,
                           "SoundManager initial music delay", 61, -1, -1, &cause);
        goto failed;
    }
    manager->delay_published = true;
    manager->last_store_pc = 64;
    manager->phase = MC_SOUND_MANAGER_READY;
    manager->active = false;
    manager->shared->active = false;
    if (error != NULL)
        *error = (mc_sound_manager_constructor_error){.pool_index = -1};
    return MC_SOUND_OK;
failed:
    manager->phase = MC_SOUND_MANAGER_FAILED;
    manager->active = false;
    manager->shared->active = false;
    if (error != NULL)
        *error = manager->constructor_failure;
    return status;
}

mc_sound_status mc_sound_manager_create(mc_sound_manager_shared *shared,
                                        mc_java_random_profile *profile,
                                        size_t pool_allocation_bytes, mc_sound_manager **out,
                                        mc_sound_error *error) {
    /* Preserve the original factory admission before allocating anything. */
    if (shared == NULL || profile == NULL || out == NULL)
        return result(error, MC_SOUND_INVALID_ARGUMENT, "SoundManager construction", -1);
    mc_sound_manager *manager = NULL;
    mc_sound_status status = mc_sound_manager_allocate(shared, &manager, error);
    if (status != MC_SOUND_OK)
        return status;
    mc_sound_manager_constructor_error construction;
    status = mc_sound_manager_construct(manager, profile, pool_allocation_bytes, &construction);
    if (status != MC_SOUND_OK) {
        if (error != NULL)
            *error = construction.cause;
        /* Canonical constructors cannot publish an active registration. The
         * candidate is freed before completed pools, matching the old eager
         * hr-factory failure cleanup followed by the old yo cleanup loop. */
        (void)mc_sound_manager_retire(&manager, NULL);
        return status;
    }
    *out = manager;
    return result(error, MC_SOUND_OK, NULL, 67);
}

static mc_sound_status pool_retirement_admit(mc_sound_pool *pool, mc_sound_error *error) {
    if (pool == NULL)
        return MC_SOUND_OK;
    mc_sound_pool_state state;
    const mc_sound_status status = mc_sound_pool_read_state(pool, &state, error);
    if (status != MC_SOUND_OK)
        return status;
    return state.registration_active
               ? result(error, MC_SOUND_BUSY, "SoundManager active pool retirement", -1)
               : MC_SOUND_OK;
}

mc_sound_status mc_sound_manager_retire(mc_sound_manager **owner, mc_sound_error *error) {
    if (owner == NULL)
        return result(error, MC_SOUND_INVALID_ARGUMENT, "SoundManager owner pointer", -1);
    mc_sound_manager *manager = *owner;
    if (manager == NULL)
        return result(error, MC_SOUND_OK, NULL, -1);
    if (manager->active || manager->shared->active)
        return result(error, MC_SOUND_BUSY, "SoundManager active retirement", -1);
    mc_sound_status status = pool_retirement_admit(manager->pending_pool, error);
    if (status != MC_SOUND_OK)
        return status;
    for (size_t i = 0; i < MC_SOUND_MANAGER_POOL_COUNT; ++i) {
        status = pool_retirement_admit(manager->pools[i], error);
        if (status != MC_SOUND_OK)
            return status;
    }
    manager->phase = MC_SOUND_MANAGER_RETIRING;
    manager->active = true;
    manager->shared->active = true;
    status = mc_sound_pool_free(manager->pending_pool, error);
    if (status != MC_SOUND_OK)
        goto retained;
    manager->pending_pool = NULL;
    for (size_t i = 0; i < MC_SOUND_MANAGER_POOL_COUNT; ++i) {
        status = mc_sound_pool_free(manager->pools[i], error);
        if (status != MC_SOUND_OK)
            goto retained;
        manager->pools[i] = NULL;
    }
    --manager->shared->managers;
    manager->shared->active = false;
    free(manager);
    *owner = NULL;
    return result(error, MC_SOUND_OK, NULL, -1);
retained:
    manager->active = false;
    manager->shared->active = false;
    return status;
}

static void state_values(const mc_sound_manager *manager, mc_sound_manager_state *out) {
    *out = (mc_sound_manager_state){.shared_initialized = manager->shared->initialized,
                                    .backend_present = manager->shared->backend != NULL,
                                    .active = manager->active,
                                    .sound_counter = manager->counter,
                                    .music_delay = manager->delay,
                                    .random = manager->random,
                                    .settings = manager->settings};
}

bool mc_sound_manager_read_construction(const mc_sound_manager *manager,
                                        mc_sound_manager_constructor_state *out) {
    if (manager == NULL || out == NULL)
        return false;
    *out = (mc_sound_manager_constructor_state){.phase = manager->phase,
                                                .active = manager->active,
                                                .last_store_pc = manager->last_store_pc,
                                                .published_pool_mask = manager->published_pool_mask,
                                                .pending_pool = manager->pending_pool,
                                                .pending_pool_index = manager->pending_pool_index,
                                                .pending_prefix = manager->pending_prefix,
                                                .counter_published = manager->counter_published,
                                                .random_published = manager->random_published,
                                                .delay_published = manager->delay_published,
                                                .failure = manager->constructor_failure};
    state_values(manager, &out->values);
    for (size_t i = 0; i < MC_SOUND_MANAGER_POOL_COUNT; ++i) {
        out->pools[i] = manager->pools[i];
        out->pool_prefixes[i] = manager->pool_prefixes[i];
    }
    return true;
}

bool mc_sound_manager_read_state(const mc_sound_manager *manager, mc_sound_manager_state *out) {
    if (manager == NULL || out == NULL || manager->phase != MC_SOUND_MANAGER_READY)
        return false;
    state_values(manager, out);
    return true;
}

const mc_sound_pool *mc_sound_manager_pool(const mc_sound_manager *manager,
                                           mc_sound_manager_pool_kind kind) {
    if (manager == NULL || manager->phase == MC_SOUND_MANAGER_RETIRING ||
        kind < MC_SOUND_MANAGER_EFFECTS || kind >= MC_SOUND_MANAGER_POOL_COUNT)
        return NULL;
    return manager->pools[(size_t)kind];
}

mc_sound_status mc_sound_manager_add_file(mc_sound_manager *manager,
                                          mc_sound_manager_pool_kind kind,
                                          mc_sound_text logical_name, const char *native_utf8_path,
                                          const mc_sound_pool_entry **out,
                                          mc_sound_pool_report *report, mc_sound_error *error) {
    if (kind < MC_SOUND_MANAGER_EFFECTS || kind >= MC_SOUND_MANAGER_POOL_COUNT)
        return result(error, MC_SOUND_INVALID_ARGUMENT, "SoundManager pool kind", -1);
    mc_sound_status status = admit(manager, error);
    if (status != MC_SOUND_OK)
        return status;
    status = mc_sound_pool_add_file(manager->pools[(size_t)kind], logical_name, native_utf8_path,
                                    out, report, error);
    manager->active = false;
    manager->shared->active = false;
    return status;
}

mc_sound_status mc_sound_manager_bind(mc_sound_manager *manager, mc_game_settings *settings,
                                      const mc_game_settings_diagnostics *diagnostics,
                                      mc_sound_manager_report *out, mc_sound_error *error) {
    mc_sound_status status = admit(manager, error);
    if (status != MC_SOUND_OK)
        return status;
    mc_sound_manager_report report = {0};
    status =
        mc_sound_pool_set_strip_digits(manager->pools[MC_SOUND_MANAGER_STREAMING], false, error);
    if (status != MC_SOUND_OK)
        goto done;
    manager->settings = settings;
    mc_game_settings_state state;
    if (!manager->shared->initialized &&
        (!mc_game_settings_read_state(settings, &state) || state.values.sound_volume != 0.0f ||
         state.values.music_volume != 0.0f)) {
        report.source_pc = 42;
        status = setup(manager, diagnostics, &report, error);
    }
    if (status == MC_SOUND_OK)
        report.source_pc = 45;
done:
    return finish(manager, &report, out, error, status);
}

mc_sound_status mc_sound_manager_detach_settings(mc_sound_manager *manager,
                                                 const mc_game_settings *expected,
                                                 mc_sound_error *error) {
    if (manager == NULL)
        return result(error, MC_SOUND_INVALID_ARGUMENT, "SoundManager native Settings borrow", -1);
    if (manager->active || manager->shared->active)
        return result(error, MC_SOUND_BUSY, "SoundManager active Settings borrow", -1);
    if (manager->phase != MC_SOUND_MANAGER_READY || manager->settings != expected)
        return result(error, MC_SOUND_INVALID_ARGUMENT,
                      "SoundManager native Settings borrow identity", -1);
    manager->settings = NULL;
    return result(error, MC_SOUND_OK, NULL, -1);
}

mc_sound_status mc_sound_manager_options_changed(mc_sound_manager *manager,
                                                 const mc_game_settings_diagnostics *diagnostics,
                                                 mc_sound_manager_report *out,
                                                 mc_sound_error *error) {
    mc_sound_status status = admit(manager, error);
    if (status != MC_SOUND_OK)
        return status;
    mc_sound_manager_report report = {0};
    mc_game_settings_state state;
    if (!manager->shared->initialized) {
        if (!mc_game_settings_read_state(manager->settings, &state)) {
            status = source_failure(&report, error, MC_SOUND_MANAGER_EXCEPTION_NULL_SETTINGS, 10);
            goto done;
        }
        if (state.values.sound_volume != 0.0f || state.values.music_volume != 0.0f) {
            report.source_pc = 31;
            status = setup(manager, diagnostics, &report, error);
            if (status != MC_SOUND_OK)
                goto done;
        }
    }
    if (manager->shared->initialized) {
        if (!mc_game_settings_read_state(manager->settings, &state)) {
            status = source_failure(&report, error, MC_SOUND_MANAGER_EXCEPTION_NULL_SETTINGS, 44);
            goto done;
        }
        const bool stop = state.values.music_volume == 0.0f;
        status = command(manager,
                         (mc_sound_manager_command){.kind = stop ? MC_SOUND_MANAGER_STOP
                                                                 : MC_SOUND_MANAGER_SET_VOLUME,
                                                    .source_name = "BgMusic",
                                                    .value = state.values.music_volume},
                         &report, error, stop ? 57 : 75);
        if (status != MC_SOUND_OK)
            goto done;
    }
    report.source_pc = 78;
done:
    return finish(manager, &report, out, error, status);
}

mc_sound_status mc_sound_manager_cleanup(mc_sound_manager *manager, mc_sound_manager_report *out,
                                         mc_sound_error *error) {
    mc_sound_status status = admit(manager, error);
    if (status != MC_SOUND_OK)
        return status;
    mc_sound_manager_report report = {0};
    if (manager->shared->initialized)
        status = command(manager, (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_CLEANUP},
                         &report, error, 9);
    if (status == MC_SOUND_OK)
        report.source_pc = 12;
    return finish(manager, &report, out, error, status);
}

/* The initialized gate precedes settings dereference in each source body. */
static mc_sound_status volume_gate(mc_sound_manager *manager, bool music, bool *run,
                                   mc_sound_manager_report *report, mc_sound_error *error) {
    *run = false;
    if (!manager->shared->initialized)
        return MC_SOUND_OK;
    mc_game_settings_state state;
    if (!mc_game_settings_read_state(manager->settings, &state))
        return source_failure(report, error, MC_SOUND_MANAGER_EXCEPTION_NULL_SETTINGS, 10);
    *run = (music ? state.values.music_volume : state.values.sound_volume) != 0.0f;
    return MC_SOUND_OK;
}

mc_sound_status mc_sound_manager_music_tick(mc_sound_manager *manager, mc_sound_manager_report *out,
                                            mc_sound_error *error) {
    mc_sound_status status = admit(manager, error);
    if (status != MC_SOUND_OK)
        return status;
    mc_sound_manager_report report = {0};
    bool run, active;
    status = volume_gate(manager, true, &run, &report, error);
    if (status != MC_SOUND_OK)
        goto done;
    if (!run) {
        report.source_pc = 18;
        goto done;
    }
    status = playing(manager, "BgMusic", &active, &report, error, 24);
    if (status != MC_SOUND_OK || active)
        goto done;
    status = playing(manager, "streaming", &active, &report, error, 35);
    if (status != MC_SOUND_OK || active)
        goto done;
    if (manager->delay > 0) {
        --manager->delay;
        report.source_pc = 58;
        goto done;
    }
    const mc_sound_pool_entry *entry;
    status = mc_sound_pool_select_any(manager->pools[MC_SOUND_MANAGER_MUSIC], &entry, NULL, error);
    report.source_pc = 63;
    if (status != MC_SOUND_OK || entry == NULL)
        goto done;
    int32_t delay;
    if (!mc_java_random_next_bounded(&manager->random, 12000, &delay)) {
        status = result(error, MC_SOUND_RANDOM_FAILED, "SoundManager music delay", 79);
        goto done;
    }
    manager->delay = delay + 12000;
    status = command(manager,
                     (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_BACKGROUND_MUSIC,
                                                .source_name = "BgMusic",
                                                .entry = entry},
                     &report, error, 103);
    if (status != MC_SOUND_OK)
        goto done;
    mc_game_settings_state state;
    mc_game_settings_read_state(manager->settings, &state);
    status = command(manager,
                     (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_SET_VOLUME,
                                                .source_name = "BgMusic",
                                                .value = state.values.music_volume},
                     &report, error, 118);
    if (status == MC_SOUND_OK)
        status = command(
            manager,
            (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "BgMusic"},
            &report, error, 126);
done:
    if (status == MC_SOUND_OK && report.source_pc != 18 && report.source_pc != 58)
        report.source_pc = 129;
    return finish(manager, &report, out, error, status);
}

mc_sound_status mc_sound_manager_listener_update(mc_sound_manager *manager,
                                                 const mc_sound_manager_listener *listener,
                                                 float partial_tick, const mc_math_trig *trig,
                                                 mc_sound_manager_report *out,
                                                 mc_sound_error *error) {
    mc_sound_status status = admit(manager, error);
    if (status != MC_SOUND_OK)
        return status;
    mc_sound_manager_report report = {0};
    bool run;
    status = volume_gate(manager, false, &run, &report, error);
    if (status != MC_SOUND_OK)
        goto done;
    if (!run) {
        report.source_pc = 18;
        goto done;
    }
    if (listener == NULL) {
        report.source_pc = 23;
        goto done;
    }
    if (trig == NULL) {
        status = result(error, MC_SOUND_INVALID_ARGUMENT, "SoundManager listener trig table", -1);
        goto done;
    }
    const float yaw =
        listener->previous_yaw + (listener->yaw - listener->previous_yaw) * partial_tick;
    const double partial = (double)partial_tick;
    const double x = listener->previous_x + (listener->x - listener->previous_x) * partial;
    const double y = listener->previous_y + (listener->y - listener->previous_y) * partial;
    const double z = listener->previous_z + (listener->z - listener->previous_z) * partial;
    const float angle = -yaw * 0.017453292f - 3.1415927f;
    const float cosine = mc_math_cos(trig, angle);
    const float sine = mc_math_sin(trig, angle);
    status = command(manager,
                     (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_LISTENER_POSITION,
                                                .x = (float)x,
                                                .y = (float)y,
                                                .z = (float)z},
                     &report, error, 158);
    if (status == MC_SOUND_OK)
        status = command(
            manager,
            (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_LISTENER_ORIENTATION,
                                       .orientation = {-sine, 0.0f, -cosine, 0.0f, 1.0f, 0.0f}},
            &report, error, 176);
    if (status == MC_SOUND_OK)
        report.source_pc = 179;
done:
    return finish(manager, &report, out, error, status);
}

mc_sound_status mc_sound_manager_stream(mc_sound_manager *manager, mc_sound_text name, float x,
                                        float y, float z, float volume, float pitch,
                                        mc_sound_manager_report *out, mc_sound_error *error) {
    (void)pitch;
    mc_sound_status status = admit(manager, error);
    if (status != MC_SOUND_OK)
        return status;
    mc_sound_manager_report report = {0};
    bool run, active;
    status = volume_gate(manager, false, &run, &report, error);
    if (status != MC_SOUND_OK)
        goto done;
    if (!run) {
        report.source_pc = 18;
        goto done;
    }
    status = playing(manager, "streaming", &active, &report, error, 28);
    if (status != MC_SOUND_OK)
        goto done;
    if (active) {
        status = command(
            manager,
            (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_STOP, .source_name = "streaming"},
            &report, error, 39);
        if (status != MC_SOUND_OK)
            goto done;
    }
    if (name.is_null) {
        report.source_pc = 46;
        goto done;
    }
    const mc_sound_pool_entry *entry;
    status =
        mc_sound_pool_select(manager->pools[MC_SOUND_MANAGER_STREAMING], name, &entry, NULL, error);
    report.source_pc = 52;
    if (status != MC_SOUND_OK || entry == NULL || !(volume > 0.0f))
        goto done;
    status = playing(manager, "BgMusic", &active, &report, error, 74);
    if (status != MC_SOUND_OK)
        goto done;
    if (active) {
        status = command(
            manager,
            (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_STOP, .source_name = "BgMusic"},
            &report, error, 85);
        if (status != MC_SOUND_OK)
            goto done;
    }
    status = command(manager,
                     (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_STREAMING_SOURCE,
                                                .source_name = "streaming",
                                                .entry = entry,
                                                .priority = true,
                                                .x = x,
                                                .y = y,
                                                .z = z,
                                                .attenuation = 2,
                                                .distance = 64.0f},
                     &report, error, 119);
    if (status != MC_SOUND_OK)
        goto done;
    mc_game_settings_state state;
    mc_game_settings_read_state(manager->settings, &state);
    status = command(manager,
                     (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_SET_VOLUME,
                                                .source_name = "streaming",
                                                .value = 0.5f * state.values.sound_volume},
                     &report, error, 137);
    if (status == MC_SOUND_OK)
        status = command(
            manager,
            (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = "streaming"},
            &report, error, 145);
done:
    if (status == MC_SOUND_OK && report.source_pc != 18 && report.source_pc != 46)
        report.source_pc = 148;
    return finish(manager, &report, out, error, status);
}

static mc_sound_status one_shot(mc_sound_manager *manager, mc_sound_text name, bool positional,
                                float x, float y, float z, float volume, float pitch,
                                mc_sound_manager_report *out, mc_sound_error *error) {
    mc_sound_status status = admit(manager, error);
    if (status != MC_SOUND_OK)
        return status;
    mc_sound_manager_report report = {0};
    bool run;
    status = volume_gate(manager, false, &run, &report, error);
    if (status != MC_SOUND_OK)
        goto done;
    if (!run) {
        report.source_pc = 18;
        goto done;
    }
    const mc_sound_pool_entry *entry;
    status =
        mc_sound_pool_select(manager->pools[MC_SOUND_MANAGER_EFFECTS], name, &entry, NULL, error);
    report.source_pc = 24;
    if (status != MC_SOUND_OK || entry == NULL || (positional && !(volume > 0.0f)))
        goto done;
    manager->counter = (manager->counter + 1) % 256;
    char source_name[16];
    const int length =
        snprintf(source_name, sizeof(source_name), "sound_%d", (int)manager->counter);
    if (length < 0 || (size_t)length >= sizeof(source_name)) {
        status = result(error, MC_SOUND_LIMIT, "SoundManager source name", positional ? 74 : 67);
        goto done;
    }
    float distance = positional ? 16.0f : 0.0f;
    if (positional && volume > 1.0f)
        distance *= volume;
    status = command(manager,
                     (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_NEW_SOURCE,
                                                .source_name = source_name,
                                                .entry = entry,
                                                .priority = positional && volume > 1.0f,
                                                .x = x,
                                                .y = y,
                                                .z = z,
                                                .attenuation = positional ? 2 : 0,
                                                .distance = distance},
                     &report, error, positional ? 132 : 94);
    if (status != MC_SOUND_OK)
        goto done;
    if (!positional) {
        if (volume > 1.0f)
            volume = 1.0f;
        volume *= 0.25f;
    }
    status =
        command(manager,
                (mc_sound_manager_command){
                    .kind = MC_SOUND_MANAGER_SET_PITCH, .source_name = source_name, .value = pitch},
                &report, error, positional ? 142 : 116);
    if (status != MC_SOUND_OK)
        goto done;
    if (positional && volume > 1.0f)
        volume = 1.0f;
    mc_game_settings_state state;
    mc_game_settings_read_state(manager->settings, &state);
    status = command(manager,
                     (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_SET_VOLUME,
                                                .source_name = source_name,
                                                .value = volume * state.values.sound_volume},
                     &report, error, positional ? 170 : 133);
    if (status == MC_SOUND_OK)
        status = command(
            manager,
            (mc_sound_manager_command){.kind = MC_SOUND_MANAGER_PLAY, .source_name = source_name},
            &report, error, positional ? 178 : 141);
done:
    if (status == MC_SOUND_OK && report.source_pc != 18)
        report.source_pc = positional ? 181 : 144;
    return finish(manager, &report, out, error, status);
}

mc_sound_status mc_sound_manager_effect(mc_sound_manager *manager, mc_sound_text name, float x,
                                        float y, float z, float volume, float pitch,
                                        mc_sound_manager_report *report, mc_sound_error *error) {
    return one_shot(manager, name, true, x, y, z, volume, pitch, report, error);
}

mc_sound_status mc_sound_manager_ui(mc_sound_manager *manager, mc_sound_text name, float volume,
                                    float pitch, mc_sound_manager_report *report,
                                    mc_sound_error *error) {
    return one_shot(manager, name, false, 0.0f, 0.0f, 0.0f, volume, pitch, report, error);
}

mc_sound_status mc_sound_manager_add_java_file(mc_sound_manager *manager,
                                               mc_sound_manager_pool_kind kind, mc_sound_text name,
                                               mc_java_file *file, mc_sound_pool_file_call **call,
                                               const mc_sound_pool_entry **entry,
                                               mc_sound_pool_file_error *error) {
    if (manager == NULL || manager->phase != MC_SOUND_MANAGER_READY ||
        (unsigned int)kind >= (unsigned int)MC_SOUND_MANAGER_POOL_COUNT) {
        mc_sound_pool_file_error e = {.status = MC_SOUND_INVALID_ARGUMENT};
        if (error != NULL)
            *error = e;
        return e.status;
    }
    /* Actual immutable source b/c/d read1. Worker does not touch manager/shared
     * backend active flags. App retirement joins cz before pool destruction. */
    mc_sound_pool *captured = manager->pools[(size_t)kind];
    mc_sound_pool_file_error e = {0};
    mc_sound_status s = mc_sound_pool_add_java_file(captured, name, file, call, entry, &e);
    e.yo_pc = 6;
    if (error != NULL)
        *error = e;
    return s;
}
