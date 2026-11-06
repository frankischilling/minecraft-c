/* Included only by the concrete shared rj/QJ owner. */
#include "../../platform/time/millis.h"
#include "java_output_internal.h"
#include "mc/core/text/mutf8.h"
#include "mc/world/storage/source_region_output.h"
#include <zlib.h>

struct mc_world_source_region_output {
    mc_world_source_regions *regions;
    source_region *region;
    mc_java_output_options options;
    mc_java_output_phase phase;
    bool active, closed, finished, z_initialized, deflater_ended;
    bool z_limit, z_allocation_failed, commit_entered, commit_returned, oversized;
    int32_t x, z, written, sector, required, timestamp;
    size_t bytes, peak_bytes, utf_bytes, count, capacity, source_closes;
    size_t sectors_appended, free_updates;
    uint64_t raw_bytes;
    mc_output_lock *close_lock;
    uint8_t *write_buffer, *utf_buffer, *compressed;
    uint8_t buffer[512];
    z_stream deflater;
    mc_world_source_region_output_error failure;
};

typedef union ro_allocation {
    /* Match the supported desktop zlib/scalar alignment without relying on
     * max_align_t, which is absent from the Windows C runtime headers. */
    z_stream stream_alignment;
    long double scalar_alignment;
    void *pointer_alignment;

    struct {
        size_t bytes;
    } size;
} ro_allocation;

static void ro_peak(mc_world_source_region_output *o) {
    if (o->bytes > o->peak_bytes)
        o->peak_bytes = o->bytes;
}

static bool ro_room(const mc_world_source_region_output *o, size_t bytes) {
    return o->bytes <= o->options.max_owner_bytes && bytes <= o->options.max_owner_bytes - o->bytes;
}

static voidpf ro_z_allocate(voidpf context, uInt items, uInt width) {
    mc_world_source_region_output *o = context;
    if ((width != 0 && (size_t)items > (SIZE_MAX - sizeof(ro_allocation)) / width) ||
        !ro_room(o, sizeof(ro_allocation) + (size_t)items * width)) {
        o->z_limit = true;
        return NULL;
    }
    const size_t bytes = sizeof(ro_allocation) + (size_t)items * width;
    ro_allocation *p = calloc(1, bytes);
    if (p == NULL) {
        o->z_allocation_failed = true;
        return NULL;
    }
    p->size.bytes = bytes;
    o->bytes += bytes;
    ro_peak(o);
    return p + 1;
}

static void ro_z_free(voidpf context, voidpf pointer) {
    if (pointer != NULL) {
        mc_world_source_region_output *o = context;
        ro_allocation *p = (ro_allocation *)pointer - 1;
        o->bytes -= p->size.bytes;
        free(p);
    }
}

static void ro_end(mc_world_source_region_output *o) {
    if (o->z_initialized) {
        (void)deflateEnd(&o->deflater);
        o->z_initialized = false;
        o->deflater_ended = true;
    }
}

static mc_java_output_status ro_failure(mc_compressed_output_error *e, mc_java_output_status s,
                                        const char *cl, const char *method, const char *desc,
                                        int pc, const char *operation, bool source) {
    return mc_output_fail(&e->output, s, cl, method, desc, pc, operation, 0, source);
}

static void ro_z_failure(mc_world_source_region_output *o, mc_compressed_output_error *e, int z) {
    e->zlib_status = z;
    ro_failure(e,
               o->z_limit               ? MC_JAVA_OUTPUT_LIMIT
               : o->z_allocation_failed ? MC_JAVA_OUTPUT_ALLOCATION
                                        : MC_JAVA_OUTPUT_NATIVE_PROVIDER,
               NULL, NULL, NULL, -1, "native region deflater", false);
}

static mc_world_source_status ro_publish(mc_world_source_region_output *o,
                                         mc_world_source_region_output_error *e,
                                         mc_world_source_region_output_error *out) {
    if (e->region.source.status == MC_WORLD_SOURCE_OK &&
        e->stream.output.status != MC_JAVA_OUTPUT_OK) {
        const mc_java_output_status s = e->stream.output.status;
        result(&e->region,
               s == MC_JAVA_OUTPUT_BUSY         ? MC_WORLD_SOURCE_BUSY
               : s == MC_JAVA_OUTPUT_LIMIT      ? MC_WORLD_SOURCE_LIMIT
               : s == MC_JAVA_OUTPUT_ALLOCATION ? MC_WORLD_SOURCE_ALLOCATION
               : s == MC_JAVA_OUTPUT_ARGUMENT   ? MC_WORLD_SOURCE_ARGUMENT
                                                : MC_WORLD_SOURCE_OUTPUT,
               e->stream.output.cause.operation, NULL, NULL, -1);
        e->region.source.output = e->stream.output;
        e->region.source.source_exception = e->stream.output.cause.source_exception;
        e->region.source.native_policy = !e->region.source.source_exception;
        e->region.source.exception_class = e->stream.output.cause.exception_class;
        if (e->region.source.source_exception)
            e->region.source.status = e->region.source.cause_status =
                MC_WORLD_SOURCE_SOURCE_EXCEPTION;
    }
    if (o != NULL) {
        o->active = false;
        if (e->region.source.status != MC_WORLD_SOURCE_OK)
            o->failure = *e;
    }
    if (out != NULL)
        *out = *e;
    return e->region.source.status;
}

static bool ro_enter(mc_world_source_region_output *o, mc_world_source_region_output_error *e) {
    if (o == NULL || o->active || o->phase != MC_JAVA_OUTPUT_READY || o->regions == NULL ||
        o->regions->active || o->regions->retiring) {
        ro_failure(&e->stream,
                   o == NULL ? MC_JAVA_OUTPUT_ARGUMENT
                   : o->active || (o->regions != NULL && o->regions->active) ? MC_JAVA_OUTPUT_BUSY
                                                                             : MC_JAVA_OUTPUT_STATE,
                   NULL, NULL, NULL, -1, "native region output admission", false);
        return false;
    }
    o->active = true;
    return true;
}

static bool ro_append(mc_world_source_region_output *o, const uint8_t *bytes, size_t count,
                      mc_compressed_output_error *e) {
    if (count > (size_t)INT32_MAX - o->count || count > o->regions->options.max_compressed_bytes ||
        o->count > o->regions->options.max_compressed_bytes - count ||
        (uint64_t)o->count > o->options.max_written_bytes ||
        (uint64_t)count > o->options.max_written_bytes - o->count) {
        ro_failure(e, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1, "native Hz byte-array bound",
                   false);
        return false;
    }
    const size_t needed = o->count + count;
    if (needed > o->capacity) {
        size_t capacity = o->capacity > (size_t)INT32_MAX / 2 ? INT32_MAX : o->capacity * 2;
        if (capacity < needed)
            capacity = needed;
        if (!ro_room(o, capacity)) {
            ro_failure(e, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1, "native Hz growth peak bound",
                       false);
            return false;
        }
        uint8_t *replacement = calloc(capacity, 1);
        if (replacement == NULL) {
            ro_failure(e, MC_JAVA_OUTPUT_ALLOCATION, NULL, NULL, NULL, -1, "native Hz growth",
                       false);
            return false;
        }
        memcpy(replacement, o->compressed, o->count);
        o->bytes += capacity;
        ro_peak(o);
        free(o->compressed);
        o->bytes -= o->capacity;
        o->capacity = capacity;
        o->compressed = replacement;
    }
    memcpy(o->compressed + o->count, bytes, count);
    o->count += count;
    return true;
}

static bool ro_stream_write(mc_world_source_region_output *o, const uint8_t *bytes, size_t count,
                            mc_compressed_output_error *e) {
    if (o->finished) {
        ro_failure(e, MC_JAVA_OUTPUT_SOURCE_IO, "java/util/zip/DeflaterOutputStream", "write",
                   "([BII)V", 10, "write beyond end of stream", true);
        return false;
    }
    if (!o->z_initialized || count > INT32_MAX || (uint64_t)count > UINT64_MAX - o->raw_bytes) {
        ro_failure(e, !o->z_initialized ? MC_JAVA_OUTPUT_STATE : MC_JAVA_OUTPUT_LIMIT, NULL, NULL,
                   NULL, -1, "native region deflater input", false);
        return false;
    }
    if (count == 0)
        return true;
    o->deflater.next_in = (Bytef *)bytes;
    o->deflater.avail_in = (uInt)count;
    while (o->deflater.avail_in != 0) {
        const uInt before = o->deflater.avail_in;
        o->deflater.next_out = o->buffer;
        o->deflater.avail_out = (uInt)sizeof(o->buffer);
        const int z = deflate(&o->deflater, Z_NO_FLUSH);
        const size_t produced = sizeof(o->buffer) - o->deflater.avail_out;
        o->raw_bytes += before - o->deflater.avail_in;
        if (z != Z_OK || (before == o->deflater.avail_in && produced == 0)) {
            ro_z_failure(o, e, z);
            return false;
        }
        if (produced != 0 && !ro_append(o, o->buffer, produced, e))
            return false;
    }
    return true;
}

/* Primitive/NBT encoder mirrors the metadata concrete DOS encoder. */
static bool ro_dos_write(mc_world_source_region_output *o, const uint8_t *bytes, size_t count,
                         const char *method, const char *descriptor, int pc,
                         mc_compressed_output_error *e) {
    if (!ro_stream_write(o, bytes, count, e)) {
        mc_output_caller(&e->output, "java/io/DataOutputStream", method, descriptor, pc);
        return false;
    }
    o->written = count > (size_t)(INT32_MAX - o->written) ? INT32_MAX : o->written + (int32_t)count;
    return true;
}

static bool ro_scalar(mc_world_source_region_output *o, uint64_t value, size_t width,
                      mc_compressed_output_error *e) {
    for (size_t i = 0; i < width; ++i)
        o->write_buffer[i] = (uint8_t)(value >> (8u * (unsigned)(width - i - 1)));
    const char *method = width == 1   ? "writeByte"
                         : width == 2 ? "writeShort"
                         : width == 4 ? "writeInt"
                                      : "writeLong";
    const char *descriptor = width == 8 ? "(J)V" : "(I)V";
    const int pc = width == 1 ? 5 : width == 2 ? 31 : width == 4 ? 53 : 108;
    return ro_dos_write(o, o->write_buffer, width, method, descriptor, pc, e);
}

static bool ro_utf(mc_world_source_region_output *o, mc_nbt_string text,
                   mc_compressed_output_error *e) {
    size_t length = 0;
    if (mc_mutf8_encoded_length(text.units, text.length, &length) != MC_MUTF8_OK ||
        length > 65535) {
        ro_failure(e, MC_JAVA_OUTPUT_STATE, NULL, NULL, NULL, -1,
                   "native validated NBT modified-UTF invariant", false);
        return false;
    }
    const size_t needed = length + 2;
    if (o->utf_bytes < needed) {
        const size_t allocation = length * 2 + 2; /* Actual DOS.writeUTF123. */
        if (!ro_room(o, allocation)) {
            ro_failure(e, MC_JAVA_OUTPUT_LIMIT, "java/io/DataOutputStream", "writeUTF",
                       "(Ljava/lang/String;Ljava/io/DataOutput;)I", 123,
                       "native compressed DOS UTF buffer bound", false);
            return false;
        }
        uint8_t *replacement = calloc(allocation, 1);
        if (replacement == NULL) {
            ro_failure(e, MC_JAVA_OUTPUT_ALLOCATION, "java/io/DataOutputStream", "writeUTF",
                       "(Ljava/lang/String;Ljava/io/DataOutput;)I", 123,
                       "native compressed DOS UTF buffer", false);
            return false;
        }
        o->bytes += allocation;
        ro_peak(o);
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
        ro_failure(e, MC_JAVA_OUTPUT_STATE, NULL, NULL, NULL, -1,
                   "native validated NBT modified-UTF encoding", false);
        return false;
    }
    return ro_dos_write(o, o->utf_buffer, needed, "write", "([BII)V", 7, e);
}

static bool ro_tag(mc_world_source_region_output *o, const mc_nbt_tag *value, bool named,
                   mc_compressed_output_error *e) {
    if (named && !ro_scalar(o, (uint64_t)value->type, 1, e))
        return false;
    if (value->type == MC_NBT_END)
        return true;
    if (named && !ro_utf(o, value->name, e))
        return false;
    switch (value->type) {
        case MC_NBT_END:
            return true;
        case MC_NBT_BYTE:
            return ro_scalar(o, (uint8_t)value->value.byte_value, 1, e);
        case MC_NBT_SHORT:
            return ro_scalar(o, (uint16_t)value->value.short_value, 2, e);
        case MC_NBT_INT:
            return ro_scalar(o, (uint32_t)value->value.int_value, 4, e);
        case MC_NBT_LONG:
            return ro_scalar(o, (uint64_t)value->value.long_value, 8, e);
        case MC_NBT_FLOAT: {
            uint32_t bits = value->value.float_bits;
            if ((bits & UINT32_C(0x7f800000)) == UINT32_C(0x7f800000) &&
                (bits & UINT32_C(0x007fffff)) != 0)
                bits = UINT32_C(0x7fc00000);
            return ro_scalar(o, bits, 4, e);
        }
        case MC_NBT_DOUBLE: {
            uint64_t bits = value->value.double_bits;
            if ((bits & UINT64_C(0x7ff0000000000000)) == UINT64_C(0x7ff0000000000000) &&
                (bits & UINT64_C(0x000fffffffffffff)) != 0)
                bits = UINT64_C(0x7ff8000000000000);
            return ro_scalar(o, bits, 8, e);
        }
        case MC_NBT_BYTE_ARRAY:
            return ro_scalar(o, value->value.byte_array.length, 4, e) &&
                   ro_dos_write(o, value->value.byte_array.data, value->value.byte_array.length,
                                "write", "([BII)V", 7, e);
        case MC_NBT_STRING:
            return ro_utf(o, value->value.string, e);
        case MC_NBT_LIST:
            if (!ro_scalar(o,
                           (uint64_t)(value->value.list.length == 0
                                          ? MC_NBT_BYTE
                                          : value->value.list.element_type),
                           1, e) ||
                !ro_scalar(o, value->value.list.length, 4, e))
                return false;
            for (size_t i = 0; i < value->value.list.length; ++i)
                if (!ro_tag(o, value->value.list.items[i], false, e))
                    return false;
            return true;
        case MC_NBT_COMPOUND:
            for (size_t i = 0; i < value->value.compound.length; ++i)
                if (!ro_tag(o, value->value.compound.items[i], true, e))
                    return false;
            return ro_scalar(o, MC_NBT_END, 1, e);
    }
    ro_failure(e, MC_JAVA_OUTPUT_STATE, NULL, NULL, NULL, -1, "native validated NBT type", false);
    return false;
}

static bool ro_raf_error(mc_world_source_region_output_error *e, source_region *q,
                         mc_java_file_status s, mc_java_file_error child, const char *method,
                         const char *desc, int pc, int parent_pc) {
    mc_java_random_access_state state;
    if (s == MC_JAVA_FILE_STATE_ERROR && mc_java_random_access_inspect(q->raf, &state) &&
        state.phase == MC_JAVA_FILE_READY && !state.native_open && state.close_reached) {
        /* The concrete source RAF was closed by rj.a while Hz kept QJ alive.
         * The lower native owner's closed-state refusal represents the reached
         * JDK IOException, not a reopened descriptor or a cache lookup. */
        s = MC_JAVA_FILE_IO_ERROR;
        child = (mc_java_file_error){s, "actual source-closed RandomAccessFile", 0};
    }
    file_failure(&e->region, s, child, "qj", method, pc);
    e->region.source.frames[0].descriptor = desc;
    e->region.source.cause_site = e->region.source.frames[0];
    if (parent_pc >= 0)
        e->region.source.prior_site = (mc_world_source_site){"qj", "a", "(II[BI)V", parent_pc};
    if (s == MC_JAVA_FILE_IO_ERROR) {
        e->region.source.status = MC_WORLD_SOURCE_UNIMPLEMENTED;
        e->region.source.operation = "java/io/IOException.printStackTrace()V";
        e->region.source.exception_class = "java/io/IOException";
        e->region.source.caught_io = e->region.caught_io = true;
        e->region.source.diagnostic_frontier = true;
        e->region.source.frames[0] = (mc_world_source_site){"qj", "a", "(II[BI)V", 441};
    }
    return false;
}

static bool ro_write_int(source_region *q, int32_t value, const char *method, const char *desc,
                         int pc, int parent_pc, mc_world_source_region_output_error *e) {
    for (unsigned i = 0; i < 4; ++i) {
        const uint8_t b = (uint8_t)((uint32_t)value >> (24u - 8u * i));
        mc_java_file_error child;
        const mc_java_file_status s = mc_java_random_access_write(q->raf, &b, 1, &child);
        if (s != MC_JAVA_FILE_OK)
            return ro_raf_error(e, q, s, child, method, desc, pc, parent_pc);
    }
    return true;
}

static bool ro_payload(mc_world_source_region_output *o, int parent_pc,
                       mc_world_source_region_output_error *e) {
    source_region *q = o->region;
    mc_java_file_error child;
    mc_java_file_status s =
        mc_java_random_access_seek(q->raf, (int64_t)mc_java_i32_mul(o->sector, 4096), &child);
    if (s != MC_JAVA_FILE_OK)
        return ro_raf_error(e, q, s, child, "a", "(I[BI)V", 33, parent_pc);
    if (!ro_write_int(q, mc_java_i32_add((int32_t)o->count, 1), "a", "(I[BI)V", 43, parent_pc, e))
        return false;
    const uint8_t selector = 2;
    s = mc_java_random_access_write(q->raf, &selector, 1, &child);
    if (s != MC_JAVA_FILE_OK)
        return ro_raf_error(e, q, s, child, "a", "(I[BI)V", 51, parent_pc);
    s = mc_java_random_access_write(q->raf, o->compressed, o->count, &child);
    return s == MC_JAVA_FILE_OK || ro_raf_error(e, q, s, child, "a", "(I[BI)V", 61, parent_pc);
}

static bool ro_offset(mc_world_source_region_output *o, int parent_pc,
                      mc_world_source_region_output_error *e) {
    source_region *q = o->region;
    const size_t index = (size_t)o->x + 32u * (size_t)o->z;
    const int32_t value =
        mc_java_i32_narrow((int64_t)(((uint32_t)o->sector << 8) | (uint32_t)o->required));
    q->offsets[index] = value; /* Actual QJ.a(III) RAM store11 before seek25. */
    mc_java_file_error child;
    const mc_java_file_status s = mc_java_random_access_seek(q->raf, (int64_t)(index * 4), &child);
    if (s != MC_JAVA_FILE_OK)
        return ro_raf_error(e, q, s, child, "a", "(III)V", 25, parent_pc);
    return ro_write_int(q, value, "a", "(III)V", 33, parent_pc, e);
}

static bool ro_list_set(mc_world_source_region_output *o, int64_t index, bool free_sector, int pc,
                        mc_world_source_region_output_error *e) {
    source_region *q = o->region;
    if (index < 0 || (uint64_t)index >= q->sectors) {
        result(&e->region, MC_WORLD_SOURCE_SOURCE_EXCEPTION, "actual QJ free-sector List index",
               "qj", "a", pc);
        e->region.source.frames[0].descriptor = "(II[BI)V";
        e->region.source.exception_class = "java/lang/IndexOutOfBoundsException";
        e->region.source.source_exception = true;
        e->region.source.native_policy = false;
        return false;
    }
    q->free_sectors[(size_t)index] = free_sector;
    ++o->free_updates;
    return true;
}

static void ro_commit_failure(mc_world_source_region_output_error *e, mc_world_source_status status,
                              const char *operation, int pc) {
    result(&e->region, status, operation, "qj", "a", pc);
    e->region.source.frames[0].descriptor = "(II[BI)V";
}

static bool ro_list_append(mc_world_source_region_output *o,
                           mc_world_source_region_output_error *e) {
    source_region *q = o->region;
    if (q->sectors == q->sector_capacity) {
        const size_t maximum = o->regions->options.max_region_bytes / 4096u;
        if (q->sectors >= maximum) {
            ro_commit_failure(e, MC_WORLD_SOURCE_LIMIT, "native QJ free-sector List bound", 372);
            return false;
        }
        size_t capacity = q->sector_capacity > maximum / 2 ? maximum : q->sector_capacity * 2;
        if (capacity <= q->sectors)
            capacity = q->sectors + 1;
        bool *replacement = malloc(capacity * sizeof(*replacement));
        if (replacement == NULL) {
            ro_commit_failure(e, MC_WORLD_SOURCE_ALLOCATION, "native QJ free-sector List growth",
                              372);
            return false;
        }
        memcpy(replacement, q->free_sectors, q->sectors * sizeof(*replacement));
        free(q->free_sectors);
        q->free_sectors = replacement;
        q->sector_capacity = capacity;
    }
    q->free_sectors[q->sectors++] = false;
    ++o->free_updates;
    return true;
}

static bool ro_commit(mc_world_source_region_output *o, mc_world_source_region_output_error *e) {
    source_region *q = o->region;
    o->commit_entered = true;
    const size_t index = (size_t)o->x + 32u * (size_t)o->z;
    const int32_t offset = q->offsets[index];
    o->sector = mc_java_i32_shr(offset, 8);
    const int32_t old_count = offset & 255;
    const int32_t required_input = mc_java_i32_add((int32_t)o->count, 5);
    if (required_input < 0) {
        ro_commit_failure(e, MC_WORLD_SOURCE_LIMIT, "native QJ safe length arithmetic", 26);
        return false;
    }
    o->required = required_input / 4096 + 1;
    if (o->required >= 256) {
        o->oversized = o->commit_returned = true;
        return true;
    }
    if (o->sector != 0 && old_count == o->required) {
        if (!ro_payload(o, 74, e))
            return false;
    } else {
        for (int32_t i = 0; i < old_count; ++i)
            if (!ro_list_set(o, (int64_t)o->sector + i, true, 103, e))
                return false;
        size_t start = 0, run = 0;
        for (size_t i = 0; i < q->sectors; ++i) {
            if (q->free_sectors[i]) {
                if (run == 0)
                    start = i;
                ++run;
                if (run >= (size_t)o->required)
                    break;
            } else
                run = 0;
        }
        if (run >= (size_t)o->required) {
            o->sector = (int32_t)start;
            if (!ro_offset(o, 261, e))
                return false;
            for (int32_t i = 0; i < o->required; ++i)
                if (!ro_list_set(o, (int64_t)o->sector + i, false, 287, e))
                    return false;
            if (!ro_payload(o, 303, e))
                return false;
        } else {
            mc_java_file_error child;
            int64_t length;
            mc_java_file_status s = mc_java_random_access_length(q->raf, &length, &child);
            if (s != MC_JAVA_FILE_OK)
                return ro_raf_error(e, q, s, child, "a", "(II[BI)V", 329, -1);
            s = mc_java_random_access_seek(q->raf, length, &child);
            if (s != MC_JAVA_FILE_OK)
                return ro_raf_error(e, q, s, child, "a", "(II[BI)V", 332, -1);
            o->sector = (int32_t)q->sectors;
            static const uint8_t zeros[4096] = {0};
            for (int32_t i = 0; i < o->required; ++i) {
                const uint64_t end = (uint64_t)length + ((uint64_t)(uint32_t)i + 1u) * 4096u;
                if (length < 0 || end > o->regions->options.max_region_bytes || end > INT32_MAX) {
                    ro_commit_failure(e, MC_WORLD_SOURCE_LIMIT, "native QJ appended extent", 361);
                    return false;
                }
                s = mc_java_random_access_write(q->raf, zeros, sizeof(zeros), &child);
                if (s != MC_JAVA_FILE_OK)
                    return ro_raf_error(e, q, s, child, "a", "(II[BI)V", 361, -1);
                ++o->sectors_appended;
                if (!ro_list_append(o, e))
                    return false;
            }
            q->growth = mc_java_i32_add(q->growth, mc_java_i32_mul(4096, o->required));
            if (!ro_payload(o, 403, e) || !ro_offset(o, 417, e))
                return false;
        }
    }
    int64_t millis;
    const mc_platform_millis_status clock = mc_platform_current_time_millis(&millis);
    if (clock != MC_PLATFORM_MILLIS_OK) {
        ro_commit_failure(e, MC_WORLD_SOURCE_CLOCK, "actual QJ timestamp clock", 423);
        e->region.source.platform_clock_status = (int)clock;
        return false;
    }
    o->timestamp = mc_java_i32_narrow(millis / 1000);
    q->timestamps[index] = o->timestamp; /* QJ.b(III) store11 before seek29. */
    mc_java_file_error child;
    const mc_java_file_status s =
        mc_java_random_access_seek(q->raf, 4096 + (int64_t)(index * 4), &child);
    if (s != MC_JAVA_FILE_OK)
        return ro_raf_error(e, q, s, child, "b", "(III)V", 29, 431);
    if (!ro_write_int(q, o->timestamp, "b", "(III)V", 37, 431, e))
        return false;
    o->commit_returned = true;
    return true;
}

mc_java_output_options mc_world_source_region_output_default_options(void) {
    return (mc_java_output_options){1024u * 1024u, UINT64_MAX};
}

mc_world_source_status mc_world_source_regions_output(mc_world_source_regions *r,
                                                      mc_java_file *root, int32_t x, int32_t z,
                                                      const mc_java_output_options *requested,
                                                      mc_world_source_region_output **out,
                                                      mc_world_source_region_output_error *error) {
    mc_world_source_region_output_error e = {0};
    const mc_java_output_options options =
        requested == NULL ? mc_world_source_region_output_default_options() : *requested;
    if (r == NULL || out == NULL || *out != NULL ||
        (root != NULL && mc_java_file_namespace(root) != r->files)) {
        ro_failure(&e.stream, MC_JAVA_OUTPUT_ARGUMENT, NULL, NULL, NULL, -1, "rj.d native domain",
                   false);
        return ro_publish(NULL, &e, error);
    }
    if (r->active || r->retiring || r->failed) {
        if (r->failed)
            e.region = r->failure;
        else
            ro_failure(&e.stream, MC_JAVA_OUTPUT_BUSY, NULL, NULL, NULL, -1,
                       "rj.d active namespace", false);
        return ro_publish(NULL, &e, error);
    }
    if (options.max_owner_bytes < sizeof(mc_world_source_region_output)) {
        ro_failure(&e.stream, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1,
                   "native region output owner bound", false);
        return ro_publish(NULL, &e, error);
    }
    mc_world_source_region_output *o = calloc(1, sizeof(*o));
    if (o == NULL) {
        ro_failure(&e.stream, MC_JAVA_OUTPUT_ALLOCATION, NULL, NULL, NULL, -1,
                   "native region output owner", false);
        return ro_publish(NULL, &e, error);
    }
    *out = o;
    o->options = options;
    o->bytes = o->peak_bytes = sizeof(*o);
    o->phase = MC_JAVA_OUTPUT_CONSTRUCTING;
    o->active = true;
    if (mc_world_source_regions_retain(r, &e.region) != MC_WORLD_SOURCE_OK)
        goto failed;
    o->regions = r;
    r->active = r->activated = true;
    if (pending_clear(r, &e.region) != MC_WORLD_SOURCE_OK ||
        find_region(r, root, x, z, &o->region, &e.region) != MC_WORLD_SOURCE_OK) {
        o->region = NULL; /* No counted QJ occurrence was acquired. */
        r->failure = e.region;
        r->failed = true;
        goto failed;
    }
    if (o->region->holds == SIZE_MAX) {
        o->region = NULL;
        ro_failure(&e.stream, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1,
                   "native Hz QJ occurrence limit", false);
        goto failed;
    }
    ++o->region->holds; /* Actual Hz.a strong QJ field, distinct cache hold. */
    if (pending_clear(r, &e.region) != MC_WORLD_SOURCE_OK)
        goto failed;
    if (!ro_room(o, 8096)) {
        ro_failure(&e.stream, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1,
                   "native Hz initial byte[8096]", false);
        goto failed;
    }
    o->compressed = calloc(8096, 1);
    if (o->compressed == NULL) {
        ro_failure(&e.stream, MC_JAVA_OUTPUT_ALLOCATION, NULL, NULL, NULL, -1,
                   "native Hz initial byte[8096]", false);
        goto failed;
    }
    o->capacity = 8096;
    o->bytes += 8096;
    ro_peak(o);
    o->x = x & 31; /* Actual Hz coordinate stores follow returning super(8096). */
    o->z = z & 31;
    o->deflater = (z_stream){.zalloc = ro_z_allocate, .zfree = ro_z_free, .opaque = o};
    const int initialized = deflateInit2(&o->deflater, Z_DEFAULT_COMPRESSION, Z_DEFLATED, MAX_WBITS,
                                         8, Z_DEFAULT_STRATEGY);
    if (initialized != Z_OK) {
        ro_z_failure(o, &e.stream, initialized);
        goto failed;
    }
    o->z_initialized = true;
    if (mc_output_lock_create(&o->close_lock, &o->bytes, o->options.max_owner_bytes,
                              &e.stream.output, "java/io/FilterOutputStream",
                              "(Ljava/io/OutputStream;)V") != MC_JAVA_OUTPUT_OK)
        goto failed;
    ro_peak(o);
    if (!ro_room(o, 8) || (o->write_buffer = calloc(8, 1)) == NULL) {
        ro_failure(&e.stream, ro_room(o, 8) ? MC_JAVA_OUTPUT_ALLOCATION : MC_JAVA_OUTPUT_LIMIT,
                   NULL, NULL, NULL, -1, "native region DOS writeBuffer", false);
        goto failed;
    }
    o->bytes += 8;
    ro_peak(o);
    o->phase = MC_JAVA_OUTPUT_READY;
    r->active = false;
    return ro_publish(o, &e, error);
failed:
    o->phase = MC_JAVA_OUTPUT_FAILED;
    if (o->regions != NULL)
        o->regions->active = false;
    return ro_publish(o, &e, error);
}

mc_world_source_status
mc_world_source_region_output_write_compound(mc_world_source_region_output *o,
                                             const mc_nbt_tag *root, const mc_nbt_limits *requested,
                                             mc_world_source_region_output_error *out) {
    mc_world_source_region_output_error e = {0};
    if (!ro_enter(o, &e))
        return ro_publish(NULL, &e, out);
    mc_nbt_limits limits = requested == NULL ? mc_nbt_default_limits() : *requested;
    if (root == NULL || root->type != MC_NBT_COMPOUND)
        ro_failure(&e.stream, MC_JAVA_OUTPUT_ARGUMENT, NULL, NULL, NULL, -1,
                   "native region compound input", false);
    else if (o->bytes >= o->options.max_owner_bytes)
        ro_failure(&e.stream, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1,
                   "native region NBT scratch", false);
    else {
        const size_t remaining = o->options.max_owner_bytes - o->bytes;
        if (limits.max_allocation_bytes > remaining)
            limits.max_allocation_bytes = remaining;
        size_t measured;
        const mc_nbt_status valid = mc_nbt_measure(root, &limits, &measured, &e.stream.nbt);
        if (valid != MC_NBT_OK)
            ro_failure(&e.stream,
                       valid == MC_NBT_LIMIT               ? MC_JAVA_OUTPUT_LIMIT
                       : valid == MC_NBT_ALLOCATION_FAILED ? MC_JAVA_OUTPUT_ALLOCATION
                                                           : MC_JAVA_OUTPUT_STATE,
                       NULL, NULL, NULL, -1, "native region NBT safety validation", false);
        else
            (void)ro_tag(o, root, true, &e.stream);
    }
    return ro_publish(o, &e, out);
}

mc_world_source_status
mc_world_source_region_output_source_close(mc_world_source_region_output *o,
                                           mc_world_source_region_output_error *out) {
    mc_world_source_region_output_error e = {0};
    if (!ro_enter(o, &e))
        return ro_publish(NULL, &e, out);
    if (o->source_closes == SIZE_MAX) {
        ro_failure(&e.stream, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1,
                   "native region close counter", false);
        return ro_publish(o, &e, out);
    }
    ++o->source_closes;
    if (o->closed)
        return ro_publish(o, &e, out);
    if (mtx_lock(&o->close_lock->mutex) != thrd_success) {
        ro_failure(&e.stream, MC_JAVA_OUTPUT_NATIVE_FAILURE, NULL, NULL, NULL, -1,
                   "native region DOS close lock", false);
        return ro_publish(o, &e, out);
    }
    o->closed = true;
    (void)mtx_unlock(&o->close_lock->mutex);
    /* DOS.flush -> DeflaterOutputStream.flush(syncFlush=false) -> inherited
     * ByteArrayOutputStream/OutputStream.flush has no concrete effect. */
    while (!o->finished) {
        const uInt before = o->deflater.avail_in;
        o->deflater.next_out = o->buffer;
        o->deflater.avail_out = (uInt)sizeof(o->buffer);
        const int z = deflate(&o->deflater, Z_FINISH);
        const size_t produced = sizeof(o->buffer) - o->deflater.avail_out;
        o->raw_bytes += before - o->deflater.avail_in;
        if (z != Z_OK && z != Z_STREAM_END) {
            ro_z_failure(o, &e.stream, z);
            return ro_publish(o, &e, out);
        }
        o->finished = z == Z_STREAM_END;
        if (produced != 0 && !ro_append(o, o->buffer, produced, &e.stream))
            return ro_publish(o, &e, out);
        if (!o->finished && produced == 0) {
            ro_z_failure(o, &e.stream, z);
            return ro_publish(o, &e, out);
        }
    }
    ro_end(o); /* Default deflater ends before actual Hz.close/QJ commit. */
    o->regions->active = true;
    const bool committed = ro_commit(o, &e);
    o->regions->active = false;
    if (!committed) {
        o->regions->failure = e.region;
        o->regions->failed = true;
    }
    return ro_publish(o, &e, out);
}

mc_world_source_status
mc_world_source_region_output_release(mc_world_source_region_output **slot,
                                      mc_world_source_region_output_error *out) {
    mc_world_source_region_output_error e = {0};
    if (slot == NULL) {
        ro_failure(&e.stream, MC_JAVA_OUTPUT_ARGUMENT, NULL, NULL, NULL, -1,
                   "native region output release slot", false);
        return ro_publish(NULL, &e, out);
    }
    mc_world_source_region_output *o = *slot;
    if (o == NULL)
        return ro_publish(NULL, &e, out);
    if (o->active || (o->regions != NULL && o->regions->active)) {
        ro_failure(&e.stream, MC_JAVA_OUTPUT_BUSY, NULL, NULL, NULL, -1,
                   "native region output active release", false);
        return ro_publish(NULL, &e, out);
    }
    o->active = true;
    o->phase = MC_JAVA_OUTPUT_RETIRING;
    ro_end(o);
    if (o->regions != NULL)
        o->regions->active = true;
    const mc_world_source_status released = region_release(&o->region, &e.region);
    if (o->regions != NULL)
        o->regions->active = false;
    if (released != MC_WORLD_SOURCE_OK) {
        /* The shared lower release helper also serves source operations. Its
         * IO status here came from native retirement, not a reached source
         * close/throw/catch. Keep the actual child cause and consumed slots. */
        native_cleanup_scope(&e.region);
        return ro_publish(o, &e, out);
    }
    if (mc_world_source_regions_release(&o->regions, &e.region) != MC_WORLD_SOURCE_OK) {
        native_cleanup_scope(&e.region);
        return ro_publish(o, &e, out);
    }
    free(o->compressed);
    free(o->utf_buffer);
    free(o->write_buffer);
    mc_output_lock_free(&o->close_lock);
    free(o);
    *slot = NULL;
    return ro_publish(NULL, &e, out);
}

bool mc_world_source_region_output_inspect(const mc_world_source_region_output *o,
                                           mc_world_source_region_output_state *out) {
    if (o == NULL || out == NULL || o->active || (o->regions != NULL && o->regions->active))
        return false;
    *out = (mc_world_source_region_output_state){
        .phase = o->phase,
        .active = o->active,
        .closed = o->closed,
        .finished = o->finished,
        .deflater_ended = o->deflater_ended,
        .commit_entered = o->commit_entered,
        .commit_returned = o->commit_returned,
        .oversized = o->oversized,
        .regions = o->regions,
        .region_identity = o->region,
        .random_access = o->region == NULL ? NULL : o->region->raf,
        .region_file = o->region == NULL ? NULL : o->region->file,
        .offsets = o->region == NULL ? NULL : o->region->offsets,
        .timestamps = o->region == NULL ? NULL : o->region->timestamps,
        .free_sectors = o->region == NULL ? NULL : o->region->free_sectors,
        .sectors = o->region == NULL ? 0 : o->region->sectors,
        .region_holds = o->region == NULL ? 0 : o->region->holds,
        .growth = o->region == NULL ? 0 : o->region->growth,
        .x = o->x,
        .z = o->z,
        .written = o->written,
        .sector = o->sector,
        .required = o->required,
        .timestamp = o->timestamp,
        .owner_bytes = o->bytes,
        .peak_owner_bytes = o->peak_bytes,
        .buffer_capacity = o->capacity,
        .source_close_calls = o->source_closes,
        .sectors_appended = o->sectors_appended,
        .free_updates = o->free_updates,
        .raw_bytes = o->raw_bytes,
        .compressed_bytes = o->compressed,
        .compressed_length = o->count,
        .failure = &o->failure};
    return true;
}

mc_world_source_status mc_world_source_regions_consume_growth(mc_world_source_regions *r,
                                                              mc_java_file *root, int32_t x,
                                                              int32_t z, int32_t *out,
                                                              mc_world_source_region_error *error) {
    mc_world_source_region_error e = {0};
    if (r == NULL || out == NULL || (root != NULL && mc_java_file_namespace(root) != r->files))
        return result(error, MC_WORLD_SOURCE_ARGUMENT, "rj.b native domain", NULL, NULL, -1);
    if (r->active || r->retiring)
        return result(error, MC_WORLD_SOURCE_BUSY, "rj.b active namespace", NULL, NULL, -1);
    if (r->failed) {
        if (error != NULL)
            *error = r->failure;
        return r->failure.source.status;
    }
    r->active = r->activated = true;
    mc_world_source_status status = pending_clear(r, &e);
    source_region *q = NULL;
    if (status == MC_WORLD_SOURCE_OK)
        status = find_region(r, root, x, z, &q, &e);
    int32_t growth = 0;
    if (status == MC_WORLD_SOURCE_OK) {
        growth = q->growth;
        q->growth = 0; /* Actual QJ.a() source reset before caller resumes. */
        status = pending_clear(r, &e);
    }
    r->active = false;
    if (status == MC_WORLD_SOURCE_OK)
        *out = growth;
    else {
        r->failed = true;
        r->failure = e;
    }
    if (error != NULL)
        *error = e;
    return status;
}
