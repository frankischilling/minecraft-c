/* Source acquisition helpers for the SAME reader/tag dispatcher. */
static void nbt_stream_context(nbt_reader *r, const char *cl, const char *method, int pc) {
    if (r->stream != NULL) {
        r->stream->context_class = cl;
        r->stream->context_method = method;
        r->stream->context_pc = pc;
    }
}

static bool nbt_stream_fault(nbt_reader *r, mc_nbt_status status, mc_nbt_source_fault fault) {
    bool first = r->error.status == MC_NBT_OK;
    nbt_reader_fail(r, status, r->position);
    mc_nbt_stream *s = r->stream;
    if (first) {
        s->error.parse = r->error;
        s->error.source_fault = fault;
        s->error.source_class = s->context_class;
        s->error.source_method = s->context_method;
        s->error.source_pc = s->context_pc;
    }
    return false;
}

static bool nbt_stream_port_failure(nbt_reader *r, mc_nbt_input_error error) {
    nbt_stream_fault(r, MC_NBT_INPUT_ERROR,
                     error.source_exception ? MC_NBT_SOURCE_INPUT : MC_NBT_SOURCE_NONE);
    r->stream->error.input = error;
    return false;
}

/* EOF itself is returned as -1. readInt/readShort must finish all of their
 * single-byte calls before the JDK EOF test; an actual IOException stops now. */
static bool nbt_stream_one(nbt_reader *r, int *out) {
    mc_nbt_stream *s = r->stream;
    if (r->position == r->limits.max_input_bytes)
        return nbt_stream_fault(r, MC_NBT_LIMIT, MC_NBT_SOURCE_NONE);
    if (s->input.context == NULL)
        return nbt_stream_fault(r, MC_NBT_SOURCE_FAILURE, MC_NBT_SOURCE_NULL_INPUT);
    if (s->input.read_one == NULL)
        return nbt_stream_fault(r, MC_NBT_INPUT_ERROR, MC_NBT_SOURCE_NONE);
    ++s->read_calls;
    mc_nbt_input_error error = {0};
    int value;
    mc_nbt_input_status status = s->input.read_one(s->input.context, &value, &error);
    if (status != MC_NBT_INPUT_OK)
        return nbt_stream_port_failure(r, error);
    if (value < -1 || value > 255)
        return nbt_stream_fault(r, MC_NBT_INPUT_ERROR, MC_NBT_SOURCE_NONE);
    if (value >= 0) {
        ++r->position;
        s->position = r->position;
    }
    *out = value;
    return true;
}

static bool nbt_stream_fully(nbt_reader *r, uint8_t *out, size_t count) {
    mc_nbt_stream *s = r->stream;
    size_t done = 0;
    while (done < count) {
        size_t request = count - done;
        if (request > r->limits.max_input_bytes - r->position)
            return nbt_stream_fault(r, MC_NBT_LIMIT, MC_NBT_SOURCE_NONE);
        if (s->input.context == NULL)
            return nbt_stream_fault(r, MC_NBT_SOURCE_FAILURE, MC_NBT_SOURCE_NULL_INPUT);
        if (s->input.read == NULL)
            return nbt_stream_fault(r, MC_NBT_INPUT_ERROR, MC_NBT_SOURCE_NONE);
        ++s->read_calls;
        mc_nbt_input_result got = {0};
        mc_nbt_input_error error = {0};
        mc_nbt_input_status status =
            s->input.read(s->input.context, out + done, request, &got, &error);
        if (status != MC_NBT_INPUT_OK)
            return nbt_stream_port_failure(r, error);
        if (got.count > request || (got.eof && got.count != 0))
            return nbt_stream_fault(r, MC_NBT_INPUT_ERROR, MC_NBT_SOURCE_NONE);
        if (got.eof)
            return nbt_stream_fault(r, MC_NBT_TRUNCATED, MC_NBT_SOURCE_EOF);
        if (got.count == 0)
            return nbt_stream_fault(r, MC_NBT_INPUT_ERROR, MC_NBT_SOURCE_NONE);
        done += got.count;
        r->position += got.count;
        s->position = r->position;
    }
    return true;
}

static bool nbt_stream_small(nbt_reader *r, uint8_t *bytes, size_t count) {
    bool eof = false;
    for (size_t i = 0; i < count; ++i) {
        int c;
        if (!nbt_stream_one(r, &c))
            return false;
        if (c < 0)
            eof = true;
        else
            bytes[i] = (uint8_t)c;
    }
    return !eof || nbt_stream_fault(r, MC_NBT_TRUNCATED, MC_NBT_SOURCE_EOF);
}

static bool nbt_stream_scratch(nbt_reader *r, size_t count, size_t size, void **out,
                               int allocation_pc) {
    mc_nbt_stream *s = r->stream;
    if (size != 0 && count > SIZE_MAX / size)
        return nbt_stream_fault(r, MC_NBT_LIMIT, MC_NBT_SOURCE_NONE);
    size_t bytes = count * size;
    if (bytes > s->maximum_owner_bytes - s->owner_bytes) {
        nbt_stream_context(r, "java/io/DataInputStream", "readUTF", allocation_pc);
        return nbt_stream_fault(r, MC_NBT_LIMIT, MC_NBT_SOURCE_NONE);
    }
    void *p = calloc(count, size);
    if (p == NULL) {
        nbt_stream_context(r, "java/io/DataInputStream", "readUTF", allocation_pc);
        return nbt_stream_fault(r, MC_NBT_ALLOCATION_FAILED, MC_NBT_SOURCE_NONE);
    }
    s->owner_bytes += bytes;
    *out = p;
    return true;
}

static bool nbt_stream_string(nbt_reader *r, mc_nbt_string *string) {
    mc_nbt_stream *s = r->stream;
    uint8_t length_bytes[2] = {0};
    if (!nbt_stream_small(r, length_bytes, 2))
        return false;
    size_t length = (size_t)length_bytes[0] * 256u + length_bytes[1];
    if (s->byte_capacity < length) {
        void *memory = NULL;
        size_t next = length * 2u;
        if (!nbt_stream_scratch(r, next, 1, &memory, 39))
            return false;
        uint8_t *old = s->bytearr;
        size_t old_bytes = s->byte_capacity;
        s->bytearr = memory;
        s->byte_capacity = next;
        s->last_data_store = 41;
        free(old);
        s->owner_bytes -= old_bytes;
        if (!nbt_stream_scratch(r, next, sizeof(uint16_t), &memory, 49))
            return false;
        uint16_t *old_chars = s->chararr;
        old_bytes = s->char_capacity * sizeof(*old_chars);
        s->chararr = memory;
        s->char_capacity = next;
        s->last_data_store = 51;
        free(old_chars);
        s->owner_bytes -= old_bytes;
    }
    if (!nbt_stream_fully(r, s->bytearr, length))
        return false;
    mc_mutf8_prefix progress;
    mc_mutf8_status status =
        mc_mutf8_decode_prefix(s->bytearr, length, s->chararr, s->char_capacity, &progress);
    if (status != MC_MUTF8_OK) {
        nbt_stream_fault(
            r, status == MC_MUTF8_INVALID_ENCODING ? MC_NBT_INVALID_UTF : MC_NBT_INPUT_ERROR,
            status == MC_MUTF8_INVALID_ENCODING ? MC_NBT_SOURCE_UTF : MC_NBT_SOURCE_NONE);
        s->error.utf_error_byte = progress.error_byte;
        s->error.utf_prefix_units = progress.written;
        s->error.utf_partial_character = progress.partial_character;
        return false;
    }
    uint16_t *units = NULL;
    if (progress.written != 0) {
        units = nbt_reader_allocate(r, progress.written, sizeof(*units));
        if (units == NULL)
            return false;
        memcpy(units, s->chararr, progress.written * sizeof(*units));
    }
    *string = (mc_nbt_string){units, progress.written};
    return true;
}

static nbt_source_tag *nbt_stream_record(nbt_reader *r, const mc_nbt_tag *tag) {
    for (nbt_source_tag *p = r->stream->records; p != NULL; p = p->next)
        if (p->tag == tag)
            return p;
    return NULL;
}

static const char *const nbt_source_classes[] = {"lh", "qp", "ul", "pp", "mi", "p",
                                                 "sz", "hn", "xb", "sp", "nu"};
/* Explicit unchanged ij.a(B) Source opcode tables, not an inferred stride. */
static const int nbt_source_new_pc[] = {60, 68, 76, 84, 92, 100, 108, 116, 124, 132, 140};
static const int nbt_source_ctor_pc[] = {64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144};

static void nbt_stream_factory_context(nbt_reader *r, uint8_t type, bool constructor) {
    mc_nbt_stream *s = r->stream;
    if (s == NULL)
        return;
    /* ij.b special-cases named End; compound End uses that same branch. */
    if (type == MC_NBT_END && s->context_pc == 11 && s->context_class != NULL &&
        strcmp(s->context_class, "ij") == 0)
        nbt_stream_context(r, "ij", "b(Ljava/io/DataInput;)Lij;", constructor ? 15 : 11);
    else
        nbt_stream_context(r, "ij", "a(B)Lij;",
                           constructor ? nbt_source_ctor_pc[type] : nbt_source_new_pc[type]);
}

static nbt_source_collection *nbt_stream_collection(nbt_reader *r, nbt_source_tag *record,
                                                    bool list, int store_pc) {
    if (store_pc == 12)
        nbt_stream_context(r, nbt_source_classes[record->tag->type], "<init>()V", 5);
    nbt_source_collection *c = nbt_reader_allocate(r, 1, sizeof(*c));
    if (c == NULL)
        return NULL;
    c->previous = record->collection;
    c->tag = record->tag;
    c->list = list;
    record->collection = c;
    record->last_store = store_pc;
    record->last_store_class = nbt_source_classes[record->tag->type];
    r->stream->last_collection = c;
    r->stream->last_collection_operation = 0;
    return c;
}

static bool nbt_stream_tag_created(nbt_reader *r, mc_nbt_tag *tag) {
    nbt_source_tag *p = nbt_reader_allocate(r, 1, sizeof(*p));
    if (p == NULL)
        return false;
    p->tag = tag;
    p->last_store = 6;
    p->last_store_class = "ij";
    p->next = r->stream->records;
    r->stream->records = p;
    r->stream->last_tag = p;
    if (r->document->root == NULL)
        r->document->root = tag; /* native raw candidate only */
    if (tag->type == MC_NBT_LIST || tag->type == MC_NBT_COMPOUND)
        return nbt_stream_collection(r, p, tag->type == MC_NBT_LIST, 12) != NULL;
    return true;
}

static void nbt_stream_tag_store(nbt_reader *r, mc_nbt_tag *tag, const char *cl, int pc) {
    if (r->stream == NULL)
        return;
    nbt_source_tag *p = nbt_stream_record(r, tag);
    if (p != NULL) {
        p->last_store = pc;
        p->last_store_class = cl;
        r->stream->last_tag = p;
    }
}

static bool nbt_stream_list_append(nbt_reader *r, nbt_source_collection *c, const mc_nbt_tag *tag) {
    if (c->count == c->capacity) {
        size_t next = c->capacity == 0 ? 10u : c->capacity + c->capacity / 2u;
        if (next <= c->capacity || next > SIZE_MAX / sizeof(*c->items))
            return nbt_stream_fault(r, MC_NBT_LIMIT, MC_NBT_SOURCE_NONE);
        const mc_nbt_tag **items = nbt_reader_allocate(r, next, sizeof(*items));
        if (items == NULL)
            return false;
        if (c->count != 0)
            memcpy(items, c->items, c->count * sizeof(*items));
        c->items = items;
        c->capacity = next;
    }
    c->items[c->count++] = tag;
    r->stream->last_collection = c;
    r->stream->last_collection_operation = 56;
    return true;
}
