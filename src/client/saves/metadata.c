#include "internal.h"

mc_client_save_status save_clear_decode(mc_client_save_format *f, mc_client_save_error *e) {
    mc_nbt_stream_error se;
    mc_nbt_status ns = mc_nbt_stream_release(&f->state.pending_reader, &se);
    if (ns != MC_NBT_OK) {
        save_result(e, ns == MC_NBT_STREAM_BUSY ? MC_CLIENT_SAVE_BUSY : MC_CLIENT_SAVE_STATE_ERROR,
                    "native DataInput retirement", NULL, NULL, -1);
        if (e != NULL)
            e->stream = se;
        return ns == MC_NBT_STREAM_BUSY ? MC_CLIENT_SAVE_BUSY : MC_CLIENT_SAVE_STATE_ERROR;
    }
    mc_nbt_document_free(f->state.pending_document);
    f->state.pending_document = NULL;
    mc_world_info_data_release(f->state.pending_data);
    f->state.pending_data = NULL;
    mc_gzip_error ge;
    mc_gzip_status gs = mc_gzip_input_release(&f->state.pending_gzip, &ge);
    if (gs != MC_GZIP_OK) {
        mc_client_save_status status =
            gs == MC_GZIP_BUSY ? MC_CLIENT_SAVE_BUSY : MC_CLIENT_SAVE_FILE_ERROR;
        save_result(e, status, "native GZIP retirement", NULL, NULL, -1);
        if (e != NULL)
            e->gzip = ge;
        return status;
    }
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_file_stream_release(&f->state.pending_stream, &fe);
    return fs == MC_JAVA_FILE_OK ? save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1)
                                 : save_file_error(e, fs, fe, NULL, NULL, -1);
}

mc_client_save_status save_clear_read(mc_client_save_format *f, mc_client_save_error *e) {
    mc_client_save_status s = save_clear_decode(f, e);
    if (s != MC_CLIENT_SAVE_OK)
        return s;
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_file_release(&f->state.pending_candidate, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    fs = mc_java_file_release(&f->state.pending_metadata, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    fs = mc_java_file_release(&f->state.pending_world, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

static mc_client_save_status gzip_failure(mc_client_save_error *e, mc_gzip_status gs,
                                          mc_gzip_error child, int call_pc, int as_pc,
                                          int print_pc) {
    mc_client_save_status status = gs == MC_GZIP_LIMIT        ? MC_CLIENT_SAVE_LIMIT
                                   : gs == MC_GZIP_ALLOCATION ? MC_CLIENT_SAVE_ALLOCATION_FAILED
                                   : gs == MC_GZIP_BUSY       ? MC_CLIENT_SAVE_BUSY
                                   : child.file.status == MC_JAVA_FILE_UNSUPPORTED_PROVIDER
                                       ? MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY
                                       : MC_CLIENT_SAVE_FILE_ERROR;
    if (child.source_exception) {
        save_result(e, MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY,
                    "java/lang/Exception.printStackTrace()V", "tq", "b(Ljava/lang/String;)Lei;",
                    print_pc);
        if (e != NULL) {
            e->caught_exception = true;
            e->gzip = child;
            e->cause_frames[0] = (mc_client_save_site){"tq", "b(Ljava/lang/String;)Lei;", call_pc};
            e->cause_frames[1] = (mc_client_save_site){"as", "a(Ljava/io/InputStream;)Lnu;", as_pc};
            e->cause_frame_count = 2;
        }
        return MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY;
    }
    save_result(e, status, child.operation, "as", "a(Ljava/io/InputStream;)Lnu;", as_pc);
    save_push(e, "tq", "b(Ljava/lang/String;)Lei;", call_pc);
    if (e != NULL)
        e->gzip = child;
    return status;
}

static mc_client_save_status stream_failure(mc_client_save_error *e, mc_nbt_status ns,
                                            mc_nbt_stream_error primary, mc_nbt_stream_error closed,
                                            int call_pc, int as_pc, int print_pc, bool source) {
    mc_client_save_status status = ns == MC_NBT_LIMIT ? MC_CLIENT_SAVE_LIMIT
                                   : ns == MC_NBT_ALLOCATION_FAILED
                                       ? MC_CLIENT_SAVE_ALLOCATION_FAILED
                                   : ns == MC_NBT_STREAM_BUSY ? MC_CLIENT_SAVE_BUSY
                                   : ns == MC_NBT_INPUT_ERROR ? MC_CLIENT_SAVE_FILE_ERROR
                                                              : MC_CLIENT_SAVE_INFO_ERROR;
    save_result(
        e, source ? MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY : status,
        source ? "java/lang/Exception.printStackTrace()V" : "actual NBT stream/input failure",
        source ? "tq" : "as", source ? "b(Ljava/lang/String;)Lei;" : "a(Ljava/io/InputStream;)Lnu;",
        source ? print_pc : as_pc);
    if (!source)
        save_push(e, "tq", "b(Ljava/lang/String;)Lei;", call_pc);
    if (e != NULL) {
        e->stream = primary;
        e->close = closed;
        e->caught_exception = source;
        if (source) {
            e->cause_frames[0] = (mc_client_save_site){"tq", "b(Ljava/lang/String;)Lei;", call_pc};
            e->cause_frames[1] = (mc_client_save_site){"as", "a(Ljava/io/InputStream;)Lnu;", as_pc};
            e->cause_frame_count = 2;
        }
    }
    return source ? MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY : status;
}

static mc_client_save_status decode_info(mc_client_save_format *f, bool old,
                                         mc_client_save_error *e) {
    int call_pc = old ? 105 : 48, print_pc = old ? 133 : 76, data_pc = old ? 114 : 57,
        allocate_pc = old ? 119 : 62, construct_pc = old ? 125 : 68;
    mc_nbt_stream_error se;
    mc_nbt_status ns =
        mc_nbt_stream_allocate(f->classes->options.max_owner_bytes, &f->state.pending_reader, &se);
    if (ns != MC_NBT_OK)
        return stream_failure(e, ns, se, (mc_nbt_stream_error){0}, call_pc, 0, print_pc, false);
    mc_gzip_options options = mc_gzip_default_options();
    options.maximum_owner_bytes = f->classes->options.max_owner_bytes;
    mc_gzip_error ge;
    mc_gzip_status gs = mc_gzip_input_allocate(&options, &f->state.pending_gzip, &ge);
    if (gs != MC_GZIP_OK)
        return gzip_failure(e, gs, ge, call_pc, 4, print_pc);
    gs = mc_gzip_input_construct(f->state.pending_gzip, f->state.pending_stream, &ge);
    if (gs != MC_GZIP_OK)
        return gzip_failure(e, gs, ge, call_pc, 9, print_pc);
    mc_nbt_byte_input port = mc_gzip_input_byte_port(f->state.pending_gzip);
    mc_nbt_limits limits = mc_nbt_default_limits();
    limits.max_allocation_bytes = f->classes->options.max_owner_bytes;
    ns = mc_nbt_stream_construct(f->state.pending_reader, &port, &limits, &se);
    if (ns != MC_NBT_OK)
        return stream_failure(e, ns, se, (mc_nbt_stream_error){0}, call_pc, 12, print_pc, false);
    ns = mc_nbt_stream_parse(f->state.pending_reader, true, &f->state.pending_document, &se);
    if (ns != MC_NBT_OK) {
        if (se.source_fault == MC_NBT_SOURCE_NONE) {
            mc_client_save_status status =
                stream_failure(e, ns, se, (mc_nbt_stream_error){0}, call_pc, 17, print_pc, false);
            if (e != NULL) {
                mc_gzip_input_last_error(f->state.pending_gzip, &e->gzip);
                if (ns == MC_NBT_INPUT_ERROR &&
                    e->gzip.file.status == MC_JAVA_FILE_UNSUPPORTED_PROVIDER) {
                    status = MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY;
                    e->status = status;
                }
            } else if (ns == MC_NBT_INPUT_ERROR) {
                mc_gzip_error cause;
                mc_gzip_input_last_error(f->state.pending_gzip, &cause);
                if (cause.file.status == MC_JAVA_FILE_UNSUPPORTED_PROVIDER)
                    status = MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY;
            }
            return status;
        }
        mc_nbt_stream_error close;
        mc_nbt_status closed = mc_nbt_stream_source_close(f->state.pending_reader, &close);
        if (closed != MC_NBT_OK) {
            mc_client_save_status status =
                stream_failure(e, closed, se, close, call_pc, 29, print_pc,
                               close.source_fault != MC_NBT_SOURCE_NONE);
            if (e != NULL) {
                mc_gzip_input_last_error(f->state.pending_gzip, &e->gzip);
                mc_gzip_input_last_close_error(f->state.pending_gzip, &e->gzip_close);
            }
            return status;
        }
        mc_client_save_status status =
            stream_failure(e, ns, se, (mc_nbt_stream_error){0}, call_pc, 17, print_pc, true);
        if (e != NULL) {
            mc_gzip_input_last_error(f->state.pending_gzip, &e->gzip);
        }
        return status;
    }
    ns = mc_nbt_stream_source_close(f->state.pending_reader, &se);
    if (ns != MC_NBT_OK) {
        mc_client_save_status status =
            stream_failure(e, ns, (mc_nbt_stream_error){0}, se, call_pc, 22, print_pc,
                           se.source_fault != MC_NBT_SOURCE_NONE);
        if (e != NULL)
            mc_gzip_input_last_close_error(f->state.pending_gzip, &e->gzip_close);
        return status;
    }
    mc_world_info_error ie;
    mc_world_info_status is = mc_world_info_data_adopt_document(&f->state.pending_document,
                                                                f->classes->options.max_owner_bytes,
                                                                &f->state.pending_data, &ie);
    if (is != MC_WORLD_INFO_OK) {
        if (is == MC_WORLD_INFO_INVALID_FIELD) {
            save_result(e, MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY,
                        "java/lang/Exception.printStackTrace()V", "tq", "b(Ljava/lang/String;)Lei;",
                        print_pc);
            if (e != NULL) {
                e->caught_exception = true;
                e->info = ie;
                e->cause_frames[0] =
                    (mc_client_save_site){"tq", "b(Ljava/lang/String;)Lei;", data_pc};
                e->cause_frame_count = 1;
            }
            return MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY;
        }
        mc_client_save_status status = is == MC_WORLD_INFO_LIMIT ? MC_CLIENT_SAVE_LIMIT
                                       : is == MC_WORLD_INFO_ALLOCATION_FAILED
                                           ? MC_CLIENT_SAVE_ALLOCATION_FAILED
                                           : MC_CLIENT_SAVE_INFO_ERROR;
        save_result(e, status, "native decoded Data adoption", "tq", "b(Ljava/lang/String;)Lei;",
                    data_pc);
        if (e != NULL)
            e->info = ie;
        return status;
    }
    is = mc_world_info_allocate(MC_WORLD_INFO_CLIENT, &f->state.pending_info);
    if (is != MC_WORLD_INFO_OK)
        return save_result(e,
                           is == MC_WORLD_INFO_ALLOCATION_FAILED ? MC_CLIENT_SAVE_ALLOCATION_FAILED
                                                                 : MC_CLIENT_SAVE_INFO_ERROR,
                           "actual ei allocation", "tq", "b(Ljava/lang/String;)Lei;", allocate_pc);
    is = mc_world_info_construct_nbt(f->state.pending_info, f->state.pending_data, &ie);
    if (is != MC_WORLD_INFO_OK) {
        if (is == MC_WORLD_INFO_INVALID_FIELD) {
            save_result(e, MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY,
                        "java/lang/Exception.printStackTrace()V", "tq", "b(Ljava/lang/String;)Lei;",
                        print_pc);
            if (e != NULL) {
                e->caught_exception = true;
                e->info = ie;
                e->cause_frames[0] =
                    (mc_client_save_site){"tq", "b(Ljava/lang/String;)Lei;", construct_pc};
                e->cause_frame_count = 1;
            }
            return MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY;
        }
        mc_client_save_status status = is == MC_WORLD_INFO_LIMIT ? MC_CLIENT_SAVE_LIMIT
                                       : is == MC_WORLD_INFO_ALLOCATION_FAILED
                                           ? MC_CLIENT_SAVE_ALLOCATION_FAILED
                                           : MC_CLIENT_SAVE_INFO_ERROR;
        save_result(e, status, "actual ei constructor", "tq", "b(Ljava/lang/String;)Lei;",
                    construct_pc);
        if (e != NULL)
            e->info = ie;
        return status;
    }
    return MC_CLIENT_SAVE_OK;
}

static mc_client_save_status candidate(mc_client_save_format *f, bool old,
                                       mc_client_save_error *e) {
    const int allocate_pc = old ? 79 : 22, construct_pc = old ? 86 : 29, exists_pc = old ? 91 : 34;
    const int stream_new_pc = old ? 97 : 40, stream_pc = old ? 102 : 45, print_pc = old ? 133 : 76;
    mc_java_file_error fe;
    mc_java_file_status fs =
        mc_java_file_allocate(f->classes->files, &f->state.pending_candidate, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, "tq", "b(Ljava/lang/String;)Lei;", allocate_pc);
    mc_world_info_name **slot = old ? &f->classes->old_name : &f->classes->current_name;
    mc_client_save_status literal_status =
        save_literal(f->classes, old ? "level.dat_old" : "level.dat", slot, e);
    if (literal_status != MC_CLIENT_SAVE_OK) {
        save_push(e, "tq", "b(Ljava/lang/String;)Lei;", old ? 84 : 27);
        return literal_status;
    }
    fs = mc_java_file_construct_child(f->state.pending_candidate, f->state.pending_world, *slot,
                                      &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, "tq", "b(Ljava/lang/String;)Lei;", construct_pc);
    /* Source astore3 only follows completed File construction. Keep the former
     * primary local alive through allocation/construction of its replacement. */
    fs = mc_java_file_release(&f->state.pending_metadata, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    f->state.pending_metadata = f->state.pending_candidate;
    f->state.pending_candidate = NULL;
    bool exists = false;
    fs = mc_java_file_exists(f->state.pending_metadata, &exists, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, "tq", "b(Ljava/lang/String;)Lei;", exists_pc);
    if (!exists)
        return MC_CLIENT_SAVE_OK;
    fs = mc_java_file_stream_allocate(f->classes->files, &f->state.pending_stream, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, "tq", "b(Ljava/lang/String;)Lei;", stream_new_pc);
    fs = mc_java_file_stream_construct(f->state.pending_stream, f->state.pending_metadata, &fe);
    if (fs != MC_JAVA_FILE_OK) {
        if (fs == MC_JAVA_FILE_IO_ERROR) {
            save_result(e, MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY,
                        "java/lang/Exception.printStackTrace()V", "tq", "b(Ljava/lang/String;)Lei;",
                        print_pc);
            if (e != NULL) {
                e->file = fe;
                e->caught_exception = true;
                e->cause_frames[0] =
                    (mc_client_save_site){"tq", "b(Ljava/lang/String;)Lei;", stream_pc};
                e->cause_frame_count = 1;
            }
            return MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY;
        }
        return save_file_error(e, fs, fe, "tq", "b(Ljava/lang/String;)Lei;", stream_pc);
    }
    return decode_info(f, old, e);
}

mc_client_save_status save_read_info(mc_client_save_format *f, mc_world_info_name *folder,
                                     mc_world_info **out, mc_client_save_error *e) {
    mc_java_file_error fe;
    mc_java_file_status fs =
        mc_java_file_allocate(f->classes->files, &f->state.pending_candidate, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, "tq", "b(Ljava/lang/String;)Lei;", 0);
    fs = mc_java_file_construct_child(f->state.pending_candidate, f->state.source_a, folder, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, "tq", "b(Ljava/lang/String;)Lei;", 9);
    f->state.pending_world = f->state.pending_candidate;
    f->state.pending_candidate = NULL;
    bool exists = false;
    fs = mc_java_file_exists(f->state.pending_world, &exists, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, "tq", "b(Ljava/lang/String;)Lei;", 14);
    if (exists) {
        mc_client_save_status s = candidate(f, false, e);
        if (s != MC_CLIENT_SAVE_OK)
            return s;
        if (f->state.pending_info == NULL) {
            s = candidate(f, true, e);
            if (s != MC_CLIENT_SAVE_OK)
                return s;
        }
    }
    mc_client_save_status s = save_clear_read(f, e);
    if (s != MC_CLIENT_SAVE_OK)
        return s;
    if (out != &f->state.pending_info) {
        *out = f->state.pending_info;
        f->state.pending_info = NULL;
    }
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_format_read_info(mc_client_save_format *f,
                                                      mc_world_info_name *folder,
                                                      mc_world_info **out,
                                                      mc_client_save_error *e) {
    if (out == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat Info output", NULL, NULL,
                           -1);
    mc_client_save_status s = save_ready(f, e);
    if (s != MC_CLIENT_SAVE_OK)
        return s;
    f->state.active = true;
    s = save_read_info(f, folder, out, e);
    f->state.active = false;
    return s;
}

mc_client_save_status mc_client_save_format_needs_conversion(mc_client_save_format *f,
                                                             mc_world_info_name *folder, bool *out,
                                                             mc_client_save_error *e) {
    if (out == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat conversion query output",
                           NULL, NULL, -1);
    mc_client_save_status s = save_ready(f, e);
    if (s != MC_CLIENT_SAVE_OK)
        return s;
    f->state.active = true;
    mc_world_info *info = NULL;
    s = save_read_info(f, folder, &info, e);
    if (s != MC_CLIENT_SAVE_OK) {
        save_push(e, "hi", "a(Ljava/lang/String;)Z", 2);
        f->state.active = false;
        return s;
    }
    bool result = false;
    if (info != NULL) {
        f->state.pending_info = info;
        int32_t version;
        mc_world_info_status ws = mc_world_info_version_get(info, &version);
        if (ws != MC_WORLD_INFO_OK) {
            f->state.active = false;
            return save_result(e, MC_CLIENT_SAVE_INFO_ERROR, "ei version getter", "hi",
                               "a(Ljava/lang/String;)Z", 11);
        }
        result = version == 0;
        mc_world_info_release(f->state.pending_info);
        f->state.pending_info = NULL;
    }
    f->state.active = false;
    *out = result;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_format_open(mc_client_save_format *f,
                                                 mc_world_info_name *folder, bool players,
                                                 mc_world_store **out, mc_client_save_error *e) {
    if (out == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat handler output", NULL,
                           NULL, -1);
    mc_client_save_status s = save_ready(f, e);
    if (s != MC_CLIENT_SAVE_OK)
        return s;
    if (f->classes->handlers == NULL) {
        (void)folder;
        (void)players;
        return save_result(e, MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY,
                           "actual mx/fm Source owner allocation/constructor inside WorldStore",
                           "hi", "a(Ljava/lang/String;Z)Lwt;", 0);
    }
    f->state.active = true;
    mc_world_source_status hs = mc_world_store_source_allocate(
        f->classes->handlers, &f->state.pending_handler, &f->handler_failure);
    if (hs != MC_WORLD_SOURCE_OK) {
        f->state.active = false;
        return save_handler_error(f, hs, &f->handler_failure, 0, e);
    }
    /* Actual hi.a File capture5 and nullable String argument8 follow NEW0. The
     * enclosing caller holds both through construct; no eager folder checks. */
    hs = mc_world_store_source_construct(f->state.pending_handler, f->state.source_a, folder,
                                         players, &f->handler_failure);
    if (hs != MC_WORLD_SOURCE_OK) {
        f->state.active = false;
        return save_handler_error(f, hs, &f->handler_failure, 10, e);
    }
    mc_world_store *completed = f->state.pending_handler;
    f->state.pending_handler = NULL;
    f->state.active = false;
    *out = completed; /* Actual returned identity at13, no copy/alternate Store. */
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_method_state mc_client_save_format_method_state(mc_client_save_method method) {
    static const mc_client_save_method_state methods[MC_CLIENT_SAVE_METHOD_COUNT] = {
        {"a()Ljava/lang/String;", "hi", true, NULL, -1},
        {"a(Ljava/lang/String;Z)Lwt;", "hi", false, "caught IOException.printStackTrace", 57},
        {"b()Ljava/util/List;", "hi", false, "tq caught diagnostic descendant", 59},
        {"c()V", "hi", false, "actual shared rj region namespace flush", 0},
        {"b(Ljava/lang/String;)Lei;", "tq", false,
         "caught Exception.printStackTrace primary76/old133", 76},
        {"c(Ljava/lang/String;)V", "tq", false, "recursive destructive route not implemented", 0},
        {"a(Ljava/lang/String;Ljava/lang/String;)V", "tq", false,
         "in-place rename/write route not implemented", 0},
        {"a(Ljava/lang/String;)Z", "hi", false,
         "tq caught diagnostic descendant; version predicate is ==0", 2},
        {"a(Ljava/lang/String;Lyb;)Z", "hi", false, "actual progress owner and conversion closure",
         2}};
    return (unsigned)method < MC_CLIENT_SAVE_METHOD_COUNT ? methods[method]
                                                          : (mc_client_save_method_state){0};
}
