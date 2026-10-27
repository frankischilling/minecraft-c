#include "mc/world/storage/gzip_input.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

struct mc_gzip_input {
    mc_gzip_options options;
    mc_gzip_state state;
    mc_raw_inflater *inflater, *pending_inflater;
    mc_java_file_stream *input, *pending_input;
    uint8_t *single, *skip, *fill, *tmp;
    uint32_t *crc;
    uint64_t native_output;
    bool native_failed;
    mc_gzip_error last_error, last_close_error, ignored_error;
};

typedef struct gzip_cursor {
    mc_gzip_input *owner;
    size_t position, end;
    bool buffered, header;
} gzip_cursor;

static void text(char out[128], const char *in) {
    size_t n = in == NULL ? 0 : strlen(in);
    if (n > 127u)
        n = 127u;
    if (n != 0)
        memcpy(out, in, n);
    out[n] = 0;
}

static mc_gzip_status result(mc_gzip_error *e, mc_gzip_status s, const char *op, bool source,
                             const char *message, const char *cl, const char *method, int pc) {
    if (e != NULL) {
        *e = (mc_gzip_error){.status = s,
                             .operation = op,
                             .source_class = cl,
                             .source_method = method,
                             .source_pc = pc,
                             .source_exception = source};
        text(e->message, message);
    }
    return s;
}

static mc_gzip_status file_error(mc_gzip_error *e, mc_java_file_status s, mc_java_file_error child,
                                 const char *operation) {
    mc_gzip_status mapped = s == MC_JAVA_FILE_LIMIT               ? MC_GZIP_LIMIT
                            : s == MC_JAVA_FILE_ALLOCATION_FAILED ? MC_GZIP_ALLOCATION
                            : s == MC_JAVA_FILE_BUSY              ? MC_GZIP_BUSY
                            : s == MC_JAVA_FILE_IO_ERROR          ? MC_GZIP_SOURCE_IO
                                                                  : MC_GZIP_NATIVE_PROVIDER;
    const char *method = strcmp(operation, "read") == 0        ? "read([BII)I"
                         : strcmp(operation, "read()") == 0    ? "read()I"
                         : strcmp(operation, "available") == 0 ? "available()I"
                         : strcmp(operation, "close") == 0     ? "close()V"
                                                               : NULL;
    result(e, mapped, operation, s == MC_JAVA_FILE_IO_ERROR,
           s == MC_JAVA_FILE_IO_ERROR ? child.operation : NULL,
           method == NULL ? NULL : "java/io/FileInputStream", method, -1);
    if (e != NULL)
        e->file = child;
    return mapped;
}

static mc_gzip_status inflate_error(mc_gzip_error *e, mc_raw_inflater_status s,
                                    mc_raw_inflater_error child, const char *operation) {
    mc_gzip_status mapped = s == MC_RAW_INFLATER_LIMIT        ? MC_GZIP_LIMIT
                            : s == MC_RAW_INFLATER_ALLOCATION ? MC_GZIP_ALLOCATION
                            : s == MC_RAW_INFLATER_BUSY       ? MC_GZIP_BUSY
                            : s == MC_RAW_INFLATER_DATA_ERROR ? MC_GZIP_SOURCE_FORMAT
                                                              : MC_GZIP_NATIVE_PROVIDER;
    const char *message = s == MC_RAW_INFLATER_DATA_ERROR
                              ? (child.message[0] != 0 ? child.message : "Invalid ZLIB data format")
                              : NULL;
    result(e, mapped, operation, s == MC_RAW_INFLATER_DATA_ERROR, message,
           s == MC_RAW_INFLATER_DATA_ERROR ? "java/util/zip/InflaterInputStream" : NULL,
           s == MC_RAW_INFLATER_DATA_ERROR ? "read([BII)I" : NULL, -1);
    if (e != NULL)
        e->inflater = child;
    return mapped;
}

static mc_gzip_status child(mc_gzip_input *g, size_t count, size_t size, void **out,
                            mc_gzip_error *e, const char *cl, int new_pc) {
    if (size != 0 && count > SIZE_MAX / size)
        return result(e, MC_GZIP_LIMIT, "GZIP child size overflow", false, NULL, cl,
                      strcmp(cl, "java/util/zip/InflaterInputStream") == 0
                          ? "<init>(Ljava/io/InputStream;Ljava/util/zip/Inflater;I)V"
                          : "<init>(Ljava/io/InputStream;I)V",
                      new_pc);
    const char *method = strcmp(cl, "java/util/zip/InflaterInputStream") == 0
                             ? "<init>(Ljava/io/InputStream;Ljava/util/zip/Inflater;I)V"
                             : "<init>(Ljava/io/InputStream;I)V";
    size_t n = count * size;
    if (n > g->options.maximum_owner_bytes - g->state.owner_bytes)
        return result(e, MC_GZIP_LIMIT, "GZIP child allocation bound", false, NULL, cl, method,
                      new_pc);
    void *p = calloc(count, size);
    if (p == NULL)
        return result(e, MC_GZIP_ALLOCATION, "GZIP child allocation", false, NULL, cl, method,
                      new_pc);
    g->state.owner_bytes += n;
    *out = p;
    return MC_GZIP_OK;
}

static void store(mc_gzip_input *g, const char *cl, int pc) {
    g->state.last_store_class = cl;
    g->state.last_store_pc = pc;
}

mc_gzip_options mc_gzip_default_options(void) {
    return (mc_gzip_options){1048576u, 134217728u, 67108864u, 67108864u, 1024u};
}

mc_gzip_status mc_gzip_input_allocate(const mc_gzip_options *options, mc_gzip_input **out,
                                      mc_gzip_error *e) {
    if (out == NULL)
        return result(e, MC_GZIP_ARGUMENT, "GZIP native output", false, NULL, NULL, NULL, -1);
    mc_gzip_options o = options == NULL ? mc_gzip_default_options() : *options;
    if (o.maximum_members == 0)
        return result(e, MC_GZIP_ARGUMENT, "GZIP member bound", false, NULL, NULL, NULL, -1);
    if (o.maximum_owner_bytes < sizeof(mc_gzip_input))
        return result(e, MC_GZIP_LIMIT, "GZIP native owner bound", false, NULL, NULL, NULL, -1);
    mc_gzip_input *g = calloc(1, sizeof(*g));
    if (g == NULL)
        return result(e, MC_GZIP_ALLOCATION, "GZIP native owner", false, NULL, NULL, NULL, -1);
    g->options = o;
    g->state.holds = 1;
    g->state.owner_bytes = sizeof(*g);
    *out = g;
    return result(e, MC_GZIP_OK, NULL, false, NULL, NULL, NULL, -1);
}

/* Read exactly one real descriptor request. A limit failure retains its real
 * buffer/cursor effects, which cannot be rolled back. Only successful input
 * count is acknowledged to the source caller. No request is silently clamped. */
static mc_gzip_status input_read(mc_gzip_input *g, uint8_t *bytes, size_t n,
                                 mc_nbt_input_result *out, mc_gzip_error *e) {
    if (g->state.compressed_read > g->options.maximum_compressed_bytes)
        return result(e, MC_GZIP_LIMIT, "GZIP compressed input bound", false, NULL, NULL, NULL, -1);
    mc_java_file_read read;
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_file_stream_read(g->input, bytes, n, &read, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return file_error(e, fs, fe, "read");
    uint64_t before = g->state.compressed_read;
    g->state.compressed_read += (uint64_t)read.count;
    if (read.count > g->options.maximum_compressed_bytes - (size_t)before)
        return result(e, MC_GZIP_LIMIT, "GZIP compressed input bound after actual read", false,
                      NULL, NULL, NULL, -1);
    *out = (mc_nbt_input_result){read.count, read.eof};
    return MC_GZIP_OK;
}

static mc_gzip_status cursor_read(gzip_cursor *c, uint8_t *bytes, size_t n,
                                  mc_nbt_input_result *out, mc_gzip_error *e) {
    mc_gzip_status s;
    if (c->buffered && c->position < c->end) {
        size_t count = c->end - c->position;
        if (count > n)
            count = n;
        if (count != 0)
            memcpy(bytes, c->owner->fill + c->position, count);
        c->position += count;
        *out = (mc_nbt_input_result){count, false};
        s = MC_GZIP_OK;
    } else {
        c->buffered = false;
        s = input_read(c->owner, bytes, n, out, e);
    }
    if (s == MC_GZIP_OK) {
        if (c->header && out->count != 0)
            *c->owner->crc = (uint32_t)crc32(*c->owner->crc, bytes, (uInt)out->count);
        if (!c->header)
            c->owner->state.trailer_bytes_taken += out->count;
    }
    return s;
}

static mc_gzip_status octet(gzip_cursor *c, uint8_t *out, mc_gzip_error *e) {
    mc_gzip_input *g = c->owner;
    int value;
    if (c->buffered && c->position < c->end)
        value = (int)g->fill[c->position++];
    else {
        c->buffered = false;
        if (g->state.compressed_read > g->options.maximum_compressed_bytes)
            return result(e, MC_GZIP_LIMIT, "GZIP compressed byte bound", false, NULL, NULL, NULL,
                          -1);
        mc_java_file_error fe;
        mc_java_file_status fs = mc_java_file_stream_read_byte(g->input, &value, &fe);
        if (fs != MC_JAVA_FILE_OK)
            return file_error(e, fs, fe, "read()");
        if (value >= 0) {
            ++g->state.compressed_read;
            if (g->state.compressed_read > g->options.maximum_compressed_bytes)
                return result(e, MC_GZIP_LIMIT, "GZIP compressed byte bound after actual read",
                              false, NULL, NULL, NULL, -1);
        }
    }
    if (value < 0)
        return result(e, MC_GZIP_SOURCE_EOF, "GZIP header/trailer byte", true, NULL,
                      "java/util/zip/GZIPInputStream", "readUByte", -1);
    *out = (uint8_t)value;
    if (c->header)
        *g->crc = (uint32_t)crc32(*g->crc, out, 1);
    else
        ++g->state.trailer_bytes_taken;
    return MC_GZIP_OK;
}

static mc_gzip_status little16(gzip_cursor *c, uint16_t *out, mc_gzip_error *e) {
    uint8_t a, b;
    mc_gzip_status s = octet(c, &a, e);
    if (s != MC_GZIP_OK)
        return s;
    s = octet(c, &b, e);
    if (s == MC_GZIP_OK)
        *out = (uint16_t)((uint16_t)b << 8u | a);
    return s;
}

static mc_gzip_status little32(gzip_cursor *c, uint32_t *out, mc_gzip_error *e) {
    uint16_t a, b;
    mc_gzip_status s = little16(c, &a, e);
    if (s != MC_GZIP_OK)
        return s;
    s = little16(c, &b, e);
    if (s == MC_GZIP_OK)
        *out = (uint32_t)b << 16u | a;
    return s;
}

static mc_gzip_status skip_bytes(gzip_cursor *c, size_t n, mc_gzip_error *e) {
    while (n != 0) {
        size_t request = n < 128u ? n : 128u;
        mc_nbt_input_result r;
        mc_gzip_status s = cursor_read(c, c->owner->tmp, request, &r, e);
        if (s != MC_GZIP_OK)
            return s;
        if (r.eof)
            return result(e, MC_GZIP_SOURCE_EOF, "GZIP header skip", true, NULL,
                          "java/util/zip/GZIPInputStream", "skipBytes", -1);
        if (r.count == 0 || r.count > request)
            return result(e, MC_GZIP_NATIVE_PROVIDER, "GZIP zero/out-of-range skip provider", false,
                          NULL, NULL, NULL, -1);
        n -= r.count;
    }
    return MC_GZIP_OK;
}

static mc_gzip_status header(gzip_cursor *c, size_t *used, mc_gzip_error *e) {
    mc_gzip_input *g = c->owner;
    c->header = true;
    *g->crc = 0;
    uint16_t magic;
    uint8_t method, flags;
    mc_gzip_status s = little16(c, &magic, e);
    if (s != MC_GZIP_OK)
        return s;
    if (magic != 0x8b1fu)
        return result(e, MC_GZIP_SOURCE_FORMAT, "GZIP magic", true, "Not in GZIP format",
                      "java/util/zip/GZIPInputStream", "readHeader", -1);
    s = octet(c, &method, e);
    if (s != MC_GZIP_OK)
        return s;
    if (method != 8u)
        return result(e, MC_GZIP_SOURCE_FORMAT, "GZIP method", true,
                      "Unsupported compression method", "java/util/zip/GZIPInputStream",
                      "readHeader", -1);
    s = octet(c, &flags, e);
    if (s != MC_GZIP_OK)
        return s;
    s = skip_bytes(c, 6, e);
    if (s != MC_GZIP_OK)
        return s;
    size_t n = 10;
    if ((flags & 4u) != 0) {
        uint16_t extra;
        s = little16(c, &extra, e);
        if (s != MC_GZIP_OK)
            return s;
        s = skip_bytes(c, extra, e);
        if (s != MC_GZIP_OK)
            return s;
        n += (size_t)extra + 2u;
    }
    for (unsigned mask = 8u; mask <= 16u; mask *= 2u)
        if ((flags & mask) != 0) {
            uint8_t b;
            do {
                s = octet(c, &b, e);
                if (s != MC_GZIP_OK)
                    return s;
                if (n == INT32_MAX)
                    return result(e, MC_GZIP_LIMIT, "GZIP header int counter admission", false,
                                  NULL, NULL, NULL, -1);
                ++n;
            } while (b != 0);
        }
    if ((flags & 2u) != 0) {
        uint16_t value, expected = (uint16_t)*g->crc;
        s = little16(c, &value, e);
        if (s != MC_GZIP_OK)
            return s;
        if (value != expected)
            return result(e, MC_GZIP_SOURCE_FORMAT, "GZIP header CRC", true, "Corrupt GZIP header",
                          "java/util/zip/GZIPInputStream", "readHeader", -1);
        if (n > (size_t)INT32_MAX - 2u)
            return result(e, MC_GZIP_LIMIT, "GZIP header int counter admission", false, NULL, NULL,
                          NULL, -1);
        n += 2u;
    }
    *g->crc = 0;
    *used = n;
    return MC_GZIP_OK;
}

mc_gzip_status mc_gzip_input_construct(mc_gzip_input *g, mc_java_file_stream *input,
                                       mc_gzip_error *error) {
    mc_gzip_error local;
    mc_gzip_error *e = error == NULL ? &local : error;
    if (g == NULL)
        return result(e, MC_GZIP_ARGUMENT, "GZIP constructor null receiver", false, NULL, NULL,
                      NULL, -1);
    if (g->state.active)
        return result(e, MC_GZIP_BUSY, "GZIP active constructor", false, NULL, NULL, NULL, -1);
    if (g->state.phase != MC_GZIP_RAW)
        return result(e, MC_GZIP_STATE, "GZIP constructor phase", false, NULL, NULL, NULL, -1);
    g->state.active = true;
    g->state.phase = MC_GZIP_CONSTRUCTING;
    mc_gzip_status s = MC_GZIP_OK;
    void *memory = NULL;
    if (input != NULL) {
        mc_java_file_error fe;
        mc_java_file_status fs = mc_java_file_stream_retain(input, &fe);
        if (fs != MC_JAVA_FILE_OK) {
            s = file_error(e, fs, fe, "native argument hold");
            goto failed;
        }
        g->pending_input = input;
        mc_raw_inflater_error ie;
        mc_raw_inflater_status is =
            mc_raw_inflater_allocate(g->options.maximum_inflater_bytes, &g->pending_inflater, &ie);
        if (is != MC_RAW_INFLATER_OK) {
            s = inflate_error(e, is, ie, "Inflater(true) allocation");
            e->source_class = "java/util/zip/GZIPInputStream";
            e->source_method = "<init>(Ljava/io/InputStream;I)V";
            e->source_pc = 6;
            goto failed;
        }
        is = mc_raw_inflater_construct(g->pending_inflater, &ie);
        if (is != MC_RAW_INFLATER_OK) {
            s = inflate_error(e, is, ie, "Inflater(true) construction");
            e->source_class = "java/util/zip/GZIPInputStream";
            e->source_method = "<init>(Ljava/io/InputStream;I)V";
            e->source_pc = 11;
            goto failed;
        }
    }
    /* Real Filter.in publication precedes inherited arrays; raw argument and
     * raw Inflater were held separately before reaching this source store. */
    g->input = g->pending_input;
    g->pending_input = NULL;
    store(g, "java/io/FilterInputStream", 6);
    g->state.super_closed = false;
    store(g, "java/util/zip/InflaterInputStream", 7);
    g->state.reach_eof = false;
    store(g, "java/util/zip/InflaterInputStream", 12);
    g->state.uses_default_inflater = false;
    store(g, "java/util/zip/InflaterInputStream", 17);
    s = child(g, 1, 1, &memory, e, "java/util/zip/InflaterInputStream", 22);
    if (s != MC_GZIP_OK)
        goto failed;
    g->single = memory;
    store(g, "java/util/zip/InflaterInputStream", 24);
    s = child(g, 512, 1, &memory, e, "java/util/zip/InflaterInputStream", 31);
    if (s != MC_GZIP_OK)
        goto failed;
    g->skip = memory;
    store(g, "java/util/zip/InflaterInputStream", 33);
    if (input == NULL) {
        s = result(e, MC_GZIP_SOURCE_NULL, "InflaterInputStream null input branch", true, NULL,
                   "java/util/zip/InflaterInputStream",
                   "<init>(Ljava/io/InputStream;Ljava/util/zip/Inflater;I)V", 37);
        goto failed;
    }
    g->inflater = g->pending_inflater;
    g->pending_inflater = NULL;
    store(g, "java/util/zip/InflaterInputStream", 68);
    s = child(g, 512, 1, &memory, e, "java/util/zip/InflaterInputStream", 73);
    if (s != MC_GZIP_OK)
        goto failed;
    g->fill = memory;
    store(g, "java/util/zip/InflaterInputStream", 75);
    s = child(g, 1, sizeof(*g->crc), &memory, e, "java/util/zip/GZIPInputStream", 23);
    if (s != MC_GZIP_OK)
        goto failed;
    g->crc = memory;
    store(g, "java/util/zip/GZIPInputStream", 30);
    g->state.source_closed = false;
    store(g, "java/util/zip/GZIPInputStream", 35);
    s = child(g, 128, 1, &memory, e, "java/util/zip/GZIPInputStream", 42);
    if (s != MC_GZIP_OK)
        goto failed;
    g->tmp = memory;
    store(g, "java/util/zip/GZIPInputStream", 44);
    g->state.uses_default_inflater = true;
    store(g, "java/util/zip/GZIPInputStream", 49);
    gzip_cursor c = {.owner = g};
    size_t used;
    s = header(&c, &used, e);
    if (s != MC_GZIP_OK)
        goto failed;
    g->state.members = 1;
    g->state.phase = MC_GZIP_READY;
    g->state.active = false;
    result(&g->last_error, MC_GZIP_OK, NULL, false, NULL, NULL, NULL, -1);
    return result(e, MC_GZIP_OK, NULL, false, NULL, NULL, NULL, -1);
failed:
    g->state.phase = MC_GZIP_FAILED;
    g->state.active = false;
    g->last_error = *e;
    return s;
}

static mc_gzip_status trailer(mc_gzip_input *g, bool *eos, mc_gzip_error *e) {
    mc_raw_inflater_state inf;
    mc_raw_inflater_error ie;
    mc_raw_inflater_status is = mc_raw_inflater_state_read(g->inflater, &inf, &ie);
    if (is != MC_RAW_INFLATER_OK)
        return inflate_error(e, is, ie, "getRemaining");
    size_t n = inf.remaining;
    if (n > g->state.fill_length)
        return result(e, MC_GZIP_NATIVE_PROVIDER, "GZIP remaining-input invariant", false, NULL,
                      NULL, NULL, -1);
    gzip_cursor c = {.owner = g,
                     .position = g->state.fill_length - n,
                     .end = g->state.fill_length,
                     .buffered = n != 0};
    g->state.trailer_bytes_taken = 0;
    uint32_t value;
    mc_gzip_status s = little32(&c, &value, e);
    if (s != MC_GZIP_OK)
        return s;
    /* Short circuit: bad CRC must NOT consume ISIZE. */
    if (value != *g->crc)
        return result(e, MC_GZIP_SOURCE_FORMAT, "GZIP trailer CRC", true, "Corrupt GZIP trailer",
                      "java/util/zip/GZIPInputStream", "readTrailer", -1);
    s = little32(&c, &value, e);
    if (s != MC_GZIP_OK)
        return s;
    if (value != (uint32_t)inf.bytes_written)
        return result(e, MC_GZIP_SOURCE_FORMAT, "GZIP trailer size", true, "Corrupt GZIP trailer",
                      "java/util/zip/GZIPInputStream", "readTrailer", -1);
    size_t available;
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_file_stream_available(g->input, &available, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return file_error(e, fs, fe, "available");
    if (available > 0 || n > 26u) {
        size_t h = 0;
        s = header(&c, &h, e);
        if (s != MC_GZIP_OK) {
            if (e->source_exception) {
                g->state.ignored_next_header = true;
                g->ignored_error = *e;
                *eos = true;
                return MC_GZIP_OK;
            }
            return s;
        }
        if (g->state.members == g->options.maximum_members)
            return result(e, MC_GZIP_LIMIT, "GZIP member bound after actual next header", false,
                          NULL, NULL, NULL, -1);
        if (h > (size_t)INT32_MAX - 8u)
            return result(e, MC_GZIP_LIMIT, "GZIP next-header int addition admission", false, NULL,
                          NULL, NULL, -1);
        size_t m = 8u + h;
        ++g->state.members;
        is = mc_raw_inflater_reset(g->inflater, &ie);
        if (is != MC_RAW_INFLATER_OK)
            return inflate_error(e, is, ie, "reset");
        if (n > m) {
            is = mc_raw_inflater_set_input(g->inflater, g->fill + g->state.fill_length - n + m,
                                           n - m, &ie);
            if (is != MC_RAW_INFLATER_OK)
                return inflate_error(e, is, ie, "next member setInput");
        }
        *eos = false;
        return MC_GZIP_OK;
    }
    *eos = true;
    return MC_GZIP_OK;
}

static mc_gzip_status read_body(mc_gzip_input *g, uint8_t *bytes, size_t cap,
                                mc_nbt_input_result *out, mc_gzip_error *e) {
    if (g->state.source_closed || g->state.super_closed)
        return result(e, MC_GZIP_SOURCE_IO, "GZIP closed read", true, "Stream closed",
                      "java/util/zip/GZIPInputStream", "ensureOpen", -1);
    if (g->state.eos) {
        *out = (mc_nbt_input_result){0, true};
        return MC_GZIP_OK;
    }
    if ((bytes == NULL && cap != 0) || cap > INT32_MAX)
        return result(e, MC_GZIP_ARGUMENT, "GZIP buffer arguments", false, NULL, NULL, NULL, -1);
    if (cap == 0) {
        *out = (mc_nbt_input_result){0, false};
        return MC_GZIP_OK;
    }
    for (;;) {
        mc_raw_inflater_error ie;
        size_t produced = 0;
        mc_raw_inflater_status is = mc_raw_inflater_pull(g->inflater, bytes, cap, &produced, &ie);
        g->state.last_native_written = produced;
        *out = (mc_nbt_input_result){produced, false};
        uint64_t before = g->native_output;
        g->native_output += (uint64_t)produced;
        if (is != MC_RAW_INFLATER_OK)
            return inflate_error(e, is, ie, "read/inflate");
        if (before > g->options.maximum_output_bytes ||
            produced > g->options.maximum_output_bytes - (size_t)before)
            return result(e, MC_GZIP_LIMIT, "GZIP output bound after actual inflate", false, NULL,
                          NULL, NULL, -1);
        if (produced != 0) {
            *g->crc = (uint32_t)crc32(*g->crc, bytes, (uInt)produced);
            g->state.output_returned += (uint64_t)produced;
            return MC_GZIP_OK;
        }
        mc_raw_inflater_state inf;
        is = mc_raw_inflater_state_read(g->inflater, &inf, &ie);
        if (is != MC_RAW_INFLATER_OK)
            return inflate_error(e, is, ie, "read/state");
        if (inf.finished || inf.needs_dictionary) {
            g->state.reach_eof = true;
            bool eos;
            mc_gzip_status s = trailer(g, &eos, e);
            if (s != MC_GZIP_OK)
                return s;
            if (eos) {
                g->state.eos = true;
                *out = (mc_nbt_input_result){0, true};
                return MC_GZIP_OK;
            }
            continue;
        }
        if (inf.remaining == 0) {
            mc_nbt_input_result r;
            mc_gzip_status s = input_read(g, g->fill, 512, &r, e);
            if (s != MC_GZIP_OK)
                return s;
            g->state.source_fill_length = r.eof ? -1 : (int32_t)r.count;
            if (r.eof)
                return result(e, MC_GZIP_SOURCE_EOF, "GZIP fill EOF", true,
                              "Unexpected end of ZLIB input stream",
                              "java/util/zip/InflaterInputStream", "fill", -1);
            if (r.count == 0)
                return result(e, MC_GZIP_NATIVE_PROVIDER, "GZIP zero-progress descriptor read",
                              false, NULL, NULL, NULL, -1);
            g->state.fill_length = r.count;
            is = mc_raw_inflater_set_input(g->inflater, g->fill, r.count, &ie);
            if (is != MC_RAW_INFLATER_OK)
                return inflate_error(e, is, ie, "fill/setInput");
        } else
            return result(e, MC_GZIP_NATIVE_PROVIDER, "GZIP zero-progress inflater", false, NULL,
                          NULL, NULL, -1);
    }
}

mc_gzip_status mc_gzip_input_read(mc_gzip_input *g, uint8_t *bytes, size_t cap,
                                  mc_nbt_input_result *out, mc_gzip_error *error) {
    mc_gzip_error local;
    mc_gzip_error *e = error == NULL ? &local : error;
    if (g == NULL || out == NULL)
        return result(e, MC_GZIP_ARGUMENT, "GZIP read receiver/output", false, NULL, NULL, NULL,
                      -1);
    if (g->state.active)
        return result(e, MC_GZIP_BUSY, "GZIP active read", false, NULL, NULL, NULL, -1);
    if (g->state.phase != MC_GZIP_READY || g->state.retiring || g->native_failed)
        return result(e, MC_GZIP_STATE, "GZIP nonready read", false, NULL, NULL, NULL, -1);
    g->state.active = true;
    ++g->state.read_calls;
    g->state.last_native_written = 0;
    mc_gzip_status s = read_body(g, bytes, cap, out, e);
    g->state.active = false;
    if (s == MC_GZIP_OK)
        result(e, s, NULL, false, NULL, NULL, NULL, -1);
    else if (!e->source_exception)
        g->native_failed = true;
    g->last_error = *e;
    return s;
}

mc_gzip_status mc_gzip_input_read_byte(mc_gzip_input *g, int *out, mc_gzip_error *error) {
    mc_gzip_error local;
    mc_gzip_error *e = error == NULL ? &local : error;
    if (g == NULL || out == NULL)
        return result(e, MC_GZIP_ARGUMENT, "GZIP byte read receiver/output", false, NULL, NULL,
                      NULL, -1);
    if (g->state.active)
        return result(e, MC_GZIP_BUSY, "GZIP active byte read", false, NULL, NULL, NULL, -1);
    if (g->state.phase != MC_GZIP_READY || g->state.retiring || g->native_failed)
        return result(e, MC_GZIP_STATE, "GZIP nonready byte read", false, NULL, NULL, NULL, -1);
    g->state.active = true;
    ++g->state.read_calls;
    g->state.last_native_written = 0;
    mc_nbt_input_result r = {0};
    mc_gzip_status status = read_body(g, g->single, 1, &r, e);
    g->state.active = false;
    if (status == MC_GZIP_OK) {
        *out = r.eof ? -1 : (int)g->single[0];
        result(e, status, NULL, false, NULL, NULL, NULL, -1);
    } else if (!e->source_exception)
        g->native_failed = true;
    g->last_error = *e;
    return status;
}

mc_gzip_status mc_gzip_input_retain(mc_gzip_input *g, mc_gzip_error *e) {
    if (g == NULL)
        return result(e, MC_GZIP_ARGUMENT, "GZIP retain", false, NULL, NULL, NULL, -1);
    if (g->state.active || g->state.retiring)
        return result(e, MC_GZIP_BUSY, "GZIP active/terminal hold", false, NULL, NULL, NULL, -1);
    if (g->state.holds == SIZE_MAX)
        return result(e, MC_GZIP_LIMIT, "GZIP hold bound", false, NULL, NULL, NULL, -1);
    ++g->state.holds;
    return result(e, MC_GZIP_OK, NULL, false, NULL, NULL, NULL, -1);
}

static mc_gzip_status source_close(mc_gzip_input *g, mc_gzip_error *e) {
    if (g == NULL)
        return result(e, MC_GZIP_ARGUMENT, "GZIP source close receiver", false, NULL, NULL, NULL,
                      -1);
    if (g->state.active)
        return result(e, MC_GZIP_BUSY, "GZIP active source close", false, NULL, NULL, NULL, -1);
    if (g->state.phase != MC_GZIP_READY || g->state.retiring)
        return result(e, MC_GZIP_STATE, "GZIP source close phase", false, NULL, NULL, NULL, -1);
    if (g->state.source_closed)
        return result(e, MC_GZIP_OK, NULL, false, NULL, NULL, NULL, -1);
    g->state.active = true;
    if (!g->state.super_closed) {
        mc_raw_inflater_error ie;
        mc_raw_inflater_status is = mc_raw_inflater_end(g->inflater, &ie);
        if (is != MC_RAW_INFLATER_OK) {
            g->state.active = false;
            return inflate_error(e, is, ie, "close/end");
        }
        mc_java_file_error fe;
        mc_java_file_status fs = mc_java_file_stream_source_close(g->input, &fe);
        if (fs != MC_JAVA_FILE_OK) {
            g->state.active = false;
            return file_error(e, fs, fe, "close");
        }
        g->state.super_closed = true;
    }
    g->state.eos = true;
    g->state.source_closed = true;
    g->state.active = false;
    return result(e, MC_GZIP_OK, NULL, false, NULL, NULL, NULL, -1);
}

mc_gzip_status mc_gzip_input_source_close(mc_gzip_input *g, mc_gzip_error *error) {
    mc_gzip_error local;
    mc_gzip_error *e = error == NULL ? &local : error;
    mc_gzip_status status = source_close(g, e);
    if (g != NULL && !g->state.active)
        g->last_close_error = *e;
    return status;
}

mc_gzip_status mc_gzip_input_release(mc_gzip_input **slot, mc_gzip_error *e) {
    if (slot == NULL)
        return result(e, MC_GZIP_ARGUMENT, "GZIP retirement slot", false, NULL, NULL, NULL, -1);
    mc_gzip_input *g = *slot;
    if (g == NULL)
        return result(e, MC_GZIP_OK, NULL, false, NULL, NULL, NULL, -1);
    if (g->state.active)
        return result(e, MC_GZIP_BUSY, "GZIP active retirement", false, NULL, NULL, NULL, -1);
    if (g->state.holds > 1) {
        --g->state.holds;
        *slot = NULL;
        return result(e, MC_GZIP_OK, NULL, false, NULL, NULL, NULL, -1);
    }
    g->state.active = true;
    g->state.retiring = true;
    g->state.phase = MC_GZIP_RETIRING;
    mc_raw_inflater_error ie;
    mc_raw_inflater_status is = mc_raw_inflater_release(&g->pending_inflater, &ie);
    if (is == MC_RAW_INFLATER_OK)
        is = mc_raw_inflater_release(&g->inflater, &ie);
    if (is != MC_RAW_INFLATER_OK) {
        g->state.active = false;
        return inflate_error(e, is, ie, "native inflater retirement");
    }
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_file_stream_release(&g->pending_input, &fe);
    if (fs == MC_JAVA_FILE_OK)
        fs = mc_java_file_stream_release(&g->input, &fe);
    if (fs != MC_JAVA_FILE_OK) {
        g->state.active = false;
        mc_gzip_status status = fs == MC_JAVA_FILE_BUSY ? MC_GZIP_BUSY : MC_GZIP_NATIVE_PROVIDER;
        result(e, status, "native input retirement", false, NULL, NULL, NULL, -1);
        if (e != NULL)
            e->file = fe;
        return status;
    }
    free(g->single);
    free(g->skip);
    free(g->fill);
    free(g->tmp);
    free(g->crc);
    *slot = NULL;
    free(g);
    return result(e, MC_GZIP_OK, NULL, false, NULL, NULL, NULL, -1);
}

mc_gzip_status mc_gzip_input_state_read(const mc_gzip_input *g, mc_gzip_state *out,
                                        mc_gzip_error *e) {
    if (g == NULL || out == NULL)
        return result(e, MC_GZIP_ARGUMENT, "GZIP state", false, NULL, NULL, NULL, -1);
    *out = g->state;
    out->source_in = g->input;
    out->pending_input = g->pending_input;
    out->source_inf = g->inflater;
    out->pending_inflater = g->pending_inflater;
    out->single_byte = g->single;
    out->skip_buffer = g->skip;
    out->fill_buffer = g->fill;
    out->tmp_buffer = g->tmp;
    out->crc = g->crc;
    return result(e, MC_GZIP_OK, NULL, false, NULL, NULL, NULL, -1);
}

mc_gzip_status mc_gzip_input_last_error(const mc_gzip_input *g, mc_gzip_error *out) {
    if (g == NULL || out == NULL)
        return MC_GZIP_ARGUMENT;
    *out = g->last_error;
    return MC_GZIP_OK;
}

mc_gzip_status mc_gzip_input_last_close_error(const mc_gzip_input *g, mc_gzip_error *out) {
    if (g == NULL || out == NULL)
        return MC_GZIP_ARGUMENT;
    *out = g->last_close_error;
    return MC_GZIP_OK;
}

static mc_nbt_input_status byte_read(void *context, uint8_t *bytes, size_t n,
                                     mc_nbt_input_result *out, mc_nbt_input_error *error) {
    mc_gzip_input *g = context;
    mc_gzip_error e;
    mc_nbt_input_result r = {0};
    mc_gzip_status s = mc_gzip_input_read(g, bytes, n, &r, &e);
    *out = r;
    if (s == MC_GZIP_OK) {
        if (error != NULL)
            *error = (mc_nbt_input_error){0};
        return MC_NBT_INPUT_OK;
    }
    if (error != NULL) {
        *error = (mc_nbt_input_error){.transport_status = (int)s,
                                      .system_error = e.file.system_error,
                                      .source_exception = e.source_exception,
                                      .operation = e.operation,
                                      .native_written_on_error =
                                          g == NULL ? 0 : g->state.last_native_written};
        text(error->message, e.message);
    }
    return MC_NBT_INPUT_FAILED;
}

static mc_nbt_input_status byte_one(void *context, int *out, mc_nbt_input_error *error) {
    mc_gzip_input *g = context;
    mc_gzip_error e;
    mc_gzip_status s = mc_gzip_input_read_byte(g, out, &e);
    if (s == MC_GZIP_OK) {
        if (error != NULL)
            *error = (mc_nbt_input_error){0};
        return MC_NBT_INPUT_OK;
    }
    if (error != NULL) {
        *error = (mc_nbt_input_error){.transport_status = (int)s,
                                      .system_error = e.file.system_error,
                                      .source_exception = e.source_exception,
                                      .operation = e.operation,
                                      .native_written_on_error =
                                          g == NULL ? 0 : g->state.last_native_written};
        text(error->message, e.message);
    }
    return MC_NBT_INPUT_FAILED;
}

static mc_nbt_input_status byte_close(void *context, mc_nbt_input_error *error) {
    mc_gzip_error e;
    mc_gzip_status s = mc_gzip_input_source_close(context, &e);
    if (s == MC_GZIP_OK) {
        if (error != NULL)
            *error = (mc_nbt_input_error){0};
        return MC_NBT_INPUT_OK;
    }
    if (error != NULL) {
        *error = (mc_nbt_input_error){.transport_status = (int)s,
                                      .system_error = e.file.system_error,
                                      .source_exception = e.source_exception,
                                      .operation = e.operation};
        text(error->message, e.message);
    }
    return MC_NBT_INPUT_FAILED;
}

mc_nbt_byte_input mc_gzip_input_byte_port(mc_gzip_input *g) {
    return (mc_nbt_byte_input){g, byte_one, byte_read, byte_close};
}
