/* Included once by the canonical store.c, after its unchanged legacy bodies. */
static const char *source_method_descriptor(const char *method);

static mc_world_source_status source_result(mc_world_source_error *e, mc_world_source_status s,
                                            const char *op, const char *owner, const char *method,
                                            const char *desc, int pc) {
    if (e != NULL) {
        memset(e, 0, sizeof(*e));
        e->status = e->cause_status = s;
        e->operation = op;
        e->native_policy = s != MC_WORLD_SOURCE_OK;
        if (owner != NULL) {
            e->frames[0] = (mc_world_source_site){owner, method, desc, pc};
            e->frame_count = 1;
        }
    }
    return s;
}

static void source_push(mc_world_source_error *e, const char *owner, const char *method,
                        const char *desc, int pc) {
    if (e == NULL)
        return;
    size_t n = e->frame_count < 6 ? e->frame_count : 5;
    memmove(e->frames + 1, e->frames, n * sizeof(*e->frames));
    e->frames[0] = (mc_world_source_site){owner, method, desc, pc};
    e->frame_count = n + 1;
}

static mc_world_source_status source_copy(mc_world_source_error *out,
                                          const mc_world_source_error *error) {
    if (out != NULL)
        *out = *error;
    return error->status;
}

static mc_world_source_status source_file(mc_world_source_error *e, mc_java_file_status s,
                                          const char *method, int pc) {
    e->status = e->cause_status = s == MC_JAVA_FILE_LIMIT               ? MC_WORLD_SOURCE_LIMIT
                                  : s == MC_JAVA_FILE_ALLOCATION_FAILED ? MC_WORLD_SOURCE_ALLOCATION
                                  : s == MC_JAVA_FILE_BUSY              ? MC_WORLD_SOURCE_BUSY
                                                                        : MC_WORLD_SOURCE_FILE;
    e->operation = e->file.operation;
    e->source_exception =
        s == MC_JAVA_FILE_SOURCE_NULL_STRING || s == MC_JAVA_FILE_SOURCE_NULL_FILE;
    e->native_policy = !e->source_exception;
    if (e->source_exception) {
        e->status = e->cause_status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
        e->exception_class = "java/lang/NullPointerException";
    }
    if (pc >= 0)
        source_push(e, "fm", method, source_method_descriptor(method), pc);
    return e->status;
}

static mc_world_source_status source_output(mc_world_source_error *e, mc_java_output_status s,
                                            int pc) {
    e->status = e->cause_status = s == MC_JAVA_OUTPUT_BUSY         ? MC_WORLD_SOURCE_BUSY
                                  : s == MC_JAVA_OUTPUT_LIMIT      ? MC_WORLD_SOURCE_LIMIT
                                  : s == MC_JAVA_OUTPUT_ALLOCATION ? MC_WORLD_SOURCE_ALLOCATION
                                                                   : MC_WORLD_SOURCE_OUTPUT;
    e->operation = e->output.cause.operation;
    e->source_exception = e->output.cause.source_exception;
    e->exception_class = e->output.cause.exception_class;
    e->native_policy = !e->source_exception;
    if (e->source_exception)
        e->status = e->cause_status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
    if (pc >= 0)
        source_push(e, "fm", "d", "()V", pc);
    return e->status;
}

mc_world_source_options mc_world_store_source_default_options(void) {
    return (mc_world_source_options){1048576u, 65536u, 64u, mc_java_output_default_options()};
}

static const char *source_method_descriptor(const char *method) {
    if (strcmp(method, "a") == 0)
        return "(Ljava/lang/String;)Ljava/io/File;";
    if (strcmp(method, "c") == 0)
        return "()Lei;";
    return strcmp(method, "<init>") == 0 ? "(Ljava/io/File;Ljava/lang/String;Z)V" : "()V";
}

static mc_world_source_status source_literal(mc_world_store_source_classes *c, const char *text,
                                             mc_world_info_name **slot, const char *method, int pc,
                                             mc_world_source_error *e) {
    if (*slot != NULL)
        return MC_WORLD_SOURCE_OK;
    if (c->literal_allocations == c->options.max_allocations)
        return source_result(e, MC_WORLD_SOURCE_LIMIT, "fm native literal allocation bound", "fm",
                             method, source_method_descriptor(method), pc);
    const size_t length = strlen(text);
    uint16_t units[32];
    if (length > sizeof(units) / sizeof(*units))
        return source_result(e, MC_WORLD_SOURCE_LIMIT, "fm native literal extent", "fm", method,
                             "()V", pc);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint8_t)text[i];
    mc_world_info_status s =
        mc_world_info_name_copy((mc_nbt_string){units, length}, c->options.max_literal_bytes, slot);
    if (s != MC_WORLD_INFO_OK) {
        source_result(
            e, s == MC_WORLD_INFO_LIMIT ? MC_WORLD_SOURCE_LIMIT : MC_WORLD_SOURCE_ALLOCATION,
            "fm native literal storage", "fm", method, source_method_descriptor(method), pc);
        e->string_status = s;
        return e->status;
    }
    ++c->literal_allocations;
    return MC_WORLD_SOURCE_OK;
}

mc_world_source_status mc_world_store_source_classes_create(mc_java_files *files,
                                                            mc_java_logging *logging,
                                                            const mc_world_source_options *options,
                                                            mc_world_store_source_classes **out,
                                                            mc_world_source_error *e) {
    if (files == NULL || logging == NULL || out == NULL)
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "fm cold class arguments", NULL, NULL,
                             NULL, -1);
    const mc_world_source_options o =
        options == NULL ? mc_world_store_source_default_options() : *options;
    if (o.max_owner_bytes < sizeof(mc_world_store_source_classes))
        return source_result(e, MC_WORLD_SOURCE_LIMIT, "fm cold class byte bound", NULL, NULL, NULL,
                             -1);
    mc_world_store_source_classes *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return source_result(e, MC_WORLD_SOURCE_ALLOCATION, "fm cold class storage", NULL, NULL,
                             NULL, -1);
    mc_java_file_status fs = mc_java_files_retain(files, &c->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&c->work, fs, "<clinit>", -1);
        mc_world_source_status status = c->work.status;
        source_copy(e, &c->work);
        free(c);
        return status;
    }
    c->files = files;
    mc_java_logging_status ls = mc_java_logging_retain(logging, &c->work.logging);
    if (ls != MC_JAVA_LOGGING_OK) {
        c->work.status = c->work.cause_status = MC_WORLD_SOURCE_LOGGING;
        c->work.operation = "fm cold Logger service hold";
        c->work.native_policy = true;
        source_copy(e, &c->work);
        (void)mc_java_files_release(&c->files, NULL);
        free(c);
        return MC_WORLD_SOURCE_LOGGING;
    }
    c->logging = logging;
    c->holds = 1;
    c->options = o;
    c->last_store_pc = -1;
    *out = c;
    return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
}

mc_world_source_status mc_world_store_source_classes_retain(mc_world_store_source_classes *c,
                                                            mc_world_source_error *e) {
    if (c == NULL)
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "fm class hold", NULL, NULL, NULL, -1);
    if (c->active || c->phase == MC_WORLD_SOURCE_RETIRING)
        return source_result(e, MC_WORLD_SOURCE_BUSY, "fm active/retiring class hold", NULL, NULL,
                             NULL, -1);
    if (c->holds == SIZE_MAX)
        return source_result(e, MC_WORLD_SOURCE_LIMIT, "fm class hold count", NULL, NULL, NULL, -1);
    ++c->holds;
    return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
}

static mc_world_source_status source_class_activate(mc_world_store_source_classes *c,
                                                    mc_world_source_error *e) {
    if (c->active || c->phase == MC_WORLD_SOURCE_RETIRING)
        return source_result(e, MC_WORLD_SOURCE_BUSY, "fm active/retiring initialization", NULL,
                             NULL, NULL, -1);
    if (c->phase == MC_WORLD_SOURCE_READY)
        return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    if (c->phase == MC_WORLD_SOURCE_FAILED) {
        source_copy(e, &c->failure);
        if (e != NULL)
            e->status = MC_WORLD_SOURCE_LATCHED;
        return MC_WORLD_SOURCE_LATCHED;
    }
    c->active = true;
    c->phase = MC_WORLD_SOURCE_CONSTRUCTING;
    source_result(&c->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    mc_world_source_status s =
        source_literal(c, "Minecraft", &c->minecraft, "<clinit>", 0, &c->work);
    if (s == MC_WORLD_SOURCE_OK) {
        mc_java_logging_status ls =
            mc_java_logging_get_minecraft(c->logging, &c->pending_logger, &c->work.logging);
        if (ls != MC_JAVA_LOGGING_OK) {
            c->work.status = c->work.cause_status = MC_WORLD_SOURCE_LOGGING;
            c->work.operation = "actual Logger.getLogger(Minecraft)";
            c->work.native_policy = true;
            source_push(&c->work, "fm", "<clinit>", "()V", 2);
            s = c->work.status;
        } else {
            c->source_a = c->pending_logger;
            c->pending_logger = NULL;
            c->last_store_pc = 5; /* Adopt the SAME returned occurrence at putstatic5. */
        }
    }
    c->phase = s == MC_WORLD_SOURCE_OK ? MC_WORLD_SOURCE_READY : MC_WORLD_SOURCE_FAILED;
    c->active = false;
    if (s != MC_WORLD_SOURCE_OK)
        c->failure = c->work;
    return source_copy(e, &c->work);
}

mc_world_source_status mc_world_store_source_classes_release(mc_world_store_source_classes **slot,
                                                             mc_world_source_error *e) {
    if (slot == NULL)
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "fm class release slot", NULL, NULL, NULL,
                             -1);
    mc_world_store_source_classes *c = *slot;
    if (c == NULL)
        return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    if (c->active)
        return source_result(e, MC_WORLD_SOURCE_BUSY, "fm active class release", NULL, NULL, NULL,
                             -1);
    if (c->holds > 1) {
        --c->holds;
        *slot = NULL;
        return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    }
    if (c->handlers != 0)
        return source_result(e, MC_WORLD_SOURCE_BUSY, "fm live handler class borrow", NULL, NULL,
                             NULL, -1);
    c->phase = MC_WORLD_SOURCE_RETIRING;
    c->active = true;
    source_result(&c->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    mc_world_source_region_error re;
    mc_world_source_status rs = mc_world_source_regions_release(&c->regions, &re);
    if (rs != MC_WORLD_SOURCE_OK) {
        c->active = false;
        c->cleanup = re.source;
        return source_copy(e, &c->cleanup);
    }
    mc_java_logging_status ls = mc_java_logger_release(&c->pending_logger, &c->work.logging);
    if (ls == MC_JAVA_LOGGING_OK)
        ls = mc_java_logger_release(&c->source_a, &c->work.logging);
    if (ls == MC_JAVA_LOGGING_OK)
        ls = mc_java_logging_release(&c->logging, &c->work.logging);
    if (ls != MC_JAVA_LOGGING_OK) {
        c->active = false;
        c->work.status = c->work.cause_status =
            ls == MC_JAVA_LOGGING_BUSY ? MC_WORLD_SOURCE_BUSY : MC_WORLD_SOURCE_LOGGING;
        c->work.operation = "fm Logger native retirement";
        c->work.native_policy = true;
        c->cleanup = c->work;
        return source_copy(e, &c->cleanup);
    }
    mc_java_file_status fs = mc_java_files_release(&c->files, &c->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        c->active = false;
        source_file(&c->work, fs, "<clinit>", -1);
        c->cleanup = c->work;
        return source_copy(e, &c->cleanup);
    }
    mc_world_info_name_release(c->minecraft);
    mc_world_info_name_release(c->players);
    mc_world_info_name_release(c->data);
    mc_world_info_name_release(c->session_lock);
    mc_world_info_name_release(c->level_name);
    mc_world_info_name_release(c->old_name);
    mc_world_info_name_release(c->data_key);
    *slot = NULL;
    free(c);
    return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
}

mc_world_source_status mc_world_store_source_classes_retire(mc_world_store_source_classes **slot,
                                                            mc_world_source_error *e) {
    if (slot == NULL)
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "fm strict class slot", NULL, NULL, NULL,
                             -1);
    if (*slot != NULL && ((*slot)->holds != 1 || (*slot)->handlers != 0 || (*slot)->active))
        return source_result(e, MC_WORLD_SOURCE_BUSY, "fm strict class live occurrences", NULL,
                             NULL, NULL, -1);
    return mc_world_store_source_classes_release(slot, e);
}

bool mc_world_store_source_classes_inspect(const mc_world_store_source_classes *c,
                                           mc_world_source_class_state *out) {
    if (c == NULL || out == NULL || c->active)
        return false;
    *out = (mc_world_source_class_state){.phase = c->phase,
                                         .holds = c->holds,
                                         .handlers = c->handlers,
                                         .literal_allocations = c->literal_allocations,
                                         .last_store_pc = c->last_store_pc,
                                         .files = c->files,
                                         .logging = c->logging,
                                         .source_a = c->source_a,
                                         .pending_logger = c->pending_logger,
                                         .minecraft = c->minecraft,
                                         .players = c->players,
                                         .data = c->data,
                                         .session_lock = c->session_lock,
                                         .failure = &c->failure,
                                         .cleanup = &c->cleanup};
    return true;
}

mc_world_source_status mc_world_store_source_allocate(mc_world_store_source_classes *c,
                                                      mc_world_store **out,
                                                      mc_world_source_error *e) {
    if (c == NULL || out == NULL)
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "mx new native arguments", NULL, NULL,
                             NULL, -1);
    mc_world_source_status s = source_class_activate(c, e);
    if (s != MC_WORLD_SOURCE_OK)
        return s;
    if (c->options.max_owner_bytes < sizeof(mc_world_store) ||
        c->options.max_owner_bytes - sizeof(mc_world_store) < sizeof(mc_world_store_source) ||
        c->options.max_allocations < 2 || c->handlers == SIZE_MAX || c->holds == SIZE_MAX)
        return source_result(e, MC_WORLD_SOURCE_LIMIT, "mx native owner allocation bound", NULL,
                             NULL, NULL, -1);
    mc_world_store *store = calloc(1, sizeof(*store));
    if (store == NULL)
        return source_result(e, MC_WORLD_SOURCE_ALLOCATION, "mx native Store allocation", NULL,
                             NULL, NULL, -1);
    mc_world_store_source *source = calloc(1, sizeof(*source));
    if (source == NULL) {
        free(store);
        return source_result(e, MC_WORLD_SOURCE_ALLOCATION, "mx native retained prefix storage",
                             NULL, NULL, NULL, -1);
    }
    ++c->holds;
    ++c->handlers;
    source->classes = c;
    source->state.holds = 1;
    source->state.allocations = 2;
    source->state.last_store_pc = -1;
    source->state.last_close_pc = -1;
    store->source = source;
    store->limits = mc_world_store_default_limits();
    *out = store;
    return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
}

/* Source IOException reaches the actual diagnostic invocation. No fabricated
 * Throwable/PrintStream or RuntimeException completion is installed here. */
static mc_world_source_status source_caught_io(mc_world_store_source *s) {
    if (s->work.frame_count != 0)
        s->work.cause_site = s->work.frames[0];
    s->work.frame_count = 0;
    s->work.status = MC_WORLD_SOURCE_UNIMPLEMENTED;
    s->work.operation = "java/io/IOException.printStackTrace()V";
    s->work.caught_io = s->work.diagnostic_frontier = true;
    s->work.native_policy = true;
    source_push(&s->work, "fm", "d", "()V", 57);
    return s->work.status;
}

static mc_world_source_status source_session(mc_world_store *store) {
    mc_world_store_source *s = store->source;
    mc_world_source_state *v = &s->state;
    mc_world_store_source_classes *c = s->classes;
    v->session_entered = true;
    mc_java_file_status fs = mc_java_file_allocate(c->files, &v->pending_file, &s->work.file);
    if (fs != MC_JAVA_FILE_OK)
        return source_file(&s->work, fs, "d", 0);
    mc_world_source_status result =
        source_literal(c, "session.lock", &c->session_lock, "d", 8, &s->work);
    if (result != MC_WORLD_SOURCE_OK)
        return result;
    fs = mc_java_file_construct_child(v->pending_file, v->source_b, c->session_lock, &s->work.file);
    if (fs != MC_JAVA_FILE_OK)
        return source_file(&s->work, fs, "d", 10);
    v->session_file = v->pending_file;
    v->pending_file = NULL;
    v->session_file_stored = true;
    mc_java_output_status os =
        mc_java_data_output_allocate(&c->options.output, &v->pending_dos, &s->work.output);
    if (os != MC_JAVA_OUTPUT_OK)
        return source_output(&s->work, os, 14);
    os = mc_java_file_output_allocate(c->files, &c->options.output, &v->pending_fos,
                                      &s->work.output);
    if (os != MC_JAVA_OUTPUT_OK)
        return source_output(&s->work, os, 18);
    os = mc_java_file_output_construct(v->pending_fos, v->session_file, &s->work.output);
    if (os != MC_JAVA_OUTPUT_OK) {
        source_output(&s->work, os, 23);
        return os == MC_JAVA_OUTPUT_SOURCE_IO && s->work.source_exception ? source_caught_io(s)
                                                                          : s->work.status;
    }
    os = mc_java_data_output_construct(v->pending_dos, v->pending_fos, &s->work.output);
    if (os != MC_JAVA_OUTPUT_OK)
        return source_output(&s->work, os, 26);
    v->output_stored = true;
    os = mc_java_data_output_write_long(v->pending_dos, store->session_token, &s->work.output);
    if (os != MC_JAVA_OUTPUT_OK) {
        source_output(&s->work, os, 35);
        if (!s->work.source_exception)
            return s->work.status; /* Native policy does not invent a Throwable/finally. */
        s->work.prior_site = (mc_world_source_site){"fm", "d", "()V", 35};
        v->last_close_pc = 47;
        mc_java_output_status close =
            mc_java_data_output_source_close(v->pending_dos, &s->work.finally_close);
        if (close != MC_JAVA_OUTPUT_OK) {
            /* Finally close supersedes the original source Throwable. Retain
             * both complete children; output remains the original write cause. */
            s->work.status = s->work.cause_status =
                close == MC_JAVA_OUTPUT_LIMIT        ? MC_WORLD_SOURCE_LIMIT
                : close == MC_JAVA_OUTPUT_BUSY       ? MC_WORLD_SOURCE_BUSY
                : close == MC_JAVA_OUTPUT_ALLOCATION ? MC_WORLD_SOURCE_ALLOCATION
                                                     : MC_WORLD_SOURCE_OUTPUT;
            s->work.operation = s->work.finally_close.cause.operation;
            s->work.exception_class = s->work.finally_close.cause.exception_class;
            s->work.source_exception = s->work.finally_close.cause.source_exception;
            s->work.native_policy = !s->work.source_exception;
            if (s->work.source_exception)
                s->work.status = s->work.cause_status = MC_WORLD_SOURCE_SOURCE_EXCEPTION;
            s->work.frame_count = 0;
            source_push(&s->work, "fm", "d", "()V", 47);
            return close == MC_JAVA_OUTPUT_SOURCE_IO && s->work.source_exception
                       ? source_caught_io(s)
                       : s->work.status;
        }
        v->close_returned = true;
        return os == MC_JAVA_OUTPUT_SOURCE_IO ? source_caught_io(s) : s->work.status;
    }
    v->write_returned = true;
    v->last_close_pc = 39;
    os = mc_java_data_output_source_close(v->pending_dos, &s->work.output);
    if (os != MC_JAVA_OUTPUT_OK) {
        source_output(&s->work, os, 39);
        return os == MC_JAVA_OUTPUT_SOURCE_IO && s->work.source_exception ? source_caught_io(s)
                                                                          : s->work.status;
    }
    v->close_returned = v->d_returned = true;
    return MC_WORLD_SOURCE_OK;
}

mc_world_source_status mc_world_store_source_construct(mc_world_store *store, mc_java_file *parent,
                                                       mc_world_info_name *folder, bool players,
                                                       mc_world_source_error *e) {
    if (store == NULL || store->source == NULL)
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "mx source constructor receiver", NULL,
                             NULL, NULL, -1);
    mc_world_store_source *s = store->source;
    mc_world_source_state *v = &s->state;
    mc_world_store_source_classes *c = s->classes;
    if (v->active || c->active || c->phase != MC_WORLD_SOURCE_READY ||
        v->phase != MC_WORLD_SOURCE_RAW)
        return source_result(e,
                             v->active || c->active ? MC_WORLD_SOURCE_BUSY : MC_WORLD_SOURCE_STATE,
                             "mx one-shot constructor admission", NULL, NULL, NULL, -1);
    v->active = true;
    c->active = true;
    v->phase = MC_WORLD_SOURCE_CONSTRUCTING;
    source_result(&s->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    /* Object.<init>1 is the actual empty Object body. No FP admission. */
    int64_t token = 0;
    mc_platform_millis_status clock = mc_platform_current_time_millis(&token);
    mc_world_source_status result = MC_WORLD_SOURCE_OK;
    if (clock != MC_PLATFORM_MILLIS_OK) {
        source_result(&s->work, MC_WORLD_SOURCE_CLOCK,
                      "actual System.currentTimeMillis native prerequisite", "fm", "<init>",
                      "(Ljava/io/File;Ljava/lang/String;Z)V", 5);
        s->work.platform_clock_status = (int)clock;
        goto failed;
    }
    store->session_token = token;
    v->e_stored = true;
    v->last_store_pc = 8;
    mc_java_file_status fs = mc_java_file_allocate(c->files, &v->pending_file, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "<init>", 12);
        goto failed;
    }
    fs = mc_java_file_construct_child(v->pending_file, parent, folder, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "<init>", 18);
        goto failed;
    }
    v->source_b = v->pending_file;
    v->pending_file = NULL;
    v->last_store_pc = 21;
    bool ignored = false;
    fs = mc_java_file_mkdirs(v->source_b, &ignored, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "<init>", 28);
        goto failed;
    }
    fs = mc_java_file_allocate(c->files, &v->pending_file, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "<init>", 33);
        goto failed;
    }
    result = source_literal(c, "players", &c->players, "<init>", 41, &s->work);
    if (result != MC_WORLD_SOURCE_OK)
        goto failed;
    fs = mc_java_file_construct_child(v->pending_file, v->source_b, c->players, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "<init>", 43);
        goto failed;
    }
    v->source_c = v->pending_file;
    v->pending_file = NULL;
    v->last_store_pc = 46;
    fs = mc_java_file_allocate(c->files, &v->pending_file, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "<init>", 50);
        goto failed;
    }
    result = source_literal(c, "data", &c->data, "<init>", 58, &s->work);
    if (result != MC_WORLD_SOURCE_OK)
        goto failed;
    fs = mc_java_file_construct_child(v->pending_file, v->source_b, c->data, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "<init>", 60);
        goto failed;
    }
    v->source_d = v->pending_file;
    v->pending_file = NULL;
    v->last_store_pc = 63;
    fs = mc_java_file_mkdirs(v->source_d, &ignored, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "<init>", 70);
        goto failed;
    }
    if (players) {
        fs = mc_java_file_mkdirs(v->source_c, &ignored, &s->work.file);
        if (fs != MC_JAVA_FILE_OK) {
            source_file(&s->work, fs, "<init>", 82);
            goto failed;
        }
    }
    result = source_session(store);
    if (result != MC_WORLD_SOURCE_OK)
        goto failed;
    v->fm_returned = v->mx_returned = true;
    v->phase = MC_WORLD_SOURCE_READY;
    v->active = false;
    c->active = false;
    return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
failed:
    if (v->session_entered)
        source_push(&s->work, "fm", "<init>", "(Ljava/io/File;Ljava/lang/String;Z)V", 87);
    source_push(&s->work, "mx", "<init>", "(Ljava/io/File;Ljava/lang/String;Z)V", 4);
    s->failure = s->work;
    v->phase = MC_WORLD_SOURCE_FAILED;
    v->active = false;
    c->active = false;
    return source_copy(e, &s->failure);
}

bool mc_world_store_is_source(const mc_world_store *store) {
    return store != NULL && store->source != NULL;
}

bool mc_world_store_source_inspect(const mc_world_store *store, mc_world_source_state *out) {
    if (!mc_world_store_is_source(store) || out == NULL || store->source->state.active)
        return false;
    const mc_world_store_source *s = store->source;
    *out = s->state;
    out->source_e = store->session_token; /* Same canonical cell, no source scalar mirror. */
    out->classes = s->classes;
    out->failure = &s->failure;
    out->cleanup = &s->cleanup;
    out->data_factory_file = s->data_file;
    out->data_factory_name = s->data_name;
    return true;
}

mc_world_source_status mc_world_store_source_retain(mc_world_store *store,
                                                    mc_world_source_error *e) {
    if (!mc_world_store_is_source(store))
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "source Store hold", NULL, NULL, NULL,
                             -1);
    mc_world_source_state *v = &store->source->state;
    if (v->active || store->source->classes->active || v->phase == MC_WORLD_SOURCE_RETIRING)
        return source_result(e, MC_WORLD_SOURCE_BUSY, "source Store active/retiring hold", NULL,
                             NULL, NULL, -1);
    if (v->holds == SIZE_MAX)
        return source_result(e, MC_WORLD_SOURCE_LIMIT, "source Store hold count", NULL, NULL, NULL,
                             -1);
    ++v->holds;
    return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
}

mc_world_source_status mc_world_store_source_release(mc_world_store **slot,
                                                     mc_world_source_error *e) {
    if (slot == NULL)
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "source Store release slot", NULL, NULL,
                             NULL, -1);
    mc_world_store *store = *slot;
    if (store == NULL)
        return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    if (!mc_world_store_is_source(store))
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "legacy Store is not source owner", NULL,
                             NULL, NULL, -1);
    mc_world_store_source *s = store->source;
    mc_world_source_state *v = &s->state;
    if (s->classes == NULL || s->classes->handlers == 0 || s->classes->holds == 0)
        return source_result(e, MC_WORLD_SOURCE_STATE, "source Store class occurrence ledger", NULL,
                             NULL, NULL, -1);
    if (v->active || s->classes->active)
        return source_result(e, MC_WORLD_SOURCE_BUSY, "source Store active release", NULL, NULL,
                             NULL, -1);
    if (v->holds > 1) {
        --v->holds;
        *slot = NULL;
        return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    }
    mc_world_storage_error graph;
    mc_world_storage_status graph_status = store_free_admit(store, &graph);
    if (graph_status != MC_WORLD_STORAGE_OK)
        return source_result(
            e, graph_status == MC_WORLD_STORAGE_BUSY ? MC_WORLD_SOURCE_BUSY : MC_WORLD_SOURCE_STATE,
            graph.operation, NULL, NULL, NULL, -1);
    v->active = true;
    s->classes->active = true;
    v->phase = MC_WORLD_SOURCE_RETIRING;
    source_result(&s->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    if (source_input_clear(s) != MC_WORLD_SOURCE_OK)
        goto incomplete;
    while (store->chunks != NULL) {
        store_chunk *entry = store->chunks;
        store->chunks = entry->next;
        store_chunk_free(entry);
        free(entry);
        --store->chunk_count;
    }
    store->last_chunk = NULL;
    mc_java_output_status os = mc_java_data_output_release(&v->pending_dos, &s->work.output);
    if (os == MC_JAVA_OUTPUT_OK)
        os = mc_java_file_output_release(&v->pending_fos, &s->work.output);
    if (os != MC_JAVA_OUTPUT_OK) {
        source_output(&s->work, os, -1);
        s->work.source_exception = false; /* Native retirement never enters source catch/finally. */
        s->work.native_policy = true;
        goto incomplete;
    }
    mc_java_file **files[] = {&s->data_file, &v->session_file, &v->pending_file,
                              &v->source_d,  &v->source_c,     &v->source_b};
    for (size_t i = 0; i < sizeof(files) / sizeof(*files); ++i) {
        mc_java_file_status fs = mc_java_file_release(files[i], &s->work.file);
        if (fs != MC_JAVA_FILE_OK) {
            source_file(&s->work, fs, "<native-retire>", -1);
            goto incomplete;
        }
    }
    mc_world_info_name_release(s->data_name);
    s->data_name = NULL;
    /* The last handler count ends immediately before its class occurrence.
     * A failed class release restores the count for exact retry admission. */
    mc_world_store_source_classes *c = s->classes;
    --c->handlers;
    c->active = false;
    mc_world_source_status cs = mc_world_store_source_classes_release(&s->classes, &s->work);
    if (cs != MC_WORLD_SOURCE_OK) {
        ++c->handlers;
        goto incomplete;
    }
    *slot = NULL;
    free(s);
    free(store);
    return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
incomplete:
    s->classes->active = false;
    s->cleanup = s->work;
    v->active = false;
    return source_copy(e, &s->cleanup);
}

mc_world_source_status mc_world_store_source_retire(mc_world_store **slot,
                                                    mc_world_source_error *e) {
    if (slot == NULL)
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "source Store strict slot", NULL, NULL,
                             NULL, -1);
    if (*slot != NULL && (!mc_world_store_is_source(*slot) || (*slot)->source->state.holds != 1))
        return source_result(e, MC_WORLD_SOURCE_BUSY, "source Store strict live occurrences", NULL,
                             NULL, NULL, -1);
    return mc_world_store_source_release(slot, e);
}

static mc_world_source_status source_admit(mc_world_store *store, mc_world_source_error *e) {
    if (!mc_world_store_is_source(store))
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "actual source handler required", NULL,
                             NULL, NULL, -1);
    mc_world_store_source *s = store->source;
    if (s->state.active || s->classes->active || s->state.phase != MC_WORLD_SOURCE_READY)
        return source_result(
            e, s->state.active || s->classes->active ? MC_WORLD_SOURCE_BUSY : MC_WORLD_SOURCE_STATE,
            "source handler operation phase", NULL, NULL, NULL, -1);
    return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
}

#include "source_metadata.inc"

mc_world_source_status mc_world_store_source_classes_bind_regions(mc_world_store_source_classes *c,
                                                                  mc_world_source_regions *r,
                                                                  mc_world_source_error *e) {
    if (c == NULL || r == NULL)
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "cold rj binding", NULL, NULL, NULL, -1);
    mc_world_source_regions_state v;
    if (c->active || c->handlers != 0 || c->regions != NULL ||
        !mc_world_source_regions_inspect(r, &v) || v.files != c->files)
        return source_result(e, MC_WORLD_SOURCE_BUSY, "cold rj binding owner/domain", NULL, NULL,
                             NULL, -1);
    mc_world_source_region_error re;
    mc_world_source_status status = mc_world_source_regions_retain(r, &re);
    if (status != MC_WORLD_SOURCE_OK)
        return source_copy(e, &re.source);
    c->regions = r;
    return source_result(e, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
}

mc_world_source_regions *mc_world_store_source_regions(const mc_world_store *store) {
    return mc_world_store_is_source(store) ? store->source->classes->regions : NULL;
}

mc_java_files *mc_world_store_source_files(const mc_world_store *store) {
    return mc_world_store_is_source(store) ? store->source->classes->files : NULL;
}

mc_world_source_status mc_world_store_source_data_file(mc_world_store *store,
                                                       mc_world_info_name *name, mc_java_file **out,
                                                       mc_world_source_error *e) {
    mc_world_source_status status = source_admit(store, e);
    if (status != MC_WORLD_SOURCE_OK)
        return status;
    if (out == NULL)
        return source_result(e, MC_WORLD_SOURCE_ARGUMENT, "fm.a(String) output", "fm", "a",
                             "(Ljava/lang/String;)Ljava/io/File;", 0);
    mc_world_store_source *s = store->source;
    s->state.active = s->classes->active = true;
    source_result(&s->work, MC_WORLD_SOURCE_OK, NULL, NULL, NULL, NULL, -1);
    mc_java_file_status fs = mc_java_file_release(&s->data_file, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "a", 0);
        goto done;
    }
    mc_world_info_name_release(s->data_name);
    s->data_name = NULL;
    fs = mc_java_file_allocate(s->classes->files, &s->data_file, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "a", 0);
        goto done;
    }
    const uint16_t null_units[] = {'n', 'u', 'l', 'l'};
    mc_nbt_string value =
        name == NULL ? (mc_nbt_string){null_units, 4} : mc_world_info_name_text(name);
    if (value.length > (SIZE_MAX / sizeof(uint16_t)) - 4u ||
        (value.length + 4u) * sizeof(uint16_t) > s->classes->options.max_literal_bytes) {
        source_result(&s->work, MC_WORLD_SOURCE_LIMIT, "fm.a StringBuilder native bound", "fm", "a",
                      "(Ljava/lang/String;)Ljava/io/File;", 21);
        goto done;
    }
    uint16_t *units = malloc((value.length + 4u) * sizeof(*units));
    if (units == NULL) {
        source_result(&s->work, MC_WORLD_SOURCE_ALLOCATION, "fm.a StringBuilder storage", "fm", "a",
                      "(Ljava/lang/String;)Ljava/io/File;", 21);
        goto done;
    }
    if (value.length != 0)
        memcpy(units, value.units, value.length * sizeof(*units));
    units[value.length] = '.';
    units[value.length + 1] = 'd';
    units[value.length + 2] = 'a';
    units[value.length + 3] = 't';
    mc_world_info_status ns =
        mc_world_info_name_copy((mc_nbt_string){units, value.length + 4u},
                                s->classes->options.max_literal_bytes, &s->data_name);
    free(units);
    if (ns != MC_WORLD_INFO_OK) {
        source_result(
            &s->work,
            ns == MC_WORLD_INFO_LIMIT ? MC_WORLD_SOURCE_LIMIT : MC_WORLD_SOURCE_ALLOCATION,
            "fm.a StringBuilder result", "fm", "a", "(Ljava/lang/String;)Ljava/io/File;", 24);
        s->work.string_status = ns;
        goto done;
    }
    fs = mc_java_file_construct_child(s->data_file, s->state.source_d, s->data_name, &s->work.file);
    if (fs != MC_JAVA_FILE_OK) {
        source_file(&s->work, fs, "a", 27);
        goto done;
    }
    *out = s->data_file;
    s->data_file = NULL;
    mc_world_info_name_release(s->data_name);
    s->data_name = NULL;
done:
    s->state.active = s->classes->active = false;
    if (s->work.status != MC_WORLD_SOURCE_OK)
        s->failure = s->work;
    return source_copy(e, &s->work);
}

static mc_world_storage_status store_source_legacy(mc_world_storage_error *error) {
    return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Source fm/mx transport requires its actual method; legacy Store transport "
                       "is not source completion");
}

static mc_world_storage_status store_source_save_frontier(mc_world_store *store,
                                                          mc_world_storage_error *error) {
    mc_world_source_error source;
    mc_world_source_status status = mc_world_store_source_check_session(store, &source);
    if (status != MC_WORLD_SOURCE_OK)
        return store_error(error,
                           status == MC_WORLD_SOURCE_BUSY    ? MC_WORLD_STORAGE_BUSY
                           : status == MC_WORLD_SOURCE_LIMIT ? MC_WORLD_STORAGE_LIMIT
                           : status == MC_WORLD_SOURCE_ALLOCATION
                               ? MC_WORLD_STORAGE_ALLOCATION_FAILED
                           : status == MC_WORLD_SOURCE_FILE ? MC_WORLD_STORAGE_IO_ERROR
                                                            : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                           source.operation);
    source_result(&store->source->failure, MC_WORLD_SOURCE_UNIMPLEMENTED,
                  "actual source ld.a rj.d/DeflaterOutputStream save transport", "ld", "a",
                  "(Lfd;Llm;)V", 16);
    return store_error(error, MC_WORLD_STORAGE_UNSUPPORTED_STATE, store->source->failure.operation);
}

static mc_world_storage_status store_source_checked_free(mc_world_store **slot,
                                                         mc_world_storage_error *error) {
    mc_world_source_status s = mc_world_store_source_release(slot, NULL);
    return store_error(
        error,
        s == MC_WORLD_SOURCE_OK     ? MC_WORLD_STORAGE_OK
        : s == MC_WORLD_SOURCE_BUSY ? MC_WORLD_STORAGE_BUSY
                                    : MC_WORLD_STORAGE_UNSUPPORTED_STATE,
        s == MC_WORLD_SOURCE_OK
            ? NULL
            : "Source Store counted native retirement; inspect retained cleanup cause");
}
