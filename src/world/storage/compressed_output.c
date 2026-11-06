#include "mc/world/storage/compressed_output.h"

#include "compressed_output_internal.h"
#include "java_output_internal.h"
#include "mc/core/text/mutf8.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

struct mc_compressed_output {
    mc_java_output_options options;
    mc_java_output_phase phase;
    bool active, closed, header_written, finished, file_closed;
    bool gzip_closed, deflater_ended, data_output_constructed, z_initialized;
    bool z_limit, z_allocation_failed;
    mc_java_file_output *file;
    mc_output_lock *close_lock;
    uint8_t *write_buffer, *utf_buffer;
    size_t bytes, peak_bytes, utf_bytes, source_closes, gzip_writes;
    int32_t written;
    uint64_t raw_bytes, compressed_bytes;
    uint32_t crc;
    z_stream deflater;
    uint8_t buffer[512];
    mc_compressed_output_error failure;
};

typedef union compressed_allocation {
    /* Match zlib and the supported desktop scalar alignment without relying
     * on max_align_t, which is absent from the Windows C runtime headers. */
    z_stream stream_alignment;
    long double scalar_alignment;
    void *pointer_alignment;

    struct {
        size_t bytes;
    } size;
} compressed_allocation;

static void peak(mc_compressed_output *o) {
    if (o->bytes > o->peak_bytes)
        o->peak_bytes = o->bytes;
}

static bool room(const mc_compressed_output *o, size_t bytes) {
    return o->bytes <= o->options.max_owner_bytes && bytes <= o->options.max_owner_bytes - o->bytes;
}

static voidpf z_allocate(voidpf context, uInt items, uInt size) {
    mc_compressed_output *o = context;
    const size_t count = (size_t)items, width = (size_t)size;
    if ((width != 0 && count > (SIZE_MAX - sizeof(compressed_allocation)) / width) ||
        !room(o, sizeof(compressed_allocation) + count * width)) {
        o->z_limit = true;
        return NULL;
    }
    const size_t bytes = sizeof(compressed_allocation) + count * width;
    compressed_allocation *p = calloc(1, bytes);
    if (p == NULL) {
        o->z_allocation_failed = true;
        return NULL;
    }
    p->size.bytes = bytes;
    o->bytes += bytes;
    peak(o);
    return p + 1;
}

static void z_free(voidpf context, voidpf pointer) {
    if (pointer == NULL)
        return;
    mc_compressed_output *o = context;
    compressed_allocation *p = (compressed_allocation *)pointer - 1;
    o->bytes -= p->size.bytes;
    free(p);
}

static mc_java_output_status failure(mc_compressed_output_error *e, mc_java_output_status status,
                                     const char *class_name, const char *method,
                                     const char *descriptor, int pc, const char *operation,
                                     bool source) {
    return mc_output_fail(&e->output, status, class_name, method, descriptor, pc, operation, 0,
                          source);
}

static mc_java_output_status publish(mc_compressed_output *o, mc_compressed_output_error *e,
                                     mc_compressed_output_error *out) {
    if (o != NULL) {
        if (e->output.status != MC_JAVA_OUTPUT_OK)
            o->failure = *e;
        o->active = false;
    }
    if (out != NULL)
        *out = *e;
    return e->output.status;
}

static bool enter(mc_compressed_output *o, mc_compressed_output_error *e) {
    if (o == NULL || o->active || o->phase != MC_JAVA_OUTPUT_READY) {
        failure(e,
                o == NULL   ? MC_JAVA_OUTPUT_ARGUMENT
                : o->active ? MC_JAVA_OUTPUT_BUSY
                            : MC_JAVA_OUTPUT_STATE,
                NULL, NULL, NULL, -1, "native compressed DataOutput live receiver", false);
        return false;
    }
    o->active = true;
    return true;
}

static mc_java_output_status z_failure(mc_compressed_output *o, mc_compressed_output_error *e,
                                       int status, const char *operation) {
    e->zlib_status = status;
    return failure(e,
                   o->z_limit               ? MC_JAVA_OUTPUT_LIMIT
                   : o->z_allocation_failed ? MC_JAVA_OUTPUT_ALLOCATION
                                            : MC_JAVA_OUTPUT_NATIVE_PROVIDER,
                   NULL, NULL, NULL, -1, operation, false);
}

static void end_deflater(mc_compressed_output *o) {
    if (o->z_initialized) {
        /* deflateEnd consumes its actual native allocations even when its
         * unfinished-stream result is Z_DATA_ERROR. No finish is synthesized. */
        (void)deflateEnd(&o->deflater);
        o->z_initialized = false;
        o->deflater_ended = true;
    }
}

static bool file_write(mc_compressed_output *o, const uint8_t *bytes, size_t count,
                       const char *method, int pc, mc_compressed_output_error *e) {
    if ((uint64_t)count > UINT64_MAX - o->compressed_bytes ||
        o->compressed_bytes > o->options.max_written_bytes ||
        (uint64_t)count > o->options.max_written_bytes - o->compressed_bytes) {
        failure(e, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1,
                "native compressed output cumulative bound", false);
        return false;
    }
    const mc_java_output_status status =
        mc_java_file_output_write_span(o->file, bytes, count, &e->output);
    o->compressed_bytes += (uint64_t)e->output.committed;
    if (status != MC_JAVA_OUTPUT_OK) {
        mc_output_caller(&e->output,
                         strcmp(method, "deflate") == 0 ? "java/util/zip/DeflaterOutputStream"
                                                        : "java/util/zip/GZIPOutputStream",
                         method, "()V", pc);
        return false;
    }
    return true;
}

mc_java_output_status mc_compressed_output_allocate(const mc_java_output_options *options,
                                                    mc_compressed_output **out,
                                                    mc_compressed_output_error *error) {
    mc_compressed_output_error e = {0};
    const mc_java_output_options limits =
        options == NULL ? mc_java_output_default_options() : *options;
    if (out == NULL || *out != NULL)
        failure(&e, MC_JAVA_OUTPUT_ARGUMENT, NULL, NULL, NULL, -1,
                "native compressed output empty allocation slot", false);
    else if (limits.max_owner_bytes < sizeof(mc_compressed_output))
        failure(&e, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1,
                "native compressed output owner bound", false);
    else {
        mc_compressed_output *o = calloc(1, sizeof(*o));
        if (o == NULL)
            failure(&e, MC_JAVA_OUTPUT_ALLOCATION, NULL, NULL, NULL, -1,
                    "native compressed output owner allocation", false);
        else {
            o->options = limits;
            o->bytes = o->peak_bytes = sizeof(*o);
            *out = o;
        }
    }
    return publish(NULL, &e, error);
}

mc_java_output_status mc_compressed_output_construct(mc_compressed_output *o,
                                                     mc_java_file_output *file,
                                                     mc_compressed_output_error *out) {
    mc_compressed_output_error e = {0};
    if (o == NULL || o->active || o->phase != MC_JAVA_OUTPUT_RAW) {
        failure(&e, o != NULL && o->active ? MC_JAVA_OUTPUT_BUSY : MC_JAVA_OUTPUT_STATE, NULL, NULL,
                NULL, -1, "native compressed output one-shot constructor", false);
        return publish(NULL, &e, out);
    }
    o->active = true;
    o->phase = MC_JAVA_OUTPUT_CONSTRUCTING;
    if (file == NULL) {
        failure(&e, MC_JAVA_OUTPUT_SOURCE_NULL, "java/util/zip/DeflaterOutputStream", "<init>",
                "(Ljava/io/OutputStream;Ljava/util/zip/Deflater;IZ)V", 23,
                "actual null GZIP underlying OutputStream", true);
        goto done;
    }
    if (mc_java_file_output_retain(file, &e.output) != MC_JAVA_OUTPUT_OK)
        goto done;
    o->file = file;
    o->deflater.zalloc = z_allocate;
    o->deflater.zfree = z_free;
    o->deflater.opaque = o;
    const int initialized = deflateInit2(&o->deflater, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                                         -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
    if (initialized != Z_OK) {
        z_failure(o, &e, initialized, "native raw default GZIP deflater construction");
        goto done;
    }
    o->z_initialized = true;
    o->crc = (uint32_t)crc32(0, Z_NULL, 0);
    static const uint8_t header[10] = {31, 139, 8, 0, 0, 0, 0, 0, 0, 255};
    if (!file_write(o, header, sizeof(header), "writeHeader", 55, &e))
        goto done;
    o->header_written = true;
    o->crc = (uint32_t)crc32(0, Z_NULL, 0); /* Actual CRC.reset48 after header. */
    if (mc_output_lock_create(&o->close_lock, &o->bytes, o->options.max_owner_bytes, &e.output,
                              "java/io/FilterOutputStream",
                              "(Ljava/io/OutputStream;)V") != MC_JAVA_OUTPUT_OK)
        goto done;
    peak(o);
    if (!room(o, 8)) {
        failure(&e, MC_JAVA_OUTPUT_LIMIT, "java/io/DataOutputStream", "<init>",
                "(Ljava/io/OutputStream;)V", 13, "native compressed DOS writeBuffer bound", false);
        goto done;
    }
    o->write_buffer = calloc(8, 1);
    if (o->write_buffer == NULL) {
        failure(&e, MC_JAVA_OUTPUT_ALLOCATION, "java/io/DataOutputStream", "<init>",
                "(Ljava/io/OutputStream;)V", 13, "native compressed DOS writeBuffer", false);
        goto done;
    }
    o->bytes += 8;
    peak(o);
    o->data_output_constructed = true;
done:
    o->phase = e.output.status == MC_JAVA_OUTPUT_OK ? MC_JAVA_OUTPUT_READY : MC_JAVA_OUTPUT_FAILED;
    return publish(o, &e, out);
}

static bool gzip_write(mc_compressed_output *o, const uint8_t *bytes, size_t count,
                       mc_compressed_output_error *e) {
    if (o->finished) {
        failure(e, MC_JAVA_OUTPUT_SOURCE_IO, "java/util/zip/DeflaterOutputStream", "write",
                "([BII)V", 10, "write beyond end of stream", true);
        return false;
    }
    if (!o->z_initialized || count > INT32_MAX || (uint64_t)count > UINT64_MAX - o->raw_bytes ||
        o->gzip_writes == SIZE_MAX) {
        failure(e, !o->z_initialized ? MC_JAVA_OUTPUT_STATE : MC_JAVA_OUTPUT_LIMIT, NULL, NULL,
                NULL, -1, "native GZIP input/deflater domain", false);
        return false;
    }
    ++o->gzip_writes;
    if (count == 0)
        return true;
    o->deflater.next_in = (Bytef *)bytes;
    o->deflater.avail_in = (uInt)count;
    while (o->deflater.avail_in != 0) {
        const uInt before = o->deflater.avail_in;
        o->deflater.next_out = o->buffer;
        o->deflater.avail_out = (uInt)sizeof(o->buffer);
        const int status = deflate(&o->deflater, Z_NO_FLUSH);
        const size_t produced = sizeof(o->buffer) - o->deflater.avail_out;
        o->raw_bytes += (uint64_t)(before - o->deflater.avail_in);
        if (status != Z_OK || (before == o->deflater.avail_in && produced == 0)) {
            z_failure(o, e, status, "native GZIP deflate write progress");
            return false;
        }
        if (produced != 0 && !file_write(o, o->buffer, produced, "deflate", 32, e)) {
            mc_output_caller(&e->output, "java/util/zip/DeflaterOutputStream", "write", "([BII)V",
                             81);
            mc_output_caller(&e->output, "java/util/zip/GZIPOutputStream", "write", "([BII)V", 4);
            return false;
        }
    }
    /* GZIP.write's CRC update follows the entire returning deflate write.
     * A later failed output write keeps its already-consumed raw input, but
     * does not credit this high-level call's CRC update. */
    o->crc = (uint32_t)crc32((uLong)o->crc, bytes, (uInt)count);
    return true;
}

static bool dos_write(mc_compressed_output *o, const uint8_t *bytes, size_t count,
                      const char *method, const char *descriptor, int pc,
                      mc_compressed_output_error *e) {
    if (!gzip_write(o, bytes, count, e)) {
        mc_output_caller(&e->output, "java/io/DataOutputStream", method, descriptor, pc);
        return false;
    }
    o->written = count > (size_t)(INT32_MAX - o->written) ? INT32_MAX : o->written + (int32_t)count;
    return true;
}

static bool scalar(mc_compressed_output *o, uint64_t value, size_t width,
                   mc_compressed_output_error *e) {
    for (size_t i = 0; i < width; ++i)
        o->write_buffer[i] = (uint8_t)(value >> (8u * (unsigned)(width - i - 1)));
    const char *method = width == 1   ? "writeByte"
                         : width == 2 ? "writeShort"
                         : width == 4 ? "writeInt"
                                      : "writeLong";
    const char *descriptor = width == 8 ? "(J)V" : "(I)V";
    const int pc = width == 1 ? 5 : width == 2 ? 31 : width == 4 ? 53 : 108;
    return dos_write(o, o->write_buffer, width, method, descriptor, pc, e);
}

static bool utf(mc_compressed_output *o, mc_nbt_string text, mc_compressed_output_error *e) {
    size_t length = 0;
    if (mc_mutf8_encoded_length(text.units, text.length, &length) != MC_MUTF8_OK ||
        length > 65535) {
        failure(e, MC_JAVA_OUTPUT_STATE, NULL, NULL, NULL, -1,
                "native validated NBT modified-UTF invariant", false);
        return false;
    }
    const size_t needed = length + 2;
    if (o->utf_bytes < needed) {
        const size_t allocation = length * 2 + 2; /* Actual DOS.writeUTF123. */
        if (!room(o, allocation)) {
            failure(e, MC_JAVA_OUTPUT_LIMIT, "java/io/DataOutputStream", "writeUTF",
                    "(Ljava/lang/String;Ljava/io/DataOutput;)I", 123,
                    "native compressed DOS UTF buffer bound", false);
            return false;
        }
        uint8_t *replacement = calloc(allocation, 1);
        if (replacement == NULL) {
            failure(e, MC_JAVA_OUTPUT_ALLOCATION, "java/io/DataOutputStream", "writeUTF",
                    "(Ljava/lang/String;Ljava/io/DataOutput;)I", 123,
                    "native compressed DOS UTF buffer", false);
            return false;
        }
        o->bytes += allocation;
        peak(o);
        free(o->utf_buffer);
        o->bytes -= o->utf_bytes;
        o->utf_buffer = replacement;
        o->utf_bytes = allocation;
    }
    o->utf_buffer[0] = (uint8_t)(length >> 8u);
    o->utf_buffer[1] = (uint8_t)length;
    size_t encoded = 0;
    if (mc_mutf8_encode(text.units, text.length, o->utf_buffer + 2, length, &encoded) !=
            MC_MUTF8_OK ||
        encoded != length) {
        failure(e, MC_JAVA_OUTPUT_STATE, NULL, NULL, NULL, -1,
                "native validated NBT modified-UTF encoding", false);
        return false;
    }
    return dos_write(o, o->utf_buffer, needed, "write", "([BII)V", 7, e);
}

static bool tag(mc_compressed_output *o, const mc_nbt_tag *value, bool named,
                mc_compressed_output_error *e) {
    if (named && !scalar(o, (uint64_t)value->type, 1, e))
        return false;
    if (value->type == MC_NBT_END)
        return true;
    if (named && !utf(o, value->name, e))
        return false;
    switch (value->type) {
        case MC_NBT_END:
            return true;
        case MC_NBT_BYTE:
            return scalar(o, (uint8_t)value->value.byte_value, 1, e);
        case MC_NBT_SHORT:
            return scalar(o, (uint16_t)value->value.short_value, 2, e);
        case MC_NBT_INT:
            return scalar(o, (uint32_t)value->value.int_value, 4, e);
        case MC_NBT_LONG:
            return scalar(o, (uint64_t)value->value.long_value, 8, e);
        case MC_NBT_FLOAT: {
            uint32_t bits = value->value.float_bits;
            if ((bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) &&
                (bits & UINT32_C(0x007fffff)) != 0)
                bits = UINT32_C(0x7fc00000);
            return scalar(o, bits, 4, e);
        }
        case MC_NBT_DOUBLE: {
            uint64_t bits = value->value.double_bits;
            if ((bits & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000) &&
                (bits & UINT64_C(0x000fffffffffffff)) != 0)
                bits = UINT64_C(0x7ff8000000000000);
            return scalar(o, bits, 8, e);
        }
        case MC_NBT_BYTE_ARRAY:
            return scalar(o, value->value.byte_array.length, 4, e) &&
                   dos_write(o, value->value.byte_array.data, value->value.byte_array.length,
                             "write", "([BII)V", 7, e);
        case MC_NBT_STRING:
            return utf(o, value->value.string, e);
        case MC_NBT_LIST:
            if (!scalar(o,
                        (uint64_t)(value->value.list.length == 0 ? MC_NBT_BYTE
                                                                 : value->value.list.element_type),
                        1, e) ||
                !scalar(o, value->value.list.length, 4, e))
                return false;
            for (size_t i = 0; i < value->value.list.length; ++i)
                if (!tag(o, value->value.list.items[i], false, e))
                    return false;
            return true;
        case MC_NBT_COMPOUND:
            for (size_t i = 0; i < value->value.compound.length; ++i)
                if (!tag(o, value->value.compound.items[i], true, e))
                    return false;
            return scalar(o, MC_NBT_END, 1, e);
    }
    failure(e, MC_JAVA_OUTPUT_STATE, NULL, NULL, NULL, -1, "native validated NBT type", false);
    return false;
}

mc_java_output_status mc_compressed_output_write_compound(mc_compressed_output *o,
                                                          const mc_nbt_tag *root,
                                                          const mc_nbt_limits *limits,
                                                          mc_compressed_output_error *out) {
    mc_compressed_output_error e = {0};
    if (!enter(o, &e))
        return publish(NULL, &e, out);
    mc_nbt_limits validation = limits == NULL ? mc_nbt_default_limits() : *limits;
    if (root == NULL || root->type != MC_NBT_COMPOUND) {
        failure(&e, MC_JAVA_OUTPUT_ARGUMENT, NULL, NULL, NULL, -1,
                "native named compound output domain", false);
    } else if (o->bytes >= o->options.max_owner_bytes) {
        failure(&e, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1,
                "native NBT validation scratch budget", false);
    } else {
        const size_t remaining = o->options.max_owner_bytes - o->bytes;
        if (validation.max_allocation_bytes > remaining)
            validation.max_allocation_bytes = remaining;
        size_t measured = 0;
        const mc_nbt_status valid = mc_nbt_measure(root, &validation, &measured, &e.nbt);
        if (valid != MC_NBT_OK)
            failure(&e,
                    valid == MC_NBT_LIMIT               ? MC_JAVA_OUTPUT_LIMIT
                    : valid == MC_NBT_ALLOCATION_FAILED ? MC_JAVA_OUTPUT_ALLOCATION
                                                        : MC_JAVA_OUTPUT_STATE,
                    NULL, NULL, NULL, -1, "native NBT output safety validation", false);
        else
            (void)tag(o, root, true, &e);
    }
    return publish(o, &e, out);
}

static void little_u32(uint8_t *bytes, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes[i] = (uint8_t)(value >> (8u * i));
}

static void trailer(const mc_compressed_output *o, uint8_t *bytes) {
    little_u32(bytes, o->crc);
    little_u32(bytes + 4, (uint32_t)o->raw_bytes);
}

static bool gzip_finish(mc_compressed_output *o, mc_compressed_output_error *e) {
    if (o->finished)
        return true;
    if (!o->z_initialized) {
        failure(e, MC_JAVA_OUTPUT_STATE, NULL, NULL, NULL, -1,
                "native GZIP finish requires live deflater", false);
        return false;
    }
    /* A failed high-level GZIP.write may have left unconsumed input on the
     * real deflater. Its source as.a finally must finish that SAME remaining
     * span. The outer owner keeps UTF/scalar buffers and the caller keeps its
     * borrowed NBT arrays live until this call/retirement completes. */
    for (;;) {
        const uInt before = o->deflater.avail_in;
        o->deflater.next_out = o->buffer;
        o->deflater.avail_out = (uInt)sizeof(o->buffer);
        const int status = deflate(&o->deflater, Z_FINISH);
        const size_t produced = sizeof(o->buffer) - o->deflater.avail_out;
        o->raw_bytes += (uint64_t)(before - o->deflater.avail_in);
        if (status != Z_OK && status != Z_STREAM_END) {
            z_failure(o, e, status, "native GZIP finish deflate");
            return false;
        }
        o->finished = status == Z_STREAM_END;
        if (o->finished && produced <= sizeof(o->buffer) - 8) {
            trailer(o, o->buffer + produced);
            if (file_write(o, o->buffer, produced + 8, "finish", 91, e))
                return true;
            break;
        }
        if (produced != 0 && !file_write(o, o->buffer, produced, "finish", 109, e))
            break;
        if (o->finished) {
            uint8_t bytes[8];
            trailer(o, bytes);
            if (file_write(o, bytes, sizeof(bytes), "finish", 131, e))
                return true;
            break;
        }
        if (produced == 0) {
            z_failure(o, e, status, "native GZIP finish progress");
            return false;
        }
    }
    /* Actual GZIP.finish catches IOException only. Native policy failures
     * leave source end/finally uncredited until checked native retirement. */
    if (e->output.cause.source_exception && e->output.status == MC_JAVA_OUTPUT_SOURCE_IO)
        end_deflater(o);
    return false;
}

static bool gzip_close(mc_compressed_output *o, mc_compressed_output_error *e) {
    if (o->gzip_closed)
        return true;
    if (!gzip_finish(o, e)) {
        if (e->output.cause.source_exception)
            end_deflater(o); /* Actual inherited close28..44. */
        mc_output_caller(&e->output, "java/util/zip/DeflaterOutputStream", "close", "()V", 8);
        return false;
    }
    end_deflater(o);
    const mc_java_output_status closed = mc_java_file_output_source_close(o->file, &e->output);
    mc_java_file_output_state file_state;
    if (mc_java_file_output_inspect(o->file, &file_state))
        o->file_closed = file_state.source_closed;
    if (closed != MC_JAVA_OUTPUT_OK) {
        mc_output_caller(&e->output, "java/util/zip/DeflaterOutputStream", "close", "()V", 49);
        return false;
    }
    o->gzip_closed = true;
    return true;
}

mc_java_output_status mc_compressed_output_source_close(mc_compressed_output *o,
                                                        mc_compressed_output_error *out) {
    mc_compressed_output_error e = {0};
    if (!enter(o, &e))
        return publish(NULL, &e, out);
    if (o->source_closes == SIZE_MAX) {
        failure(&e, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1,
                "native compressed source-close counter", false);
        return publish(o, &e, out);
    }
    ++o->source_closes;
    if (o->closed)
        return publish(o, &e, out);
    if (mtx_lock(&o->close_lock->mutex) != thrd_success) {
        failure(&e, MC_JAVA_OUTPUT_NATIVE_FAILURE, NULL, NULL, NULL, -1,
                "native compressed DOS close lock", false);
        return publish(o, &e, out);
    }
    o->closed = true; /* Actual outer Filter.closed27, before flush43. */
    (void)mtx_unlock(&o->close_lock->mutex);
    const mc_java_output_status flushed = mc_java_file_output_flush(o->file, &e.output);
    if (flushed != MC_JAVA_OUTPUT_OK) {
        mc_output_caller(&e.output, "java/util/zip/DeflaterOutputStream", "flush", "()V", 82);
        mc_output_caller(&e.output, "java/io/FilterOutputStream", "close", "()V", 43);
        if (!e.output.cause.source_exception)
            return publish(o, &e, out);
        mc_compressed_output_error closing = {0};
        if (!gzip_close(o, &closing)) {
            if (closing.output.cause.source_exception) {
                closing.output.suppressed = e.output.cause;
                closing.output.suppressed_caller_count = e.output.caller_count;
                memcpy(closing.output.suppressed_callers, e.output.callers,
                       sizeof(e.output.callers));
                closing.output.suppressed_file = e.output.file;
            }
            e = closing;
            mc_output_caller(&e.output, "java/io/FilterOutputStream", "close", "()V", 131);
        }
    } else if (!gzip_close(o, &e))
        mc_output_caller(&e.output, "java/io/FilterOutputStream", "close", "()V", 54);
    return publish(o, &e, out);
}

mc_java_output_status mc_compressed_output_release(mc_compressed_output **slot,
                                                   mc_compressed_output_error *out) {
    mc_compressed_output_error e = {0};
    if (slot == NULL) {
        failure(&e, MC_JAVA_OUTPUT_ARGUMENT, NULL, NULL, NULL, -1,
                "native compressed output retirement slot", false);
        return publish(NULL, &e, out);
    }
    mc_compressed_output *o = *slot;
    if (o == NULL)
        return publish(NULL, &e, out);
    if (o->active || o->phase == MC_JAVA_OUTPUT_CONSTRUCTING) {
        failure(&e, MC_JAVA_OUTPUT_BUSY, NULL, NULL, NULL, -1,
                "native compressed output active retirement", false);
        return publish(NULL, &e, out);
    }
    o->active = true;
    o->phase = MC_JAVA_OUTPUT_RETIRING;
    end_deflater(o);
    if (mc_java_file_output_release(&o->file, &e.output) != MC_JAVA_OUTPUT_OK)
        return publish(o, &e, out);
    free(o->utf_buffer);
    free(o->write_buffer);
    mc_output_lock_free(&o->close_lock);
    free(o);
    *slot = NULL;
    return publish(NULL, &e, out);
}

bool mc_compressed_output_inspect(const mc_compressed_output *o, mc_compressed_output_state *out) {
    if (o == NULL || out == NULL || o->active)
        return false;
    *out = (mc_compressed_output_state){.phase = o->phase,
                                        .active = o->active,
                                        .closed = o->closed,
                                        .header_written = o->header_written,
                                        .finished = o->finished,
                                        .file_closed = o->file_closed,
                                        .gzip_closed = o->gzip_closed,
                                        .deflater_ended = o->deflater_ended,
                                        .data_output_constructed = o->data_output_constructed,
                                        .file = o->file,
                                        .written_count = o->written,
                                        .raw_bytes = o->raw_bytes,
                                        .compressed_bytes = o->compressed_bytes,
                                        .owner_bytes = o->bytes,
                                        .peak_owner_bytes = o->peak_bytes,
                                        .source_close_calls = o->source_closes,
                                        .gzip_write_calls = o->gzip_writes,
                                        .utf_buffer_bytes = o->utf_bytes,
                                        .crc = o->crc,
                                        .failure = &o->failure};
    return true;
}

mc_java_output_status mc_compressed_output_terminal_admit(const mc_compressed_output *o,
                                                          const mc_java_file_output *file,
                                                          bool *retains_file) {
    if (o == NULL || retains_file == NULL)
        return MC_JAVA_OUTPUT_ARGUMENT;
    if (o->active || o->phase == MC_JAVA_OUTPUT_CONSTRUCTING)
        return MC_JAVA_OUTPUT_BUSY;
    if (o->phase != MC_JAVA_OUTPUT_RAW && o->phase != MC_JAVA_OUTPUT_FAILED &&
        o->phase != MC_JAVA_OUTPUT_READY && o->phase != MC_JAVA_OUTPUT_RETIRING)
        return MC_JAVA_OUTPUT_STATE;
    if ((o->file != NULL && o->file != file) ||
        (o->phase == MC_JAVA_OUTPUT_RAW && o->file != NULL) ||
        (o->phase == MC_JAVA_OUTPUT_READY &&
         (o->file == NULL || !o->header_written || !o->data_output_constructed)))
        return MC_JAVA_OUTPUT_STATE;
    *retains_file = o->file != NULL;
    return MC_JAVA_OUTPUT_OK;
}
