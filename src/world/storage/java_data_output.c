#include "java_output_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static mc_java_output_status finish(mc_java_data_output *d, mc_java_output_error *e,
                                    mc_java_output_error *out) {
    if (e->status != MC_JAVA_OUTPUT_OK)
        d->failure = *e;
    d->active = false;
    if (out != NULL)
        *out = *e;
    return e->status;
}

static mc_java_output_status enter(mc_java_data_output *d, mc_java_output_error *e) {
    if (d == NULL || d->active || d->phase != MC_JAVA_OUTPUT_READY)
        return mc_output_fail(e,
                              d == NULL   ? MC_JAVA_OUTPUT_ARGUMENT
                              : d->active ? MC_JAVA_OUTPUT_BUSY
                                          : MC_JAVA_OUTPUT_STATE,
                              NULL, NULL, NULL, -1, "native DOS receiver admission", 0, false);
    d->active = true;
    return MC_JAVA_OUTPUT_OK;
}

mc_java_output_status mc_java_data_output_allocate(const mc_java_output_options *options,
                                                   mc_java_data_output **out,
                                                   mc_java_output_error *error) {
    mc_java_output_error e = {0};
    const mc_java_output_options o = options == NULL ? mc_java_output_default_options() : *options;
    if (out == NULL)
        mc_output_fail(&e, MC_JAVA_OUTPUT_ARGUMENT, NULL, NULL, NULL, -1, "native DOS output", 0,
                       false);
    else if (o.max_owner_bytes < sizeof(mc_java_data_output))
        mc_output_fail(&e, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1, "native DOS owner bound", 0,
                       false);
    else {
        mc_java_data_output *d = calloc(1, sizeof(*d));
        if (d == NULL)
            mc_output_fail(&e, MC_JAVA_OUTPUT_ALLOCATION, NULL, NULL, NULL, -1, "native DOS owner",
                           0, false);
        else {
            d->options = o;
            d->holds = 1;
            d->bytes = sizeof(*d);
            d->last_store = -1;
            *out = d;
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_java_output_status mc_java_data_output_construct(mc_java_data_output *d, mc_java_file_output *f,
                                                    mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (d == NULL || d->active || d->phase != MC_JAVA_OUTPUT_RAW) {
        mc_output_fail(&e, d != NULL && d->active ? MC_JAVA_OUTPUT_BUSY : MC_JAVA_OUTPUT_STATE,
                       NULL, NULL, NULL, -1, "one-shot native DOS constructor", 0, false);
        if (out != NULL)
            *out = e;
        return e.status;
    }
    d->active = true;
    d->phase = MC_JAVA_OUTPUT_CONSTRUCTING;
    bool filter_completed = false;
    if (mc_output_lock_create(&d->lock, &d->bytes, d->options.max_owner_bytes, &e,
                              "java/io/FilterOutputStream",
                              "(Ljava/io/OutputStream;)V") != MC_JAVA_OUTPUT_OK)
        goto done;
    d->last_class = "java/io/FilterOutputStream";
    d->last_method = "<init>";
    d->last_store = 12;
    if (f != NULL && mc_java_file_output_retain(f, &e) != MC_JAVA_OUTPUT_OK) {
        mc_output_caller(&e, "java/io/FilterOutputStream", "<init>", "(Ljava/io/OutputStream;)V",
                         17);
        goto done;
    }
    d->out = f;
    d->last_store = 17; /* Source NULL out is legal in this constructor. */
    filter_completed = true;
    d->bytearr = NULL;
    d->last_class = "java/io/DataOutputStream";
    d->last_store = 7;
    if (d->bytes > d->options.max_owner_bytes || 8 > d->options.max_owner_bytes - d->bytes) {
        mc_output_fail(&e, MC_JAVA_OUTPUT_LIMIT, "java/io/DataOutputStream", "<init>",
                       "(Ljava/io/OutputStream;)V", 13, "native writeBuffer bound", 0, false);
        goto done;
    }
    d->write_buffer = calloc(8, 1);
    if (d->write_buffer == NULL) {
        mc_output_fail(&e, MC_JAVA_OUTPUT_ALLOCATION, "java/io/DataOutputStream", "<init>",
                       "(Ljava/io/OutputStream;)V", 13, "native writeBuffer allocation", 0, false);
        goto done;
    }
    d->bytes += 8;
    d->last_store = 15;
done:
    if (e.status != MC_JAVA_OUTPUT_OK && !filter_completed)
        mc_output_caller(&e, "java/io/DataOutputStream", "<init>", "(Ljava/io/OutputStream;)V", 2);
    d->phase = e.status == MC_JAVA_OUTPUT_OK ? MC_JAVA_OUTPUT_READY : MC_JAVA_OUTPUT_FAILED;
    return finish(d, &e, out);
}

mc_java_output_status mc_java_data_output_retain(mc_java_data_output *d,
                                                 mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (d == NULL || d->active || d->phase == MC_JAVA_OUTPUT_RETIRING || d->holds == SIZE_MAX)
        mc_output_fail(&e,
                       d == NULL              ? MC_JAVA_OUTPUT_ARGUMENT
                       : d->active            ? MC_JAVA_OUTPUT_BUSY
                       : d->holds == SIZE_MAX ? MC_JAVA_OUTPUT_LIMIT
                                              : MC_JAVA_OUTPUT_STATE,
                       NULL, NULL, NULL, -1, "native DOS occurrence retain", 0, false);
    else
        ++d->holds;
    if (out != NULL)
        *out = e;
    return e.status;
}

mc_java_output_status mc_java_data_output_write_long(mc_java_data_output *d, int64_t value,
                                                     mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (enter(d, &e) != MC_JAVA_OUTPUT_OK) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    /* Exact source byte stores11/23/35/47/59/71/84/96 before dereferencing
     * nullable Filter.out at invocation108. No eager closed/out check. */
    const uint64_t bits = (uint64_t)value;
    for (size_t i = 0; i < 8; ++i)
        d->write_buffer[i] = (uint8_t)(bits >> (56u - 8u * (unsigned)i));
    if (d->out == NULL)
        mc_output_fail(&e, MC_JAVA_OUTPUT_SOURCE_NULL, "java/io/DataOutputStream", "writeLong",
                       "(J)V", 108, "actual null OutputStream receiver", 0, true);
    else {
        mc_output_file_write_long_bytes(d->out, d->write_buffer, &e);
        if (e.status != MC_JAVA_OUTPUT_OK)
            mc_output_caller(&e, "java/io/DataOutputStream", "writeLong", "(J)V", 108);
        else {
            /* Native overflow-free equivalent of incCount's Java wrap test.
             * written is nonnegative; max-8 exactly preserves saturation. */
            d->written = d->written > INT32_MAX - 8 ? INT32_MAX : d->written + 8;
            d->last_class = "java/io/DataOutputStream";
            d->last_method = "incCount";
            d->last_store = 16;
        }
    }
    return finish(d, &e, out);
}

static mc_java_output_status flush(mc_java_data_output *d, mc_java_output_error *e) {
    if (d->out == NULL)
        return mc_output_fail(e, MC_JAVA_OUTPUT_SOURCE_NULL, "java/io/FilterOutputStream", "flush",
                              "()V", 4, "actual null OutputStream flush receiver", 0, true);
    const mc_java_output_status s = mc_java_file_output_flush(d->out, e);
    if (s != MC_JAVA_OUTPUT_OK)
        mc_output_caller(e, "java/io/FilterOutputStream", "flush", "()V", 4);
    return s;
}

mc_java_output_status mc_java_data_output_flush(mc_java_data_output *d, mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (enter(d, &e) != MC_JAVA_OUTPUT_OK) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    (void)flush(d, &e);
    return finish(d, &e, out);
}

static mc_java_output_status close_child(mc_java_data_output *d, mc_java_output_error *e, int pc) {
    if (d->out == NULL)
        return mc_output_fail(e, MC_JAVA_OUTPUT_SOURCE_NULL, "java/io/FilterOutputStream", "close",
                              "()V", pc, "actual null OutputStream close receiver", 0, true);
    const mc_java_output_status s = mc_java_file_output_source_close(d->out, e);
    if (s != MC_JAVA_OUTPUT_OK)
        mc_output_caller(e, "java/io/FilterOutputStream", "close", "()V", pc);
    return s;
}

mc_java_output_status mc_java_data_output_source_close(mc_java_data_output *d,
                                                       mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (enter(d, &e) != MC_JAVA_OUTPUT_OK) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    ++d->source_closes;
    if (d->closed)
        return finish(d, &e, out); /* Actual Filter close7. */
    if (mtx_lock(&d->lock->mutex) != thrd_success) {
        mc_output_fail(&e, MC_JAVA_OUTPUT_NATIVE_FAILURE, NULL, NULL, NULL, -1,
                       "native DOS close lock", 0, false);
        return finish(d, &e, out);
    }
    d->closed = true;
    d->last_class = "java/io/FilterOutputStream";
    d->last_method = "close";
    d->last_store = 27;
    (void)mtx_unlock(&d->lock->mutex);
    const mc_java_output_status fs = flush(d, &e); /* Invoke43 after closed27. */
    if (fs != MC_JAVA_OUTPUT_OK) {
        mc_output_caller(&e, "java/io/FilterOutputStream", "close", "()V", 43);
        if (!e.cause.source_exception)
            return finish(d, &e, out); /* Native fault is not Throwable. */
        mc_java_output_error closing = {0};
        const mc_java_output_status cs = close_child(d, &closing, 131);
        if (cs != MC_JAVA_OUTPUT_OK) {
            if (closing.cause.source_exception) {
                closing.suppressed = e.cause;
                closing.suppressed_caller_count = e.caller_count;
                memcpy(closing.suppressed_callers, e.callers, sizeof(e.callers));
                closing.suppressed_file = e.file;
            }
            e = closing; /* In this concrete profile, close's new failure wins. */
        }
    } else
        (void)close_child(d, &e, 54);
    return finish(d, &e, out);
}

mc_java_output_status mc_java_data_output_release(mc_java_data_output **slot,
                                                  mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (slot == NULL)
        mc_output_fail(&e, MC_JAVA_OUTPUT_ARGUMENT, NULL, NULL, NULL, -1, "native DOS release slot",
                       0, false);
    else if (*slot != NULL) {
        mc_java_data_output *d = *slot;
        if (d->active || d->phase == MC_JAVA_OUTPUT_CONSTRUCTING)
            mc_output_fail(&e, MC_JAVA_OUTPUT_BUSY, NULL, NULL, NULL, -1,
                           "native DOS active release", 0, false);
        else if (d->holds > 1) {
            --d->holds;
            *slot = NULL;
        } else {
            d->active = true;
            d->phase = MC_JAVA_OUTPUT_RETIRING;
            if (mc_java_file_output_release(&d->out, &e) != MC_JAVA_OUTPUT_OK)
                return finish(d, &e, out);
            free(d->write_buffer);
            mc_output_lock_free(&d->lock);
            free(d);
            *slot = NULL;
        }
    }
    if (out != NULL)
        *out = e;
    return e.status;
}

bool mc_java_data_output_inspect(const mc_java_data_output *d, mc_java_data_output_state *out) {
    if (d == NULL || out == NULL)
        return false;
    *out = (mc_java_data_output_state){
        .phase = d->phase,
        .holds = d->holds,
        .owner_bytes = d->bytes,
        .source_close_calls = d->source_closes,
        .out = d->out,
        .close_lock = d->lock != NULL && d->lock->ready ? d->lock : NULL,
        .pending_close_lock = d->lock != NULL && !d->lock->ready ? d->lock : NULL,
        .write_buffer = d->write_buffer,
        .bytearr = d->bytearr,
        .written = d->written,
        .active = d->active,
        .closed = d->closed,
        .last_store_pc = d->last_store,
        .last_store_class = d->last_class,
        .last_store_method = d->last_method,
        .failure = &d->failure};
    return true;
}

mc_java_output_status mc_java_data_output_write_array(mc_java_data_output *d,
                                                      const mc_java_byte_array_view *array,
                                                      int32_t offset, int32_t length,
                                                      mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (enter(d, &e) != MC_JAVA_OUTPUT_OK) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    /* Actual getfield out1 precedes invoked FOS null/bounds checks at7. */
    mc_java_file_output *captured = d->out;
    if (captured == NULL)
        mc_output_fail(&e, MC_JAVA_OUTPUT_SOURCE_NULL, "java/io/DataOutputStream", "write",
                       "([BII)V", 7, "actual null OutputStream receiver", 0, true);
    else if (mc_java_file_output_write_array(captured, array, offset, length, &e) !=
             MC_JAVA_OUTPUT_OK)
        mc_output_caller(&e, "java/io/DataOutputStream", "write", "([BII)V", 7);
    else {
        d->written = length > INT32_MAX - d->written ? INT32_MAX : d->written + length;
        d->last_class = "java/io/DataOutputStream";
        d->last_method = "incCount";
        d->last_store = 16;
    }
    return finish(d, &e, out);
}
