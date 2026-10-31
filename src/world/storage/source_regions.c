#include "mc/world/storage/source_regions.h"
#include "mc/core/java/java.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct source_region {
    mc_java_file *file;
    mc_java_random_access *raf;
    int32_t offsets[1024], timestamps[1024];
    bool *free_sectors;
    size_t sectors;
    int32_t growth;
    int64_t modified;
    uint32_t hash;
    struct source_region *next;
} source_region;

struct mc_world_source_regions {
    mc_java_files *files;
    mc_world_source_regions_options options;
    size_t holds, count, capacity;
    bool active, activated, retiring, failed;
    source_region *table[512], *pending;
    mc_java_file *directory, *file;
    mc_world_info_name *name;
    mc_world_source_region_record *record;
    mc_world_source_region_error failure, cleanup, work;
};

static mc_world_source_status result(mc_world_source_region_error *e, mc_world_source_status s,
                                     const char *op, const char *cl, const char *method, int pc) {
    if (e != NULL) {
        *e = (mc_world_source_region_error){0};
        e->source.status = e->source.cause_status = s;
        e->source.operation = op;
        e->source.native_policy = s != MC_WORLD_SOURCE_OK;
        e->region_pc = pc;
        if (cl != NULL) {
            e->source.frames[0] =
                (mc_world_source_site){cl, method,
                                       strcmp(method, "<init>") == 0 ? "(Ljava/io/File;)V"
                                       : strcmp(cl, "rj") == 0       ? "(Ljava/io/File;II)Lqj;"
                                                               : "(II)Ljava/io/DataInputStream;",
                                       pc};
            e->source.frame_count = 1;
        }
    }
    return s;
}

static mc_world_source_status file_failure(mc_world_source_region_error *e,
                                           mc_java_file_status status, mc_java_file_error file,
                                           const char *cl, const char *method, int pc) {
    result(e,
           status == MC_JAVA_FILE_BUSY                ? MC_WORLD_SOURCE_BUSY
           : status == MC_JAVA_FILE_LIMIT             ? MC_WORLD_SOURCE_LIMIT
           : status == MC_JAVA_FILE_ALLOCATION_FAILED ? MC_WORLD_SOURCE_ALLOCATION
                                                      : MC_WORLD_SOURCE_FILE,
           file.operation, cl, method, pc);
    e->source.file = file;
    e->random_access = file;
    e->source.source_exception = status == MC_JAVA_FILE_IO_ERROR;
    e->source.native_policy = !e->source.source_exception;
    return e->source.status;
}

static mc_world_source_status diagnostic(mc_world_source_region_error *e, const char *cl, int pc) {
    e->source.cause_site = e->source.frames[0];
    e->source.status = MC_WORLD_SOURCE_UNIMPLEMENTED;
    e->source.operation = "java/io/IOException.printStackTrace()V";
    e->source.caught_io = e->caught_io = e->source.diagnostic_frontier = true;
    e->source.frames[0] =
        (mc_world_source_site){cl, strcmp(cl, "qj") == 0 ? "<init>" : "a",
                               strcmp(cl, "qj") == 0 ? "(Ljava/io/File;)V" : "()V", pc};
    e->source.frame_count = 1;
    return e->source.status;
}

mc_world_source_regions_options mc_world_source_regions_default_options(void) {
    return (mc_world_source_regions_options){268435456u, 67108864u, 8388608u};
}

mc_world_source_status
mc_world_source_regions_create(mc_java_files *files, const mc_world_source_regions_options *options,
                               mc_world_source_regions **out, mc_world_source_region_error *e) {
    if (files == NULL || out == NULL)
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "cold rj arguments", NULL, NULL, -1);
    mc_world_source_regions *r = calloc(1, sizeof(*r));
    if (r == NULL)
        return result(e, MC_WORLD_SOURCE_ALLOCATION, "cold rj owner", NULL, NULL, -1);
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_files_retain(files, &fe);
    if (fs != MC_JAVA_FILE_OK) {
        free(r);
        return file_failure(e, fs, fe, NULL, NULL, -1);
    }
    r->files = files;
    r->holds = 1;
    r->capacity = 16;
    r->options = options == NULL ? mc_world_source_regions_default_options() : *options;
    *out = r;
    return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, -1);
}

mc_world_source_status mc_world_source_regions_retain(mc_world_source_regions *r,
                                                      mc_world_source_region_error *e) {
    if (r == NULL)
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "rj retain", NULL, NULL, -1);
    if (r->active || r->retiring)
        return result(e, MC_WORLD_SOURCE_BUSY, "rj active/retiring hold", NULL, NULL, -1);
    if (r->holds == SIZE_MAX)
        return result(e, MC_WORLD_SOURCE_LIMIT, "rj hold count", NULL, NULL, -1);
    ++r->holds;
    return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, -1);
}

static mc_world_source_status region_release(source_region **slot,
                                             mc_world_source_region_error *e) {
    source_region *q = *slot;
    if (q == NULL)
        return MC_WORLD_SOURCE_OK;
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_random_access_release(&q->raf, &fe);
    if (fs == MC_JAVA_FILE_OK)
        fs = mc_java_file_release(&q->file, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return file_failure(e, fs, fe, NULL, NULL, -1);
    free(q->free_sectors);
    free(q);
    *slot = NULL;
    return MC_WORLD_SOURCE_OK;
}

static mc_world_source_status pending_clear(mc_world_source_regions *r,
                                            mc_world_source_region_error *e) {
    mc_world_source_status status = region_release(&r->pending, e);
    if (status != MC_WORLD_SOURCE_OK)
        return status;
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_file_release(&r->file, &fe);
    if (fs == MC_JAVA_FILE_OK)
        fs = mc_java_file_release(&r->directory, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return file_failure(e, fs, fe, NULL, NULL, -1);
    mc_world_info_name_release(r->name);
    r->name = NULL;
    mc_world_source_region_record_free(r->record);
    r->record = NULL;
    return MC_WORLD_SOURCE_OK;
}

/* Native pressure policy retains the same qj strongly until this source
 * capacity flush/explicit flush. Java SoftReference GC timing is not modeled.
 * HashMap bucket order is retained for the selected JDK8+ ordinary-bin profile. */
static mc_world_source_status flush_body(mc_world_source_regions *r,
                                         mc_world_source_region_error *e) {
    for (size_t i = 0; i < r->capacity; ++i) {
        for (source_region *q = r->table[i]; q != NULL; q = q->next) {
            mc_java_file_error fe;
            mc_java_file_status fs = mc_java_random_access_close(q->raf, &fe);
            if (fs != MC_JAVA_FILE_OK) {
                file_failure(e, fs, fe, "rj", "a", 46);
                return fs == MC_JAVA_FILE_IO_ERROR ? diagnostic(e, "rj", 54) : e->source.status;
            }
        }
    }
    for (size_t i = 0; i < r->capacity; ++i) {
        while (r->table[i] != NULL) {
            source_region *q = r->table[i], *next = q->next;
            mc_world_source_status status = region_release(&q, e);
            if (status != MC_WORLD_SOURCE_OK)
                return status;
            r->table[i] = next;
            --r->count;
        }
    }
    return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, -1);
}

mc_world_source_status mc_world_source_regions_flush(mc_world_source_regions *r,
                                                     mc_world_source_region_error *e) {
    if (r == NULL)
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "rj flush receiver", NULL, NULL, -1);
    if (r->active || r->retiring)
        return result(e, MC_WORLD_SOURCE_BUSY, "rj active flush", NULL, NULL, -1);
    if (r->failed) {
        if (e != NULL)
            *e = r->failure;
        return r->failure.source.status;
    }
    r->active = true;
    r->activated = true;
    mc_world_source_status status = flush_body(r, &r->work);
    r->active = false;
    if (status != MC_WORLD_SOURCE_OK) {
        r->failure = r->work;
        r->failed = true;
    }
    if (e != NULL)
        *e = r->work;
    return status;
}

mc_world_source_status mc_world_source_regions_release(mc_world_source_regions **slot,
                                                       mc_world_source_region_error *e) {
    if (slot == NULL)
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "rj release slot", NULL, NULL, -1);
    mc_world_source_regions *r = *slot;
    if (r == NULL)
        return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, -1);
    if (r->active)
        return result(e, MC_WORLD_SOURCE_BUSY, "rj active retirement", NULL, NULL, -1);
    if (r->holds > 1) {
        --r->holds;
        *slot = NULL;
        return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, -1);
    }
    r->retiring = true;
    r->active = true;
    mc_world_source_status status = pending_clear(r, &r->work);
    /* Native final retirement closes descriptors, but never credits a source
     * rj.a invocation, clears its failure, or enters its IOException catch. */
    for (size_t i = 0; status == MC_WORLD_SOURCE_OK && i < r->capacity; ++i) {
        while (r->table[i] != NULL) {
            source_region *q = r->table[i], *next = q->next;
            status = region_release(&q, &r->work);
            if (status != MC_WORLD_SOURCE_OK)
                break;
            r->table[i] = next;
            --r->count;
        }
    }
    if (status == MC_WORLD_SOURCE_OK) {
        mc_java_file_error fe;
        mc_java_file_status fs = mc_java_files_release(&r->files, &fe);
        if (fs != MC_JAVA_FILE_OK)
            status = file_failure(&r->work, fs, fe, NULL, NULL, -1);
    }
    r->active = false;
    if (status != MC_WORLD_SOURCE_OK) {
        r->cleanup = r->work;
        if (e != NULL)
            *e = r->cleanup;
        return status;
    }
    free(r);
    *slot = NULL;
    return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, -1);
}

mc_world_source_status mc_world_source_regions_retire(mc_world_source_regions **slot,
                                                      mc_world_source_region_error *e) {
    if (slot == NULL)
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "rj strict slot", NULL, NULL, -1);
    if (*slot != NULL && ((*slot)->holds != 1 || (*slot)->active))
        return result(e, MC_WORLD_SOURCE_BUSY, "rj live occurrences", NULL, NULL, -1);
    return mc_world_source_regions_release(slot, e);
}

static mc_world_source_status make_name(mc_world_source_regions *r, const char *text,
                                        mc_world_source_region_error *e) {
    uint16_t units[96];
    size_t n = strlen(text);
    if (n > sizeof(units) / sizeof(*units))
        return result(e, MC_WORLD_SOURCE_LIMIT, "rj filename extent", "rj", "a", 50);
    for (size_t i = 0; i < n; ++i)
        units[i] = (uint8_t)text[i];
    mc_world_info_status s =
        mc_world_info_name_copy((mc_nbt_string){units, n}, r->options.max_path_bytes, &r->name);
    if (s != MC_WORLD_INFO_OK)
        return result(e,
                      s == MC_WORLD_INFO_LIMIT ? MC_WORLD_SOURCE_LIMIT : MC_WORLD_SOURCE_ALLOCATION,
                      "rj filename String", "rj", "a", 50);
    return MC_WORLD_SOURCE_OK;
}

static bool file_hash(mc_java_file *file, uint32_t *out) {
    mc_nbt_string path = mc_world_info_name_text(mc_java_file_path(file));
    uint32_t h = 0;
    for (size_t i = 0; i < path.length; ++i) {
        uint16_t c = path.units[i];
#ifdef _WIN32
        /* Exact Java File hash/equality for ASCII, including case folding.
         * Non-ASCII WinNTFileSystem lowercasing is an explicit SDK dependency,
         * never a locale-sensitive C tolower approximation. */
        if (c > 127)
            return false;
        if (c >= 'A' && c <= 'Z')
            c = (uint16_t)(c + ('a' - 'A'));
#endif
        h = 31u * h + c;
    }
    h ^= 1234321u;
    *out = h ^ (h >> 16);
    return true;
}

static bool file_equal(mc_java_file *a, mc_java_file *b) {
    mc_nbt_string x = mc_world_info_name_text(mc_java_file_path(a)),
                  y = mc_world_info_name_text(mc_java_file_path(b));
    if (x.length != y.length)
        return false;
    for (size_t i = 0; i < x.length; ++i) {
        uint16_t p = x.units[i], q = y.units[i];
#ifdef _WIN32
        if (p >= 'A' && p <= 'Z')
            p = (uint16_t)(p + 32);
        if (q >= 'A' && q <= 'Z')
            q = (uint16_t)(q + 32);
#endif
        if (p != q)
            return false;
    }
    return true;
}

static mc_java_file_status read_byte(source_region *q, uint8_t *out, mc_java_file_error *e) {
    mc_java_file_read read;
    mc_java_file_status s = mc_java_random_access_read(q->raf, out, 1, &read, e);
    if (s == MC_JAVA_FILE_OK && read.eof) {
        if (e != NULL)
            *e = (mc_java_file_error){MC_JAVA_FILE_IO_ERROR, "RandomAccessFile.read EOFException",
                                      0};
        s = MC_JAVA_FILE_IO_ERROR;
    }
    return s;
}

static mc_java_file_status read_int(source_region *q, int32_t *out, mc_java_file_error *e) {
    uint32_t value = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t c;
        mc_java_file_status s = read_byte(q, &c, e);
        if (s != MC_JAVA_FILE_OK)
            return s;
        value = (value << 8) | c;
    }
    *out = mc_java_i32_narrow((int64_t)value);
    return MC_JAVA_FILE_OK;
}

static mc_world_source_status construct_region(mc_world_source_regions *r,
                                               mc_world_source_region_error *e) {
    source_region *q = calloc(1, sizeof(*q));
    if (q == NULL)
        return result(e, MC_WORLD_SOURCE_ALLOCATION, "qj owner", "rj", "a", 125);
    r->pending = q;
    q->file = r->file;
    r->file = NULL;
    mc_java_file_error fe;
    bool exists;
    mc_java_file_status fs = mc_java_file_exists(q->file, &exists, &fe);
    int pc = 64;
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    if (exists) {
        pc = 72;
        fs = mc_java_file_last_modified(q->file, &q->modified, &fe);
        if (fs != MC_JAVA_FILE_OK)
            goto failed;
    }
    pc = 79;
    fs = mc_java_random_access_allocate(&q->raf, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    pc = 86;
    fs = mc_java_random_access_construct(q->raf, q->file, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    int64_t length;
    pc = 96;
    fs = mc_java_random_access_length(q->raf, &length, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    if (length < 4096) {
        const uint8_t zero = 0;
        for (size_t i = 0; i < 8192; ++i) {
            pc = i < 4096 ? 120 : 143;
            fs = mc_java_random_access_write(q->raf, &zero, 1, &fe);
            if (fs != MC_JAVA_FILE_OK)
                goto failed;
        }
        q->growth = 8192;
    }
    pc = 168;
    fs = mc_java_random_access_length(q->raf, &length, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    if (((uint64_t)length & 4095u) != 0) {
        const uint8_t zero = 0;
        int32_t i = 0;
        for (;;) {
            pc = 188;
            fs = mc_java_random_access_length(q->raf, &length, &fe);
            if (fs != MC_JAVA_FILE_OK)
                goto failed;
            if (i >= (int32_t)((uint64_t)length & 4095u))
                break;
            pc = 204;
            fs = mc_java_random_access_write(q->raf, &zero, 1, &fe);
            if (fs != MC_JAVA_FILE_OK)
                goto failed;
            ++i;
        }
    }
    pc = 217;
    fs = mc_java_random_access_length(q->raf, &length, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    if (length < 0 || (uint64_t)length > r->options.max_region_bytes || length > INT32_MAX)
        return result(e, MC_WORLD_SOURCE_LIMIT, "qj native region extent", "qj", "<init>", 220);
    q->sectors = (size_t)length / 4096u;
    if (q->sectors < 2) {
        result(e, MC_WORLD_SOURCE_SOURCE_EXCEPTION, "qj ArrayList.set index", "qj", "<init>", 285);
        e->source.source_exception = true;
        e->source.native_policy = false;
        e->source.exception_class = "java/lang/IndexOutOfBoundsException";
        return e->source.status;
    }
    q->free_sectors = malloc(q->sectors * sizeof(*q->free_sectors));
    if (q->free_sectors == NULL)
        return result(e, MC_WORLD_SOURCE_ALLOCATION, "qj sector free List", "qj", "<init>", 232);
    memset(q->free_sectors, 1, q->sectors * sizeof(*q->free_sectors));
    q->free_sectors[0] = q->free_sectors[1] = false;
    pc = 294;
    fs = mc_java_random_access_seek(q->raf, 0, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    for (size_t i = 0; i < 1024; ++i) {
        pc = 310;
        fs = read_int(q, &q->offsets[i], &fe);
        if (fs != MC_JAVA_FILE_OK)
            goto failed;
        int32_t sector = mc_java_i32_shr(q->offsets[i], 8);
        uint32_t count = (uint32_t)q->offsets[i] & 255u;
        if (q->offsets[i] != 0 && (int64_t)sector + count <= (int64_t)q->sectors) {
            for (uint32_t j = 0; j < count; ++j) {
                if (sector < 0) {
                    result(e, MC_WORLD_SOURCE_SOURCE_EXCEPTION, "qj ArrayList.set negative sector",
                           "qj", "<init>", 380);
                    e->source.source_exception = true;
                    e->source.native_policy = false;
                    e->source.exception_class = "java/lang/IndexOutOfBoundsException";
                    return e->source.status;
                }
                q->free_sectors[(size_t)sector + j] = false;
            }
        }
    }
    for (size_t i = 0; i < 1024; ++i) {
        pc = 409;
        fs = read_int(q, &q->timestamps[i], &fe);
        if (fs != MC_JAVA_FILE_OK)
            goto failed;
    }
    return result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, -1);
failed:
    file_failure(e, fs, fe, "qj", "<init>", pc);
    return fs == MC_JAVA_FILE_IO_ERROR ? diagnostic(e, "qj", 433) : e->source.status;
}

static void rehash(mc_world_source_regions *r) {
    source_region *old[512];
    memcpy(old, r->table, sizeof(old));
    memset(r->table, 0, sizeof(r->table));
    size_t old_capacity = r->capacity;
    r->capacity *= 2;
    for (size_t i = 0; i < old_capacity; ++i) {
        source_region *q = old[i];
        while (q != NULL) {
            source_region *next = q->next;
            q->next = NULL;
            size_t bucket = q->hash & (r->capacity - 1u);
            source_region **tail = &r->table[bucket];
            while (*tail != NULL)
                tail = &(*tail)->next;
            *tail = q;
            q = next;
        }
    }
}

static mc_world_source_status find_region(mc_world_source_regions *r, mc_java_file *root, int32_t x,
                                          int32_t z, source_region **out,
                                          mc_world_source_region_error *e) {
    mc_world_source_status status;
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_file_allocate(r->files, &r->directory, &fe);
    int pc = 0;
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    status = make_name(r, "region", e);
    if (status != MC_WORLD_SOURCE_OK)
        return status;
    pc = 7;
    fs = mc_java_file_construct_child(r->directory, root, r->name, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    mc_world_info_name_release(r->name);
    r->name = NULL;
    pc = 11;
    fs = mc_java_file_allocate(r->files, &r->file, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    char name[96];
    int n = snprintf(name, sizeof(name), "r.%d.%d.mcr", (int)mc_java_i32_shr(x, 5),
                     (int)mc_java_i32_shr(z, 5));
    if (n < 0 || (size_t)n >= sizeof(name))
        return result(e, MC_WORLD_SOURCE_LIMIT, "rj integer filename", "rj", "a", 50);
    status = make_name(r, name, e);
    if (status != MC_WORLD_SOURCE_OK)
        return status;
    pc = 53;
    fs = mc_java_file_construct_child(r->file, r->directory, r->name, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    uint32_t hash;
    if (!file_hash(r->file, &hash))
        return result(e, MC_WORLD_SOURCE_UNIMPLEMENTED,
                      "WinNTFileSystem non-ASCII File hash/compare provider", "rj", "a", 63);
    for (source_region *q = r->table[hash & (r->capacity - 1u)]; q != NULL; q = q->next)
        if (q->hash == hash && file_equal(q->file, r->file)) {
            *out = q;
            return MC_WORLD_SOURCE_OK;
        }
    bool exists;
    pc = 97;
    fs = mc_java_file_exists(r->directory, &exists, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto failed;
    if (!exists) {
        bool ignored;
        pc = 104;
        fs = mc_java_file_mkdirs(r->directory, &ignored, &fe);
        if (fs != MC_JAVA_FILE_OK)
            goto failed;
    }
    if (r->count >= 256) {
        status = flush_body(r, e);
        if (status != MC_WORLD_SOURCE_OK)
            return status;
    }
    status = construct_region(r, e);
    if (status != MC_WORLD_SOURCE_OK)
        return status;
    source_region *q = r->pending;
    q->hash = hash;
    source_region **tail = &r->table[hash & (r->capacity - 1u)];
    size_t bins = 0;
    while (*tail != NULL) {
        tail = &(*tail)->next;
        ++bins;
    }
    if (bins >= 8 && r->capacity >= 64)
        return result(e, MC_WORLD_SOURCE_UNIMPLEMENTED, "rj HashMap tree-bin SDK owner", "rj", "a",
                      150);
    *tail = q;
    r->pending = NULL;
    ++r->count;
    if (r->count > r->capacity * 3u / 4u || (bins >= 8 && r->capacity < 64))
        rehash(r);
    *out = q;
    return MC_WORLD_SOURCE_OK;
failed:
    return file_failure(e, fs, fe, "rj", "a", pc);
}

mc_world_source_status mc_world_source_regions_read(mc_world_source_regions *r, mc_java_file *root,
                                                    int32_t x, int32_t z,
                                                    mc_world_source_region_record **out,
                                                    mc_world_source_region_error *e) {
    if (r == NULL || out == NULL || (root != NULL && mc_java_file_namespace(root) != r->files))
        return result(e, MC_WORLD_SOURCE_ARGUMENT, "rj source File/domain/output", NULL, NULL, -1);
    if (r->active || r->retiring)
        return result(e, MC_WORLD_SOURCE_BUSY, "rj active read", NULL, NULL, -1);
    if (r->failed) {
        if (e != NULL)
            *e = r->failure;
        return r->failure.source.status;
    }
    r->active = true;
    r->activated = true;
    mc_world_source_status status = pending_clear(r, &r->work);
    if (status == MC_WORLD_SOURCE_OK)
        result(&r->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, -1);
    source_region *q = NULL;
    if (status == MC_WORLD_SOURCE_OK)
        status = find_region(r, root, x, z, &q, &r->work);
    if (status != MC_WORLD_SOURCE_OK)
        goto done;
    mc_java_file_error fe;
    mc_java_file_status fs = MC_JAVA_FILE_OK;
    int32_t offset = q->offsets[((uint32_t)x & 31u) + ((uint32_t)z & 31u) * 32u];
    int32_t sector = mc_java_i32_shr(offset, 8);
    int32_t sectors = offset & 255;
    if (offset == 0 || (int64_t)sector + sectors > (int64_t)q->sectors)
        goto absent;
    int pc = 85;
    fs = mc_java_random_access_seek(q->raf, (int64_t)mc_java_i32_mul(sector, 4096), &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto io;
    int32_t length;
    pc = 92;
    fs = read_int(q, &length, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto io;
    if (length > 4096 * sectors)
        goto absent;
    uint8_t selector;
    pc = 152;
    fs = read_byte(q, &selector, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto io;
    if (selector != 1 && selector != 2)
        goto absent;
    int32_t array_length = mc_java_i32_sub(length, 1);
    int array_pc = selector == 1 ? 167 : 219;
    if (array_length < 0) {
        status = result(&r->work, MC_WORLD_SOURCE_SOURCE_EXCEPTION, "qj negative byte-array length",
                        "qj", "a", array_pc);
        r->work.source.exception_class = "java/lang/NegativeArraySizeException";
        r->work.source.source_exception = true;
        r->work.source.native_policy = false;
        goto done;
    }
    if ((size_t)array_length > r->options.max_compressed_bytes) {
        status = result(&r->work, MC_WORLD_SOURCE_LIMIT, "qj compressed byte-array bound", "qj",
                        "a", array_pc);
        goto done;
    }
    r->record = calloc(1, sizeof(*r->record));
    if (r->record == NULL) {
        status = result(&r->work, MC_WORLD_SOURCE_ALLOCATION, "qj byte-array owner", "qj", "a",
                        array_pc);
        goto done;
    }
    r->record->length = (size_t)array_length;
    r->record->compression = selector;
    r->record->bytes = calloc(r->record->length == 0 ? 1 : r->record->length, 1);
    if (r->record->bytes == NULL) {
        status = result(&r->work, MC_WORLD_SOURCE_ALLOCATION, "qj compressed byte-array", "qj", "a",
                        array_pc);
        goto done;
    }
    mc_java_file_read ignored;
    pc = selector == 1 ? 177 : 229;
    fs = mc_java_random_access_read(q->raf, r->record->bytes, r->record->length, &ignored, &fe);
    if (fs != MC_JAVA_FILE_OK)
        goto io;
    result(&r->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, -1);
    goto done;
io:
    if (fs != MC_JAVA_FILE_IO_ERROR) {
        status = file_failure(&r->work, fs, fe, "qj", "a", pc);
        goto done;
    }
    /* qj catches only its reached IOException and its debug sink is empty. */
    result(&r->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, -1);
    r->work.caught_io = true;
    r->work.random_access = fe;
absent:
    r->work.source_null = true;
done:
    if (status == MC_WORLD_SOURCE_OK) {
        /* Returned compressed RAM array is independent of these dead locals. */
        mc_world_source_region_record *record = r->work.source_null ? NULL : r->record;
        if (record != NULL)
            r->record = NULL;
        mc_world_source_status cleanup = pending_clear(r, &r->work);
        if (cleanup != MC_WORLD_SOURCE_OK) {
            status = cleanup;
            r->record = record;
        } else
            *out = record;
    }
    r->active = false;
    if (status != MC_WORLD_SOURCE_OK) {
        r->failure = r->work;
        r->failed = true;
    }
    if (e != NULL)
        *e = r->work;
    return status;
}

void mc_world_source_region_record_free(mc_world_source_region_record *r) {
    if (r != NULL) {
        free(r->bytes);
        free(r);
    }
}

bool mc_world_source_regions_inspect(const mc_world_source_regions *r,
                                     mc_world_source_regions_state *out) {
    if (r == NULL || out == NULL || r->active)
        return false;
    *out = (mc_world_source_regions_state){.holds = r->holds,
                                           .entries = r->count,
                                           .active = r->active,
                                           .activated = r->activated,
                                           .retiring = r->retiring,
                                           .files = r->files,
                                           .failure = &r->failure,
                                           .cleanup = &r->cleanup,
                                           .pending_directory = r->directory,
                                           .pending_file = r->file,
                                           .pending_random_access =
                                               r->pending == NULL ? NULL : r->pending->raf};
    return true;
}
