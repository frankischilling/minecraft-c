/* Concrete fm.c/fm.b call prefixes on the SAME Store. Included once. */
static mc_world_source_status source_input_nbt(mc_world_store_source *s, mc_nbt_status ns,
                                               const char *method, int pc) {
    mc_world_source_error *e = &s->work;
    e->status = e->cause_status = ns == MC_NBT_LIMIT               ? MC_WORLD_SOURCE_LIMIT
                                  : ns == MC_NBT_ALLOCATION_FAILED ? MC_WORLD_SOURCE_ALLOCATION
                                  : ns == MC_NBT_STREAM_BUSY       ? MC_WORLD_SOURCE_BUSY
                                                                   : MC_WORLD_SOURCE_FILE;
    e->operation = "actual DataInputStream acquisition/operation";
    e->source_exception = e->input.source_fault != MC_NBT_SOURCE_NONE;
    e->native_policy = !e->source_exception;
    if (e->source_exception)
        e->status = e->cause_status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
    e->frame_count = 0;
    if (pc >= 0)
        source_push(e, "fm", method, source_method_descriptor(method), pc);
    return e->status;
}

static mc_world_source_status source_input_gzip(mc_world_store_source *s, mc_gzip_status gs, int pc,
                                                int as_pc) {
    mc_world_source_error *e = &s->work;
    e->status = e->cause_status = gs == MC_GZIP_LIMIT             ? MC_WORLD_SOURCE_LIMIT
                                  : gs == MC_GZIP_ALLOCATION      ? MC_WORLD_SOURCE_ALLOCATION
                                  : gs == MC_GZIP_BUSY            ? MC_WORLD_SOURCE_BUSY
                                  : gs == MC_GZIP_NATIVE_PROVIDER ? MC_WORLD_SOURCE_UNIMPLEMENTED
                                                                  : MC_WORLD_SOURCE_FILE;
    e->operation = e->gzip.operation;
    e->source_exception = e->gzip.source_exception;
    e->native_policy = !e->source_exception;
    if (e->source_exception)
        e->status = e->cause_status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
    e->frame_count = 0;
    if (as_pc >= 0)
        source_push(e, "as", "a", "(Ljava/io/InputStream;)Lnu;", as_pc);
    if (pc >= 0)
        source_push(e, "fm", "c", "()Lei;", pc);
    return e->status;
}

static mc_world_source_status source_metadata_caught(mc_world_store_source *s, int cause_pc) {
    mc_world_source_error *e = &s->work;
    e->cause_site = (mc_world_source_site){"fm", "c", "()Lei;", cause_pc};
    e->status = MC_WORLD_SOURCE_UNIMPLEMENTED;
    e->cause_status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
    e->operation = "java/lang/Exception.printStackTrace()V";
    e->caught_exception = e->source_exception = e->diagnostic_frontier = true;
    e->native_policy = false;
    e->frame_count = 0;
    s->input->last_pc = s->input->old_file ? 105 : 51;
    source_push(e, "fm", "c", "()Lei;", s->input->old_file ? 105 : 51);
    return e->status;
}

static mc_world_source_status source_session_exception_owner(mc_world_store_source *s, int pc) {
    /* No us/RuntimeException/Throwable identity is fabricated. Its actual NEW
     * cannot complete yet. In particular mismatch has NOT thrown51 or run61. */
    s->input->last_pc = pc;
    s->work.status = MC_WORLD_SOURCE_UNIMPLEMENTED;
    s->work.operation = "actual us/RuntimeException/Throwable allocation owner";
    s->work.native_policy = true;
    s->work.frame_count = 0;
    source_push(&s->work, "fm", "b", "()V", pc);
    return s->work.status;
}

static mc_nbt_input_status source_fis_error(mc_java_file_status fs, mc_java_file_error fe,
                                            mc_nbt_input_error *error) {
    if (error != NULL)
        *error = (mc_nbt_input_error){.transport_status = (int)fs,
                                      .system_error = fe.system_error,
                                      .operation = fe.operation,
                                      .source_exception = fs == MC_JAVA_FILE_IO_ERROR};
    return fs == MC_JAVA_FILE_OK ? MC_NBT_INPUT_OK : MC_NBT_INPUT_FAILED;
}

static mc_nbt_input_status source_fis_one(void *context, int *out, mc_nbt_input_error *e) {
    mc_java_file_error fe = {0};
    mc_java_file_status fs = mc_java_file_stream_read_byte(context, out, &fe);
    return source_fis_error(fs, fe, e);
}

static mc_nbt_input_status source_fis_read(void *context, uint8_t *bytes, size_t capacity,
                                           mc_nbt_input_result *out, mc_nbt_input_error *e) {
    mc_java_file_error fe = {0};
    mc_java_file_read got = {0};
    mc_java_file_status fs = mc_java_file_stream_read(context, bytes, capacity, &got, &fe);
    if (fs == MC_JAVA_FILE_OK)
        *out = (mc_nbt_input_result){got.count, got.eof};
    return source_fis_error(fs, fe, e);
}

static mc_nbt_input_status source_fis_close(void *context, mc_nbt_input_error *e) {
    mc_java_file_error fe = {0};
    mc_java_file_status fs = mc_java_file_stream_source_close(context, &fe);
    return source_fis_error(fs, fe, e);
}

static mc_world_source_status source_input_clear(mc_world_store_source *s) {
    mc_world_source_input_state *r = s->input;
    if (r == NULL)
        return MC_WORLD_SOURCE_OK;
    mc_nbt_status ns = mc_nbt_stream_release(&r->reader, &s->work.input);
    if (ns != MC_NBT_OK)
        return source_input_nbt(s, ns, "<native-retire>", -1);
    mc_world_info_release(r->info);
    r->info = NULL;
    mc_world_info_data_release(r->data);
    r->data = NULL;
    mc_nbt_document_free(r->document);
    r->document = NULL;
    mc_gzip_status gs = mc_gzip_input_release(&r->gzip, &s->work.gzip);
    if (gs != MC_GZIP_OK)
        return source_input_gzip(s, gs, -1, -1);
    mc_java_file_status fs = mc_java_file_stream_release(&r->stream, &s->work.file);
    if (fs == MC_JAVA_FILE_OK)
        fs = mc_java_file_release(&r->candidate, &s->work.file);
    if (fs == MC_JAVA_FILE_OK)
        fs = mc_java_file_release(&r->file, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "<native-retire>", -1);
        s->work.source_exception = false;
        s->work.native_policy = true;
        return s->work.status;
    }
    free(r);
    s->input = NULL;
    return MC_WORLD_SOURCE_OK;
}

mc_world_source_status mc_world_store_source_input_clear(mc_world_store *store,
                                                         mc_world_source_error *e) {
    if (!mc_world_store_is_source(store))
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "source input retirement receiver", NULL,
                             NULL, NULL, -1);
    mc_world_store_source *s = store->source;
    if (s->state.active || s->classes->active || s->state.phase == MC_WORLD_SOURCE_RETIRING)
        return source_result(e, MC_WORLD_SOURCE_BUSY, "source input active/terminal retirement",
                             NULL, NULL, NULL, -1);
    s->state.active = s->classes->active = true;
    source_result(&s->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    mc_world_source_status result = source_input_clear(s);
    s->state.active = s->classes->active = false;
    if (result != MC_WORLD_SOURCE_OK)
        s->cleanup = s->work;
    return source_copy(e, &s->work);
}

bool mc_world_store_source_input_inspect(const mc_world_store *store,
                                         mc_world_source_input_state *out) {
    if (!mc_world_store_is_source(store) || out == NULL || store->source->state.active ||
        store->source->classes->active || store->source->input == NULL)
        return false;
    *out = *store->source->input;
    return true;
}

static mc_world_source_status source_input_begin(mc_world_store *store,
                                                 mc_world_source_input_call call,
                                                 mc_world_store_source **out,
                                                 mc_world_source_error *e) {
    if (!mc_world_store_is_source(store))
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "source input method receiver", NULL,
                             NULL, NULL, -1);
    mc_world_store_source *s = store->source;
    if (s->state.active || s->classes->active || s->state.phase != MC_WORLD_SOURCE_READY)
        return source_result(
            e, s->state.active || s->classes->active ? MC_WORLD_SOURCE_BUSY : MC_WORLD_SOURCE_STATE,
            "source input method admission", NULL, NULL, NULL, -1);
    s->state.active = s->classes->active = true;
    source_result(&s->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    mc_world_source_status status = source_input_clear(s);
    if (status != MC_WORLD_SOURCE_OK) {
        s->cleanup = s->work;
        s->state.active = s->classes->active = false;
        return source_copy(e, &s->work);
    }
    if (s->classes->options.max_owner_bytes < sizeof(*s->input)) {
        source_result(&s->work, MC_WORLD_SOURCE_LIMIT, "native source input prefix bound", NULL,
                      NULL, NULL, -1);
    } else {
        s->input = calloc(1, sizeof(*s->input));
        if (s->input == NULL)
            source_result(&s->work, MC_WORLD_SOURCE_ALLOCATION, "native source input prefix", NULL,
                          NULL, NULL, -1);
    }
    if (s->work.status != MC_WORLD_SOURCE_OK) {
        s->state.active = s->classes->active = false;
        return source_copy(e, &s->work);
    }
    s->input->call = call;
    s->input->last_pc = s->input->close_pc = -1;
    *out = s;
    return MC_WORLD_SOURCE_OK;
}

static mc_world_source_status source_input_end(mc_world_store_source *s, mc_world_source_error *e) {
    s->state.active = s->classes->active = false;
    if (s->work.status != MC_WORLD_SOURCE_OK)
        s->failure = s->work;
    return source_copy(e, &s->work);
}

static mc_world_source_status source_metadata_decode(mc_world_store_source *s) {
    mc_world_source_input_state *r = s->input;
    mc_world_store_source_classes *c = s->classes;
    const int call_pc = r->old_file ? 83 : 29;
    r->last_pc = call_pc;
    mc_nbt_status ns =
        mc_nbt_stream_allocate(c->options.max_owner_bytes, &r->reader, &s->work.input);
    if (ns != MC_NBT_OK) {
        source_input_nbt(s, ns, "c", call_pc);
        s->work.prior_site = (mc_world_source_site){"as", "a", "(Ljava/io/InputStream;)Lnu;", 0};
        return s->work.status;
    }
    mc_gzip_options go = mc_gzip_default_options();
    go.maximum_owner_bytes = c->options.max_owner_bytes;
    mc_gzip_status gs = mc_gzip_input_allocate(&go, &r->gzip, &s->work.gzip);
    if (gs == MC_GZIP_OK)
        gs = mc_gzip_input_construct(r->gzip, r->stream, &s->work.gzip);
    if (gs != MC_GZIP_OK) {
        source_input_gzip(s, gs, call_pc, r->gzip == NULL ? 4 : 9);
        return s->work.source_exception ? source_metadata_caught(s, call_pc) : s->work.status;
    }
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = c->options.max_owner_bytes;
    mc_nbt_byte_input port = mc_gzip_input_byte_port(r->gzip);
    ns = mc_nbt_stream_construct(r->reader, &port, &limits, &s->work.input);
    if (ns != MC_NBT_OK) {
        source_input_nbt(s, ns, "c", call_pc);
        s->work.prior_site = (mc_world_source_site){"as", "a", "(Ljava/io/InputStream;)Lnu;", 12};
        return s->work.status;
    }
    r->input_constructed = true;
    ns = mc_nbt_stream_parse(r->reader, true, &r->document, &s->work.input);
    if (ns != MC_NBT_OK) {
        source_input_nbt(s, ns, "c", call_pc);
        (void)mc_gzip_input_last_error(r->gzip, &s->work.gzip);
        s->work.prior_site = (mc_world_source_site){"as", "a", "(Ljava/io/InputStream;)Lnu;", 17};
        if (!s->work.source_exception)
            return s->work.status; /* Native policy is not a Throwable/finally. */
        mc_nbt_status cs = mc_nbt_stream_source_close(r->reader, &s->work.input_close);
        (void)mc_gzip_input_last_close_error(r->gzip, &s->work.gzip_close);
        r->close_pc = 29; /* as, not fm. */
        if (cs != MC_NBT_OK) {
            if (s->work.input_close.source_fault == MC_NBT_SOURCE_NONE) {
                s->work.status = s->work.cause_status =
                    cs == MC_NBT_STREAM_BUSY ? MC_WORLD_SOURCE_BUSY : MC_WORLD_SOURCE_FILE;
                s->work.native_policy = true;
                s->work.source_exception = false;
                return s->work.status;
            }
            s->work.prior_site.pc = 29; /* Source finally supersedes the read cause. */
        } else {
            r->closed = true;
        }
        return source_metadata_caught(s, call_pc);
    }
    r->parsed = true;
    ns = mc_nbt_stream_source_close(r->reader, &s->work.input_close);
    r->close_pc = 22;
    if (ns != MC_NBT_OK) {
        (void)mc_gzip_input_last_close_error(r->gzip, &s->work.gzip_close);
        s->work.input = s->work.input_close;
        source_input_nbt(s, ns, "c", call_pc);
        s->work.prior_site = (mc_world_source_site){"as", "a", "(Ljava/io/InputStream;)Lnu;", 22};
        return s->work.source_exception ? source_metadata_caught(s, call_pc) : s->work.status;
    }
    r->closed = true;
    r->last_pc = r->old_file ? 88 : 34;
    mc_world_source_status result =
        source_literal(c, "Data", &c->data_key, "c", r->last_pc, &s->work);
    if (result != MC_WORLD_SOURCE_OK)
        return result;
    r->last_pc = r->old_file ? 90 : 36;
    mc_world_info_status is = mc_world_info_data_adopt_document(
        &r->document, c->options.max_owner_bytes, &r->data, &s->work.info);
    if (is == MC_WORLD_INFO_OK) {
        r->last_pc = r->old_file ? 94 : 40;
        is = mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &r->info);
        if (is != MC_WORLD_INFO_OK)
            s->work.info = (mc_world_info_error){.status = is};
    }
    if (is == MC_WORLD_INFO_OK) {
        r->last_pc = r->old_file ? 99 : 45;
        is = mc_world_info_construct_nbt(r->info, r->data, &s->work.info);
    }
    if (is != MC_WORLD_INFO_OK) {
        s->work.status = s->work.cause_status = is == MC_WORLD_INFO_LIMIT ? MC_WORLD_SOURCE_LIMIT
                                                : is == MC_WORLD_INFO_ALLOCATION_FAILED
                                                    ? MC_WORLD_SOURCE_ALLOCATION
                                                    : MC_WORLD_SOURCE_FILE;
        s->work.frame_count = 0;
        source_push(&s->work, "fm", "c", "()Lei;", r->last_pc);
        s->work.operation = "actual decoded Data/ei construction";
        s->work.native_policy = true;
        return is == MC_WORLD_INFO_INVALID_FIELD ? source_metadata_caught(s, r->last_pc)
                                                 : s->work.status;
    }
    return MC_WORLD_SOURCE_OK;
}

static mc_world_source_status source_metadata_candidate(mc_world_store_source *s, bool old) {
    mc_world_source_input_state *r = s->input;
    mc_world_store_source_classes *c = s->classes;
    r->old_file = old;
    r->last_pc = old ? 54 : 0;
    mc_java_file_status fs = mc_java_file_allocate(c->files, &r->candidate, &s->work.file);
    if (fs != MC_JAVA_FILE_OK)
        return source_file(&s->work, fs, "c", r->last_pc);
    r->last_pc = old ? 62 : 8;
    mc_world_source_status result =
        source_literal(c, old ? "level.dat_old" : "level.dat", old ? &c->old_name : &c->level_name,
                       "c", r->last_pc, &s->work);
    if (result != MC_WORLD_SOURCE_OK)
        return result;
    r->last_pc = old ? 64 : 10;
    fs = mc_java_file_construct_child(r->candidate, s->state.source_b,
                                      old ? c->old_name : c->level_name, &s->work.file);
    if (fs != MC_JAVA_FILE_OK)
        return source_file(&s->work, fs, "c", r->last_pc);
    fs = mc_java_file_release(&r->file, &s->work.file);
    if (fs != MC_JAVA_FILE_OK)
        return source_file(&s->work, fs, "<native-local-retire>", -1);
    r->file = r->candidate;
    r->candidate = NULL;
    r->last_pc = old ? 69 : 15;
    fs = mc_java_file_exists(r->file, &r->exists, &s->work.file);
    if (fs != MC_JAVA_FILE_OK)
        return source_file(&s->work, fs, "c", r->last_pc);
    if (!r->exists)
        return MC_WORLD_SOURCE_OK;
    r->last_pc = old ? 75 : 21;
    fs = mc_java_file_stream_allocate(c->files, &r->stream, &s->work.file);
    if (fs != MC_JAVA_FILE_OK)
        return source_file(&s->work, fs, "c", r->last_pc);
    r->last_pc = old ? 80 : 26;
    fs = mc_java_file_stream_construct(r->stream, r->file, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "c", r->last_pc);
        return fs == MC_JAVA_FILE_IO_ERROR ? source_metadata_caught(s, r->last_pc) : s->work.status;
    }
    return source_metadata_decode(s);
}

mc_world_source_status mc_world_store_source_read_info(mc_world_store *store, mc_world_info **out,
                                                       mc_world_source_error *e) {
    if (out == NULL)
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "fm.c native output slot", NULL, NULL,
                             NULL, -1);
    mc_world_store_source *s;
    mc_world_source_status status =
        source_input_begin(store, MC_WORLD_SOURCE_INPUT_METADATA, &s, e);
    if (status != MC_WORLD_SOURCE_OK)
        return status;
    status = source_metadata_candidate(s, false);
    if (status == MC_WORLD_SOURCE_OK && s->input->info == NULL)
        status = source_metadata_candidate(s, true);
    if (status == MC_WORLD_SOURCE_OK) {
        s->input->source_returned = true;
        s->input->last_pc = s->input->info == NULL ? 109 : s->input->old_file ? 102 : 48;
        *out = s->input->info;
        s->input->info = NULL;
        s->input->output_transferred = true;
    }
    return source_input_end(s, e);
}

mc_world_source_status mc_world_store_source_check_session(mc_world_store *store,
                                                           mc_world_source_error *e) {
    mc_world_store_source *s;
    mc_world_source_status status = source_input_begin(store, MC_WORLD_SOURCE_INPUT_SESSION, &s, e);
    if (status != MC_WORLD_SOURCE_OK)
        return status;
    mc_world_source_input_state *r = s->input;
    mc_world_store_source_classes *c = s->classes;
    r->last_pc = 0;
    mc_java_file_status fs = mc_java_file_allocate(c->files, &r->candidate, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "b", 0);
        goto done;
    }
    status = source_literal(c, "session.lock", &c->session_lock, "b", 8, &s->work);
    if (status != MC_WORLD_SOURCE_OK)
        goto done;
    r->last_pc = 10;
    fs = mc_java_file_construct_child(r->candidate, s->state.source_b, c->session_lock,
                                      &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "b", 10);
        goto done;
    }
    r->file = r->candidate;
    r->candidate = NULL;
    r->last_pc = 14;
    mc_nbt_status ns =
        mc_nbt_stream_allocate(c->options.max_owner_bytes, &r->reader, &s->work.input);
    if (ns != MC_NBT_OK) {
        source_input_nbt(s, ns, "b", 14);
        goto done;
    }
    r->last_pc = 18;
    fs = mc_java_file_stream_allocate(c->files, &r->stream, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "b", 18);
        goto done;
    }
    r->last_pc = 23;
    fs = mc_java_file_stream_construct(r->stream, r->file, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "b", 23);
        if (fs == MC_JAVA_FILE_IO_ERROR) {
            s->work.caught_io = true;
            s->work.source_exception = true;
            s->work.cause_status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
            s->work.cause_site = (mc_world_source_site){"fm", "b", "()V", 23};
            source_session_exception_owner(s, 70);
        }
        goto done;
    }
    mc_nbt_byte_input port = {r->stream, source_fis_one, source_fis_read, source_fis_close};
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = c->options.max_owner_bytes;
    r->last_pc = 26;
    ns = mc_nbt_stream_construct(r->reader, &port, &limits, &s->work.input);
    if (ns != MC_NBT_OK) {
        source_input_nbt(s, ns, "b", 26);
        goto done;
    }
    r->input_constructed = true;
    r->last_pc = 31;
    ns = mc_nbt_stream_read_long(r->reader, &r->observed_token, &s->work.input);
    if (ns != MC_NBT_OK) {
        source_input_nbt(s, ns, "b", 31);
        s->work.cause_site = (mc_world_source_site){"fm", "b", "()V", 31};
        if (!s->work.source_exception)
            goto done;
        r->close_pc = 61;
        mc_nbt_status cs = mc_nbt_stream_source_close(r->reader, &s->work.input_close);
        if (cs != MC_NBT_OK && s->work.input_close.source_fault == MC_NBT_SOURCE_NONE) {
            s->work.status = s->work.cause_status =
                cs == MC_NBT_STREAM_BUSY ? MC_WORLD_SOURCE_BUSY : MC_WORLD_SOURCE_FILE;
            s->work.native_policy = true;
            s->work.source_exception = false;
            goto done;
        }
        r->closed = cs == MC_NBT_OK;
        s->work.caught_io = true;
        if (cs != MC_NBT_OK)
            s->work.prior_site = (mc_world_source_site){"fm", "b", "()V", 61};
        source_session_exception_owner(s, 70);
        goto done;
    }
    r->token_read = true;
    r->token_matches = r->observed_token == store->session_token; /* actual live e read35 */
    if (!r->token_matches) {
        s->work.session_mismatch = true;
        s->work.cause_site = (mc_world_source_site){"fm", "b", "()V", 35};
        source_session_exception_owner(s, 42);
        goto done;
    }
    r->close_pc = 53;
    ns = mc_nbt_stream_source_close(r->reader, &s->work.input_close);
    if (ns != MC_NBT_OK) {
        s->work.input = s->work.input_close;
        source_input_nbt(s, ns, "b", 53);
        s->work.cause_site = (mc_world_source_site){"fm", "b", "()V", 53};
        if (s->work.source_exception) {
            s->work.caught_io = true;
            source_session_exception_owner(s, 70);
        }
        goto done;
    }
    r->closed = r->source_returned = true;
    r->last_pc = 80;
done:
    return source_input_end(s, e);
}
