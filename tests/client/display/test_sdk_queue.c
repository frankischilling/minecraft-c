#include "sdk_queue.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(void) {
    int result = 1;
    mc_client_sdk_queue *queue = NULL;
    mc_client_sdk_bytes *input = NULL, *output = NULL, *tail = NULL;
    mc_client_sdk_queue_error error = {0};
    mc_client_sdk_bytes_state state;
    uint8_t pattern[22];
    for (size_t i = 0; i < sizeof(pattern); ++i)
        pattern[i] = (uint8_t)(i + 1);
    CHECK(mc_client_sdk_queue_create(22, &queue, &error) == MC_CLIENT_SDK_QUEUE_OK);
    CHECK(mc_client_sdk_queue_inspect(queue, &state) && state.capacity == 4400 &&
          state.position == 0);
    CHECK(mc_client_sdk_bytes_create(22, &input) == MC_CLIENT_SDK_QUEUE_OK);
    for (size_t i = 0; i < 200; ++i) {
        mc_client_sdk_bytes_clear(input);
        pattern[0] = (uint8_t)i;
        CHECK(mc_client_sdk_bytes_write(input, pattern, sizeof(pattern)));
        mc_client_sdk_bytes_flip(input);
        bool accepted = false;
        CHECK(mc_client_sdk_queue_put(queue, input, &accepted, &error) == MC_CLIENT_SDK_QUEUE_OK &&
              accepted);
        CHECK(mc_client_sdk_bytes_remaining(input) == 0);
    }
    mc_client_sdk_bytes_clear(input);
    CHECK(mc_client_sdk_bytes_write(input, pattern, sizeof(pattern)));
    mc_client_sdk_bytes_flip(input);
    bool accepted = true;
    CHECK(mc_client_sdk_queue_put(queue, input, &accepted, &error) == MC_CLIENT_SDK_QUEUE_OK &&
          !accepted);
    CHECK(mc_client_sdk_bytes_inspect(input, &state) && state.position == 0 && state.limit == 22);
    CHECK(mc_client_sdk_bytes_limit(input, 21));
    accepted = true;
    CHECK(mc_client_sdk_queue_put(queue, input, &accepted, &error) ==
          MC_CLIENT_SDK_QUEUE_SOURCE_EVENT_SIZE);
    CHECK(error.pc == 11 && accepted && mc_client_sdk_bytes_remaining(input) == 21);
    CHECK(mc_client_sdk_bytes_create(5, &output) == MC_CLIENT_SDK_QUEUE_OK);
    CHECK(mc_client_sdk_queue_copy(queue, output, &error) == MC_CLIENT_SDK_QUEUE_OK);
    CHECK(mc_client_sdk_bytes_inspect(output, &state) && state.position == 5 && state.data[0] == 0);
    for (size_t i = 1; i < 5; ++i)
        CHECK(state.data[i] == (uint8_t)(i + 1));
    CHECK(mc_client_sdk_queue_inspect(queue, &state) && state.position == 4395 &&
          state.limit == 4400);
    CHECK(mc_client_sdk_bytes_create(4400, &tail) == MC_CLIENT_SDK_QUEUE_OK);
    CHECK(mc_client_sdk_queue_copy(queue, tail, &error) == MC_CLIENT_SDK_QUEUE_OK);
    CHECK(mc_client_sdk_bytes_inspect(tail, &state) && state.position == 4395);
    for (size_t absolute = 5; absolute < 4400; ++absolute) {
        const uint8_t expected =
            absolute % 22 == 0 ? (uint8_t)(absolute / 22) : (uint8_t)(absolute % 22 + 1);
        CHECK(state.data[absolute - 5] == expected);
    }
    CHECK(mc_client_sdk_queue_inspect(queue, &state) && state.position == 0 && state.limit == 4400);
    mc_client_sdk_queue_free(&queue);
    CHECK(mc_client_sdk_queue_create(-1, &queue, &error) ==
          MC_CLIENT_SDK_QUEUE_SOURCE_NEGATIVE_CAPACITY);
    CHECK(queue == NULL && error.pc == 15);
    CHECK(mc_client_sdk_queue_create(INT32_MIN, &queue, &error) == MC_CLIENT_SDK_QUEUE_OK);
    CHECK(mc_client_sdk_queue_inspect(queue, &state) && state.capacity == 0);
    mc_client_sdk_queue_free(&queue);
    CHECK(mc_client_sdk_queue_create(0, &queue, &error) == MC_CLIENT_SDK_QUEUE_OK);
    CHECK(mc_client_sdk_bytes_limit(input, 0));
    accepted = false;
    CHECK(mc_client_sdk_queue_put(queue, input, &accepted, &error) == MC_CLIENT_SDK_QUEUE_OK &&
          accepted);
    CHECK(mc_client_sdk_queue_inspect(queue, &state) && state.position == 0 && state.capacity == 0);
    result = 0;
cleanup:
    mc_client_sdk_bytes_free(&tail);
    mc_client_sdk_bytes_free(&output);
    mc_client_sdk_bytes_free(&input);
    mc_client_sdk_queue_free(&queue);
    if (result == 0)
        printf("SDK_QUEUE_OK|checks=%u\n", checks);
    return result;
}
