/* Ownership/entry points; the sole parser above handles both input modes. */
static mc_nbt_status nbt_stream_result(mc_nbt_stream_error *e, mc_nbt_status status, size_t at,
                                       const char *cl, const char *method, int pc) {
    if (e != NULL)
        *e = (mc_nbt_stream_error){
            .parse = {status, at}, .source_class = cl, .source_method = method, .source_pc = pc};
    return status;
}

mc_nbt_status mc_nbt_stream_allocate(size_t maximum, mc_nbt_stream **out, mc_nbt_stream_error *e) {
    if (out == NULL)
        return nbt_stream_result(e, MC_NBT_INVALID_ARGUMENT, 0, NULL, NULL, -1);
    if (maximum < sizeof(mc_nbt_stream))
        return nbt_stream_result(e, MC_NBT_LIMIT, 0, NULL, NULL, -1);
    mc_nbt_stream *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return nbt_stream_result(e, MC_NBT_ALLOCATION_FAILED, 0, NULL, NULL, -1);
    s->maximum_owner_bytes = maximum;
    s->owner_bytes = sizeof(*s);
    *out = s;
    return nbt_stream_result(e, MC_NBT_OK, 0, NULL, NULL, -1);
}

static mc_nbt_status nbt_stream_constructor_child(mc_nbt_stream *s, size_t count, size_t size,
                                                  void **out, mc_nbt_stream_error *e, int pc) {
    if (size != 0 && count > SIZE_MAX / size)
        return nbt_stream_result(e, MC_NBT_LIMIT, 0, "java/io/DataInputStream",
                                 "<init>(Ljava/io/InputStream;)V", pc);
    size_t n = count * size;
    if (n > s->maximum_owner_bytes - s->owner_bytes)
        return nbt_stream_result(e, MC_NBT_LIMIT, 0, "java/io/DataInputStream",
                                 "<init>(Ljava/io/InputStream;)V", pc);
    void *memory = calloc(count, size);
    if (memory == NULL)
        return nbt_stream_result(e, MC_NBT_ALLOCATION_FAILED, 0, "java/io/DataInputStream",
                                 "<init>(Ljava/io/InputStream;)V", pc);
    s->owner_bytes += n;
    *out = memory;
    return MC_NBT_OK;
}

mc_nbt_status mc_nbt_stream_construct(mc_nbt_stream *s, const mc_nbt_byte_input *input,
                                      const mc_nbt_limits *requested, mc_nbt_stream_error *error) {
    mc_nbt_stream_error local;
    mc_nbt_stream_error *e = error == NULL ? &local : error;
    if (s == NULL || input == NULL)
        return nbt_stream_result(e, MC_NBT_INVALID_ARGUMENT, 0, NULL, NULL, -1);
    if (s->active)
        return nbt_stream_result(e, MC_NBT_STREAM_BUSY, 0, NULL, NULL, -1);
    if (s->phase != MC_NBT_STREAM_RAW)
        return nbt_stream_result(e, MC_NBT_STREAM_STATE, 0, NULL, NULL, -1);
    s->active = true;
    s->phase = MC_NBT_STREAM_CONSTRUCTING;
    s->limits = requested == NULL ? mc_nbt_default_limits() : *requested;
    mc_nbt_status status;
    if (!nbt_limits_valid(&s->limits)) {
        status = nbt_stream_result(e, MC_NBT_INVALID_ARGUMENT, 0, NULL, NULL, -1);
        goto failed;
    }
    s->input = *input;
    s->last_data_store = 6; /* actual Filter.in, borrowed under containing owner hold */
    void *memory = NULL;
    status = nbt_stream_constructor_child(s, 80, 1, &memory, e, 8);
    if (status != MC_NBT_OK)
        goto failed;
    s->bytearr = memory;
    s->byte_capacity = 80;
    s->last_data_store = 10;
    status = nbt_stream_constructor_child(s, 80, sizeof(uint16_t), &memory, e, 16);
    if (status != MC_NBT_OK)
        goto failed;
    s->chararr = memory;
    s->char_capacity = 80;
    s->last_data_store = 18;
    status = nbt_stream_constructor_child(s, 8, 1, &memory, e, 24);
    if (status != MC_NBT_OK)
        goto failed;
    s->read_buffer = memory;
    s->last_data_store = 26;
    s->constructed = true;
    s->phase = MC_NBT_STREAM_READY;
    s->active = false;
    return nbt_stream_result(e, MC_NBT_OK, 0, NULL, NULL, -1);
failed:
    s->phase = MC_NBT_STREAM_FAILED;
    s->active = false;
    s->error = *e;
    return status;
}

mc_nbt_status mc_nbt_stream_read_long(mc_nbt_stream *s, int64_t *out, mc_nbt_stream_error *error) {
    if (s == NULL || out == NULL)
        return nbt_stream_result(error, MC_NBT_INVALID_ARGUMENT, 0, NULL, NULL, -1);
    if (s->active)
        return nbt_stream_result(error, MC_NBT_STREAM_BUSY, s->position, NULL, NULL, -1);
    if (s->phase != MC_NBT_STREAM_READY)
        return nbt_stream_result(error, MC_NBT_STREAM_STATE, s->position, NULL, NULL, -1);
    s->active = true;
    s->error = (mc_nbt_stream_error){0};
    nbt_reader reader = {.limits = s->limits,
                         .position = s->position,
                         .error = {MC_NBT_OK, s->position},
                         .stream = s};
    nbt_stream_context(&reader, "java/io/DataInputStream", "readLong()J", 8);
    uint64_t bits;
    if (!nbt_read_u64(&reader, &bits)) {
        s->active = false;
        if (error != NULL)
            *error = s->error;
        return s->error.parse.status;
    }
    *out = bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(UINT64_MAX - bits);
    s->active = false;
    return nbt_stream_result(error, MC_NBT_OK, s->position, NULL, NULL, -1);
}

mc_nbt_status mc_nbt_stream_parse(mc_nbt_stream *s, bool compound, mc_nbt_document **out,
                                  mc_nbt_stream_error *error) {
    if (s == NULL || out == NULL)
        return nbt_stream_result(error, MC_NBT_INVALID_ARGUMENT, 0, NULL, NULL, -1);
    if (s->active)
        return nbt_stream_result(error, MC_NBT_STREAM_BUSY, s->position, NULL, NULL, -1);
    if (s->phase != MC_NBT_STREAM_READY)
        return nbt_stream_result(error, MC_NBT_STREAM_STATE, s->position, NULL, NULL, -1);
    s->active = true;
    s->phase = MC_NBT_STREAM_PARSING;
    s->error = (mc_nbt_stream_error){0};
    if (sizeof(mc_nbt_document) > s->limits.max_allocation_bytes) {
        s->error.parse = (mc_nbt_error){MC_NBT_LIMIT, 0};
        goto failed;
    }
    s->document = calloc(1, sizeof(*s->document));
    if (s->document == NULL) {
        s->error.parse = (mc_nbt_error){MC_NBT_ALLOCATION_FAILED, 0};
        goto failed;
    }
    s->document->arena.bytes = sizeof(*s->document);
    s->document->arena.limit = s->limits.max_allocation_bytes;
    s->document->holds = 1;
    nbt_reader reader = {
        .limits = s->limits, .document = s->document, .error = {MC_NBT_OK, 0}, .stream = s};
    nbt_stream_context(&reader, "ij", "b(Ljava/io/DataInput;)Lij;", 1);
    uint8_t type;
    mc_nbt_tag *root = NULL;
    if (nbt_read_u8(&reader, &type)) {
        nbt_stream_context(&reader, "ij", "b(Ljava/io/DataInput;)Lij;",
                           type == MC_NBT_END ? 11 : 20);
        root = nbt_read_tag(&reader, type, true, 1);
    }
    s->position = reader.position;
    if (reader.error.status == MC_NBT_OK && compound && root->type != MC_NBT_COMPOUND) {
        nbt_stream_context(&reader, "as", "a(Ljava/io/DataInput;)Lnu;", 23);
        nbt_stream_fault(&reader, MC_NBT_ROOT_TYPE, MC_NBT_SOURCE_ROOT);
    }
    if (reader.error.status != MC_NBT_OK) {
        if (s->error.parse.status == MC_NBT_OK)
            s->error.parse = reader.error;
        goto failed;
    }
    s->document->root = root;
    /* Reader keeps its own hold so native partial/projection state remains
     * live even if the independently owned output occurrence is released. */
    if (mc_nbt_document_retain(s->document) != MC_NBT_OK) {
        s->error.parse = (mc_nbt_error){MC_NBT_LIMIT, s->position};
        goto failed;
    }
    *out = s->document;
    s->phase = MC_NBT_STREAM_COMPLETE;
    s->active = false;
    return nbt_stream_result(error, MC_NBT_OK, s->position, NULL, NULL, -1);
failed:
    s->phase = MC_NBT_STREAM_FAILED;
    s->active = false;
    if (error != NULL)
        *error = s->error;
    return s->error.parse.status;
}

mc_nbt_status mc_nbt_stream_source_close(mc_nbt_stream *s, mc_nbt_stream_error *e) {
    if (s == NULL)
        return nbt_stream_result(e, MC_NBT_INVALID_ARGUMENT, 0, NULL, NULL, -1);
    if (s->active)
        return nbt_stream_result(e, MC_NBT_STREAM_BUSY, s->position, NULL, NULL, -1);
    if (!s->constructed || s->phase == MC_NBT_STREAM_RETIRING)
        return nbt_stream_result(e, MC_NBT_STREAM_STATE, s->position, NULL, NULL, -1);
    s->active = true;
    ++s->close_calls;
    mc_nbt_input_error input = {0};
    mc_nbt_input_status status;
    if (s->input.context == NULL) {
        s->active = false;
        nbt_stream_result(e, MC_NBT_SOURCE_FAILURE, s->position, "java/io/FilterInputStream",
                          "close()V", 4);
        if (e != NULL)
            e->source_fault = MC_NBT_SOURCE_NULL_INPUT;
        return MC_NBT_SOURCE_FAILURE;
    }
    if (s->input.close == NULL) {
        s->active = false;
        return nbt_stream_result(e, MC_NBT_INPUT_ERROR, s->position, "java/io/FilterInputStream",
                                 "close()V", 4);
    }
    status = s->input.close(s->input.context, &input);
    s->active = false;
    if (status != MC_NBT_INPUT_OK) {
        nbt_stream_result(e, MC_NBT_INPUT_ERROR, s->position, "java/io/FilterInputStream",
                          "close()V", 4);
        if (e != NULL) {
            e->input = input;
            e->source_fault = input.source_exception ? MC_NBT_SOURCE_INPUT : MC_NBT_SOURCE_NONE;
        }
        return MC_NBT_INPUT_ERROR;
    }
    return nbt_stream_result(e, MC_NBT_OK, s->position, NULL, NULL, -1);
}

mc_nbt_status mc_nbt_stream_state_read(const mc_nbt_stream *s, mc_nbt_stream_state *out,
                                       mc_nbt_stream_error *e) {
    if (s == NULL || out == NULL)
        return nbt_stream_result(e, MC_NBT_INVALID_ARGUMENT, 0, NULL, NULL, -1);
    *out = (mc_nbt_stream_state){
        .phase = s->phase,
        .active = s->active,
        .position = s->position,
        .read_calls = s->read_calls,
        .close_calls = s->close_calls,
        .owner_bytes = s->owner_bytes,
        .source_in = s->input.context,
        .bytearr = s->bytearr,
        .read_buffer = s->read_buffer,
        .chararr = s->chararr,
        .bytearr_capacity = s->byte_capacity,
        .chararr_capacity = s->char_capacity,
        .last_data_input_store = s->last_data_store,
        .last_data_input_store_class =
            s->last_data_store == 6 ? "java/io/FilterInputStream" : "java/io/DataInputStream",
        .candidate = s->document,
        .last_tag = s->last_tag == NULL ? NULL : s->last_tag->tag,
        .pending_tag = s->pending_tag,
        .last_collection = s->last_collection,
        .last_collection_count = s->last_collection == NULL ? 0 : s->last_collection->count,
        .last_array = s->last_array,
        .last_array_length = s->last_array_length,
        .last_tag_store = s->last_tag == NULL ? 0 : s->last_tag->last_store,
        .last_tag_store_class = s->last_tag == NULL ? NULL : s->last_tag->last_store_class,
        .last_collection_operation_pc = s->last_collection_operation};
    return nbt_stream_result(e, MC_NBT_OK, s->position, NULL, NULL, -1);
}

mc_nbt_status mc_nbt_stream_release(mc_nbt_stream **slot, mc_nbt_stream_error *e) {
    if (slot == NULL)
        return nbt_stream_result(e, MC_NBT_INVALID_ARGUMENT, 0, NULL, NULL, -1);
    mc_nbt_stream *s = *slot;
    if (s == NULL)
        return nbt_stream_result(e, MC_NBT_OK, 0, NULL, NULL, -1);
    if (s->active)
        return nbt_stream_result(e, MC_NBT_STREAM_BUSY, s->position, NULL, NULL, -1);
    s->phase = MC_NBT_STREAM_RETIRING;
    s->active = true;
    /* Native cleanup NEVER manufactures source Filter.close/finally. The real
     * concrete input is borrowed and still held by the containing owner. */
    mc_nbt_document_free(s->document);
    free(s->bytearr);
    free(s->chararr);
    free(s->read_buffer);
    *slot = NULL;
    free(s);
    return nbt_stream_result(e, MC_NBT_OK, 0, NULL, NULL, -1);
}

mc_nbt_status mc_nbt_stream_read_array(mc_nbt_stream *s, uint8_t *bytes, int32_t length,
                                       bool is_null, int32_t *out, mc_nbt_stream_error *error) {
    if (s == NULL || out == NULL || length < 0 || (!is_null && length != 0 && bytes == NULL) ||
        (is_null && (bytes != NULL || length != 0)))
        return nbt_stream_result(error, MC_NBT_INVALID_ARGUMENT, 0, NULL, NULL, -1);
    if (s->active)
        return nbt_stream_result(error, MC_NBT_STREAM_BUSY, s->position, NULL, NULL, -1);
    if (s->phase != MC_NBT_STREAM_READY)
        return nbt_stream_result(error, MC_NBT_STREAM_STATE, s->position, NULL, NULL, -1);
    s->active = true;
    s->error = (mc_nbt_stream_error){0};
    /* Actual getfield in1, before evaluating b.length7. */
    const mc_nbt_byte_input captured = s->input;
    mc_nbt_stream_error *e = &s->error;
    if (is_null) {
        nbt_stream_result(e, MC_NBT_SOURCE_FAILURE, s->position, "java/io/DataInputStream",
                          "read([B)I", 7);
        e->source_fault = MC_NBT_SOURCE_NULL_ARRAY;
        goto done;
    }
    if (captured.context == NULL) {
        nbt_stream_result(e, MC_NBT_SOURCE_FAILURE, s->position, "java/io/DataInputStream",
                          "read([B)I", 8);
        e->source_fault = MC_NBT_SOURCE_NULL_INPUT;
        goto done;
    }
    if (captured.read == NULL) {
        nbt_stream_result(e, MC_NBT_INPUT_ERROR, s->position, "java/io/DataInputStream",
                          "read([B)I", 8);
        goto done;
    }
    if ((size_t)length > s->limits.max_input_bytes - s->position || s->read_calls == SIZE_MAX) {
        nbt_stream_result(e, MC_NBT_LIMIT, s->position, "java/io/DataInputStream", "read([B)I", 8);
        goto done;
    }
    ++s->read_calls;
    mc_nbt_input_result got = {0};
    mc_nbt_input_error child = {0};
    static uint8_t empty;
    mc_nbt_input_status status =
        captured.read(captured.context, length == 0 ? &empty : bytes, (size_t)length, &got, &child);
    if (status != MC_NBT_INPUT_OK) {
        nbt_stream_result(e, MC_NBT_INPUT_ERROR, s->position, "java/io/DataInputStream",
                          "read([B)I", 8);
        e->input = child;
        e->source_fault = child.source_exception ? MC_NBT_SOURCE_INPUT : MC_NBT_SOURCE_NONE;
        goto done;
    }
    if (got.count > (size_t)length || (got.eof && got.count != 0)) {
        nbt_stream_result(e, MC_NBT_INPUT_ERROR, s->position, "java/io/DataInputStream",
                          "read([B)I", 8);
        goto done;
    }
    s->position += got.count;
    *out = got.eof ? -1 : (int32_t)got.count;
    nbt_stream_result(e, MC_NBT_OK, s->position, NULL, NULL, -1);
done:
    s->active = false;
    if (error != NULL)
        *error = *e;
    return e->parse.status;
}
