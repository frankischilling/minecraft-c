#include "mc/core/java/writer.h"
#include "../../platform/files/byte_output.h"
#include "mc/core/java/source_encoder.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <share.h>
#include <wchar.h>
#include <windows.h>
#endif

_Static_assert(CHAR_BIT == 8, "OutputStreamWriter requires eight-bit bytes");

#define BYTE_CAPACITY 8192u
#define PATH_LIMIT (1024u * 1024u)

struct mc_java_writer {
    mc_java_writer_output output;
    mc_java_writer_profile profile;
    mc_java_writer_state state;
    bool active;
    bool leftover;
    uint16_t high;
    size_t position, limit;
    uint8_t bytes[BYTE_CAPACITY];
    FILE *file;
};

struct mc_java_source_encoder {
    mc_java_writer *writer;
    mc_java_source_encoder_output output;
    mc_java_source_write_result last_write;
};

static mc_java_writer_status result(mc_java_writer_error *out, mc_java_writer_status status,
                                    int system_error, const char *operation) {
    if (out != NULL)
        *out = (mc_java_writer_error){
            .status = status, .system_error = system_error, .operation = operation};
    return status;
}

static bool io_error(mc_java_writer_status status) {
    return status == MC_JAVA_WRITER_IO_ERROR || status == MC_JAVA_WRITER_INTERRUPTED_IO;
}

static mc_java_writer_status callback_result(mc_java_writer_status status,
                                             mc_java_writer_error cause,
                                             mc_java_writer_error *error) {
    if (status != MC_JAVA_WRITER_OK && !io_error(status))
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "Writer output callback contract");
    return result(error, status, status == MC_JAVA_WRITER_OK ? 0 : cause.system_error,
                  status == MC_JAVA_WRITER_OK ? NULL : cause.operation);
}

static mc_java_writer_status enter(mc_java_writer *writer, mc_java_writer_error *error) {
    if (writer == NULL)
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "null Writer");
    if (writer->active)
        return result(error, MC_JAVA_WRITER_BUSY, 0, "Writer callback reentry");
    writer->active = true;
    return MC_JAVA_WRITER_OK;
}

static mc_java_writer_status record_policy(mc_java_writer *writer, mc_java_writer_status status,
                                           mc_java_writer_error cause, bool writing,
                                           mc_java_writer_error *error) {
    if (io_error(status)) {
        if (writer->state.first_io_error.status == MC_JAVA_WRITER_OK)
            writer->state.first_io_error = cause;
        if (writer->profile.mode == MC_JAVA_WRITER_PRINT) {
            if (writing && status == MC_JAVA_WRITER_INTERRUPTED_IO)
                writer->state.interrupted_write = true;
            else
                writer->state.trouble = true;
            return result(error, MC_JAVA_WRITER_OK, 0, NULL);
        }
    }
    return result(error, status, cause.system_error, cause.operation);
}

/* OutputStream.write is a void operation. Short native successes finish that
 * operation, but an exception must leave ByteBuffer flipped with position0,
 * even if the sink has already committed a prefix. clear is success-only. */
static mc_java_writer_status drain(mc_java_writer *writer, mc_java_source_encoder *source,
                                   mc_java_writer_error *error) {
    writer->limit = writer->position;
    writer->position = 0;
    if (source != NULL) {
        if (writer->limit == 0) {
            writer->limit = BYTE_CAPACITY;
            return result(error, MC_JAVA_WRITER_OK, 0, NULL);
        }
        mc_java_source_write_result observed = {0};
        mc_java_writer_error cause = {0};
        const mc_java_writer_status status = source->output.write(
            source->output.context, writer->bytes, writer->limit, &observed, &cause);
        source->last_write = observed;
        source->last_write.outcome_rejected = false;
        source->last_write.rejected_source_returned = false;
        source->last_write.rejected_swallowed_io = false;
        if ((unsigned int)status > MC_JAVA_WRITER_BUSY ||
            (status == MC_JAVA_WRITER_OK && !observed.source_returned) ||
            (status != MC_JAVA_WRITER_OK && (observed.source_returned || observed.swallowed_io)) ||
            (observed.swallowed_io &&
             (!observed.source_returned || !io_error(observed.swallowed_error.status))) ||
            (!observed.swallowed_io && observed.swallowed_error.status != MC_JAVA_WRITER_OK)) {
            /* A malformed callback cannot certify normal source return.
             * Preserve its reported flags/cause and physical-progress facts
             * separately, while the actual byte buffer stays flipped. */
            source->last_write.outcome_rejected = true;
            source->last_write.rejected_source_returned = observed.source_returned;
            source->last_write.rejected_swallowed_io = observed.swallowed_io;
            source->last_write.source_returned = false;
            source->last_write.swallowed_io = false;
            return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0,
                          "source OutputStream void-result contract");
        }
        if (status != MC_JAVA_WRITER_OK)
            return result(error, status, cause.system_error, cause.operation);
        /* The source call returned normally. Clear exactly as StreamEncoder
         * does, even if that call swallowed IO after fewer physical bytes.
         * Never feed a fictional physical committed count to legacy sinks. */
        writer->limit = BYTE_CAPACITY;
        return result(error, MC_JAVA_WRITER_OK, 0, NULL);
    }
    size_t sent = 0;
    while (sent < writer->limit) {
        size_t committed = 0;
        mc_java_writer_error cause = {0};
        mc_java_writer_status status = writer->output.write(
            writer->output.context, writer->bytes + sent, writer->limit - sent, &committed, &cause);
        if (committed > writer->limit - sent)
            return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "Writer output count");
        status = callback_result(status, cause, error);
        if (status != MC_JAVA_WRITER_OK)
            return status;
        if (committed == 0)
            return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0,
                          "Writer output made no progress");
        sent += committed;
    }
    writer->limit = BYTE_CAPACITY;
    return result(error, MC_JAVA_WRITER_OK, 0, NULL);
}

static bool high(uint16_t unit) {
    return unit >= 0xd800u && unit <= 0xdbffu;
}

static bool low(uint16_t unit) {
    return unit >= 0xdc00u && unit <= 0xdfffu;
}

static uint8_t single_byte(mc_java_reader_charset charset, uint16_t unit) {
    if (unit < 0x80u)
        return (uint8_t)unit;
    if (charset == MC_JAVA_READER_ISO8859_1)
        return unit <= 0xffu ? (uint8_t)unit : (uint8_t)'?';
    if (charset == MC_JAVA_READER_WINDOWS1252) {
        if (unit >= 0xa0u && unit <= 0xffu)
            return (uint8_t)unit;
        /* Undefined bytes have no reverse mapping, including U+0081/U+FFFD. */
        static const uint16_t extended[32] = {
            0x20ac, 0,      0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160,
            0x2039, 0x0152, 0,      0x017d, 0,      0,      0x2018, 0x2019, 0x201c, 0x201d, 0x2022,
            0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0,      0x017e, 0x0178};
        for (size_t i = 0; i < 32u; ++i)
            if (extended[i] != 0 && unit == extended[i])
                return (uint8_t)(0x80u + i);
    }
    return (uint8_t)'?';
}

/* Encode up to an incomplete last high surrogate. ISO/1252 array encoders
 * test destination room before inspecting it; ASCII/UTF8 inspect that input
 * first. This affects overflow callbacks at an exact8192-byte boundary. */
static mc_java_writer_status encode(mc_java_writer *writer, mc_java_source_encoder *source,
                                    const uint16_t *units, size_t length, size_t *offset,
                                    bool final, mc_java_writer_error *error) {
    while (*offset < length) {
        const uint16_t unit = units[*offset];
        const size_t room = writer->limit - writer->position;
        if (room == 0 && (writer->state.charset == MC_JAVA_READER_WINDOWS1252 ||
                          writer->state.charset == MC_JAVA_READER_ISO8859_1)) {
            mc_java_writer_status status = drain(writer, source, error);
            if (status != MC_JAVA_WRITER_OK)
                return status;
            continue;
        }
        size_t consumed = 1, width = 1;
        uint32_t point = unit;
        bool replacement = low(unit);
        if (high(unit)) {
            if (*offset + 1u == length && !final)
                return result(error, MC_JAVA_WRITER_OK, 0, NULL);
            if (*offset + 1u < length && low(units[*offset + 1u])) {
                consumed = 2;
                point = 0x10000u + (((uint32_t)unit - 0xd800u) << 10) +
                        ((uint32_t)units[*offset + 1u] - 0xdc00u);
            } else {
                replacement = true;
            }
        }
        uint8_t encoded[4];
        if (writer->state.charset != MC_JAVA_READER_UTF8) {
            encoded[0] = consumed == 2u || replacement ? (uint8_t)'?'
                                                       : single_byte(writer->state.charset, unit);
        } else if (replacement) {
            encoded[0] = (uint8_t)'?';
        } else if (point < 0x80u) {
            encoded[0] = (uint8_t)point;
        } else if (point < 0x800u) {
            width = 2;
            encoded[0] = (uint8_t)(0xc0u | (point >> 6));
            encoded[1] = (uint8_t)(0x80u | (point & 0x3fu));
        } else if (point < 0x10000u) {
            width = 3;
            encoded[0] = (uint8_t)(0xe0u | (point >> 12));
            encoded[1] = (uint8_t)(0x80u | ((point >> 6) & 0x3fu));
            encoded[2] = (uint8_t)(0x80u | (point & 0x3fu));
        } else {
            width = 4;
            encoded[0] = (uint8_t)(0xf0u | (point >> 18));
            encoded[1] = (uint8_t)(0x80u | ((point >> 12) & 0x3fu));
            encoded[2] = (uint8_t)(0x80u | ((point >> 6) & 0x3fu));
            encoded[3] = (uint8_t)(0x80u | (point & 0x3fu));
        }
        if (room < width) {
            mc_java_writer_status status = drain(writer, source, error);
            if (status != MC_JAVA_WRITER_OK)
                return status;
            continue;
        }
        memcpy(writer->bytes + writer->position, encoded, width);
        writer->position += width;
        *offset += consumed;
    }
    return result(error, MC_JAVA_WRITER_OK, 0, NULL);
}

/* The first incoming unit is taken into the two-unit leftover buffer before
 * encoding/callbacks. Failure retains the old haveLeftoverChar/leftoverChar
 * fields until the exact success/underflow points below. */
static mc_java_writer_status leftovers(mc_java_writer *writer, mc_java_source_encoder *source,
                                       const uint16_t *units, size_t length, size_t *offset,
                                       bool final, mc_java_writer_error *error) {
    if (!writer->leftover && !final)
        return MC_JAVA_WRITER_OK;
    uint16_t pair[2];
    size_t count = 0;
    if (writer->leftover)
        pair[count++] = writer->high;
    if (*offset < length)
        pair[count++] = units[(*offset)++];
    for (;;) {
        size_t used = 0;
        mc_java_writer_status status = encode(writer, source, pair, count, &used, final, error);
        if (status != MC_JAVA_WRITER_OK)
            return status;
        if (used != count) {
            writer->high = pair[used];
            if (*offset < length) {
                pair[0] = writer->high;
                pair[1] = units[(*offset)++];
                count = 2;
                continue;
            }
            return MC_JAVA_WRITER_OK;
        }
        writer->leftover = false;
        return MC_JAVA_WRITER_OK;
    }
}

static mc_java_writer_status write_units(mc_java_writer *writer, mc_java_source_encoder *source,
                                         const uint16_t *units, size_t length,
                                         mc_java_writer_error *error) {
    if (writer->state.encoder_closed ||
        (writer->profile.mode == MC_JAVA_WRITER_PRINT && !writer->state.print_writer_out_present))
        return result(error, MC_JAVA_WRITER_IO_ERROR, 0, "Stream closed");
    if (length == 0)
        return result(error, MC_JAVA_WRITER_OK, 0, NULL);
    size_t offset = 0;
    mc_java_writer_status status = leftovers(writer, source, units, length, &offset, false, error);
    if (status == MC_JAVA_WRITER_OK)
        status = encode(writer, source, units, length, &offset, false, error);
    if (status == MC_JAVA_WRITER_OK && offset < length) {
        writer->leftover = true;
        writer->high = units[offset];
    }
    return status;
}

static mc_java_writer_status flush_encoder(mc_java_writer *writer, mc_java_writer_error *error) {
    if (writer->state.encoder_closed)
        return result(error, MC_JAVA_WRITER_IO_ERROR, 0, "Stream closed");
    if (writer->position != 0) {
        mc_java_writer_status status = drain(writer, NULL, error);
        if (status != MC_JAVA_WRITER_OK)
            return status;
    }
    mc_java_writer_error cause = {0};
    mc_java_writer_status status = writer->output.flush(writer->output.context, &cause);
    return callback_result(status, cause, error);
}

static mc_java_writer_status close_encoder(mc_java_writer *writer, mc_java_writer_error *error) {
    if (writer->state.encoder_closed)
        return result(error, MC_JAVA_WRITER_OK, 0, NULL);
    size_t offset = 0;
    mc_java_writer_error primary = {0};
    mc_java_writer_status status = leftovers(writer, NULL, NULL, 0, &offset, true, &primary);
    if (status == MC_JAVA_WRITER_OK && writer->position != 0)
        status = drain(writer, NULL, &primary);
    if (status == MC_JAVA_WRITER_OK) {
        mc_java_writer_error cause = {0};
        status = writer->output.flush(writer->output.context, &cause);
        status = callback_result(status, cause, &primary);
    }
    writer->state.sink_close_attempted = true;
    mc_java_writer_error close_error = {0};
    mc_java_writer_status closed = writer->output.close(writer->output.context, &close_error);
    closed = callback_result(closed, close_error, &close_error);
    writer->state.encoder_closed = true;
    if (status != MC_JAVA_WRITER_OK && closed != MC_JAVA_WRITER_OK)
        writer->state.secondary_close_error = close_error;
    if (status == MC_JAVA_WRITER_OK)
        return result(error, closed, close_error.system_error, close_error.operation);
    return result(error, status, primary.system_error, primary.operation);
}

static bool valid_units(const uint16_t *units, size_t length) {
    return units != NULL || length == 0;
}

static mc_java_writer_status construct_writer(const mc_java_writer_output *output,
                                              const mc_java_writer_profile *profile,
                                              mc_java_writer **out, mc_java_writer_error *error) {
    mc_java_reader_charset selected = profile->charset;
    if (selected == MC_JAVA_READER_DEFAULT) {
        const mc_java_reader_status found = mc_java_reader_default_charset(&selected);
        if (found != MC_JAVA_READER_OK)
            return result(error,
                          found == MC_JAVA_READER_ALLOCATION_FAILED
                              ? MC_JAVA_WRITER_ALLOCATION_FAILED
                              : MC_JAVA_WRITER_UNSUPPORTED_CHARSET,
                          0, "Writer default charset");
    }
    mc_java_writer *writer = malloc(sizeof(*writer));
    if (writer == NULL)
        return result(error, MC_JAVA_WRITER_ALLOCATION_FAILED, 0, "Writer allocation");
    *writer = (mc_java_writer){.output = *output, .profile = *profile, .limit = BYTE_CAPACITY};
    writer->state.charset = selected;
    writer->state.print_writer_out_present = profile->mode == MC_JAVA_WRITER_PRINT;
    *out = writer;
    return result(error, MC_JAVA_WRITER_OK, 0, NULL);
}

mc_java_writer_status mc_java_writer_create(const mc_java_writer_output *output,
                                            const mc_java_writer_profile *profile,
                                            mc_java_writer **out, mc_java_writer_error *error) {
    if (output == NULL || output->write == NULL || output->flush == NULL || output->close == NULL ||
        profile == NULL || out == NULL ||
        (unsigned int)profile->charset > MC_JAVA_READER_US_ASCII ||
        (unsigned int)profile->mode > MC_JAVA_WRITER_PRINT ||
        (unsigned int)profile->lines > MC_JAVA_WRITER_CRLF)
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "Writer construction");
    return construct_writer(output, profile, out, error);
}

mc_java_writer_status mc_java_writer_write(mc_java_writer *writer, const uint16_t *units,
                                           size_t length, mc_java_writer_error *error) {
    if (!valid_units(units, length))
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "Writer UTF16 input");
    if (length > INT32_MAX)
        return result(error, MC_JAVA_WRITER_LIMIT, 0, "Writer UTF16 length");
    mc_java_writer_status status = enter(writer, error);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    mc_java_writer_error cause = {0};
    status = write_units(writer, NULL, units, length, &cause);
    status = record_policy(writer, status, cause, true, error);
    writer->active = false;
    return status;
}

static mc_java_writer_status print_text(mc_java_writer *writer,
                                        const mc_java_properties_string_view *text, bool newline,
                                        mc_java_writer_error *error) {
    static const uint16_t null_text[] = {'n', 'u', 'l', 'l'};
    static const uint16_t lf[] = {'\n'}, crlf[] = {'\r', '\n'};
    const uint16_t *units = text == NULL ? null_text : text->units;
    const size_t length = text == NULL ? 4u : text->length;
    if (!valid_units(units, length) || writer == NULL ||
        writer->profile.mode != MC_JAVA_WRITER_PRINT)
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "PrintWriter String input");
    if (length > INT32_MAX)
        return result(error, MC_JAVA_WRITER_LIMIT, 0, "PrintWriter String length");
    mc_java_writer_status status = enter(writer, error);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    mc_java_writer_error cause = {0};
    status = write_units(writer, NULL, units, length, &cause);
    status = record_policy(writer, status, cause, true, error);
    if (status == MC_JAVA_WRITER_OK && newline) {
        status = write_units(writer, NULL, writer->profile.lines == MC_JAVA_WRITER_LF ? lf : crlf,
                             writer->profile.lines == MC_JAVA_WRITER_LF ? 1u : 2u, &cause);
        status = record_policy(writer, status, cause, true, error);
    }
    writer->active = false;
    return status;
}

mc_java_writer_status mc_java_writer_print(mc_java_writer *writer,
                                           const mc_java_properties_string_view *text,
                                           mc_java_writer_error *error) {
    return print_text(writer, text, false, error);
}

mc_java_writer_status mc_java_writer_println(mc_java_writer *writer,
                                             const mc_java_properties_string_view *text,
                                             mc_java_writer_error *error) {
    return print_text(writer, text, true, error);
}

mc_java_writer_status mc_java_writer_flush(mc_java_writer *writer, mc_java_writer_error *error) {
    mc_java_writer_status status = enter(writer, error);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    mc_java_writer_error cause = {0};
    status = flush_encoder(writer, &cause);
    status = record_policy(writer, status, cause, false, error);
    writer->active = false;
    return status;
}

mc_java_writer_status mc_java_writer_close(mc_java_writer *writer, mc_java_writer_error *error) {
    mc_java_writer_status status = enter(writer, error);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    writer->state.source_close_reached = true;
    mc_java_writer_error cause = {0};
    if (writer->profile.mode == MC_JAVA_WRITER_PRINT && !writer->state.print_writer_out_present)
        status = MC_JAVA_WRITER_OK;
    else {
        status = close_encoder(writer, &cause);
        if (status == MC_JAVA_WRITER_OK)
            writer->state.print_writer_out_present = false;
    }
    status = record_policy(writer, status, cause, false, error);
    writer->active = false;
    return status;
}

mc_java_writer_status mc_java_writer_check_error(mc_java_writer *writer, bool *out,
                                                 mc_java_writer_error *error) {
    if (writer == NULL || out == NULL || writer->profile.mode != MC_JAVA_WRITER_PRINT)
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "PrintWriter checkError");
    mc_java_writer_status status = enter(writer, error);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    mc_java_writer_error cause = {0};
    if (writer->state.print_writer_out_present) {
        status = flush_encoder(writer, &cause);
        status = record_policy(writer, status, cause, false, error);
    }
    if (status == MC_JAVA_WRITER_OK) {
        *out = writer->state.trouble;
        result(error, MC_JAVA_WRITER_OK, 0, NULL);
    }
    writer->active = false;
    return status;
}

bool mc_java_writer_read_state(const mc_java_writer *writer, mc_java_writer_state *out) {
    if (writer == NULL || out == NULL || writer->active)
        return false;
    *out = writer->state;
    return true;
}

mc_java_writer_status mc_java_writer_retire(mc_java_writer **owner, mc_java_writer_retirement *out,
                                            mc_java_writer_error *error) {
    if (owner == NULL)
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "Writer retirement slot");
    mc_java_writer *writer = *owner;
    if (writer == NULL) {
        if (out != NULL)
            *out = (mc_java_writer_retirement){0};
        return result(error, MC_JAVA_WRITER_OK, 0, NULL);
    }
    mc_java_writer_status status = enter(writer, error);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    mc_java_writer_retirement retired = {.source_close_reached =
                                             writer->state.source_close_reached};
    mc_java_writer_error cause = {0};
    if (!writer->state.sink_close_attempted) {
        retired.additional_sink_close = true;
        writer->state.sink_close_attempted = true;
        status = writer->output.close(writer->output.context, &cause);
        status = callback_result(status, cause, &cause);
    }
    free(writer);
    *owner = NULL;
    if (out != NULL)
        *out = retired;
    return result(error, status, cause.system_error, cause.operation);
}

static mc_java_writer_status source_flush_result(mc_java_writer_status status,
                                                 mc_java_writer_error cause,
                                                 mc_java_writer_error *error) {
    if ((unsigned int)status > MC_JAVA_WRITER_BUSY)
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0,
                      "source OutputStream status contract");
    return result(error, status, status == MC_JAVA_WRITER_OK ? 0 : cause.system_error,
                  status == MC_JAVA_WRITER_OK ? NULL : cause.operation);
}

mc_java_writer_status mc_java_source_encoder_create(const mc_java_source_encoder_output *output,
                                                    mc_java_reader_charset charset,
                                                    mc_java_source_encoder **out,
                                                    mc_java_writer_error *error) {
    if (output == NULL || output->write == NULL || output->flush == NULL ||
        output->retire == NULL || out == NULL || (unsigned int)charset > MC_JAVA_READER_US_ASCII)
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "source encoder construction");
    mc_java_source_encoder *source = malloc(sizeof(*source));
    if (source == NULL)
        return result(error, MC_JAVA_WRITER_ALLOCATION_FAILED, 0,
                      "source encoder wrapper allocation");
    *source = (mc_java_source_encoder){.output = *output};
    const mc_java_writer_profile profile = {charset, MC_JAVA_WRITER_OUTPUT_STREAM,
                                            MC_JAVA_WRITER_LF};
    /* No legacy sink callback is installed or invoked. The writer pointer is
     * private to this wrapper; every data-plane call passes source explicitly. */
    const mc_java_writer_output unused_legacy_output = {0};
    const mc_java_writer_status status =
        construct_writer(&unused_legacy_output, &profile, &source->writer, error);
    if (status != MC_JAVA_WRITER_OK) {
        free(source);
        return status;
    }
    *out = source;
    return result(error, MC_JAVA_WRITER_OK, 0, NULL);
}

mc_java_writer_status mc_java_source_encoder_write(mc_java_source_encoder *source,
                                                   const uint16_t *units, size_t length,
                                                   mc_java_writer_error *error) {
    if (source == NULL || !valid_units(units, length))
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "source encoder UTF16 input");
    if (length > INT32_MAX)
        return result(error, MC_JAVA_WRITER_LIMIT, 0, "source encoder UTF16 length");
    mc_java_writer *writer = source->writer;
    mc_java_writer_status status = enter(writer, error);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    mc_java_writer_error cause = {0};
    status = write_units(writer, source, units, length, &cause);
    status = record_policy(writer, status, cause, true, error);
    writer->active = false;
    return status;
}

static mc_java_writer_status source_flush(mc_java_source_encoder *source, bool full,
                                          mc_java_writer_error *error) {
    if (source == NULL)
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "source encoder flush owner");
    mc_java_writer *writer = source->writer;
    mc_java_writer_status status = enter(writer, error);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    mc_java_writer_error cause = {0};
    if (writer->state.encoder_closed)
        status = result(&cause, MC_JAVA_WRITER_IO_ERROR, 0, "Stream closed");
    else if (writer->position != 0)
        status = drain(writer, source, &cause);
    if (status == MC_JAVA_WRITER_OK && full) {
        mc_java_writer_error callback_error = {0};
        status = source->output.flush(source->output.context, &callback_error);
        status = source_flush_result(status, callback_error, &cause);
    }
    status = record_policy(writer, status, cause, false, error);
    writer->active = false;
    return status;
}

mc_java_writer_status mc_java_source_encoder_flush_buffer(mc_java_source_encoder *source,
                                                          mc_java_writer_error *error) {
    return source_flush(source, false, error);
}

mc_java_writer_status mc_java_source_encoder_flush(mc_java_source_encoder *source,
                                                   mc_java_writer_error *error) {
    return source_flush(source, true, error);
}

bool mc_java_source_encoder_read_state(const mc_java_source_encoder *source,
                                       mc_java_writer_state *out,
                                       mc_java_source_write_result *last_write) {
    if (source == NULL || out == NULL || source->writer->active)
        return false;
    *out = source->writer->state;
    if (last_write != NULL)
        *last_write = source->last_write;
    return true;
}

mc_java_writer_status mc_java_source_encoder_retire(mc_java_source_encoder **slot,
                                                    mc_java_writer_error *error) {
    if (slot == NULL)
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "source encoder retirement slot");
    mc_java_source_encoder *source = *slot;
    if (source == NULL)
        return result(error, MC_JAVA_WRITER_OK, 0, NULL);
    mc_java_writer *writer = source->writer;
    mc_java_writer_status status = enter(writer, error);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    mc_java_writer_error cause = {0};
    status = source->output.retire(source->output.context, &cause);
    status = source_flush_result(status, cause, &cause);
    if (status == MC_JAVA_WRITER_BUSY) {
        writer->active = false;
        return result(error, status, cause.system_error, cause.operation);
    }
    /* Retire contract: BUSY preserves the borrowed context; every other
     * result has detached it. Never synthesize close/flush/final replacement. */
    free(writer);
    free(source);
    *slot = NULL;
    return result(error, status, cause.system_error, cause.operation);
}

static mc_java_writer_status file_write(void *context, const uint8_t *bytes, size_t length,
                                        size_t *committed, mc_java_writer_error *error) {
    mc_java_writer *writer = context;
    if (writer->file == NULL)
        return result(error, MC_JAVA_WRITER_IO_ERROR, 0, "FileOutputStream closed");
    mc_byte_output_error cause;
    const bool ok = mc_byte_output_write(writer->file, bytes, length, committed, &cause);
    return result(error, ok ? MC_JAVA_WRITER_OK : MC_JAVA_WRITER_IO_ERROR, cause.system_error,
                  ok ? NULL : "FileOutputStream write");
}

static mc_java_writer_status file_flush(void *context, mc_java_writer_error *error) {
    const mc_java_writer *writer = context;
    /* Keep the legacy adapter's closed admission; the shared native flush is
     * the actual OutputStream no-op, also used by the binary source owner. */
    if (writer->file != NULL)
        mc_byte_output_flush(writer->file);
    return result(error, writer->file == NULL ? MC_JAVA_WRITER_IO_ERROR : MC_JAVA_WRITER_OK, 0,
                  writer->file == NULL ? "FileOutputStream closed" : NULL);
}

static mc_java_writer_status file_close(void *context, mc_java_writer_error *error) {
    mc_java_writer *writer = context;
    mc_byte_output_error cause;
    const bool ok = mc_byte_output_close(&writer->file, &cause);
    return result(error, ok ? MC_JAVA_WRITER_OK : MC_JAVA_WRITER_IO_ERROR, cause.system_error,
                  ok ? NULL : "FileOutputStream close");
}

mc_java_writer_status mc_java_file_writer_open(const char *path,
                                               const mc_java_writer_profile *profile,
                                               mc_java_writer **out, mc_java_writer_error *error) {
    if (path == NULL || out == NULL)
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "FileWriter path");
    size_t length = 0;
    while (length <= PATH_LIMIT && path[length] != '\0')
        ++length;
    if (length == 0 || length > PATH_LIMIT)
        return result(error, length == 0 ? MC_JAVA_WRITER_INVALID_ARGUMENT : MC_JAVA_WRITER_LIMIT,
                      0, "FileWriter path length");
    const mc_java_writer_output output = {NULL, file_write, file_flush, file_close};
    mc_java_writer *writer = NULL;
    mc_java_writer_status status = mc_java_writer_create(&output, profile, &writer, error);
    if (status != MC_JAVA_WRITER_OK)
        return status;
    FILE *file = NULL;
    mc_byte_output_error native_error;
    bool opened;
#ifdef _WIN32
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, NULL, 0);
    if (count == 0) {
        free(writer);
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "FileWriter UTF8 path");
    }
    wchar_t *native = malloc(((size_t)count + 1u) * sizeof(*native));
    if (native == NULL) {
        free(writer);
        return result(error, MC_JAVA_WRITER_ALLOCATION_FAILED, 0, "FileWriter native path");
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, (int)length, native, count) !=
        count) {
        free(native);
        free(writer);
        return result(error, MC_JAVA_WRITER_INVALID_ARGUMENT, 0, "FileWriter UTF8 conversion");
    }
    native[count] = L'\0';
    opened = mc_byte_output_open(native, &file, &native_error);
    free(native);
#else
    opened = mc_byte_output_open(path, &file, &native_error);
#endif
    if (!opened) {
        free(writer);
        return result(error, MC_JAVA_WRITER_IO_ERROR, native_error.system_error,
                      "FileWriter open/create/truncate");
    }
    if (!mc_byte_output_unbuffered(&file, &native_error)) {
        free(writer);
        result(error, MC_JAVA_WRITER_IO_ERROR, native_error.system_error,
               "FileWriter unbuffered admission");
        if (error != NULL) {
            error->native_cleanup_failed = native_error.cleanup_failed;
            error->native_cleanup_system_error = native_error.cleanup_system_error;
        }
        return MC_JAVA_WRITER_IO_ERROR;
    }
    writer->file = file;
    writer->state.native_file = true;
    writer->output.context = writer;
    *out = writer;
    return result(error, MC_JAVA_WRITER_OK, 0, NULL);
}
