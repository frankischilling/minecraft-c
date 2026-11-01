/* Existing APIs only: this TEST-ONLY increment builds against the old runtime
 * and must fail its first fm42 terminal-admission OK assertion there. */
typedef struct ss_terminal_snapshot {
    unsigned char world[sizeof(mc_world_runtime)];
    mc_world_source_state handler;
    mc_world_source_input_state input;
    mc_java_file_state file;
    mc_java_file_stream_state stream;
    mc_nbt_stream_state reader;
    uint8_t token_buffer[8];
    mc_world_source_error failure, cleanup;
} ss_terminal_snapshot;

static int ss_terminal_capture(source_save_fixture *s, ss_terminal_snapshot *out) {
    fixture *f = &s->game;
    memcpy(out->world, f->worlds[0], sizeof(out->world));
    VERIFY(mc_world_store_source_inspect(f->stores[0], &out->handler) &&
           mc_world_store_source_input_inspect(f->stores[0], &out->input) &&
           out->input.file != NULL && out->input.stream != NULL && out->input.reader != NULL &&
           mc_java_file_read_state(out->input.file, &out->file, NULL) == MC_JAVA_FILE_OK &&
           mc_java_file_stream_read_state(out->input.stream, &out->stream, NULL) ==
               MC_JAVA_FILE_OK &&
           mc_nbt_stream_state_read(out->input.reader, &out->reader, NULL) == MC_NBT_OK &&
           out->reader.read_buffer != NULL && out->handler.failure != NULL &&
           out->handler.cleanup != NULL);
    memcpy(out->token_buffer, out->reader.read_buffer, sizeof(out->token_buffer));
    out->failure = *out->handler.failure;
    out->cleanup = *out->handler.cleanup;
    return 0;
}

static bool ss_terminal_same(const ss_terminal_snapshot *a, const ss_terminal_snapshot *b) {
    const mc_java_file_state *fa = &a->file, *fb = &b->file;
    const mc_java_file_stream_state *sa = &a->stream, *sb = &b->stream;
    const mc_nbt_stream_state *ra = &a->reader, *rb = &b->reader;
    return memcmp(a->world, b->world, sizeof(a->world)) == 0 &&
           memcmp(&a->handler, &b->handler, sizeof(a->handler)) == 0 &&
           memcmp(&a->input, &b->input, sizeof(a->input)) == 0 &&
           memcmp(&a->failure, &b->failure, sizeof(a->failure)) == 0 &&
           memcmp(&a->cleanup, &b->cleanup, sizeof(a->cleanup)) == 0 &&
           memcmp(a->token_buffer, b->token_buffer, sizeof(a->token_buffer)) == 0 &&
           fa->phase == fb->phase && fa->holds == fb->holds &&
           fa->prefix_length == fb->prefix_length && fa->path == fb->path &&
           fa->exists_calls == fb->exists_calls && fa->mkdir_calls == fb->mkdir_calls &&
           fa->canonicalize_calls == fb->canonicalize_calls &&
           fa->last_native_error == fb->last_native_error && fa->active == fb->active &&
           fa->has_failed_array == fb->has_failed_array && sa->phase == sb->phase &&
           sa->active == sb->active && sa->native_open == sb->native_open &&
           sa->source_close_reached == sb->source_close_reached &&
           sa->source_close_succeeded == sb->source_close_succeeded && sa->holds == sb->holds &&
           sa->read_calls == sb->read_calls && sa->native_read_calls == sb->native_read_calls &&
           sa->byte_read_calls == sb->byte_read_calls &&
           sa->native_bytes_read == sb->native_bytes_read && ra->phase == rb->phase &&
           ra->active == rb->active && ra->position == rb->position &&
           ra->read_calls == rb->read_calls && ra->close_calls == rb->close_calls &&
           ra->owner_bytes == rb->owner_bytes && ra->source_in == rb->source_in &&
           ra->bytearr == rb->bytearr && ra->read_buffer == rb->read_buffer &&
           ra->chararr == rb->chararr && ra->bytearr_capacity == rb->bytearr_capacity &&
           ra->chararr_capacity == rb->chararr_capacity &&
           ra->last_data_input_store == rb->last_data_input_store &&
           ra->last_data_input_store_class == rb->last_data_input_store_class &&
           ra->candidate == rb->candidate && ra->last_tag == rb->last_tag &&
           ra->pending_tag == rb->pending_tag && ra->last_collection == rb->last_collection &&
           ra->last_collection_count == rb->last_collection_count &&
           ra->last_array == rb->last_array && ra->last_array_length == rb->last_array_length &&
           ra->last_tag_store == rb->last_tag_store &&
           ra->last_tag_store_class == rb->last_tag_store_class &&
           ra->last_collection_operation_pc == rb->last_collection_operation_pc;
}

static int ss_terminal_attempt(source_save_fixture *s, const char *phase,
                               mc_world_storage_status expected) {
    fixture *f = &s->game;
    ss_terminal_snapshot *before = calloc(1, sizeof(*before));
    ss_terminal_snapshot *after = calloc(1, sizeof(*after));
    VERIFY(before != NULL && after != NULL && ss_terminal_capture(s, before) == 0);
    mc_world_terminal_source_view view;
    mc_world_storage_error error;
    VERIFY(mc_world_runtime_terminal_source_inspect(f->worlds[0], &view, &error) ==
           MC_WORLD_STORAGE_OK);
    mc_world_terminal_store_anchor anchors[4] = {{&f->stores[0]}};
    for (size_t i = 0; i < 3; ++i) {
        VERIFY(view.store_slots[i] != NULL && *view.store_slots[i] == f->stores[0] &&
               view.store_slots[i] != &f->stores[0]);
        anchors[i + 1].slot = view.store_slots[i];
        for (size_t j = 0; j < i; ++j)
            VERIFY(view.store_slots[i] != view.store_slots[j]);
    }
    const mc_world_storage_status result = mc_world_store_terminal_source_admit(
        f->stores[0], anchors, sizeof(anchors) / sizeof(*anchors), &error);
    VERIFY(ss_terminal_capture(s, after) == 0 && ss_terminal_same(before, after));
    printf("{\"event\":\"terminal\",\"phase\":\"%s\",\"status\":%d,\"expected\":%d,"
           "\"unchanged\":1,\"fis_holds\":%zu,\"native_open\":%d,\"reader_close_calls\":%zu,"
           "\"session_token\":%lld}"
           "\n",
           phase, (int)result, (int)expected, before->stream.holds, before->stream.native_open,
           before->reader.close_calls, (long long)before->handler.source_e);
    VERIFY(fflush(stdout) == 0);
    if (result != expected)
        fprintf(stderr, "session terminal phase=%s status=%d expected=%d operation=%s\n", phase,
                (int)result, (int)expected, error.operation == NULL ? "none" : error.operation);
    free(before);
    free(after);
    VERIFY(result == expected && error.status == expected);
    return 0;
}

static int ss_session_terminal(source_save_fixture *s) {
    fixture *f = &s->game;
    VERIFY(ss_terminal_attempt(s, "before-fault", MC_WORLD_STORAGE_OK) == 0);
    char path[8192];
    const int n = snprintf(path, sizeof(path), "%s/world/session.lock", s->root);
    VERIFY(n > 0 && (size_t)n < sizeof(path));
    uint8_t token[8], mismatch[8], extra;
    FILE *file = ss_fopen(path, "rb");
    VERIFY(file != NULL && fread(token, 1, sizeof(token), file) == sizeof(token) &&
           fread(&extra, 1, 1, file) == 0 && fclose(file) == 0);
    memcpy(mismatch, token, sizeof(mismatch));
    mismatch[0] ^= 1u;
    file = ss_fopen(path, "wb");
    VERIFY(file != NULL && fwrite(mismatch, 1, sizeof(mismatch), file) == sizeof(mismatch) &&
           fclose(file) == 0);
    VERIFY(mc_world_runtime_save_detailed(f->worlds[0], true, &s->lifecycle) !=
           MC_CHUNK_LIFECYCLE_OK);
    const mc_world_source_error failure = s->lifecycle.source_session;
    mc_world_source_input_state input;
    mc_java_file_stream_state stream;
    mc_nbt_stream_state reader;
    VERIFY(failure.status == MC_WORLD_SOURCE_UNIMPLEMENTED && failure.native_policy &&
           failure.session_mismatch && !failure.source_exception && !failure.caught_io &&
           !failure.diagnostic_frontier && failure.exception_class == NULL &&
           failure.frame_count == 1 && failure.frames[0].pc == 42 &&
           strcmp(failure.frames[0].class_name, "fm") == 0 &&
           strcmp(failure.frames[0].method, "b") == 0 && failure.cause_site.pc == 35 &&
           mc_world_store_source_input_inspect(f->stores[0], &input) &&
           input.call == MC_WORLD_SOURCE_INPUT_SESSION && input.last_pc == 42 &&
           input.close_pc == -1 && input.token_read && !input.token_matches && !input.closed &&
           !input.source_returned &&
           mc_java_file_stream_read_state(input.stream, &stream, NULL) == MC_JAVA_FILE_OK &&
           stream.phase == MC_JAVA_FILE_READY && stream.holds == 1 && !stream.active &&
           stream.native_open && !stream.source_close_reached && !stream.source_close_succeeded &&
           stream.native_bytes_read == 8 &&
           mc_nbt_stream_state_read(input.reader, &reader, NULL) == MC_NBT_OK &&
           reader.phase == MC_NBT_STREAM_READY && !reader.active && reader.position == 8 &&
           reader.close_calls == 0 && reader.source_in == input.stream);
    VERIFY(ss_event(s, "failed") == 0);
    /* The decisive old-runtime RED is here, after an actual World save reached
     * fm42. It cannot be a missing new helper/link/configuration failure. */
    if (ss_terminal_attempt(s, "fm42", MC_WORLD_STORAGE_OK) != 0) {
        /* The old-runtime RED still performs actual native cleanup so its
         * decisive gate assertion is not buried under leaked-owner reports. */
        VERIFY(ss_stop(s) == 0);
        file = ss_fopen(path, "wb");
        VERIFY(file != NULL && fwrite(token, 1, sizeof(token), file) == sizeof(token) &&
               fclose(file) == 0);
        return 1;
    }
    VERIFY(mc_java_file_stream_retain(input.stream, NULL) == MC_JAVA_FILE_OK);
    mc_java_file_stream *external = input.stream;
    VERIFY(ss_terminal_attempt(s, "external-fis", MC_WORLD_STORAGE_BUSY) == 0);
    VERIFY(mc_java_file_stream_release(&external, NULL) == MC_JAVA_FILE_OK && external == NULL &&
           ss_terminal_attempt(s, "fis-released", MC_WORLD_STORAGE_OK) == 0);
    VERIFY(mc_world_store_source_retain(f->stores[0], NULL) == MC_WORLD_SOURCE_OK);
    mc_world_store *external_handler = f->stores[0];
    VERIFY(ss_terminal_attempt(s, "external-handler", MC_WORLD_STORAGE_BUSY) == 0);
    VERIFY(mc_world_store_source_release(&external_handler, NULL) == MC_WORLD_SOURCE_OK &&
           external_handler == NULL &&
           ss_terminal_attempt(s, "handler-released", MC_WORLD_STORAGE_OK) == 0);
    /* Actual final Handler native cleanup, not an early input-clear or source
     * close. Restore only the authored fault file after the complete graph. */
    VERIFY(ss_stop(s) == 0);
    file = ss_fopen(path, "wb");
    VERIFY(file != NULL && fwrite(token, 1, sizeof(token), file) == sizeof(token) &&
           fclose(file) == 0);
    return 0;
}
