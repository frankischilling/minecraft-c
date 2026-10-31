#include "mc/storage/nbt/input.h"
#include "url_internal.h"

/* This is the SAME canonical mc_nbt_stream DataInputStream owner, not a
 * parser/tag adapter. Only the concrete URL byte port is defined here. The
 * outer cz call retains its actual HTTP input until stream_release returns. */
static mc_nbt_input_status input_error(mc_nbt_input_error *out, const mc_java_url_error *e) {
    *out = (mc_nbt_input_error){.transport_status = (int)e->status,
                                .system_error = e->native_error,
                                .source_exception = e->source_exception,
                                .operation = e->operation};
    return MC_NBT_INPUT_FAILED;
}

static mc_nbt_input_status read_port(void *context, uint8_t *bytes, size_t length,
                                     mc_nbt_input_result *out, mc_nbt_input_error *error) {
    mc_java_url_error e = {0};
    int32_t count = 0;
    if (mc_java_http_read(context, bytes, length, &count, &e) != MC_JAVA_URL_OK)
        return input_error(error, &e);
    *out = (mc_nbt_input_result){.count = count < 0 ? 0u : (size_t)count, .eof = count < 0};
    return MC_NBT_INPUT_OK;
}

static mc_nbt_input_status one_port(void *context, int *out, mc_nbt_input_error *error) {
    uint8_t value = 0;
    mc_nbt_input_result read = {0};
    if (read_port(context, &value, 1, &read, error) != MC_NBT_INPUT_OK)
        return MC_NBT_INPUT_FAILED;
    *out = read.eof ? -1 : (int)value;
    return MC_NBT_INPUT_OK;
}

static mc_nbt_input_status close_port(void *context, mc_nbt_input_error *error) {
    mc_java_url_error e = {0};
    return mc_java_http_source_close(context, &e) == MC_JAVA_URL_OK ? MC_NBT_INPUT_OK
                                                                    : input_error(error, &e);
}

static mc_java_url_status mapped(mc_java_url_error *out, const mc_nbt_stream_error *e) {
    mc_java_url_status status =
        e->parse.status == MC_NBT_OK                  ? MC_JAVA_URL_OK
        : e->parse.status == MC_NBT_STREAM_BUSY       ? MC_JAVA_URL_BUSY
        : e->parse.status == MC_NBT_LIMIT             ? MC_JAVA_URL_LIMIT
        : e->parse.status == MC_NBT_ALLOCATION_FAILED ? MC_JAVA_URL_ALLOCATION
        : e->parse.status == MC_NBT_SOURCE_FAILURE    ? MC_JAVA_URL_SOURCE_NULL
        : e->parse.status == MC_NBT_INPUT_ERROR && e->input.transport_status > MC_JAVA_URL_OK &&
                e->input.transport_status <= MC_JAVA_URL_SOURCE_NULL
            ? (mc_java_url_status)e->input.transport_status
        : e->parse.status == MC_NBT_STREAM_STATE ? MC_JAVA_URL_STATE
                                                 : MC_JAVA_URL_NATIVE;
    mc_url_fail(out, status,
                e->input.operation == NULL ? "canonical DataInputStream native owner"
                                           : e->input.operation,
                e->input.system_error);
    out->source_class = e->source_class;
    out->source_method = e->source_method;
    out->source_pc = e->source_pc;
    return status;
}

mc_java_url_status mc_java_url_data_input_allocate(size_t maximum, mc_java_url_data_input **out,
                                                   mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    mc_nbt_stream_error child = {0};
    (void)mc_nbt_stream_allocate(maximum, out, &child);
    return mapped(e, &child);
}

mc_java_url_status mc_java_url_data_input_construct(mc_java_url_data_input *raw,
                                                    mc_java_http_input *input,
                                                    mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    mc_nbt_byte_input port = input == NULL
                                 ? (mc_nbt_byte_input){0}
                                 : (mc_nbt_byte_input){input, one_port, read_port, close_port};
    mc_nbt_stream_error child = {0};
    (void)mc_nbt_stream_construct(raw, &port, NULL, &child);
    return mapped(e, &child);
}

mc_java_url_status mc_java_url_data_input_read(mc_java_url_data_input *s, uint8_t *bytes,
                                               int32_t length, bool is_null, int32_t *out,
                                               mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    mc_nbt_stream_error child = {0};
    (void)mc_nbt_stream_read_array(s, bytes, length, is_null, out, &child);
    mc_java_url_status status = mapped(e, &child);
    if (status != MC_JAVA_URL_OK) {
        mc_nbt_stream_state state = {0};
        mc_nbt_stream_error ignored = {0};
        if (mc_nbt_stream_state_read(s, &state, &ignored) == MC_NBT_OK && state.source_in != NULL) {
            mc_java_http_state actual;
            if (mc_java_http_inspect(state.source_in, &actual))
                e->response_code = actual.response_code;
        }
    }
    return status;
}

mc_java_url_status mc_java_url_data_input_close(mc_java_url_data_input *s, mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    mc_nbt_stream_error child = {0};
    (void)mc_nbt_stream_source_close(s, &child);
    return mapped(e, &child);
}

mc_java_url_status mc_java_url_data_input_release(mc_java_url_data_input **slot,
                                                  mc_java_url_error *e) {
    if (e == NULL)
        return MC_JAVA_URL_ARGUMENT;
    mc_nbt_stream_error child = {0};
    (void)mc_nbt_stream_release(slot, &child);
    return mapped(e, &child);
}

bool mc_java_url_data_input_inspect(const mc_java_url_data_input *s,
                                    mc_java_url_data_input_state *out) {
    if (out == NULL)
        return false;
    mc_nbt_stream_state state;
    mc_nbt_stream_error e;
    if (mc_nbt_stream_state_read(s, &state, &e) != MC_NBT_OK)
        return false;
    *out = (mc_java_url_data_input_state){.phase = (unsigned int)state.phase,
                                          .holds = 1,
                                          .last_store = state.last_data_input_store,
                                          .in = state.source_in,
                                          .bytearr = state.bytearr,
                                          .read_buffer = state.read_buffer,
                                          .chararr = state.chararr};
    return true;
}
