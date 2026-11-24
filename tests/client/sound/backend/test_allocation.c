#include "internal.h"

#include <stdio.h>
#include <stdlib.h>

static mc_sound_backend *fault_owner;
static bool armed;
static unsigned failures, checks;

/* Only backend_alloc is substituted. Observe a real published decoded entry,
 * then fail its first device-name ledger allocation. No owner state is forged. */
void *mc_test_sound_calloc(size_t count, size_t bytes) {
    if (armed && fault_owner != NULL && fault_owner->cache_size == 1 &&
        fault_owner->device_buffer_capacity == 0) {
        armed = false;
        ++failures;
        return NULL;
    }
    return calloc(count, bytes);
}

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expression);                       \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    bool okay = false;
    mc_java_random_profile *profile = NULL;
    mc_sound_pool *pool = NULL;
    mc_sound_backend *backend = NULL;
    mc_sound_manager_backend interface = {0};
    mc_sound_backend_error error = {0};
    void *instance = NULL;
    char path[4096];
    CHECK(argc == 2);
    const int length = snprintf(path, sizeof(path), "%s/mono32k.ogg", argv[1]);
    CHECK(length >= 0 && (size_t)length < sizeof(path));
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_sound_pool_create(profile, 1024u * 1024u, &pool, NULL) == MC_SOUND_OK);
    const uint16_t name[] = {'o', 'o', 'm', '.', 'o', 'g', 'g'};
    const mc_sound_pool_entry *entry = NULL;
    CHECK(mc_sound_pool_add_file(pool, (mc_sound_text){name, 7, false}, path, &entry, NULL, NULL) ==
          MC_SOUND_OK);
    mc_sound_backend_options options = mc_sound_backend_default_options();
    options.loopback = options.manual_service = true;
    CHECK(mc_sound_backend_create(profile, &options, &backend, &error) == MC_SOUND_BACKEND_OK);
    CHECK(mc_sound_backend_manager_interface(backend, &interface));
    for (int configuration = MC_SOUND_MANAGER_LIBRARY_OPENAL;
         configuration <= MC_SOUND_MANAGER_CODEC_WAV; ++configuration)
        CHECK(interface.configure(interface.context, (mc_sound_manager_configuration)configuration,
                                  NULL) == MC_SOUND_OK);
    CHECK(interface.construct(interface.context, &instance, NULL) == MC_SOUND_OK);
    const mc_sound_manager_command command = {
        .kind = MC_SOUND_MANAGER_NEW_SOURCE, .source_name = "allocation", .entry = entry};
    CHECK(interface.command(interface.context, instance, &command, NULL) == MC_SOUND_OK);
    fault_owner = backend;
    armed = true;
    CHECK(mc_sound_backend_service_commands(backend, &error) == MC_SOUND_BACKEND_ALLOCATION_FAILED);
    CHECK(failures == 1 && !armed);
    CHECK(error.status == MC_SOUND_BACKEND_ALLOCATION_FAILED);
    CHECK(backend->cache_size == 1 && backend->device_buffer_names == NULL &&
          backend->device_buffer_count == 0 && backend->device_buffer_capacity == 0);
    CHECK(backend->state.source_rejections == 0);
    okay = true;
cleanup:
    if (!okay)
        fprintf(stderr, "Allocation failure profile: status %d, injected %u, cache %zu\n",
                (int)error.status, failures, backend == NULL ? 0 : backend->cache_size);
    armed = false;
    fault_owner = NULL;
    if (instance != NULL) {
        const mc_sound_manager_command cleanup = {.kind = MC_SOUND_MANAGER_CLEANUP};
        if (interface.command(interface.context, instance, &cleanup, NULL) != MC_SOUND_OK ||
            interface.retire(interface.context, &instance, NULL) != MC_SOUND_OK)
            return 1;
    }
    if (mc_sound_backend_retire(&backend, NULL) != MC_SOUND_BACKEND_OK ||
        mc_sound_pool_free(pool, NULL) != MC_SOUND_OK)
        return 1;
    mc_java_random_profile_free(profile);
    if (okay)
        printf("Backend injected-allocation checks: %u\n", checks);
    return okay ? 0 : 1;
}
