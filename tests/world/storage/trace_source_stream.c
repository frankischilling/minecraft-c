/* Authored finite protocol projection over the real owners. No parser body. */
#include "../../../src/world/storage/java_file_text.h"
#include "mc/core/java/java.h"
#include "mc/world/storage/gzip_input.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <wchar.h>
#endif
static int compare(mc_nbt_string a, mc_nbt_string b) {
    size_t n = a.length < b.length ? a.length : b.length;
    for (size_t i = 0; i < n; ++i) {
        if (a.units[i] != b.units[i])
            return a.units[i] < b.units[i] ? -1 : 1;
    }
    return a.length == b.length ? 0 : a.length < b.length ? -1 : 1;
}

static void string(mc_nbt_string s) {
    for (size_t i = 0; i < s.length; ++i)
        printf("%04x", (unsigned)s.units[i]);
}

/* Sorting is solely this authored observation projection, never map mutation
 * or a claim that Java HashMap/native map iteration has the same order. */
static void tag(const mc_nbt_tag *t) {
    switch (t->type) {
        case MC_NBT_END:
            fputs("E", stdout);
            break;
        case MC_NBT_BYTE:
            printf("B:%d", (int)t->value.byte_value);
            break;
        case MC_NBT_SHORT:
            printf("S:%d", (int)t->value.short_value);
            break;
        case MC_NBT_INT:
            printf("I:%" PRId32, t->value.int_value);
            break;
        case MC_NBT_LONG:
            printf("J:%" PRId64, t->value.long_value);
            break;
        case MC_NBT_FLOAT:
            printf("F:%08" PRIx32, t->value.float_bits);
            break;
        case MC_NBT_DOUBLE:
            printf("D:%016" PRIx64, t->value.double_bits);
            break;
        case MC_NBT_BYTE_ARRAY:
            fputs("A:", stdout);
            for (size_t i = 0; i < t->value.byte_array.length; ++i)
                printf("%02x", (unsigned)t->value.byte_array.data[i]);
            break;
        case MC_NBT_STRING:
            fputs("T:", stdout);
            string(t->value.string);
            break;
        case MC_NBT_LIST:
            printf("L:%d[", (int)mc_java_i8_narrow(t->value.list.element_type));
            for (size_t i = 0; i < t->value.list.length; ++i) {
                tag(t->value.list.items[i]);
                putchar(';');
            }
            putchar(']');
            break;
        case MC_NBT_COMPOUND: {
            fputs("C:{", stdout);
            const mc_nbt_tag *prior = NULL;
            for (size_t rank = 0; rank < t->value.compound.length; ++rank) {
                const mc_nbt_tag *best = NULL;
                for (size_t i = 0; i < t->value.compound.length; ++i) {
                    const mc_nbt_tag *v = t->value.compound.items[i];
                    if ((prior == NULL || compare(v->name, prior->name) > 0) &&
                        (best == NULL || compare(v->name, best->name) < 0))
                        best = v;
                }
                if (best == NULL)
                    exit(3);
                string(best->name);
                putchar('=');
                tag(best);
                putchar(';');
                prior = best;
            }
            putchar('}');
            break;
        }
    }
}

static const char *gzip_exception(mc_gzip_error e) {
    switch (e.status) {
        case MC_GZIP_SOURCE_FORMAT:
            return "java.util.zip.ZipException";
        case MC_GZIP_SOURCE_EOF:
            return "java.io.EOFException";
        case MC_GZIP_SOURCE_NULL:
            return "java.lang.NullPointerException";
        default:
            return "java.io.IOException";
    }
}

static const char *nbt_exception(mc_nbt_stream_error e) {
    switch (e.source_fault) {
        case MC_NBT_SOURCE_EOF:
            return "java.io.EOFException";
        case MC_NBT_SOURCE_UTF:
            return "java.io.UTFDataFormatException";
        case MC_NBT_SOURCE_NEGATIVE_ARRAY:
            return "java.lang.NegativeArraySizeException";
        case MC_NBT_SOURCE_NULL_TAG:
        case MC_NBT_SOURCE_NULL_INPUT:
            return "java.lang.NullPointerException";
        default:
            return "java.io.IOException";
    }
}

static int run(int mode, mc_nbt_string path) {
    mc_java_files *files = NULL;
    mc_java_file *f = NULL;
    mc_java_file_stream *in = NULL;
    mc_gzip_input *g = NULL;
    mc_nbt_stream *r = NULL;
    mc_nbt_document *doc = NULL;
    mc_world_info_name *name = NULL;
    mc_java_file_error fe;
    mc_gzip_error ge;
    mc_nbt_stream_error ne;
    int rc = 1;
    const char *thrown = NULL;
    uint8_t *raw = NULL;
    size_t raw_count = 0;
    bool compound = mode == 1;
#define NEED(x)                                                                                    \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Source stream trace:%d: %s\n", __LINE__, #x);                         \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)
    NEED(mc_world_info_name_copy(path, SIZE_MAX, &name) == MC_WORLD_INFO_OK);
    NEED(mc_java_files_create(NULL, &files, &fe) == MC_JAVA_FILE_OK);
    NEED(mc_java_file_allocate(files, &f, &fe) == MC_JAVA_FILE_OK);
    NEED(mc_java_file_construct_string(f, name, &fe) == MC_JAVA_FILE_OK);
    NEED(mc_java_file_stream_allocate(files, &in, &fe) == MC_JAVA_FILE_OK);
    NEED(mc_java_file_stream_construct(in, f, &fe) == MC_JAVA_FILE_OK);
    if (mode != 2)
        NEED(mc_nbt_stream_allocate(1048576, &r, &ne) == MC_NBT_OK);
    NEED(mc_gzip_input_allocate(NULL, &g, &ge) == MC_GZIP_OK);
    mc_gzip_status gs = mc_gzip_input_construct(g, in, &ge);
    if (gs != MC_GZIP_OK) {
        NEED(ge.source_exception);
        thrown = gzip_exception(ge);
        goto output;
    }
    if (mode == 2) {
        raw = malloc(1048576);
        NEED(raw != NULL);
        for (;;) {
            int byte;
            gs = mc_gzip_input_read_byte(g, &byte, &ge);
            if (gs != MC_GZIP_OK) {
                NEED(ge.source_exception);
                thrown = gzip_exception(ge);
                break;
            }
            if (byte < 0)
                break;
            NEED(raw_count < 1048576);
            raw[raw_count++] = (uint8_t)byte;
        }
        goto output;
    }
    mc_nbt_byte_input port = mc_gzip_input_byte_port(g);
    NEED(mc_nbt_stream_construct(r, &port, NULL, &ne) == MC_NBT_OK);
    mc_nbt_status ns = mc_nbt_stream_parse(r, compound, &doc, &ne);
    if (ns != MC_NBT_OK) {
        NEED(ne.source_fault != MC_NBT_SOURCE_NONE);
        if (ne.source_fault == MC_NBT_SOURCE_INPUT) {
            NEED(mc_gzip_input_last_error(g, &ge) == MC_GZIP_OK);
            thrown = gzip_exception(ge);
        } else
            thrown = nbt_exception(ne);
    }
    if (compound) {
        mc_nbt_stream_error close;
        ns = mc_nbt_stream_source_close(r, &close);
        if (ns != MC_NBT_OK) {
            NEED(close.source_fault != MC_NBT_SOURCE_NONE);
            thrown = "java.io.IOException";
        }
    }
output:;
    mc_java_file_stream_state state;
    NEED(mc_java_file_stream_read_state(in, &state, &fe) == MC_JAVA_FILE_OK);
    fputs(thrown == NULL ? "RESULT\tOK\t" : "RESULT\tTHROW\t", stdout);
    if (thrown != NULL)
        fputs(thrown, stdout);
    else if (mode == 2) {
        fputs("R:", stdout);
        for (size_t i = 0; i < raw_count; ++i)
            printf("%02x", (unsigned)raw[i]);
    } else
        tag(mc_nbt_document_root(doc));
    printf("\t%s\n", state.source_close_succeeded ? "CLOSED" : "OPEN");
    rc = 0;
done:;
    free(raw);
    bool clean = true;
    if (mc_nbt_stream_release(&r, &ne) != MC_NBT_OK)
        clean = false;
    mc_nbt_document_free(doc);
    if (mc_gzip_input_release(&g, &ge) != MC_GZIP_OK)
        clean = false;
    if (mc_java_file_stream_release(&in, &fe) != MC_JAVA_FILE_OK)
        clean = false;
    if (mc_java_file_release(&f, &fe) != MC_JAVA_FILE_OK)
        clean = false;
    if (mc_java_files_release(&files, &fe) != MC_JAVA_FILE_OK)
        clean = false;
    mc_world_info_name_release(name);
    if (!clean) {
        fputs("Source stream trace: native retirement failure\n", stderr);
        return 3;
    }
    return rc;
#undef NEED
}

static int arguments(void) {
    fputs("Source stream trace input: expected ij|as|gzip and fixture path\n", stderr);
    return 2;
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    if (argc != 3 || (wcscmp(argv[1], L"ij") != 0 && wcscmp(argv[1], L"as") != 0 &&
                      wcscmp(argv[1], L"gzip") != 0))
        return arguments();
    return run(wcscmp(argv[1], L"gzip") == 0 ? 2
               : wcscmp(argv[1], L"as") == 0 ? 1
                                             : 0,
               (mc_nbt_string){(const uint16_t *)argv[2], wcslen(argv[2])});
}
#else
int main(int argc, char **argv) {
    if (argc != 3 ||
        (strcmp(argv[1], "ij") != 0 && strcmp(argv[1], "as") != 0 && strcmp(argv[1], "gzip") != 0))
        return arguments();
    mc_world_info_name *name = NULL;
    mc_java_file_error e;
    mc_java_file_status status = mc_file_decode_utf8(argv[2], SIZE_MAX, &name, &e);
    if (status != MC_JAVA_FILE_OK) {
        if (status == MC_JAVA_FILE_LIMIT || status == MC_JAVA_FILE_ALLOCATION_FAILED) {
            fputs("Source stream trace: argument String ownership\n", stderr);
            return 3;
        }
        return arguments();
    }
    int rc = run(strcmp(argv[1], "gzip") == 0 ? 2
                 : strcmp(argv[1], "as") == 0 ? 1
                                              : 0,
                 mc_world_info_name_text(name));
    mc_world_info_name_release(name);
    return rc;
}
#endif
