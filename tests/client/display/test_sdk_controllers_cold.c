/* Exact native environment TU once. The test allocator always forwards to
 * real calloc; its one failure control frees that real allocation then returns
 * NULL. This is native allocation policy, not a Source controller/provider. */
#include "client/display/sdk_controllers_native.h"
#include "client/display/sdk_input.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdio.h>
#include <stdlib.h>

static bool allocation_failure;
static size_t allocator_calls;

static void *real_control_calloc(size_t count, size_t size) {
    ++allocator_calls;
    void *memory = calloc(count, size);
    if (allocation_failure) {
        free(memory);
        return NULL;
    }
    return memory;
}

#define calloc real_control_calloc
#include "../../../src/client/display/sdk_controllers_native.c"
#undef calloc

static unsigned checks;
#define CHECK(value)                                                                               \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(value)) {                                                                            \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #value);                            \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int result = 1;
    mc_client_sdk_controller_environment *environment = NULL;
    mc_client_sdk_input_error error = {0};
    mc_client_sdk_controllers_state untouched = {.controller_count = 123};
    const SDL_InitFlags before = SDL_WasInit(0);
    CHECK(mc_client_sdk_controllers_create(NULL, &error) == MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT);
    CHECK(error.native_policy && error.site.pc == -1);
    CHECK(mc_client_sdk_controllers_poll(NULL, &error) == MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT);
    CHECK(!mc_client_sdk_controllers_inspect(NULL, &untouched) &&
          untouched.controller_count == 123);
    CHECK(mc_client_sdk_controller_environment_allocate(NULL, &error) ==
          MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT);
    allocation_failure = true;
    CHECK(mc_client_sdk_controller_environment_allocate(&environment, &error) ==
          MC_CLIENT_SDK_INPUT_ALLOCATION_FAILED);
    CHECK(environment == NULL && allocator_calls == 1 && error.site.pc == 7);
    allocation_failure = false;
    CHECK(mc_client_sdk_controller_environment_allocate(&environment, &error) ==
          MC_CLIENT_SDK_INPUT_OK);
    CHECK(environment != NULL && environment->phase == MC_CLIENT_SDK_CONTROLLER_ENV_ALLOCATED &&
          !environment->reference_owned && !environment->initialization_attempted &&
          !environment->enumeration_returned && environment->ids == NULL &&
          environment->init_calls == 0 && environment->enumeration_calls == 0);
    mc_client_sdk_controller_environment *same = environment;
    const size_t calls_before = allocator_calls;
    CHECK(mc_client_sdk_controller_environment_allocate(&environment, &error) ==
          MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT);
    CHECK(environment == same && allocator_calls == calls_before);
    CHECK(mc_client_sdk_controller_environment_retire(&environment, NULL, &error) ==
              MC_CLIENT_SDK_INPUT_OK &&
          environment == NULL);
    CHECK(mc_client_sdk_controller_environment_retire(&environment, NULL, &error) ==
          MC_CLIENT_SDK_INPUT_OK);
    CHECK(mc_client_sdk_controller_environment_retire(NULL, NULL, &error) ==
          MC_CLIENT_SDK_INPUT_INVALID_ARGUMENT);
    CHECK(error.site.class_name == NULL && error.site.pc == -1);
    CHECK(SDL_WasInit(0) == before);
    result = 0;
cleanup:
    allocation_failure = false;
    if (mc_client_sdk_controller_environment_retire(&environment, NULL, &error) !=
        MC_CLIENT_SDK_INPUT_OK)
        result = 1;
    if (result == 0 && printf("SDK Controllers cold ownership: %u checks\n", checks) < 0)
        result = 1;
    return result;
}
