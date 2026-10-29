#include "mc/storage/compression/inflater.h"
#include "kernel.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mc_raw_inflater {
    mc_raw_inflater_phase phase;
    bool active, armed, finished, dictionary;
    compression_allocator allocator;
    z_stream stream;
    uint64_t read, written;
};

static mc_raw_inflater_status result(mc_raw_inflater_error *e, mc_raw_inflater_status s,
                                     const char *op, int z, mc_compression_status child) {
    if (e != NULL)
        *e = (mc_raw_inflater_error){
            .status = s, .compression = child, .operation = op, .backend_result = z};
    return s;
}

static mc_raw_inflater_status backend(mc_raw_inflater *r, mc_raw_inflater_error *e, const char *op,
                                      int z) {
    mc_compression_status c = compression_backend_status(z, &r->allocator);
    mc_raw_inflater_status s = c == MC_COMPRESSION_LIMIT               ? MC_RAW_INFLATER_LIMIT
                               : c == MC_COMPRESSION_ALLOCATION_FAILED ? MC_RAW_INFLATER_ALLOCATION
                               : c == MC_COMPRESSION_INVALID_STREAM    ? MC_RAW_INFLATER_DATA_ERROR
                                                                       : MC_RAW_INFLATER_BACKEND;
    result(e, s, op, z, c);
    if (e != NULL && r->stream.msg != NULL) {
        size_t n = strlen(r->stream.msg);
        e->message_truncated = n >= sizeof(e->message);
        size_t copied = n < sizeof(e->message) - 1u ? n : sizeof(e->message) - 1u;
        memcpy(e->message, r->stream.msg, copied);
        e->message[copied] = 0;
    }
    return s;
}

mc_raw_inflater_status mc_raw_inflater_allocate(size_t maximum, mc_raw_inflater **out,
                                                mc_raw_inflater_error *e) {
    if (out == NULL)
        return result(e, MC_RAW_INFLATER_ARGUMENT, "raw inflater output", 0, MC_COMPRESSION_OK);
    if (maximum < sizeof(mc_raw_inflater))
        return result(e, MC_RAW_INFLATER_LIMIT, "raw inflater owner bound", 0,
                      MC_COMPRESSION_LIMIT);
    mc_raw_inflater *r = calloc(1, sizeof(*r));
    if (r == NULL)
        return result(e, MC_RAW_INFLATER_ALLOCATION, "raw inflater owner", 0,
                      MC_COMPRESSION_ALLOCATION_FAILED);
    r->allocator.limit = maximum - sizeof(*r);
    r->stream.zalloc = compression_zalloc;
    r->stream.zfree = compression_zfree;
    r->stream.opaque = &r->allocator;
    *out = r;
    return result(e, MC_RAW_INFLATER_OK, NULL, 0, MC_COMPRESSION_OK);
}

static mc_raw_inflater_status construct(mc_raw_inflater *r, bool nowrap, mc_raw_inflater_error *e) {
    if (r == NULL)
        return result(e, MC_RAW_INFLATER_ARGUMENT, "raw inflater null receiver", 0,
                      MC_COMPRESSION_OK);
    if (r->active)
        return result(e, MC_RAW_INFLATER_BUSY, "raw inflater active constructor", 0,
                      MC_COMPRESSION_OK);
    if (r->phase != MC_RAW_INFLATER_RAW)
        return result(e, MC_RAW_INFLATER_STATE, "raw inflater constructor phase", 0,
                      MC_COMPRESSION_OK);
    r->active = true;
    int z = inflateInit2(&r->stream, nowrap ? -15 : 15);
    r->active = false;
    if (z != Z_OK) {
        r->phase = MC_RAW_INFLATER_FAILED;
        return backend(r, e, nowrap ? "inflateInit2(-15)" : "inflateInit2(15)", z);
    }
    r->armed = true;
    r->phase = MC_RAW_INFLATER_READY;
    return result(e, MC_RAW_INFLATER_OK, NULL, z, MC_COMPRESSION_OK);
}

mc_raw_inflater_status mc_raw_inflater_construct(mc_raw_inflater *r, mc_raw_inflater_error *e) {
    return construct(r, true, e);
}

mc_raw_inflater_status mc_raw_inflater_construct_wrapped(mc_raw_inflater *r,
                                                         mc_raw_inflater_error *e) {
    return construct(r, false, e);
}

static mc_raw_inflater_status ready(mc_raw_inflater *r, mc_raw_inflater_error *e) {
    if (r == NULL)
        return result(e, MC_RAW_INFLATER_ARGUMENT, "raw inflater null receiver", 0,
                      MC_COMPRESSION_OK);
    if (r->active)
        return result(e, MC_RAW_INFLATER_BUSY, "raw inflater active operation", 0,
                      MC_COMPRESSION_OK);
    if (r->phase != MC_RAW_INFLATER_READY || !r->armed)
        return result(e, MC_RAW_INFLATER_STATE, "raw inflater nonready operation", 0,
                      MC_COMPRESSION_OK);
    return MC_RAW_INFLATER_OK;
}

mc_raw_inflater_status mc_raw_inflater_set_input(mc_raw_inflater *r, const uint8_t *data, size_t n,
                                                 mc_raw_inflater_error *e) {
    mc_raw_inflater_status s = ready(r, e);
    if (s != MC_RAW_INFLATER_OK)
        return s;
    if ((data == NULL && n != 0) || n > UINT_MAX)
        return result(e, MC_RAW_INFLATER_ARGUMENT, "raw inflater input argument", 0,
                      MC_COMPRESSION_OK);
    if (r->stream.avail_in != 0)
        return result(e, MC_RAW_INFLATER_STATE, "raw inflater unread input replacement", 0,
                      MC_COMPRESSION_OK);
    r->stream.next_in = n == 0 ? Z_NULL : (Bytef *)data;
    r->stream.avail_in = (uInt)n;
    return result(e, MC_RAW_INFLATER_OK, NULL, 0, MC_COMPRESSION_OK);
}

mc_raw_inflater_status mc_raw_inflater_pull(mc_raw_inflater *r, uint8_t *out, size_t cap,
                                            size_t *written, mc_raw_inflater_error *e) {
    mc_raw_inflater_status s = ready(r, e);
    if (s != MC_RAW_INFLATER_OK)
        return s;
    if (written == NULL || (out == NULL && cap != 0) || cap > UINT_MAX)
        return result(e, MC_RAW_INFLATER_ARGUMENT, "raw inflater pull arguments", 0,
                      MC_COMPRESSION_OK);
    if (cap == 0) {
        *written = 0;
        return result(e, MC_RAW_INFLATER_OK, NULL, 0, MC_COMPRESSION_OK);
    }
    r->active = true;
    uInt before = r->stream.avail_in;
    r->stream.next_out = out;
    r->stream.avail_out = (uInt)cap;
    int z = inflate(&r->stream, Z_NO_FLUSH);
    size_t produced = cap - r->stream.avail_out;
    r->read += (uint64_t)(before - r->stream.avail_in);
    r->written += (uint64_t)produced;
    r->finished = z == Z_STREAM_END;
    r->dictionary = z == Z_NEED_DICT;
    r->active = false;
    *written = produced;
    if (z != Z_OK && z != Z_STREAM_END && z != Z_BUF_ERROR && z != Z_NEED_DICT)
        return backend(r, e, "inflate", z);
    return result(e, MC_RAW_INFLATER_OK, NULL, z, MC_COMPRESSION_OK);
}

mc_raw_inflater_status mc_raw_inflater_reset(mc_raw_inflater *r, mc_raw_inflater_error *e) {
    mc_raw_inflater_status s = ready(r, e);
    if (s != MC_RAW_INFLATER_OK)
        return s;
    r->active = true;
    int z = inflateReset(&r->stream);
    r->active = false;
    if (z != Z_OK)
        return backend(r, e, "inflateReset", z);
    r->stream.next_in = Z_NULL;
    r->stream.avail_in = 0;
    r->stream.next_out = Z_NULL;
    r->stream.avail_out = 0;
    r->read = 0;
    r->written = 0;
    r->finished = false;
    r->dictionary = false;
    return result(e, MC_RAW_INFLATER_OK, NULL, z, MC_COMPRESSION_OK);
}

mc_raw_inflater_status mc_raw_inflater_end(mc_raw_inflater *r, mc_raw_inflater_error *e) {
    if (r == NULL)
        return result(e, MC_RAW_INFLATER_ARGUMENT, "raw inflater null end", 0, MC_COMPRESSION_OK);
    if (r->active)
        return result(e, MC_RAW_INFLATER_BUSY, "raw inflater active end", 0, MC_COMPRESSION_OK);
    if (!r->armed)
        return result(e, MC_RAW_INFLATER_OK, NULL, 0, MC_COMPRESSION_OK);
    r->active = true;
    r->armed = false;
    int z = inflateEnd(&r->stream);
    r->active = false;
    if (z != Z_OK) {
        r->phase = MC_RAW_INFLATER_FAILED;
        return backend(r, e, "inflateEnd", z);
    }
    return result(e, MC_RAW_INFLATER_OK, NULL, z, MC_COMPRESSION_OK);
}

mc_raw_inflater_status mc_raw_inflater_state_read(const mc_raw_inflater *r,
                                                  mc_raw_inflater_state *out,
                                                  mc_raw_inflater_error *e) {
    if (r == NULL || out == NULL)
        return result(e, MC_RAW_INFLATER_ARGUMENT, "raw inflater state", 0, MC_COMPRESSION_OK);
    *out = (mc_raw_inflater_state){r->phase,
                                   r->active,
                                   r->armed,
                                   r->finished,
                                   r->dictionary,
                                   r->stream.avail_in,
                                   sizeof(*r) + r->allocator.live_bytes,
                                   r->read,
                                   r->written};
    return result(e, MC_RAW_INFLATER_OK, NULL, 0, MC_COMPRESSION_OK);
}

mc_raw_inflater_status mc_raw_inflater_release(mc_raw_inflater **slot, mc_raw_inflater_error *e) {
    if (slot == NULL)
        return result(e, MC_RAW_INFLATER_ARGUMENT, "raw inflater retirement slot", 0,
                      MC_COMPRESSION_OK);
    mc_raw_inflater *r = *slot;
    if (r == NULL)
        return result(e, MC_RAW_INFLATER_OK, NULL, 0, MC_COMPRESSION_OK);
    if (r->active)
        return result(e, MC_RAW_INFLATER_BUSY, "raw inflater active retirement", 0,
                      MC_COMPRESSION_OK);
    r->phase = MC_RAW_INFLATER_RETIRING;
    mc_raw_inflater_status s = mc_raw_inflater_end(r, e);
    if (s != MC_RAW_INFLATER_OK)
        return s;
    compression_release_all(&r->allocator);
    *slot = NULL;
    free(r);
    return result(e, MC_RAW_INFLATER_OK, NULL, 0, MC_COMPRESSION_OK);
}
