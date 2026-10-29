#include "mc/core/java/url.h"
#include "mc/storage/nbt/input.h"
#include <stdio.h>
static unsigned int checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "cz canonical DIS %u line%d: %s\n", checks, __LINE__, #x);             \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

int main(void) {
    int result = 1;
    mc_java_url_data_input *input = NULL, *failed = NULL;
    mc_java_url_error e = {0};
    mc_nbt_stream_error ne = {0};
    mc_nbt_stream_state native = {0};
    mc_java_url_data_input_state view = {0};
    CHECK(mc_java_url_data_input_allocate(65536u, &input, &e) == MC_JAVA_URL_OK);
    mc_nbt_stream *same = input;
    CHECK(mc_nbt_stream_state_read(same, &native, &ne) == MC_NBT_OK &&
          native.phase == MC_NBT_STREAM_RAW);
    size_t raw_bytes = native.owner_bytes;
    CHECK(mc_java_url_data_input_construct(input, NULL, &e) == MC_JAVA_URL_OK);
    CHECK(mc_java_url_data_input_inspect(input, &view) && view.last_store == 26 &&
          view.in == NULL && view.bytearr != NULL && view.chararr != NULL &&
          view.read_buffer != NULL);
    CHECK(mc_nbt_stream_state_read(same, &native, &ne) == MC_NBT_OK &&
          native.bytearr == view.bytearr && native.chararr == view.chararr &&
          native.read_buffer == view.read_buffer && native.candidate == NULL &&
          native.last_tag == NULL && native.pending_tag == NULL && native.last_collection == NULL &&
          native.last_array == NULL);
    for (size_t i = 0; i < 80u; ++i)
        CHECK(view.bytearr[i] == 0 && view.chararr[i] == 0);
    for (size_t i = 0; i < 8u; ++i)
        CHECK(view.read_buffer[i] == 0);
    int32_t count = 73;
    CHECK(mc_java_url_data_input_read(input, NULL, 0, true, &count, &e) ==
              MC_JAVA_URL_SOURCE_NULL &&
          e.source_pc == 7 && count == 73);
    CHECK(mc_java_url_data_input_read(input, NULL, 0, false, &count, &e) ==
              MC_JAVA_URL_SOURCE_NULL &&
          e.source_pc == 8 && count == 73);
    CHECK(mc_nbt_stream_state_read(same, &native, &ne) == MC_NBT_OK &&
          native.phase == MC_NBT_STREAM_READY && native.read_calls == 0);
    CHECK(mc_java_url_data_input_release(&input, &e) == MC_JAVA_URL_OK && input == NULL);
    CHECK(mc_java_url_data_input_allocate(raw_bytes + 80u, &failed, &e) == MC_JAVA_URL_OK);
    CHECK(mc_java_url_data_input_construct(failed, NULL, &e) == MC_JAVA_URL_LIMIT &&
          e.source_pc == 16);
    CHECK(mc_java_url_data_input_inspect(failed, &view) && view.last_store == 10 &&
          view.bytearr != NULL && view.chararr == NULL && view.read_buffer == NULL);
    CHECK(mc_java_url_data_input_release(&failed, &e) == MC_JAVA_URL_OK && failed == NULL);
    result = 0;
cleanup:
    mc_java_url_data_input_release(&failed, &e);
    mc_java_url_data_input_release(&input, &e);
    if (result == 0)
        printf("canonical DIS: %u checks\n", checks);
    return result;
}
