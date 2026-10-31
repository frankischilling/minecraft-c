/* Included once by storage.c: the source manager uses the SAME collections. */
struct saved_source {
    mc_world_saved_data_source_state state;
    mc_world_saved_data_source_error failure, cleanup, work;
    mc_nbt_document *document;
};

static const char *saved_source_class(const mc_world_saved_data *owner) {
    return owner->side == MC_WORLD_INFO_CLIENT ? "hc" : "es";
}

static mc_world_storage_status saved_source_result(mc_world_saved_data_source_error *error,
                                                   mc_world_storage_status status,
                                                   const char *operation) {
    if (error != NULL)
        *error = (mc_world_saved_data_source_error){.status = status,
                                                    .cause_status = status,
                                                    .operation = operation,
                                                    .native_policy = status != MC_WORLD_STORAGE_OK};
    return status;
}

static mc_world_source_site saved_source_site(const mc_world_saved_data *owner, const char *method,
                                              int pc) {
    return (mc_world_source_site){
        saved_source_class(owner), method,
        strcmp(method, "<init>") == 0
            ? (owner->side == MC_WORLD_INFO_CLIENT ? "(Lwt;)V" : "(Lom;)V")
            : "()V",
        pc};
}

static mc_world_storage_status saved_source_fail(mc_world_saved_data *owner,
                                                 mc_world_storage_status status,
                                                 const char *operation, const char *method, int pc,
                                                 bool source_exception,
                                                 mc_world_saved_data_source_error *error) {
    saved_source *s = owner->source;
    s->work.status = s->work.cause_status = status;
    s->work.operation = operation;
    s->work.site = saved_source_site(owner, method, pc);
    s->work.native_policy = !source_exception;
    s->work.source_exception = source_exception;
    if (source_exception) {
        /* The actual Exception handler requires a real Throwable/printing
         * owner. No normal-return or source-close credit is invented here. */
        s->work.cause_site = s->work.site;
        s->work.site = saved_source_site(owner, "b", 152);
        s->work.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
        s->work.operation = "java/lang/Exception.printStackTrace()V";
        s->work.caught_exception = s->work.diagnostic_frontier = true;
        s->state.last_body_pc = 152;
    }
    s->failure = s->work;
    s->state.phase = MC_WORLD_SAVED_DATA_SOURCE_FAILED;
    s->state.active = false;
    if (error != NULL)
        *error = s->failure;
    return s->failure.status;
}

static mc_world_storage_status saved_source_file_status(mc_java_file_status status) {
    return status == MC_JAVA_FILE_LIMIT               ? MC_WORLD_STORAGE_LIMIT
           : status == MC_JAVA_FILE_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : status == MC_JAVA_FILE_BUSY              ? MC_WORLD_STORAGE_BUSY
           : status == MC_JAVA_FILE_IO_ERROR          ? MC_WORLD_STORAGE_IO_ERROR
                                                      : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status saved_source_handler_status(mc_world_source_status status) {
    return status == MC_WORLD_SOURCE_LIMIT              ? MC_WORLD_STORAGE_LIMIT
           : status == MC_WORLD_SOURCE_ALLOCATION       ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : status == MC_WORLD_SOURCE_BUSY             ? MC_WORLD_STORAGE_BUSY
           : status == MC_WORLD_SOURCE_SOURCE_EXCEPTION ? MC_WORLD_STORAGE_INVALID_DATA
                                                        : MC_WORLD_STORAGE_UNSUPPORTED_STATE;
}

static mc_world_storage_status saved_source_nbt_status(mc_nbt_status status) {
    return status == MC_NBT_LIMIT               ? MC_WORLD_STORAGE_LIMIT
           : status == MC_NBT_ALLOCATION_FAILED ? MC_WORLD_STORAGE_ALLOCATION_FAILED
           : status == MC_NBT_STREAM_BUSY       ? MC_WORLD_STORAGE_BUSY
                                                : MC_WORLD_STORAGE_INVALID_DATA;
}

static mc_nbt_input_status saved_source_fis_error(mc_java_file_status status,
                                                  mc_java_file_error detail,
                                                  mc_nbt_input_error *error) {
    if (error != NULL)
        *error = (mc_nbt_input_error){.transport_status = (int)status,
                                      .system_error = detail.system_error,
                                      .source_exception = status == MC_JAVA_FILE_IO_ERROR,
                                      .operation = detail.operation};
    return status == MC_JAVA_FILE_OK ? MC_NBT_INPUT_OK : MC_NBT_INPUT_FAILED;
}

static mc_nbt_input_status saved_source_fis_one(void *context, int *out,
                                                mc_nbt_input_error *error) {
    mc_java_file_error detail = {0};
    mc_java_file_status status = mc_java_file_stream_read_byte(context, out, &detail);
    return saved_source_fis_error(status, detail, error);
}

static mc_nbt_input_status saved_source_fis_read(void *context, uint8_t *bytes, size_t capacity,
                                                 mc_nbt_input_result *out,
                                                 mc_nbt_input_error *error) {
    mc_java_file_error detail = {0};
    mc_java_file_read result = {0};
    mc_java_file_status status =
        mc_java_file_stream_read(context, bytes, capacity, &result, &detail);
    if (status == MC_JAVA_FILE_OK)
        *out = (mc_nbt_input_result){result.count, result.eof};
    return saved_source_fis_error(status, detail, error);
}

static mc_nbt_input_status saved_source_fis_close(void *context, mc_nbt_input_error *error) {
    mc_java_file_error detail = {0};
    mc_java_file_status status = mc_java_file_stream_source_close(context, &detail);
    return saved_source_fis_error(status, detail, error);
}

static mc_world_storage_status saved_source_guard(const mc_world_saved_data *owner, bool io,
                                                  mc_world_storage_error *error) {
    if (owner->source == NULL)
        return MC_WORLD_STORAGE_OK;
    if (owner->source->state.active)
        return saved_error(error, MC_WORLD_STORAGE_BUSY, "SavedData source call active");
    if (owner->source->state.phase != MC_WORLD_SAVED_DATA_SOURCE_READY)
        return saved_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "SavedData source constructor has not returned");
    if (io && owner->source->state.source_a != NULL)
        return saved_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "SavedData source descendant file writer/reader dependency");
    return MC_WORLD_STORAGE_OK;
}

mc_world_storage_status mc_world_saved_data_source_allocate(
    mc_world_info_side side, const mc_world_saved_data_limits *limits, mc_world_saved_data **out,
    mc_world_saved_data_source_error *error) {
    mc_world_saved_data_limits chosen =
        limits == NULL ? mc_world_saved_data_default_limits() : *limits;
    if (out == NULL || (side != MC_WORLD_INFO_CLIENT && side != MC_WORLD_INFO_SERVER) ||
        chosen.nbt.max_depth == 0 || chosen.nbt.max_depth > 128 ||
        chosen.nbt.max_input_bytes > MC_WS_FILE_MAX_BYTES ||
        chosen.nbt.max_output_bytes > MC_WS_FILE_MAX_BYTES)
        return saved_source_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                   "SavedData source allocation arguments");
    if (chosen.max_allocation_bytes < sizeof(mc_world_saved_data) + sizeof(saved_source))
        return saved_source_result(error, MC_WORLD_STORAGE_LIMIT, "SavedData source owner budget");
    mc_world_saved_data *owner = calloc(1, sizeof(*owner));
    saved_source *source = calloc(1, sizeof(*source));
    if (owner == NULL || source == NULL) {
        free(source);
        free(owner);
        return saved_source_result(error, MC_WORLD_STORAGE_ALLOCATION_FAILED,
                                   "SavedData source owner");
    }
    owner->limits = chosen;
    owner->allocated = sizeof(*owner) + sizeof(*source);
    owner->class_bound = true;
    owner->side = side;
    owner->source = source;
    source->state = (mc_world_saved_data_source_state){.phase = MC_WORLD_SAVED_DATA_SOURCE_RAW,
                                                       .side = side,
                                                       .last_store_pc = -1,
                                                       .last_body_pc = -1,
                                                       .close_pc = -1};
    *out = owner;
    return saved_source_result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status
mc_world_saved_data_source_construct(mc_world_saved_data *owner, mc_world_store *store,
                                     mc_world_saved_data_source_error *error) {
    if (owner == NULL || owner->source == NULL)
        return saved_source_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                   "SavedData source constructor receiver");
    saved_source *s = owner->source;
    if (s->state.active)
        return saved_source_result(error, MC_WORLD_STORAGE_BUSY, "SavedData source call active");
    if (s->state.phase != MC_WORLD_SAVED_DATA_SOURCE_RAW) {
        if (s->state.phase == MC_WORLD_SAVED_DATA_SOURCE_FAILED) {
            if (error != NULL)
                *error = s->failure;
            return s->failure.status;
        }
        return saved_source_result(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                   "SavedData source constructor already attempted");
    }
    s->state.active = true;
    s->state.phase = MC_WORLD_SAVED_DATA_SOURCE_CONSTRUCTING;
    s->work = (mc_world_saved_data_source_error){0};
    s->state.source_b = &owner->named_cache;
    s->state.b_stored = true;
    s->state.last_store_pc = 12;
    s->state.source_c = &owner->ordered_list;
    s->state.c_stored = true;
    s->state.last_store_pc = 23;
    s->state.source_d = &owner->id_map;
    s->state.d_stored = true;
    s->state.last_store_pc = 34;
    if (store != NULL) {
        mc_world_source_status hs = mc_world_store_source_retain(store, &s->work.handler);
        if (hs != MC_WORLD_SOURCE_OK)
            return saved_source_fail(owner, saved_source_handler_status(hs),
                                     "SavedData source Store occurrence", "<init>", 39, false,
                                     error);
        s->state.source_a = store;
    }
    s->state.a_stored = true;
    s->state.last_store_pc = 39;
    s->state.map_cleared = true; /* d is the actual newly constructed empty map. */
    s->state.last_body_pc = 4;
    if (store == NULL) {
        s->state.last_body_pc = 16;
    } else {
        s->state.last_body_pc = 23;
        /* Current canonical source Store is client mx/fm, implementing wt.
         * Do not silently use it as the distinct server om implementation. */
        if (owner->side != MC_WORLD_INFO_CLIENT)
            return saved_source_fail(owner, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "server om source SaveHandler namespace", "b", 23, false,
                                     error);
        s->state.last_body_pc = 21;
        static const uint16_t literal[] = {'i', 'd', 'c', 'o', 'u', 'n', 't', 's'};
        s->work.string_status =
            mc_world_info_name_copy((mc_nbt_string){literal, sizeof(literal) / sizeof(literal[0])},
                                    owner->limits.max_allocation_bytes, &s->state.idcounts);
        if (s->work.string_status != MC_WORLD_INFO_OK)
            return saved_source_fail(owner,
                                     s->work.string_status == MC_WORLD_INFO_ALLOCATION_FAILED
                                         ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                                         : MC_WORLD_STORAGE_LIMIT,
                                     "native idcounts literal retention", "b", 21, false, error);
        s->state.last_body_pc = 23;
        mc_world_source_status hs = mc_world_store_source_data_file(
            store, s->state.idcounts, &s->state.file, &s->work.handler);
        if (hs != MC_WORLD_SOURCE_OK)
            return saved_source_fail(owner, saved_source_handler_status(hs),
                                     "actual source SaveHandler data File", "b", 23,
                                     s->work.handler.source_exception, error);
        s->state.file_stored = true;
        s->state.last_body_pc = 28;
        if (s->state.file != NULL) {
            s->state.last_body_pc = 34;
            mc_java_file_status fs =
                mc_java_file_exists(s->state.file, &s->state.exists, &s->work.file);
            if (fs != MC_JAVA_FILE_OK)
                return saved_source_fail(owner, saved_source_file_status(fs),
                                         "actual idcounts File.exists", "b", 34,
                                         fs == MC_JAVA_FILE_IO_ERROR, error);
            if (s->state.exists) {
                s->state.last_body_pc = 40;
                mc_nbt_status ns = mc_nbt_stream_allocate(owner->limits.max_allocation_bytes,
                                                          &s->state.reader, &s->work.input);
                if (ns != MC_NBT_OK)
                    return saved_source_fail(owner, saved_source_nbt_status(ns),
                                             "actual DataInputStream allocation", "b", 40, false,
                                             error);
                s->state.last_body_pc = 44;
                fs = mc_java_file_stream_allocate(mc_world_store_source_files(store),
                                                  &s->state.stream, &s->work.file);
                if (fs != MC_JAVA_FILE_OK)
                    return saved_source_fail(owner, saved_source_file_status(fs),
                                             "actual FileInputStream allocation", "b", 44, false,
                                             error);
                s->state.last_body_pc = 49;
                fs = mc_java_file_stream_construct(s->state.stream, s->state.file, &s->work.file);
                if (fs != MC_JAVA_FILE_OK)
                    return saved_source_fail(owner, saved_source_file_status(fs),
                                             "actual FileInputStream constructor", "b", 49,
                                             fs == MC_JAVA_FILE_IO_ERROR, error);
                mc_nbt_byte_input input = {s->state.stream, saved_source_fis_one,
                                           saved_source_fis_read, saved_source_fis_close};
                s->state.last_body_pc = 52;
                ns = mc_nbt_stream_construct(s->state.reader, &input, &owner->limits.nbt,
                                             &s->work.input);
                if (ns != MC_NBT_OK)
                    return saved_source_fail(
                        owner, saved_source_nbt_status(ns), "actual DataInputStream constructor",
                        "b", 52, s->work.input.source_fault != MC_NBT_SOURCE_NONE, error);
                s->state.input_constructed = true;
                s->state.last_body_pc = 57;
                ns = mc_nbt_stream_parse(s->state.reader, true, &s->document, &s->work.input);
                if (ns != MC_NBT_OK)
                    return saved_source_fail(
                        owner, saved_source_nbt_status(ns), "actual raw NBT input", "b", 57,
                        s->work.input.source_fault != MC_NBT_SOURCE_NONE, error);
                s->state.document = s->document;
                s->state.parsed = true;
                s->state.last_body_pc = s->state.close_pc = 62;
                ns = mc_nbt_stream_source_close(s->state.reader, &s->work.input);
                if (ns != MC_NBT_OK)
                    return saved_source_fail(
                        owner, saved_source_nbt_status(ns), "actual DataInputStream.close", "b", 62,
                        s->work.input.source_fault != MC_NBT_SOURCE_NONE, error);
                s->state.close_returned = true;
                const mc_nbt_tag *root = mc_nbt_document_root(s->document);
                s->state.last_body_pc = 66;
                for (size_t i = 0; i < root->value.compound.length; ++i) {
                    const mc_nbt_tag *tag = root->value.compound.items[i];
                    s->state.last_body_pc = 100;
                    if (tag->type != MC_NBT_SHORT)
                        continue;
                    s->state.last_body_pc = 138;
                    saved_id_entry *entry = saved_find(owner, &tag->name);
                    mc_world_storage_status status = MC_WORLD_STORAGE_OK;
                    if (entry != NULL)
                        entry->id.value = tag->value.short_value;
                    else
                        status = saved_insert(owner, &tag->name, tag->value.short_value, &entry,
                                              &s->work.storage);
                    if (status != MC_WORLD_STORAGE_OK)
                        return saved_source_fail(owner, status, s->work.storage.operation, "b", 138,
                                                 false, error);
                    ++s->state.imported_shorts;
                }
            }
        }
        s->state.last_body_pc = 155;
    }
    s->state.load_returned = s->state.constructor_returned = true;
    s->state.phase = MC_WORLD_SAVED_DATA_SOURCE_READY;
    s->state.active = false;
    return saved_source_result(error, MC_WORLD_STORAGE_OK, NULL);
}

mc_world_storage_status mc_world_saved_data_terminal_store_slot(const mc_world_saved_data *owner,
                                                                mc_world_store *const **out,
                                                                mc_world_storage_error *error) {
    if (owner == NULL || out == NULL)
        return saved_error(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                           "SavedData terminal owner/output");
    const saved_source *s = owner->source;
    if (s == NULL || !owner->class_bound || owner->side != MC_WORLD_INFO_CLIENT ||
        s->state.side != MC_WORLD_INFO_CLIENT)
        return saved_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "SavedData terminal canonical client HC scope");
    if (s->state.active)
        return saved_error(error, MC_WORLD_STORAGE_BUSY, "SavedData terminal active HC");
    if (s->state.phase != MC_WORLD_SAVED_DATA_SOURCE_READY || !s->state.constructor_returned ||
        !s->state.load_returned || !s->state.b_stored || !s->state.c_stored || !s->state.d_stored ||
        !s->state.a_stored || !s->state.map_cleared || s->state.source_b != &owner->named_cache ||
        s->state.source_c != &owner->ordered_list || s->state.source_d != &owner->id_map ||
        s->state.document != s->document || owner->storage.path != NULL)
        return saved_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           "SavedData terminal completed HC fields");
    if (s->state.source_a != NULL) {
        mc_world_source_state store;
        if (!mc_world_store_source_inspect(s->state.source_a, &store) || store.active ||
            store.phase == MC_WORLD_SOURCE_CONSTRUCTING || store.phase == MC_WORLD_SOURCE_RETIRING)
            return saved_error(error, MC_WORLD_STORAGE_BUSY, "SavedData terminal active Store");
    }
    if (s->state.file != NULL) {
        mc_java_file_state file;
        if (mc_java_file_read_state(s->state.file, &file, NULL) != MC_JAVA_FILE_OK)
            return saved_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "SavedData terminal actual File owner");
        if (file.active || file.phase == MC_JAVA_FILE_CONSTRUCTING ||
            file.phase == MC_JAVA_FILE_RETIRING)
            return saved_error(error, MC_WORLD_STORAGE_BUSY, "SavedData terminal active File");
    }
    if (s->state.stream != NULL) {
        mc_java_file_stream_state stream;
        if (mc_java_file_stream_read_state(s->state.stream, &stream, NULL) != MC_JAVA_FILE_OK)
            return saved_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "SavedData terminal actual FileInputStream owner");
        if (stream.active || stream.phase == MC_JAVA_FILE_CONSTRUCTING ||
            stream.phase == MC_JAVA_FILE_RETIRING)
            return saved_error(error, MC_WORLD_STORAGE_BUSY,
                               "SavedData terminal active FileInputStream");
    }
    if (s->state.reader != NULL) {
        mc_nbt_stream_state reader;
        if (mc_nbt_stream_state_read(s->state.reader, &reader, NULL) != MC_NBT_OK)
            return saved_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "SavedData terminal actual DataInput owner");
        if (reader.active || reader.phase == MC_NBT_STREAM_CONSTRUCTING ||
            reader.phase == MC_NBT_STREAM_PARSING || reader.phase == MC_NBT_STREAM_RETIRING)
            return saved_error(error, MC_WORLD_STORAGE_BUSY, "SavedData terminal active DataInput");
        /* This HC creates its input from this known typed FileInputStream.
         * A RAW/failed child may not have published Filter.in. Never cast the
         * untyped input context into a guessed concrete owner. */
        if (reader.source_in != NULL && reader.source_in != s->state.stream)
            return saved_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                               "SavedData terminal borrowed input identity");
    }
    /* A completed HC can retain caught/failed child diagnostics. No child
     * READY or source-close outcome is manufactured by this passive view. */
    *out = &s->state.source_a;
    return saved_error(error, MC_WORLD_STORAGE_OK, NULL);
}

bool mc_world_saved_data_source_inspect(const mc_world_saved_data *owner,
                                        mc_world_saved_data_source_state *out) {
    if (owner == NULL || owner->source == NULL || out == NULL || owner->source->state.active)
        return false;
    *out = owner->source->state;
    out->failure = &owner->source->failure;
    out->cleanup = &owner->source->cleanup;
    return true;
}

mc_world_storage_status mc_world_saved_data_retire(mc_world_saved_data **slot,
                                                   mc_world_saved_data_source_error *error) {
    if (slot == NULL)
        return saved_source_result(error, MC_WORLD_STORAGE_INVALID_ARGUMENT,
                                   "SavedData retirement slot");
    mc_world_saved_data *owner = *slot;
    if (owner == NULL)
        return saved_source_result(error, MC_WORLD_STORAGE_OK, NULL);
    saved_source *s = owner->source;
    if (s == NULL) {
        saved_free(owner);
        *slot = NULL;
        return saved_source_result(error, MC_WORLD_STORAGE_OK, NULL);
    }
    if (s->state.active)
        return saved_source_result(error, MC_WORLD_STORAGE_BUSY, "SavedData source call active");
    s->state.active = true;
    s->state.phase = MC_WORLD_SAVED_DATA_SOURCE_RETIRING;
    s->work = (mc_world_saved_data_source_error){0};
    mc_nbt_status ns = mc_nbt_stream_release(&s->state.reader, &s->work.input);
    mc_world_storage_status result = MC_WORLD_STORAGE_OK;
    if (ns != MC_NBT_OK) {
        result = saved_source_nbt_status(ns);
        s->work.operation = "native SavedData DataInput retirement";
    } else {
        mc_nbt_document_free(s->document);
        s->document = NULL;
        s->state.document = NULL;
        mc_java_file_status fs = mc_java_file_stream_release(&s->state.stream, &s->work.file);
        if (fs == MC_JAVA_FILE_OK)
            fs = mc_java_file_release(&s->state.file, &s->work.file);
        if (fs != MC_JAVA_FILE_OK) {
            result = saved_source_file_status(fs);
            s->work.operation = "native SavedData File/input retirement";
        } else {
            mc_world_info_name_release(s->state.idcounts);
            s->state.idcounts = NULL;
            if (s->state.source_a != NULL) {
                mc_world_source_status hs =
                    mc_world_store_source_release(&s->state.source_a, &s->work.handler);
                if (hs != MC_WORLD_SOURCE_OK) {
                    result = saved_source_handler_status(hs);
                    s->work.operation = "native SavedData Store occurrence retirement";
                }
            }
        }
    }
    if (result != MC_WORLD_STORAGE_OK) {
        s->work.status = s->work.cause_status = result;
        s->work.native_policy = true;
        s->work.site = saved_source_site(owner, "<native-retire>", -1);
        s->cleanup = s->work;
        s->state.active = false;
        if (error != NULL)
            *error = s->cleanup;
        return result;
    }
    free(s);
    owner->source = NULL;
    saved_free(owner);
    *slot = NULL;
    return saved_source_result(error, MC_WORLD_STORAGE_OK, NULL);
}
