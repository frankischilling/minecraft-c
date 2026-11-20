#include "source_screen_internal.h"

#include <limits.h>

static bool failed(mc_client_source_fu *o, mc_client_screen_error *e, mc_client_screen_status s,
                   const char *operation, unsigned int pc) {
    o->base.last_pc = pc;
    mc_screen_fail(e, s, operation, "fu", "<init>", "()V", pc);
    return false;
}

static void *allocate_at(mc_client_source_fu *o, size_t bytes, unsigned int pc,
                         mc_client_screen_error *e) {
    o->base.last_pc = pc;
    void *value = mc_screen_allocate(&o->base.memory, bytes);
    if (value == NULL)
        (void)failed(o, e, o->base.memory.allocation_status, "fu retained native allocation budget",
                     pc);
    return value;
}

static bool literal(mc_client_source_fu *o, mc_player_string **slot, const uint16_t *units,
                    size_t length, unsigned int pc, mc_client_screen_error *e) {
    if (*slot != NULL)
        return true;
    e->string = mc_player_string_create(units, length, slot);
    if (e->string != MC_PLAYER_OK)
        return failed(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "fu canonical literal resolution", pc);
    return true;
}

static bool catch_exception(mc_client_source_fu *o, mc_client_screen_exception kind,
                            unsigned int pc, mc_client_screen_error *e) {
    o->caught = kind;
    o->caught_pc = pc;
    e->operation = "fu caught actual source Exception";
    e->site = (mc_client_screen_site){"fu", "<init>", "()V", pc};
    e->native_policy = false;
    o->caught_cause = *e;
    o->base.last_pc = 114; /* Handler113 stores the exception, then return114. */
    return true;
}

static mc_java_properties_status read_input(void *context, uint8_t *destination, size_t capacity,
                                            size_t *count) {
    mc_client_source_fu *o = context;
    if (o->ready_status != MC_CLIENT_CLASS_STREAM_OK) {
        *count = 0;
        return MC_JAVA_PROPERTIES_INVALID_ARGUMENT;
    }
    o->input_error = (mc_client_image_io_error){0};
    o->input_status = o->input_reader->bytes.read(o->input_reader->bytes.context, destination,
                                                  capacity, count, &o->input_error);
    if (o->input_status == MC_CLIENT_IMAGE_IO_OK)
        return MC_JAVA_PROPERTIES_OK;
    /* Preserve complete typed source/native outcome separately. This lower
     * byte interface has one failure sentinel; it does not classify a catch. */
    return MC_JAVA_PROPERTIES_IO_ERROR;
}

static bool ready_input(void *context) {
    mc_client_source_fu *o = context;
    if (o->ready_status != MC_CLIENT_CLASS_STREAM_OK)
        return false;
    bool ready = false;
    o->ready_status = mc_client_class_stream_ready(o->stream, &ready, &o->ready_error);
    return o->ready_status == MC_CLIENT_CLASS_STREAM_OK && ready;
}

static void *reader_allocate(void *context, size_t bytes) {
    mc_client_source_fu *o = context;
    if (o->ready_status != MC_CLIENT_CLASS_STREAM_OK)
        return NULL;
    return mc_screen_allocate(&o->base.memory, bytes);
}

static bool string_value(mc_client_source_fu *o, const uint16_t *units, size_t length,
                         unsigned int pc, mc_player_string **out, mc_client_screen_error *e) {
    mc_screen_string_hold *hold = allocate_at(o, sizeof(*hold), pc, e);
    if (hold == NULL)
        return false;
    hold->next = o->strings;
    o->strings = hold;
    e->string = mc_player_string_create(units, length, &hold->value);
    if (e->string != MC_PLAYER_OK)
        return failed(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "fu actual String owner", pc);
    *out = hold->value;
    return true;
}

static bool trim_line(mc_client_source_fu *o, mc_player_string *line, mc_player_string **out,
                      mc_client_screen_error *e) {
    mc_player_string_view v;
    if (!mc_player_string_read(line, &v))
        return failed(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "fu live readLine String", 63);
    size_t first = 0, last = v.length;
    while (first < last && v.units[first] <= 0x20u)
        ++first;
    while (last > first && v.units[last - 1u] <= 0x20u)
        --last;
    if (first == 0 && last == v.length) {
        *out = line;
        return true;
    }
    if (first == last) {
        *out = (mc_player_string *)mc_player_empty_literal();
        return true;
    }
    return string_value(o, v.units + first, last - first, 63, out, e);
}

static bool append_splash(mc_client_source_fu *o, mc_player_string *value,
                          mc_client_screen_error *e) {
    mc_client_screen_list *list = o->splashes;
    list->mod_count =
        mc_java_i32_add(list->mod_count, 1); /* Actual ArrayList.add putfield7, before grow. */
    if (list->size >= (size_t)INT32_MAX)
        return failed(o, e, MC_CLIENT_SCREEN_LIMIT, "fu native List size bound", 76);
    if (list->size == list->capacity) {
        size_t capacity = list->capacity == 0 ? 10u : list->capacity + list->capacity / 2u;
        if (capacity <= list->size)
            capacity = list->size + 1u;
        if (capacity > SIZE_MAX / sizeof(*list->entries))
            return failed(o, e, MC_CLIENT_SCREEN_LIMIT, "fu List native array bound", 76);
        void **entries = allocate_at(o, capacity * sizeof(*entries), 76, e);
        if (entries == NULL)
            return false;
        for (size_t i = 0; i < list->size; ++i)
            entries[i] = list->entries[i];
        list->entries = entries;
        list->capacity = capacity;
    }
    list->entries[list->size++] = value; /* Actual List.add result is ignored. */
    return true;
}

bool mc_screen_menu_construct_locked(mc_client_source_fu *o, mc_client_screen_error *e) {
    static const uint16_t missingno[] = {'m', 'i', 's', 's', 'i', 'n', 'g', 'n', 'o'};
    static const uint16_t path[] = {'/', 't', 'i', 't', 'l', 'e', '/', 's', 'p', 'l',
                                    'a', 's', 'h', 'e', 's', '.', 't', 'x', 't'};
    mc_client_screen_classes *c = o->base.classes;
    o->fields.i = 0.0f;
    mc_screen_store(&o->base, "fu", 6);
    if (!literal(o, &c->missingno, missingno, sizeof(missingno) / sizeof(missingno[0]), 10, e))
        return false;
    o->fields.j = c->missingno;
    mc_screen_store(&o->base, "fu", 12);
    o->splashes = allocate_at(o, sizeof(*o->splashes), 15, e);
    if (o->splashes == NULL)
        return false;
    o->splashes->kind = MC_SCREEN_LIST_STRINGS;
    o->splashes->native_live = true;
    o->buffered_reader = allocate_at(o, sizeof(*o->buffered_reader), 23, e);
    if (o->buffered_reader == NULL)
        return false;
    o->input_reader = allocate_at(o, sizeof(*o->input_reader), 27, e);
    if (o->input_reader == NULL)
        return false;
    if (!literal(o, &c->splash_path, path, sizeof(path) / sizeof(path[0]), 33, e))
        return false;
    o->base.last_pc = 35;
    e->stream.status = mc_client_class_stream_open(
        c->resources, c->splash_path, o->base.memory.options.maximum_name_bytes,
        o->base.memory.options.maximum_read_bytes, &o->stream, &e->stream);
    if (e->stream.status != MC_CLIENT_CLASS_STREAM_OK)
        return failed(o, e, MC_CLIENT_SCREEN_STREAM_FAILED, "fu actual class resource stream", 35);
    /* Charset.forName("UTF-8")40 resolves the standard mandatory charset;
     * no byte decoding/read occurs here. The concrete IR binds input at43.
     * Distinct RAW BR23/IR27 identities stay retained on every failure. */
    o->base.last_pc = 43;
    if (o->stream == NULL)
        return catch_exception(o, MC_CLIENT_SCREEN_NULL_STREAM_EXCEPTION, 43, e);
    o->input_reader->bytes = mc_client_class_stream_image_input(o->stream);
    if (o->input_reader->bytes.is_null || o->input_reader->bytes.read == NULL)
        return failed(o, e, MC_CLIENT_SCREEN_STREAM_FAILED, "fu admitted real InputStream", 43);
    o->input_reader->constructed = true;
    o->base.last_pc = 46;
    const mc_java_properties_allocator allocator = {o, reader_allocate};
    const mc_java_reader_input input = {{o, read_input}, ready_input, NULL};
    e->reader =
        mc_java_reader_create(&allocator, &input, MC_JAVA_READER_UTF8, &o->buffered_reader->reader);
    if (e->reader != MC_JAVA_READER_OK)
        return failed(o, e, MC_CLIENT_SCREEN_READER_FAILED,
                      "fu actual shared BufferedReader/InputStreamReader kernel", 46);
    o->buffered_reader->constructed = true;
    o->last_line = (mc_player_string *)mc_player_empty_literal(); /* ldc50 */
    for (;;) {
        o->base.last_pc = 54;
        if (o->lines_read >= o->base.memory.options.maximum_lines)
            return failed(o, e, MC_CLIENT_SCREEN_LIMIT, "fu native readLine work limit", 54);
        ++o->lines_read;
        const mc_java_reader_line *line = NULL;
        o->input_status = MC_CLIENT_IMAGE_IO_OK;
        o->ready_status = MC_CLIENT_CLASS_STREAM_OK;
        o->ready_error = (mc_client_class_stream_error){0};
        o->input_error = (mc_client_image_io_error){0};
        e->reader = mc_java_reader_read_line(o->buffered_reader->reader, &line);
        if (o->ready_status != MC_CLIENT_CLASS_STREAM_OK) {
            e->stream = o->ready_error;
            return failed(o, e,
                          o->ready_status == MC_CLIENT_CLASS_STREAM_BUSY
                              ? MC_CLIENT_SCREEN_BUSY
                              : MC_CLIENT_SCREEN_STREAM_FAILED,
                          "fu typed native InputStream availability failure", 54);
        }
        if (e->reader != MC_JAVA_READER_OK) {
            (void)mc_client_class_stream_inspect(o->stream, &e->stream);
            e->input = o->input_error;
            if (e->reader == MC_JAVA_READER_IO_ERROR &&
                o->input_status == MC_CLIENT_IMAGE_IO_SOURCE_IO)
                return catch_exception(o, MC_CLIENT_SCREEN_READ_IO_EXCEPTION, 54, e);
            return failed(o, e, MC_CLIENT_SCREEN_READER_FAILED, "fu typed native readLine failure",
                          54);
        }
        if (line == NULL) {
            o->last_line = NULL;
            break;
        }
        /* The existing reader distinguishes its canonical empty literal by
         * NULL units. Fresh zero-length char[] String views have nonnull units. */
        if (line->length == 0 && line->units == NULL)
            o->last_line = (mc_player_string *)mc_player_empty_literal();
        else if (!string_value(o, line->units, line->length, 54, &o->last_line, e))
            return false;
        o->base.last_pc = 63;
        if (!trim_line(o, o->last_line, &o->last_trimmed, e))
            return false;
        mc_player_string_view trimmed;
        if (!mc_player_string_read(o->last_trimmed, &trimmed))
            return failed(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "fu trimmed String length", 68);
        if (trimmed.length != 0 && !append_splash(o, o->last_trimmed, e))
            return false;
    }
    /* Receiver j85 remains this same fu. Then live static Random87, size91,
     * nextInt96 and get99; only successful selection publishes j107. */
    o->base.last_pc = 96;
    int32_t index = 0;
    if (!mc_java_random_next_bounded(c->a, (int32_t)o->splashes->size, &index))
        return catch_exception(o, MC_CLIENT_SCREEN_EMPTY_SPLASH_EXCEPTION, 96, e);
    o->fields.j = o->splashes->entries[(size_t)index];
    mc_screen_store(&o->base, "fu", 107);
    o->base.last_pc = 114;
    return true;
}

bool mc_screen_menu_cleanup_locked(mc_client_source_fu *o, mc_client_screen_error *e) {
    e->stream.status = mc_client_class_stream_retire(&o->stream, &e->stream);
    if (e->stream.status == MC_CLIENT_CLASS_STREAM_OK)
        return true;
    mc_screen_fail(e,
                   e->stream.status == MC_CLIENT_CLASS_STREAM_BUSY ? MC_CLIENT_SCREEN_BUSY
                                                                   : MC_CLIENT_SCREEN_STREAM_FAILED,
                   "fu native retained open-stream retirement (not source close)", NULL, NULL, NULL,
                   0);
    return false;
}

void mc_screen_menu_strings_free(mc_client_source_fu *o) {
    for (mc_screen_string_hold *h = o->strings; h != NULL; h = h->next)
        mc_player_string_release(h->value);
}
