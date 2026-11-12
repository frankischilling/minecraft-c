#include "font_mapping.h"
#include <stdlib.h>
#include <string.h>

enum { FONT_MAPPING_UNITS = 65536, FONT_MAPPING_INPUT = 1024 * 1024 };

typedef struct mapping_allocation {
    struct mapping_allocation *next;

    union {
        long double align;
        void *pointer;
        uint64_t integer;
    } alignment;
} mapping_allocation;

struct mc_client_font_mapping {
    size_t holds, bytes, allocations, max_bytes, max_allocations, input_bytes;
    mc_registry_phase phase;
    const mc_registry_resources *resources;
    mc_player_string *empty, *a, *local;
    uint16_t *b;
    mapping_allocation *arena;
    mc_java_properties_input stream;
    bool opened, closed;
    mc_client_font_mapping_error failure;
};

static mc_client_font_mapping_status fail(mc_client_font_mapping *o,
                                          mc_client_font_mapping_status status,
                                          const char *operation, const char *method, int32_t pc) {
    o->failure.status = status;
    o->failure.operation = operation;
    o->failure.method = method;
    o->failure.pc = pc;
    return status;
}

static void *allocate(void *context, size_t bytes) {
    mc_client_font_mapping *o = context;
    if (bytes > SIZE_MAX - sizeof(mapping_allocation) ||
        bytes + sizeof(mapping_allocation) > o->max_bytes - o->bytes ||
        o->allocations >= o->max_allocations) {
        fail(o, MC_CLIENT_FONT_MAPPING_LIMIT, "fp native arena bound", "a", 24);
        return NULL;
    }
    mapping_allocation *a = calloc(1, sizeof(*a) + bytes);
    if (a == NULL) {
        fail(o, MC_CLIENT_FONT_MAPPING_ALLOCATION_FAILED, "fp native arena allocation", "a", 24);
        return NULL;
    }
    a->next = o->arena;
    o->arena = a;
    o->bytes += sizeof(*a) + bytes;
    ++o->allocations;
    return a + 1;
}

mc_client_font_mapping_status mc_client_font_mapping_create(const mc_registry_resources *resources,
                                                            mc_player_string *empty,
                                                            size_t max_bytes,
                                                            size_t max_allocations,
                                                            mc_client_font_mapping **out) {
    mc_player_string_view literal;
    if (resources == NULL || out == NULL || !mc_player_string_read(empty, &literal) ||
        literal.length != 0)
        return MC_CLIENT_FONT_MAPPING_INVALID_ARGUMENT;
    mc_client_font_mapping *o = calloc(1, sizeof(*o));
    if (o == NULL)
        return MC_CLIENT_FONT_MAPPING_ALLOCATION_FAILED;
    if (mc_player_string_retain(empty) != MC_PLAYER_OK) {
        free(o);
        return MC_CLIENT_FONT_MAPPING_LIMIT;
    }
    o->holds = 1;
    o->resources = resources;
    o->empty = empty;
    o->max_bytes = max_bytes;
    o->max_allocations = max_allocations;
    *out = o;
    return MC_CLIENT_FONT_MAPPING_OK;
}

mc_client_font_mapping_status mc_client_font_mapping_retain(mc_client_font_mapping *o) {
    if (o == NULL)
        return MC_CLIENT_FONT_MAPPING_INVALID_ARGUMENT;
    if (o->holds == SIZE_MAX)
        return MC_CLIENT_FONT_MAPPING_LIMIT;
    ++o->holds;
    return MC_CLIENT_FONT_MAPPING_OK;
}

mc_client_font_mapping_status mc_client_font_mapping_release(mc_client_font_mapping *o) {
    if (o == NULL)
        return MC_CLIENT_FONT_MAPPING_OK;
    if (o->phase == MC_REGISTRY_INITIALIZING)
        return MC_CLIENT_FONT_MAPPING_BUSY;
    if (--o->holds != 0)
        return MC_CLIENT_FONT_MAPPING_OK;
    /* Stream source owns opened contexts. No source close is invented after
     * a caught read exception; its eventual native resource retirement owns it. */
    mc_player_string_release(o->empty);
    mc_player_string_release(o->a);
    mc_player_string_release(o->local);
    while (o->arena != NULL) {
        mapping_allocation *a = o->arena;
        o->arena = a->next;
        free(a);
    }
    free(o);
    return MC_CLIENT_FONT_MAPPING_OK;
}

bool mc_client_font_mapping_inspect(const mc_client_font_mapping *o,
                                    mc_client_font_mapping_state *s) {
    if (o == NULL || s == NULL)
        return false;
    *s = (mc_client_font_mapping_state){o->phase, o->a,           o->b,     o->local,
                                        o->bytes, o->allocations, o->holds, o->failure};
    return true;
}

static mc_java_properties_status read_bytes(void *context, uint8_t *bytes, size_t capacity,
                                            size_t *count) {
    mc_client_font_mapping *o = context;
    if (bytes == NULL || count == NULL || capacity == 0 || !o->opened || o->closed ||
        o->stream.read == NULL)
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    const size_t remaining = FONT_MAPPING_INPUT - o->input_bytes;
    const size_t request = capacity > remaining ? remaining + 1 : capacity;
    size_t actual = 0;
    mc_java_properties_status status = o->stream.read(o->stream.context, bytes, request, &actual);
    if (actual > request || (status != MC_JAVA_PROPERTIES_OK && actual != 0))
        status = MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    if (status == MC_JAVA_PROPERTIES_OK && actual > remaining)
        status = MC_JAVA_PROPERTIES_LIMIT;
    o->failure.stream = status;
    if (status != MC_JAVA_PROPERTIES_OK) {
        *count = 0;
        return status;
    }
    o->input_bytes += actual;
    *count = actual;
    return MC_JAVA_PROPERTIES_OK;
}

static bool ready(void *context) {
    mc_client_font_mapping *o = context;
    return o->resources->ready != NULL && o->resources->ready(o->resources->context, &o->stream);
}

static mc_java_reader_status close_reader(void *context) {
    mc_client_font_mapping *o = context;
    if (o->closed)
        return MC_JAVA_READER_OK;
    if (o->resources->close == NULL)
        return MC_JAVA_READER_UNIMPLEMENTED_CLOSE;
    o->failure.close = o->resources->close(o->resources->context, &o->stream);
    o->closed = true;
    switch (o->failure.close) {
        case MC_REGISTRY_OK:
            return MC_JAVA_READER_OK;
        case MC_REGISTRY_IO_FAILED:
            return MC_JAVA_READER_IO_ERROR;
        case MC_REGISTRY_LIMIT:
            return MC_JAVA_READER_LIMIT;
        case MC_REGISTRY_ALLOCATION_FAILED:
            return MC_JAVA_READER_ALLOCATION_FAILED;
        case MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY:
            return MC_JAVA_READER_UNIMPLEMENTED_CLOSE;
        default:
            return MC_JAVA_READER_INVALID_ARGUMENT;
    }
}

static bool reader_result(mc_client_font_mapping *o, mc_java_reader_status status, int32_t pc) {
    o->failure.reader = status;
    if (status == MC_JAVA_READER_OK)
        return true;
    if (status == MC_JAVA_READER_IO_ERROR) {
        o->failure.source_exception_caught = true;
        return true;
    }
    const mc_client_font_mapping_status native =
        o->failure.status != MC_CLIENT_FONT_MAPPING_OK ? o->failure.status
        : status == MC_JAVA_READER_LIMIT || o->failure.stream == MC_JAVA_PROPERTIES_LIMIT
            ? MC_CLIENT_FONT_MAPPING_LIMIT
        : status == MC_JAVA_READER_ALLOCATION_FAILED ||
                o->failure.stream == MC_JAVA_PROPERTIES_ALLOCATION_FAILED
            ? MC_CLIENT_FONT_MAPPING_ALLOCATION_FAILED
        : status == MC_JAVA_READER_UNIMPLEMENTED_CLOSE
            ? MC_CLIENT_FONT_MAPPING_UNIMPLEMENTED_DEPENDENCY
            : MC_CLIENT_FONT_MAPPING_READER_FAILED;
    fail(o, native, "fp reached native reader failure", "a", pc);
    return false;
}

static bool load(mc_client_font_mapping *o) {
    if (mc_player_string_retain(o->empty) != MC_PLAYER_OK) {
        fail(o, MC_CLIENT_FONT_MAPPING_LIMIT, "fp initial String native hold", "a", 0);
        return false;
    }
    o->local = o->empty;
    if (o->resources->open == NULL) {
        fail(o, MC_CLIENT_FONT_MAPPING_UNIMPLEMENTED_DEPENDENCY, "fp real class-loader open", "a",
             16);
        return false;
    }
    o->failure.resource = o->resources->open(o->resources->context, "/font.txt", &o->stream);
    if (o->failure.resource == MC_REGISTRY_NULL_REFERENCE ||
        o->failure.resource == MC_REGISTRY_IO_FAILED) {
        o->failure.source_exception_caught = true;
        return true;
    }
    if (o->failure.resource != MC_REGISTRY_OK || o->stream.read == NULL) {
        fail(o,
             o->failure.resource == MC_REGISTRY_LIMIT ? MC_CLIENT_FONT_MAPPING_LIMIT
             : o->failure.resource == MC_REGISTRY_ALLOCATION_FAILED
                 ? MC_CLIENT_FONT_MAPPING_ALLOCATION_FAILED
                 : MC_CLIENT_FONT_MAPPING_UNIMPLEMENTED_DEPENDENCY,
             "fp real class-loader stream", "a", 16);
        return false;
    }
    o->opened = true;
    const mc_java_properties_allocator allocator = {o, allocate};
    const mc_java_reader_input input = {{o, read_bytes}, ready, close_reader};
    mc_java_reader *reader = NULL;
    const mc_java_reader_status created =
        mc_java_reader_create(&allocator, &input, MC_JAVA_READER_UTF8, &reader);
    if (!reader_result(o, created, 24))
        return false;
    for (;;) {
        const mc_java_reader_line *line = NULL;
        const mc_java_reader_status status = mc_java_reader_read_line(reader, &line);
        if (!reader_result(o, status, 34))
            return false;
        if (status == MC_JAVA_READER_IO_ERROR)
            return true; /* actual exception78; no finally close */
        if (line == NULL)
            break;
        if (line->length != 0 && line->units[0] == '#')
            continue;
        mc_player_string_view prefix;
        if (!mc_player_string_read(o->local, &prefix)) {
            fail(o, MC_CLIENT_FONT_MAPPING_INVALID_ARGUMENT, "fp live local String", "a", 57);
            return false;
        }
        if (line->length > FONT_MAPPING_UNITS - prefix.length) {
            fail(o, MC_CLIENT_FONT_MAPPING_LIMIT, "fp String unit native bound", "a", 64);
            return false;
        }
        const size_t length = prefix.length + line->length;
        uint16_t *joined = length == 0 ? NULL : allocate(o, length * sizeof(*joined));
        if (length != 0 && joined == NULL) {
            o->failure.pc = 64;
            return false;
        }
        if (prefix.length != 0)
            memcpy(joined, prefix.units, prefix.length * sizeof(*joined));
        if (line->length != 0)
            memcpy(joined + prefix.length, line->units, line->length * sizeof(*joined));
        mc_player_string *next = NULL;
        /* Pinned JDK17 StringBuilder.toString delegates empty Latin1 output
         * to StringLatin1.newString, which returns the canonical literal.
         * Nonempty concatenation still gains a distinct String identity. */
        mc_player_status copied;
        if (length == 0) {
            copied = mc_player_string_retain(o->empty);
            if (copied == MC_PLAYER_OK)
                next = o->empty;
        } else {
            copied = mc_player_string_create(joined, length, &next);
        }
        if (copied != MC_PLAYER_OK) {
            fail(o,
                 copied == MC_PLAYER_ALLOCATION_FAILED ? MC_CLIENT_FONT_MAPPING_ALLOCATION_FAILED
                                                       : MC_CLIENT_FONT_MAPPING_LIMIT,
                 "fp concatenated String native allocation", "a", 64);
            return false;
        }
        mc_player_string_release(o->local);
        o->local = next; /* actual local store67, only after complete concat */
    }
    o->failure.normal_close_reached = true;
    return reader_result(o, mc_java_reader_close(reader), 72);
}

mc_client_font_mapping_status
mc_client_font_mapping_active_use(mc_client_font_mapping *o, const mc_player_string **out,
                                  mc_client_font_mapping_error *error) {
    mc_client_font_mapping_error admission = {0};
    if (o == NULL || out == NULL || o->phase == MC_REGISTRY_INITIALIZING) {
        admission.status = o != NULL && o->phase == MC_REGISTRY_INITIALIZING
                               ? MC_CLIENT_FONT_MAPPING_BUSY
                               : MC_CLIENT_FONT_MAPPING_INVALID_ARGUMENT;
        if (error != NULL)
            *error = admission;
        return admission.status;
    }
    if (o->phase == MC_REGISTRY_FAILED) {
        admission = o->failure;
        admission.cause = o->failure.status;
        admission.status = MC_CLIENT_FONT_MAPPING_PREVIOUS_CLASS_FAILURE;
        if (error != NULL)
            *error = admission;
        return admission.status;
    }
    if (o->phase == MC_REGISTRY_COLD) {
        o->phase = MC_REGISTRY_INITIALIZING;
        if (!load(o))
            goto failed;
        o->a = o->local; /* actual fp.<clinit> putstatic a3 */
        o->local = NULL;
        o->b = allocate(o, 15u * sizeof(*o->b));
        if (o->b == NULL) {
            o->failure.method = "<clinit>";
            o->failure.pc = 8;
            goto failed;
        }
        static const uint16_t forbidden[15] = {'/', '\n', '\r', '\t', 0,   '\f', '`', '?',
                                               '*', '\\', '<',  '>',  '|', '"',  ':'};
        memcpy(o->b, forbidden, sizeof(forbidden));
        o->phase = MC_REGISTRY_READY;
    }
    *out = o->a;
    if (error != NULL)
        *error = o->failure;
    return MC_CLIENT_FONT_MAPPING_OK;
failed:
    o->phase = MC_REGISTRY_FAILED;
    if (error != NULL)
        *error = o->failure;
    return o->failure.status;
}
