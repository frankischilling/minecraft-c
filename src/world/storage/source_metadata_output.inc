/* Concrete fm.a(ei,List) transport, entered after actual ei.a(List)2.
 * The caller's completed Info record is not serialized or sampled again. */
#define SOURCE_WRITE_DESC "(Lei;Ljava/util/List;)V"

static mc_world_source_status source_write_file_error(mc_world_store_source *s,
                                                      mc_java_file_status status, int pc) {
    source_file(&s->work, status, "<native-write>", -1);
    s->work.frame_count = 0;
    source_push(&s->work, "fm", "a", SOURCE_WRITE_DESC, pc);
    s->output->last_pc = pc;
    return s->work.status;
}

static mc_world_source_status source_write_caught(mc_world_store_source *s, int pc) {
    s->work.cause_site = (mc_world_source_site){"fm", "a", SOURCE_WRITE_DESC, pc};
    s->work.status = MC_WORLD_SOURCE_UNIMPLEMENTED;
    s->work.cause_status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
    s->work.operation = "java/lang/Exception.printStackTrace()V";
    s->work.caught_exception = s->work.source_exception = s->work.diagnostic_frontier = true;
    s->work.native_policy = false;
    s->work.frame_count = 0;
    source_push(&s->work, "fm", "a", SOURCE_WRITE_DESC, 147);
    s->output->last_pc = 147;
    return s->work.status;
}

static mc_world_source_status source_write_output_error(mc_world_store_source *s,
                                                        mc_java_output_status status, int pc,
                                                        bool compressed) {
    if (compressed) {
        s->work.output = s->work.compressed.output;
        s->work.prior_site = (mc_world_source_site){"as", "a", "(Lnu;Ljava/io/OutputStream;)V",
                                                    s->output->compressed_pc};
    }
    source_output(&s->work, status, -1);
    s->work.frame_count = 0;
    source_push(&s->work, "fm", "a", SOURCE_WRITE_DESC, pc);
    s->output->last_pc = pc;
    return s->work.source_exception ? source_write_caught(s, pc) : s->work.status;
}

static mc_world_source_status source_write_clear(mc_world_store_source *s) {
    mc_world_source_output_state *r = s->output;
    if (r == NULL)
        return MC_WORLD_SOURCE_OK;
    mc_java_output_status os =
        mc_compressed_output_release(&r->compressed_output, &s->work.compressed);
    if (os != MC_JAVA_OUTPUT_OK) {
        s->work.output = s->work.compressed.output;
        source_output(&s->work, os, -1);
        s->work.source_exception = s->work.caught_exception = false;
        s->work.diagnostic_frontier = false;
        s->work.native_policy = true;
        return s->work.status;
    }
    os = mc_java_file_output_release(&r->file_output, &s->work.output);
    if (os != MC_JAVA_OUTPUT_OK) {
        source_output(&s->work, os, -1);
        s->work.source_exception = s->work.caught_exception = false;
        s->work.diagnostic_frontier = false;
        s->work.native_policy = true;
        return s->work.status;
    }
    mc_java_file **files[] = {&r->new_file, &r->old_file, &r->current_file};
    for (size_t i = 0; i < sizeof(files) / sizeof(*files); ++i) {
        const mc_java_file_status fs = mc_java_file_release(files[i], &s->work.file);
        if (fs != MC_JAVA_FILE_OK)
            return source_write_file_error(s, fs, -1);
    }
    free(r);
    s->output = NULL;
    return MC_WORLD_SOURCE_OK;
}

mc_world_source_status mc_world_store_source_output_clear(mc_world_store *store,
                                                          mc_world_source_error *out) {
    mc_world_source_status status = source_admit(store, out);
    if (status != MC_WORLD_SOURCE_OK)
        return status;
    mc_world_store_source *s = store->source;
    s->state.active = s->classes->active = true;
    source_result(&s->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    status = source_write_clear(s);
    if (status != MC_WORLD_SOURCE_OK) {
        /* Native retirement never invokes Exception.printStackTrace/finally. */
        s->work.source_exception = s->work.caught_exception = false;
        s->work.diagnostic_frontier = false;
        s->work.native_policy = true;
        s->cleanup = s->work;
    }
    s->state.active = s->classes->active = false;
    return source_copy(out, &s->work);
}

bool mc_world_store_source_output_inspect(const mc_world_store *store,
                                          mc_world_source_output_state *out) {
    if (!mc_world_store_is_source(store) || out == NULL || store->source->state.active ||
        store->source->classes->active || store->source->output == NULL)
        return false;
    *out = *store->source->output;
    return true;
}

static mc_world_source_status source_write_literal(mc_world_store_source *s, const char *text,
                                                   mc_world_info_name **slot, int pc) {
    s->output->last_pc = pc;
    const mc_world_source_status status = source_literal(s->classes, text, slot, "a", pc, &s->work);
    if (status != MC_WORLD_SOURCE_OK) {
        s->work.frame_count = 0;
        source_push(&s->work, "fm", "a", SOURCE_WRITE_DESC, pc);
    }
    return status;
}

static mc_world_source_status source_write_file(mc_world_store_source *s, mc_java_file **slot,
                                                const char *text, mc_world_info_name **name,
                                                int new_pc, int literal_pc, int constructor_pc) {
    mc_world_source_output_state *r = s->output;
    r->last_pc = new_pc;
    mc_java_file_status fs = mc_java_file_allocate(s->classes->files, slot, &s->work.file);
    if (fs != MC_JAVA_FILE_OK)
        return source_write_file_error(s, fs, new_pc);
    r->last_pc = literal_pc;
    mc_world_source_status status = source_write_literal(s, text, name, literal_pc);
    if (status != MC_WORLD_SOURCE_OK)
        return status;
    r->last_pc = constructor_pc;
    fs = mc_java_file_construct_child(*slot, s->state.source_b, *name, &s->work.file);
    return fs == MC_JAVA_FILE_OK ? MC_WORLD_SOURCE_OK
                                 : source_write_file_error(s, fs, constructor_pc);
}

static mc_world_source_status source_write_compressed(mc_world_store_source *s,
                                                      const mc_nbt_tag *root) {
    mc_world_source_output_state *r = s->output;
    r->compressed_pc = 0;
    mc_java_output_options compressed_options = s->classes->options.output;
    compressed_options.max_owner_bytes = s->classes->options.max_owner_bytes;
    mc_java_output_status os = mc_compressed_output_allocate(
        &compressed_options, &r->compressed_output, &s->work.compressed);
    if (os == MC_JAVA_OUTPUT_OK) {
        r->compressed_pc = 9;
        os = mc_compressed_output_construct(r->compressed_output, r->file_output,
                                            &s->work.compressed);
    }
    if (os != MC_JAVA_OUTPUT_OK) {
        mc_compressed_output_state state;
        if (mc_compressed_output_inspect(r->compressed_output, &state) && state.header_written)
            r->compressed_pc = 12; /* Reached outer DOS construction after gzip9. */
        return source_write_output_error(s, os, 79, true);
    }
    r->compressed_constructed = true;
    r->compressed_pc = 18;
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = s->classes->options.max_owner_bytes;
    os = mc_compressed_output_write_compound(r->compressed_output, root, &limits,
                                             &s->work.compressed);
    if (os != MC_JAVA_OUTPUT_OK) {
        /* as catches Throwable only over16..21. Native safety/provider faults
         * retain their actual prefix without inventing a Java finally. */
        if (s->work.compressed.output.cause.source_exception) {
            r->close_pc = 30;
            const mc_java_output_status closed =
                mc_compressed_output_source_close(r->compressed_output, &s->work.compressed_close);
            if (closed != MC_JAVA_OUTPUT_OK) {
                /* This source handler does not addSuppressed. Failed finally
                 * close replaces the original write failure. */
                s->work.compressed = s->work.compressed_close;
                r->compressed_pc = 30;
                os = closed;
            } else
                r->closed = true;
        }
        return source_write_output_error(s, os, 79, true);
    }
    r->nbt_written = true;
    r->close_pc = 22; /* Outside as16..21; never call source close twice. */
    os = mc_compressed_output_source_close(r->compressed_output, &s->work.compressed_close);
    if (os != MC_JAVA_OUTPUT_OK) {
        s->work.compressed = s->work.compressed_close;
        r->compressed_pc = 22;
        return source_write_output_error(s, os, 79, true);
    }
    r->closed = true;
    r->compressed_pc = 35;
    return MC_WORLD_SOURCE_OK;
}

#define SOURCE_WRITE_FILE_CALL(pc, expression)                                                     \
    do {                                                                                           \
        r->last_pc = (pc);                                                                         \
        const mc_java_file_status source_write_file_status = (expression);                         \
        if (source_write_file_status != MC_JAVA_FILE_OK) {                                         \
            status = source_write_file_error(s, source_write_file_status, (pc));                   \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)

mc_world_source_status mc_world_store_source_write_info_record(mc_world_store *store,
                                                               const mc_world_info_record *record,
                                                               mc_world_source_error *out) {
    mc_world_source_status status = source_admit(store, out);
    if (status != MC_WORLD_SOURCE_OK)
        return status;
    const mc_nbt_tag *value = mc_world_info_record_root(record);
    if (value == NULL || value->type != MC_NBT_COMPOUND)
        return source_result(out, MC_WORLD_SOURCE_ARGUMENT, "completed Info record operand", "fm",
                             "a", SOURCE_WRITE_DESC, 5);
    mc_world_store_source *s = store->source;
    s->state.active = s->classes->active = true;
    source_result(&s->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    status = source_write_clear(s);
    if (status != MC_WORLD_SOURCE_OK)
        goto done;
    if (s->classes->options.max_owner_bytes < sizeof(*s->output)) {
        status = source_result(&s->work, MC_WORLD_SOURCE_LIMIT, "native output prefix byte bound",
                               "fm", "a", SOURCE_WRITE_DESC, 6);
        goto done;
    }
    s->output = calloc(1, sizeof(*s->output));
    if (s->output == NULL) {
        status = source_result(&s->work, MC_WORLD_SOURCE_ALLOCATION, "native source output prefix",
                               "fm", "a", SOURCE_WRITE_DESC, 6);
        goto done;
    }
    mc_world_source_output_state *r = s->output;
    r->last_pc = 10;
    r->close_pc = r->compressed_pc = -1;
    /* The actual output root is fresh, unlike legacy unknown-root retention.
     * The immutable Info payload is borrowed for this synchronous call only. */
    mc_nbt_tag root = {.type = MC_NBT_COMPOUND};
    r->root_constructed = true;
    status = source_write_literal(s, "Data", &s->classes->data_key, 17);
    if (status != MC_WORLD_SOURCE_OK)
        goto done;
    mc_nbt_tag data = *value;
    data.name = mc_world_info_name_text(s->classes->data_key);
    const mc_nbt_tag *items[] = {&data};
    root.value.compound.items = items;
    root.value.compound.length = 1;
    r->data_attached = true;
    r->last_pc = 20;
    status = source_write_file(s, &r->new_file, "level.dat_new", &s->classes->new_name, 23, 31, 33);
    if (status == MC_WORLD_SOURCE_OK)
        status =
            source_write_file(s, &r->old_file, "level.dat_old", &s->classes->old_name, 38, 46, 48);
    if (status == MC_WORLD_SOURCE_OK)
        status = source_write_file(s, &r->current_file, "level.dat", &s->classes->level_name, 53,
                                   61, 63);
    if (status != MC_WORLD_SOURCE_OK)
        goto done;
    r->last_pc = 70;
    mc_java_output_status os = mc_java_file_output_allocate(
        s->classes->files, &s->classes->options.output, &r->file_output, &s->work.output);
    if (os == MC_JAVA_OUTPUT_OK) {
        r->last_pc = 76;
        os = mc_java_file_output_construct(r->file_output, r->new_file, &s->work.output);
    }
    if (os != MC_JAVA_OUTPUT_OK) {
        status = source_write_output_error(s, os, r->last_pc, false);
        goto done;
    }
    r->file_constructed = true;
    r->last_pc = 79;
    status = source_write_compressed(s, &root);
    if (status != MC_WORLD_SOURCE_OK)
        goto done;
    SOURCE_WRITE_FILE_CALL(84, mc_java_file_exists(r->old_file, &r->old_exists, &s->work.file));
    r->old_exists_called = true;
    if (r->old_exists) {
        SOURCE_WRITE_FILE_CALL(92,
                               mc_java_file_delete(r->old_file, &r->old_deleted, &s->work.file));
        r->old_delete_called = true;
    }
    SOURCE_WRITE_FILE_CALL(
        100, mc_java_file_rename_to(r->current_file, r->old_file, &r->old_renamed, &s->work.file));
    r->old_rename_called = true;
    SOURCE_WRITE_FILE_CALL(106,
                           mc_java_file_exists(r->current_file, &r->current_exists, &s->work.file));
    r->current_exists_called = true;
    if (r->current_exists) {
        SOURCE_WRITE_FILE_CALL(
            114, mc_java_file_delete(r->current_file, &r->current_deleted, &s->work.file));
        r->current_delete_called = true;
    }
    SOURCE_WRITE_FILE_CALL(
        122, mc_java_file_rename_to(r->new_file, r->current_file, &r->new_renamed, &s->work.file));
    r->new_rename_called = true;
    SOURCE_WRITE_FILE_CALL(128, mc_java_file_exists(r->new_file, &r->new_exists, &s->work.file));
    r->new_exists_called = true;
    if (r->new_exists) {
        SOURCE_WRITE_FILE_CALL(136,
                               mc_java_file_delete(r->new_file, &r->new_deleted, &s->work.file));
        r->new_delete_called = true;
    }
    r->last_pc = 150;
    r->source_returned = true;
done:
    s->state.active = s->classes->active = false;
    if (status != MC_WORLD_SOURCE_OK)
        s->failure = s->work;
    return source_copy(out, &s->work);
}

#undef SOURCE_WRITE_FILE_CALL

static mc_world_storage_status store_source_write_record(mc_world_store *store,
                                                         const mc_world_info_record *record,
                                                         mc_world_storage_error *error) {
    mc_world_source_error source;
    const mc_world_source_status status =
        mc_world_store_source_write_info_record(store, record, &source);
    return store_error(error,
                       status == MC_WORLD_SOURCE_OK           ? MC_WORLD_STORAGE_OK
                       : status == MC_WORLD_SOURCE_BUSY       ? MC_WORLD_STORAGE_BUSY
                       : status == MC_WORLD_SOURCE_LIMIT      ? MC_WORLD_STORAGE_LIMIT
                       : status == MC_WORLD_SOURCE_ALLOCATION ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                       : status == MC_WORLD_SOURCE_FILE || status == MC_WORLD_SOURCE_OUTPUT
                           ? MC_WORLD_STORAGE_IO_ERROR
                           : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       source.operation);
}

#undef SOURCE_WRITE_DESC
