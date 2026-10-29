#include "java_file_native.h"
#include "java_output_internal.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

mc_java_output_status mc_output_fail(mc_java_output_error *e, mc_java_output_status s,
                                     const char *c, const char *m, const char *d, int pc,
                                     const char *op, int code, bool source) {
    if (e != NULL) {
        e->status = s;
        e->cause = (mc_java_output_cause){
            .status = s,
            .site = {c, m, d, pc},
            .operation = op,
            .exception_class =
                !source                           ? NULL
                : s == MC_JAVA_OUTPUT_SOURCE_NULL ? "java/lang/NullPointerException"
                : s == MC_JAVA_OUTPUT_SOURCE_IO
                    ? (m != NULL && (strcmp(m, "open") == 0 || strcmp(m, "<init>") == 0)
                           ? "java/io/FileNotFoundException"
                           : "java/io/IOException")
                    : NULL,
            .system_error = code,
            .source_exception = source};
    }
    return s;
}

void mc_output_caller(mc_java_output_error *e, const char *c, const char *m, const char *d,
                      int pc) {
    if (e != NULL && e->caller_count < 4)
        e->callers[e->caller_count++] = (mc_java_output_site){c, m, d, pc};
}

mc_java_output_status mc_output_file_error(mc_java_output_error *e, mc_java_file_status s,
                                           mc_java_file_error child, int pc) {
    const mc_java_output_status mapped =
        s == MC_JAVA_FILE_BUSY                   ? MC_JAVA_OUTPUT_BUSY
        : s == MC_JAVA_FILE_LIMIT                ? MC_JAVA_OUTPUT_LIMIT
        : s == MC_JAVA_FILE_ALLOCATION_FAILED    ? MC_JAVA_OUTPUT_ALLOCATION
        : s == MC_JAVA_FILE_UNSUPPORTED_PROVIDER ? MC_JAVA_OUTPUT_NATIVE_PROVIDER
        : s == MC_JAVA_FILE_IO_ERROR             ? MC_JAVA_OUTPUT_SOURCE_IO
                                                 : MC_JAVA_OUTPUT_STATE;
    mc_output_fail(e, mapped, "java/io/FileOutputStream", "<init>", "(Ljava/io/File;Z)V", pc,
                   child.operation, child.system_error, s == MC_JAVA_FILE_IO_ERROR);
    if (e != NULL)
        e->file = child;
    return mapped;
}

mc_java_output_options mc_java_output_default_options(void) {
    return (mc_java_output_options){64u * 1024u, UINT64_MAX};
}

mc_java_output_status mc_output_lock_create(mc_output_lock **slot, size_t *bytes, size_t max,
                                            mc_java_output_error *e, const char *owner,
                                            const char *descriptor) {
    if (*bytes > max || sizeof(mc_output_lock) > max - *bytes)
        return mc_output_fail(e, MC_JAVA_OUTPUT_LIMIT, owner, "<init>", descriptor, 5,
                              "native close-lock allocation bound", 0, false);
    mc_output_lock *lock = calloc(1, sizeof(*lock));
    if (lock == NULL)
        return mc_output_fail(e, MC_JAVA_OUTPUT_ALLOCATION, owner, "<init>", descriptor, 5,
                              "native close-lock allocation", 0, false);
    *slot = lock; /* Retained native candidate even if native mutex admission fails. */
    *bytes += sizeof(*lock);
    if (mtx_init(&lock->mutex, mtx_plain) != thrd_success)
        return mc_output_fail(e, MC_JAVA_OUTPUT_NATIVE_FAILURE, owner, "<init>", descriptor, 5,
                              "native close-lock initialization", 0, false);
    lock->ready = true;
    return MC_JAVA_OUTPUT_OK;
}

void mc_output_lock_free(mc_output_lock **slot) {
    if (*slot != NULL) {
        if ((*slot)->ready)
            mtx_destroy(&(*slot)->mutex);
        free(*slot);
        *slot = NULL;
    }
}

static mc_java_output_status finish(mc_java_file_output *f, mc_java_output_error *e,
                                    mc_java_output_error *out) {
    if (e->status != MC_JAVA_OUTPUT_OK)
        f->failure = *e;
    f->active = false;
    if (out != NULL)
        *out = *e;
    return e->status;
}

static mc_java_output_status admit(mc_java_file_output *f, mc_java_output_error *e) {
    if (f == NULL || f->active || f->phase != MC_JAVA_OUTPUT_READY)
        return mc_output_fail(e,
                              f == NULL   ? MC_JAVA_OUTPUT_ARGUMENT
                              : f->active ? MC_JAVA_OUTPUT_BUSY
                                          : MC_JAVA_OUTPUT_STATE,
                              NULL, NULL, NULL, -1, "native FOS receiver admission", 0, false);
    return MC_JAVA_OUTPUT_OK;
}

mc_java_output_status mc_java_file_output_allocate(mc_java_files *files,
                                                   const mc_java_output_options *options,
                                                   mc_java_file_output **out,
                                                   mc_java_output_error *error) {
    mc_java_output_error e = {0};
    const mc_java_output_options o = options == NULL ? mc_java_output_default_options() : *options;
    if (files == NULL || out == NULL)
        mc_output_fail(&e, MC_JAVA_OUTPUT_ARGUMENT, NULL, NULL, NULL, -1, "native FOS allocation",
                       0, false);
    else if (o.max_owner_bytes < sizeof(mc_java_file_output))
        mc_output_fail(&e, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1, "native FOS owner bound", 0,
                       false);
    else {
        mc_java_file_output *f = calloc(1, sizeof(*f));
        if (f == NULL)
            mc_output_fail(&e, MC_JAVA_OUTPUT_ALLOCATION, NULL, NULL, NULL, -1, "native FOS owner",
                           0, false);
        else {
            mc_java_file_error cause = {0};
            const mc_java_file_status s = mc_java_files_retain(files, &cause);
            if (s != MC_JAVA_FILE_OK) {
                mc_output_file_error(&e, s, cause, -1);
                free(f);
            } else {
                f->files = files;
                f->options = o;
                f->holds = 1;
                f->bytes = sizeof(*f);
                f->last_store = -1;
                *out = f;
            }
        }
    }
    if (error != NULL)
        *error = e;
    return e.status;
}

mc_java_output_status mc_java_file_output_construct(mc_java_file_output *f, mc_java_file *file,
                                                    mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (f == NULL || f->active || f->phase != MC_JAVA_OUTPUT_RAW) {
        mc_output_fail(&e, f != NULL && f->active ? MC_JAVA_OUTPUT_BUSY : MC_JAVA_OUTPUT_STATE,
                       NULL, NULL, NULL, -1, "one-shot native FOS construction", 0, false);
        if (out != NULL)
            *out = e;
        return e.status;
    }
    f->phase = MC_JAVA_OUTPUT_CONSTRUCTING;
    f->active = true;
    if (mc_output_lock_create(&f->lock, &f->bytes, f->options.max_owner_bytes, &e,
                              "java/io/FileOutputStream",
                              "(Ljava/io/File;Z)V") != MC_JAVA_OUTPUT_OK)
        goto done;
    f->last_store = 12;
    if (file == NULL) {
        mc_output_fail(&e, MC_JAVA_OUTPUT_SOURCE_NULL, "java/io/FileOutputStream", "<init>",
                       "(Ljava/io/File;Z)V", 51, "actual null File path", 0, true);
        goto done;
    }
    mc_java_file_error fe = {0};
    mc_java_file_state fs;
    if (mc_java_file_read_state(file, &fs, &fe) != MC_JAVA_FILE_OK ||
        fs.phase != MC_JAVA_FILE_READY || fs.active || mc_java_file_namespace(file) != f->files) {
        mc_output_fail(&e, MC_JAVA_OUTPUT_STATE, "java/io/FileOutputStream", "<init>",
                       "(Ljava/io/File;Z)V", 19, "actual File native lifetime/namespace", 0, false);
        e.file = fe;
        goto done;
    }
    mc_java_file_status s = mc_java_file_retain(file, &fe);
    if (s != MC_JAVA_FILE_OK) {
        mc_output_file_error(&e, s, fe, 19);
        goto done;
    }
    f->argument = file;
    mc_world_info_name *path = mc_java_file_path(file); /* Actual getPath19, no encoding. */
    const mc_nbt_string units = mc_world_info_name_text(path);
    for (size_t i = 0; i < units.length; ++i)
        if (units.units[i] == 0) {
            mc_output_fail(&e, MC_JAVA_OUTPUT_SOURCE_IO, "java/io/FileOutputStream", "<init>",
                           "(Ljava/io/File;Z)V", 69, "actual invalid NUL File path", EINVAL, true);
            goto done;
        }
    if (f->bytes > f->options.max_owner_bytes ||
        sizeof(*f->fd) > f->options.max_owner_bytes - f->bytes) {
        mc_output_fail(&e, MC_JAVA_OUTPUT_LIMIT, "java/io/FileOutputStream", "<init>",
                       "(Ljava/io/File;Z)V", 74, "native FileDescriptor bound", 0, false);
        goto done;
    }
    f->fd = calloc(1, sizeof(*f->fd));
    if (f->fd == NULL) {
        mc_output_fail(&e, MC_JAVA_OUTPUT_ALLOCATION, "java/io/FileOutputStream", "<init>",
                       "(Ljava/io/File;Z)V", 74, "native FileDescriptor owner", 0, false);
        goto done;
    }
    f->bytes += sizeof(*f->fd);
    f->last_store = 81;
    f->fd->parent = f; /* Actual attach89, native noncounted back-reference. */
    if (mc_world_info_name_retain(path) != MC_WORLD_INFO_OK) {
        mc_output_fail(&e, MC_JAVA_OUTPUT_LIMIT, "java/io/FileOutputStream", "<init>",
                       "(Ljava/io/File;Z)V", 94, "native path occurrence", 0, false);
        goto done;
    }
    f->path = path;
    f->last_store = 94;
    mc_file_native_char *native = NULL;
    s = mc_java_file_native_path_copy(file, &native, &fe);
    if (s != MC_JAVA_FILE_OK) {
        mc_output_file_error(&e, s, fe, 100);
        goto done;
    }
    int code = 0;
    if (mc_byte_output_path_profile(native, &code) == MC_BYTE_OUTPUT_PATH_SPECIAL) {
        free(native);
        mc_output_fail(&e, MC_JAVA_OUTPUT_NATIVE_PROVIDER, "java/io/FileOutputStream", "open",
                       "(Ljava/lang/String;Z)V", 3, "native special output provider", code, false);
        goto done;
    }
    mc_byte_output_error ne;
    const bool opened = mc_byte_output_open(native, &f->fd->file, &ne);
    free(native); /* Preserve actual transport free before unbuffered admission. */
    if (!opened) {
        mc_output_fail(
            &e, ne.system_error == ENOMEM ? MC_JAVA_OUTPUT_ALLOCATION : MC_JAVA_OUTPUT_SOURCE_IO,
            "java/io/FileOutputStream", "open", "(Ljava/lang/String;Z)V", 3,
            "actual native open/create/truncate", ne.system_error, ne.system_error != ENOMEM);
        goto done;
    }
    if (!mc_byte_output_unbuffered(&f->fd->file, &ne)) {
        ++f->native_closes;
        mc_output_fail(&e, MC_JAVA_OUTPUT_NATIVE_FAILURE, "java/io/FileOutputStream", "open",
                       "(Ljava/lang/String;Z)V", 3, "native unbuffered admission", ne.system_error,
                       false);
        e.cause.consumed = ne.consumed;
        e.cleanup_failed = ne.cleanup_failed;
        e.cleanup_system_error = ne.cleanup_system_error;
        goto done;
    }
    f->cleanable = true; /* Native explicit-retirement registration at actual107. */
done:
    if (e.status != MC_JAVA_OUTPUT_OK)
        mc_output_caller(&e, "java/io/FileOutputStream", "<init>", "(Ljava/io/File;)V", 3);
    f->phase = e.status == MC_JAVA_OUTPUT_OK ? MC_JAVA_OUTPUT_READY : MC_JAVA_OUTPUT_FAILED;
    return finish(f, &e, out);
}

mc_java_output_status mc_java_file_output_retain(mc_java_file_output *f,
                                                 mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (f == NULL || f->active || f->phase == MC_JAVA_OUTPUT_RETIRING || f->holds == SIZE_MAX)
        mc_output_fail(&e,
                       f == NULL              ? MC_JAVA_OUTPUT_ARGUMENT
                       : f->active            ? MC_JAVA_OUTPUT_BUSY
                       : f->holds == SIZE_MAX ? MC_JAVA_OUTPUT_LIMIT
                                              : MC_JAVA_OUTPUT_STATE,
                       NULL, NULL, NULL, -1, "native FOS occurrence retain", 0, false);
    else
        ++f->holds;
    if (out != NULL)
        *out = e;
    return e.status;
}

static mc_java_output_status file_write_bytes(mc_java_file_output *f, const uint8_t *bytes,
                                              size_t length, mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (admit(f, &e) != MC_JAVA_OUTPUT_OK) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    f->active = true;
    if (length == 0)
        return finish(f, &e, out);
    if (f->closed || f->fd == NULL || f->fd->file == NULL)
        mc_output_fail(&e, MC_JAVA_OUTPUT_SOURCE_IO, "java/io/FileOutputStream", "write", "([BII)V",
                       16, "actual closed binary write", EBADF, true);
    else if (f->committed > f->options.max_written_bytes ||
             (uint64_t)length > f->options.max_written_bytes - f->committed)
        mc_output_fail(&e, MC_JAVA_OUTPUT_LIMIT, "java/io/FileOutputStream", "write", "([BII)V", 16,
                       "native cumulative output bound", 0, false);
    else {
        size_t sent = 0;
        while (sent < length) {
            size_t n = 0;
            mc_byte_output_error ne;
            if (f->writes == SIZE_MAX) {
                mc_output_fail(&e, MC_JAVA_OUTPUT_LIMIT, NULL, NULL, NULL, -1,
                               "native output operation counter bound", 0, false);
                break;
            }
            ++f->writes;
            const bool ok = mc_byte_output_write(f->fd->file, bytes + sent, length - sent, &n, &ne);
            if (n > length - sent) {
                mc_output_fail(&e, MC_JAVA_OUTPUT_NATIVE_FAILURE, NULL, NULL, NULL, -1,
                               "native output count contract", 0, false);
                break;
            }
            sent += n;
            f->committed += (uint64_t)n;
            e.committed = sent;
            if (!ok) {
                /* Restart/interrupt policy of native JDK writeBytes is not
                 * established by Java Code alone. Preserve the actual prefix
                 * as a native frontier rather than inventing its Throwable. */
                const bool source = ne.system_error != EINTR && ne.system_error != ENOMEM;
                mc_output_fail(&e,
                               source ? MC_JAVA_OUTPUT_SOURCE_IO : MC_JAVA_OUTPUT_NATIVE_PROVIDER,
                               "java/io/FileOutputStream", "write", "([BII)V", 16,
                               "actual binary write", ne.system_error, source);
                break;
            }
        }
    }
    return finish(f, &e, out);
}

mc_java_output_status mc_output_file_write_long_bytes(mc_java_file_output *f,
                                                      const uint8_t bytes[8],
                                                      mc_java_output_error *out) {
    return file_write_bytes(f, bytes, 8, out);
}

mc_java_output_status mc_java_file_output_write_span(mc_java_file_output *f, const uint8_t *bytes,
                                                     size_t length, mc_java_output_error *out) {
    if (bytes == NULL || length > INT32_MAX) {
        mc_java_output_error error = {0};
        const mc_java_output_status status =
            bytes == NULL ? MC_JAVA_OUTPUT_ARGUMENT : MC_JAVA_OUTPUT_LIMIT;
        mc_output_fail(&error, status, NULL, NULL, NULL, -1, "native FOS nonnull counted byte span",
                       0, false);
        if (out != NULL)
            *out = error;
        return status;
    }
    return file_write_bytes(f, bytes, length, out);
}

mc_java_output_status mc_java_file_output_flush(mc_java_file_output *f, mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (admit(f, &e) == MC_JAVA_OUTPUT_OK) {
        f->active = true;
        mc_byte_output_flush(f->fd == NULL ? NULL : f->fd->file);
        return finish(f, &e, out);
    }
    if (out != NULL)
        *out = e;
    return e.status;
}

mc_java_output_status mc_java_file_output_source_close(mc_java_file_output *f,
                                                       mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (admit(f, &e) != MC_JAVA_OUTPUT_OK) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    f->active = true;
    ++f->source_closes;
    f->source_close_reached = true;
    if (!f->closed) {
        if (mtx_lock(&f->lock->mutex) != thrd_success) {
            mc_output_fail(&e, MC_JAVA_OUTPUT_NATIVE_FAILURE, NULL, NULL, NULL, -1,
                           "native FOS close lock", 0, false);
            return finish(f, &e, out);
        }
        f->closed = true;
        f->last_store = 27;
        (void)mtx_unlock(&f->lock->mutex);
        mc_byte_output_error ne;
        if (f->fd != NULL && f->fd->file != NULL)
            ++f->native_closes;
        if (f->fd != NULL && !mc_byte_output_close(&f->fd->file, &ne)) {
            mc_output_fail(&e, MC_JAVA_OUTPUT_SOURCE_IO, "java/io/FileOutputStream", "close", "()V",
                           65, "actual consumed native close", ne.system_error, true);
            e.cause.consumed = ne.consumed;
        }
    }
    if (e.status == MC_JAVA_OUTPUT_OK)
        f->source_close_succeeded = true;
    return finish(f, &e, out);
}

mc_java_output_status mc_java_file_output_release(mc_java_file_output **slot,
                                                  mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (slot == NULL) {
        mc_output_fail(&e, MC_JAVA_OUTPUT_ARGUMENT, NULL, NULL, NULL, -1, "native FOS slot", 0,
                       false);
    } else if (*slot != NULL) {
        mc_java_file_output *f = *slot;
        if (f->active || f->phase == MC_JAVA_OUTPUT_CONSTRUCTING)
            mc_output_fail(&e, MC_JAVA_OUTPUT_BUSY, NULL, NULL, NULL, -1,
                           "native FOS active release", 0, false);
        else if (f->holds > 1) {
            --f->holds;
            *slot = NULL;
        } else {
            f->active = true;
            f->phase = MC_JAVA_OUTPUT_RETIRING;
            if (f->fd != NULL && f->fd->file != NULL) {
                mc_byte_output_error ne;
                ++f->native_closes;
                if (!mc_byte_output_close(&f->fd->file, &ne)) {
                    mc_output_fail(&e, MC_JAVA_OUTPUT_NATIVE_FAILURE, NULL, NULL, NULL, -1,
                                   "native FOS retirement close", ne.system_error, false);
                    e.cause.consumed = ne.consumed;
                    return finish(f, &e, out);
                }
            }
            mc_java_file_error fe = {0};
            mc_java_file_status fs = mc_java_file_release(&f->argument, &fe);
            if (fs == MC_JAVA_FILE_OK)
                fs = mc_java_files_release(&f->files, &fe);
            if (fs != MC_JAVA_FILE_OK) {
                const mc_java_output_status mapped = fs == MC_JAVA_FILE_BUSY ? MC_JAVA_OUTPUT_BUSY
                                                     : fs == MC_JAVA_FILE_LIMIT
                                                         ? MC_JAVA_OUTPUT_LIMIT
                                                         : MC_JAVA_OUTPUT_NATIVE_FAILURE;
                mc_output_fail(&e, mapped, NULL, NULL, NULL, -1, fe.operation, fe.system_error,
                               false);
                e.file = fe;
                return finish(f, &e, out);
            }
            mc_world_info_name_release(f->path);
            if (f->fd != NULL)
                f->fd->parent = NULL;
            free(f->fd);
            mc_output_lock_free(&f->lock);
            free(f);
            *slot = NULL;
        }
    }
    if (out != NULL)
        *out = e;
    return e.status;
}

bool mc_java_file_output_inspect(const mc_java_file_output *f, mc_java_file_output_state *out) {
    if (f == NULL || out == NULL)
        return false;
    *out = (mc_java_file_output_state){.phase = f->phase,
                                       .holds = f->holds,
                                       .owner_bytes = f->bytes,
                                       .native_write_calls = f->writes,
                                       .source_close_calls = f->source_closes,
                                       .native_close_calls = f->native_closes,
                                       .bytes_committed = f->committed,
                                       .argument = f->argument,
                                       .path = f->path,
                                       .close_lock = f->last_store >= 12 ? f->lock : NULL,
                                       .descriptor = f->fd,
                                       .pending_close_lock =
                                           f->lock != NULL && !f->lock->ready ? f->lock : NULL,
                                       .descriptor_parent = f->fd == NULL ? NULL : f->fd->parent,
                                       .active = f->active,
                                       .native_open = f->fd != NULL && f->fd->file != NULL,
                                       .source_closed = f->closed,
                                       .source_close_reached = f->source_close_reached,
                                       .source_close_succeeded = f->source_close_succeeded,
                                       .cleanable_registered = f->cleanable,
                                       .last_store_pc = f->last_store,
                                       .last_store_method = f->last_store < 0 ? NULL
                                                            : f->closed       ? "close"
                                                                              : "<init>",
                                       .failure = &f->failure};
    return true;
}

mc_java_output_status mc_java_file_output_write_array(mc_java_file_output *f,
                                                      const mc_java_byte_array_view *array,
                                                      int32_t offset, int32_t length,
                                                      mc_java_output_error *out) {
    mc_java_output_error e = {0};
    if (admit(f, &e) != MC_JAVA_OUTPUT_OK) {
        if (out != NULL)
            *out = e;
        return e.status;
    }
    if (array != NULL && (array->length < 0 || (array->length != 0 && array->bytes == NULL)))
        mc_output_fail(&e, MC_JAVA_OUTPUT_ARGUMENT, NULL, NULL, NULL, -1, "native byte-array view",
                       0, false);
    else if (array == NULL)
        mc_output_fail(&e, MC_JAVA_OUTPUT_SOURCE_NULL, "java/io/FileOutputStream", "write",
                       "([BII)V", 16, "actual null byte array", 0, true);
    else if (offset < 0 || length < 0 || offset > array->length ||
             length > array->length - offset) {
        mc_output_fail(&e, MC_JAVA_OUTPUT_SOURCE_BOUNDS, "java/io/FileOutputStream", "write",
                       "([BII)V", 16, "actual byte-array bounds", 0, true);
        e.cause.exception_class = "java/lang/IndexOutOfBoundsException";
    } else {
        /* No pointer arithmetic on a permitted empty native array. */
        static const uint8_t empty = 0;
        const uint8_t *bytes = length == 0 ? &empty : array->bytes + (size_t)offset;
        return file_write_bytes(f, bytes, (size_t)length, out);
    }
    if (out != NULL)
        *out = e;
    return e.status;
}
