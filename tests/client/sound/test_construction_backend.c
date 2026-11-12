/* Actual cold production backend only. No device or worker is initialized by
 * SoundManager construction, and no body is replaced with a successful stub. */
#include "mc/client/sound/backend.h"
#include "mc/client/sound/construction.h"
#include "random_profile_internal.h"
#include <stdio.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Sound cold construction check%u line%d: %s\n", checks, __LINE__, #x); \
            result = 1;                                                                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(void) {
    int result = 0;
    mc_java_random_profile *profile = NULL;
    mc_sound_backend *backend = NULL;
    mc_sound_manager_shared *shared = NULL;
    mc_sound_manager *manager = NULL;
    mc_java_random_constructor_input tape[4];
    uint64_t u = UINT64_C(8682522807148012);
    for (size_t i = 0; i < 4; ++i) {
        u *= UINT64_C(1181783497276652981);
        tape[i] = (mc_java_random_constructor_input){u, (int64_t)i + 512};
    }
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_java_random_profile_bind_constructor_inputs(profile, tape, 4) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_sound_backend_create(profile, NULL, &backend, NULL) == MC_SOUND_BACKEND_OK);
    mc_sound_manager_backend api;
    CHECK(mc_sound_backend_manager_interface(backend, &api));
    CHECK(profile->constructor_input_consumed == 0 && !atomic_load(&profile->constructor_admitted));
    CHECK(mc_sound_manager_shared_create(&api, &shared, NULL) == MC_SOUND_OK &&
          mc_sound_manager_allocate(shared, &manager, NULL) == MC_SOUND_OK);
    CHECK(profile->constructor_input_consumed == 0);
    CHECK(mc_sound_manager_construct(manager, profile, MC_SOUND_POOL_MAX_ALLOCATION_BYTES, NULL) ==
          MC_SOUND_OK);
    CHECK(mc_java_random_profile_constructor_inputs_complete(profile) == MC_JAVA_RANDOM_PROFILE_OK);
    mc_sound_backend_state state;
    CHECK(mc_sound_backend_read_state(backend, &state) && !state.constructed &&
          !state.device_open && !state.context_current && !state.command_worker_started &&
          !state.stream_worker_started && !state.command_worker_alive &&
          !state.stream_worker_alive && state.submitted == 0 && state.applied == 0 &&
          state.sources == 0 && state.device_buffers == 0 && state.decoded_buffers == 0 &&
          state.device_buffers_created == 0 && state.queued_commands == 0 &&
          state.deferred_plays == 0);
    CHECK(profile->math == NULL &&
          atomic_load(&profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_COLD);
    CHECK(mc_sound_manager_shared_retire(&shared, NULL) == MC_SOUND_BUSY && shared != NULL);
    CHECK(mc_sound_manager_retire(&manager, NULL) == MC_SOUND_OK && manager == NULL);
    CHECK(mc_sound_manager_shared_retire(&shared, NULL) == MC_SOUND_OK && shared == NULL);
    CHECK(mc_sound_backend_read_state(backend, &state) && !state.constructed &&
          !state.device_open && !state.command_worker_started && !state.stream_worker_started);
cleanup:
    if (mc_sound_manager_retire(&manager, NULL) != MC_SOUND_OK)
        result = 1;
    if (mc_sound_manager_shared_retire(&shared, NULL) != MC_SOUND_OK)
        result = 1;
    if (mc_sound_backend_retire(&backend, NULL) != MC_SOUND_BACKEND_OK)
        result = 1;
    mc_java_random_profile_free(profile);
    if (result == 0)
        printf("Sound cold backend construction: %u checks;zero activation\n", checks);
    return result;
}
