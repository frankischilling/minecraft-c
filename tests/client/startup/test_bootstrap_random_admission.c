#include "../../../src/client/runtime/bootstrap_random_internal.h"

#include <emmintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static const mc_client_application_options options = {
    .assets = ".", .username = "Bootstrap admission", .width = 854, .height = 480};

static bool close_partial(mc_client_bootstrap **context) {
    const mc_client_bootstrap_status status = mc_client_bootstrap_close(context);
    if (status != MC_CLIENT_BOOTSTRAP_OK || *context != NULL) {
        fprintf(stderr, "Bootstrap admission cleanup refused: %d\n", (int)status);
        return false;
    }
    return true;
}

static int invalid_config(void) {
    int result = 1;
    mc_client_bootstrap *context = NULL;
    mc_client_bootstrap_random_terminal terminal = {0};
    unsigned char before[sizeof(terminal)];
    memcpy(before, &terminal, sizeof(before));
    mc_client_bootstrap_random_config config = {.mode = (mc_client_bootstrap_random_mode)99,
                                                .terminal = &terminal};

    CHECK(mc_client_bootstrap_open_standalone_with_random_inputs(&options, &config, &context) ==
          MC_CLIENT_BOOTSTRAP_INVALID_ARGUMENT);
    CHECK(context == NULL && memcmp(before, &terminal, sizeof(before)) == 0);
    config.mode = MC_CLIENT_BOOTSTRAP_RANDOM_RECORD;
    config.terminal = NULL;
    CHECK(mc_client_bootstrap_open_with_random_inputs(&options, &config, &context) ==
          MC_CLIENT_BOOTSTRAP_INVALID_ARGUMENT);
    CHECK(context == NULL && memcmp(before, &terminal, sizeof(before)) == 0);
    result = 0;
cleanup:
    return close_partial(&context) ? result : 1;
}

static int rejected_capacity(void) {
    int result = 1;
    mc_client_bootstrap *context = NULL;
    mc_client_bootstrap_random_terminal terminal = {0};
    const size_t capacity = (size_t)MC_JAVA_RANDOM_PROFILE_MAX_CONSTRUCTOR_INPUTS + 1u;
    mc_java_random_constructor_record *records = calloc(capacity, sizeof(*records));
    CHECK(records != NULL);
    records[0].committed_uniquifier = UINT64_C(0x123456789abcdef0);
    records[capacity - 1u].nanotime = -INT64_C(731);
    const mc_client_bootstrap_random_config config = {
        .mode = MC_CLIENT_BOOTSTRAP_RANDOM_RECORD,
        .data.record = {.records = records, .capacity = capacity},
        .terminal = &terminal};

    /* The genuine resource owners retain only "." and "./jar" paths here.
     * The actual oversized binding fails before startup services open assets
     * or construct Frame/Canvas/App/SDK owners. */
    CHECK(mc_client_bootstrap_open_standalone_with_random_inputs(&options, &config, &context) ==
          MC_CLIENT_BOOTSTRAP_OWNER_FAILED);
    CHECK(context != NULL && mc_client_bootstrap_frame(context) == NULL && !terminal.present);
    CHECK(records[0].committed_uniquifier == UINT64_C(0x123456789abcdef0) &&
          records[capacity - 1u].nanotime == -INT64_C(731));
    CHECK(close_partial(&context));
    CHECK(terminal.present && terminal.profile_retired && terminal.binding_attempted &&
          terminal.mode == MC_CLIENT_BOOTSTRAP_RANDOM_RECORD &&
          terminal.binding_status == MC_JAVA_RANDOM_PROFILE_LIMIT &&
          terminal.completion_status == MC_JAVA_RANDOM_PROFILE_LIMIT);
    CHECK(terminal.replay_view_status == MC_JAVA_RANDOM_PROFILE_OK && !terminal.replay.bound &&
          terminal.replay.count == 0 && terminal.replay.consumed == 0 &&
          terminal.replay.failure == MC_JAVA_RANDOM_INPUT_NONE);
    CHECK(terminal.record_view_status == MC_JAVA_RANDOM_PROFILE_OK && !terminal.recording.bound &&
          !terminal.recording.active && terminal.recording.capacity == 0 &&
          terminal.recording.recorded == 0 && terminal.recording.failures == 0 &&
          terminal.recording.first_failure == 0);
    unsigned char retired[sizeof(terminal)];
    memcpy(retired, &terminal, sizeof(retired));
    CHECK(mc_client_bootstrap_close(&context) == MC_CLIENT_BOOTSTRAP_OK && context == NULL);
    CHECK(memcmp(retired, &terminal, sizeof(retired)) == 0);
    result = 0;
cleanup:
    /* A refused actual close retains the buffer until process exit. It does
     * not authorize freeing a borrow still owned by the partial bootstrap. */
    if (!close_partial(&context))
        return 1;
    free(records);
    return result;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u) {
        fputs("bootstrap admission requires the qualified strict SSE environment\n", stderr);
        return 1;
    }
    if (invalid_config() || rejected_capacity())
        return 1;
    printf("Bootstrap Random admission: %u checks passed; source-windows=0\n", checks);
    return 0;
}
