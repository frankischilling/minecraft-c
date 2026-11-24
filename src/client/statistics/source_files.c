#include "internal.h"
#include <string.h>

mc_client_stats_status stats_source_file_error(mc_client_stats_error *error,
                                               mc_java_file_status status, mc_java_file_error child,
                                               const char *cl, const char *method,
                                               const char *descriptor, unsigned int pc) {
    stats_source_child(error, child);
    error->system_error = child.system_error;
    return stats_fail(error,
                      status == MC_JAVA_FILE_BUSY                ? MC_CLIENT_STATS_BUSY
                      : status == MC_JAVA_FILE_LIMIT             ? MC_CLIENT_STATS_LIMIT
                      : status == MC_JAVA_FILE_ALLOCATION_FAILED ? MC_CLIENT_STATS_ALLOCATION_FAILED
                      : status == MC_JAVA_FILE_UNSUPPORTED_PROVIDER
                          ? MC_CLIENT_STATS_UNSUPPORTED_PROFILE
                      : status == MC_JAVA_FILE_IO_ERROR ? MC_CLIENT_STATS_IO_FAILED
                                                        : MC_CLIENT_STATS_INVALID_PHASE,
                      child.operation, cl, method, descriptor, pc);
}

static mc_client_stats_status file_result(mc_client_stats_error *error, mc_java_file_status status,
                                          mc_java_file_error child, const char *cl,
                                          unsigned int pc) {
    return status == MC_JAVA_FILE_OK
               ? MC_CLIENT_STATS_OK
               : stats_source_file_error(error, status, child, cl, "<init>",
                                         strcmp(cl, "xi") == 0 ? "(Lgr;Ljava/io/File;)V"
                                                               : "(Lgr;Lxi;Ljava/io/File;)V",
                                         pc);
}

mc_client_stats_status stats_source_begin(mc_client_stats_writer *writer,
                                          mc_client_stats_classes *classes, mc_java_files *files,
                                          mc_java_file *parent, mc_client_stats_error *error) {
    if (classes == NULL || files == NULL || writer->memory->source != NULL)
        return stats_fail(error, MC_CLIENT_STATS_INVALID_ARGUMENT, "xi concrete source services",
                          NULL, NULL, NULL, 0);
    stats_source *source = stats_alloc(writer->memory, sizeof(*source), error);
    if (source == NULL)
        return error->status;
    *source = (stats_source){0};
    writer->memory->source = source; /* Native backing, never xi source fields. */
    if (mc_client_stats_classes_retain(classes, error) != MC_CLIENT_STATS_OK)
        return error->status;
    source->classes = classes;
    mc_java_file_error child = {0};
    mc_java_file_status status = mc_java_files_retain(files, &child);
    if (status != MC_JAVA_FILE_OK)
        return file_result(error, status, child, "xi", 0);
    source->files = files;
    if (parent != NULL) {
        mc_java_file_state state;
        status = mc_java_file_read_state(parent, &state, &child);
        if (status != MC_JAVA_FILE_OK)
            return file_result(error, status, child, "xi", 0);
        if (state.phase != MC_JAVA_FILE_READY || state.active ||
            mc_java_file_namespace(parent) != files)
            return stats_fail(error, MC_CLIENT_STATS_INVALID_ARGUMENT,
                              "xi actual File namespace/lifetime", NULL, NULL, NULL, 0);
        status = mc_java_file_retain(parent, &child);
        if (status != MC_JAVA_FILE_OK)
            return file_result(error, status, child, "xi", 0);
        source->parent = parent;
    }
    return MC_CLIENT_STATS_OK;
}

static mc_client_stats_status raw_file(mc_client_stats_writer *writer, const char *cl,
                                       unsigned int pc, mc_client_stats_error *error) {
    stats_source *source = writer->memory->source;
    if (source->pending_file != NULL)
        return stats_fail(error, MC_CLIENT_STATS_INVALID_PHASE, "xi pending File not consumed", cl,
                          "<init>", NULL, pc);
    mc_java_file_error child = {0};
    const mc_java_file_status status =
        mc_java_file_allocate(source->files, &source->pending_file, &child);
    return file_result(error, status, child, cl, pc);
}

static void clear_name(stats_source *source) {
    mc_world_info_name_release(source->pending_name);
    source->pending_name = NULL;
}

static void clear_builder(stats_source *source) {
    if (source->builder != NULL) {
        stats_builder_clear(source->builder);
        stats_free(source->builder);
        source->builder = NULL;
    }
}

static mc_client_stats_status builder_begin(mc_client_stats_writer *writer, const char *cl,
                                            unsigned int pc, mc_client_stats_error *error) {
    stats_source *source = writer->memory->source;
    if (source->builder != NULL)
        return stats_fail(error, MC_CLIENT_STATS_INVALID_PHASE, "xi pending StringBuilder", cl,
                          "<init>", NULL, pc);
    source->builder = stats_alloc(writer->memory, sizeof(*source->builder), error);
    if (source->builder == NULL) {
        error->class_name = cl;
        error->method = "<init>";
        error->descriptor =
            strcmp(cl, "xi") == 0 ? "(Lgr;Ljava/io/File;)V" : "(Lgr;Lxi;Ljava/io/File;)V";
        error->bytecode = pc;
        return error->status;
    }
    *source->builder =
        (stats_builder){.memory = writer->memory, .maximum = writer->options.limits.max_text_units};
    return MC_CLIENT_STATS_OK;
}

static mc_client_stats_status builder_name(mc_client_stats_writer *writer, const char *cl,
                                           unsigned int pc, mc_client_stats_error *error) {
    stats_source *source = writer->memory->source;
    const mc_world_info_status status =
        mc_world_info_name_copy((mc_nbt_string){source->builder->units, source->builder->length},
                                source->classes->max_string_bytes, &source->pending_name);
    return status == MC_WORLD_INFO_OK
               ? MC_CLIENT_STATS_OK
               : stats_fail(
                     error,
                     status == MC_WORLD_INFO_LIMIT               ? MC_CLIENT_STATS_LIMIT
                     : status == MC_WORLD_INFO_ALLOCATION_FAILED ? MC_CLIENT_STATS_ALLOCATION_FAILED
                                                                 : MC_CLIENT_STATS_INVALID_PHASE,
                     "Stats StringBuilder.toString owner", cl, "<init>",
                     strcmp(cl, "xi") == 0 ? "(Lgr;Ljava/io/File;)V" : "(Lgr;Lxi;Ljava/io/File;)V",
                     pc);
}

static bool prefix(mc_nbt_string name) {
    static const uint16_t units[] = {'s', 't', 'a', 't', 's', '_'};
    return name.length >= 6 && memcmp(name.units, units, sizeof(units)) == 0;
}

static bool suffix(mc_nbt_string name) {
    static const uint16_t units[] = {'.', 'd', 'a', 't'};
    return name.length >= 4 && memcmp(name.units + name.length - 4, units, sizeof(units)) == 0;
}

static mc_client_stats_status entry_name(mc_client_stats_writer *writer, mc_java_file *entry,
                                         unsigned int pc, mc_client_stats_error *error) {
    if (entry == NULL)
        return stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                               "xi null listed File receiver", "xi", "<init>",
                               "(Lgr;Ljava/io/File;)V", pc);
    mc_java_file_error child = {0};
    mc_java_file_status status =
        mc_java_file_get_name(entry, &writer->memory->source->pending_name, &child);
    return file_result(error, status, child, "xi", pc);
}

mc_client_stats_status stats_source_directory(mc_client_stats_writer *writer,
                                              mc_client_stats_report *report,
                                              mc_client_stats_error *error) {
    stats_source *source = writer->memory->source;
    if (raw_file(writer, "xi", 31, error) != MC_CLIENT_STATS_OK)
        return error->status;
    if (stats_classes_literal(source->classes, &source->pending_name, error) != MC_CLIENT_STATS_OK)
        return error->status; /* Actual ldc36 after raw File31. */
    mc_java_file_error child = {0};
    mc_java_file_status status = mc_java_file_construct_child(source->pending_file, source->parent,
                                                              source->pending_name, &child);
    if (file_result(error, status, child, "xi", 38) != MC_CLIENT_STATS_OK)
        return error->status;
    source->directory = source->pending_file;
    source->pending_file = NULL;
    clear_name(source);
    writer->pc = 38;
    bool value = false;
    status = mc_java_file_exists(source->directory, &value, &child);
    if (file_result(error, status, child, "xi", 43) != MC_CLIENT_STATS_OK)
        return error->status;
    writer->pc = 43;
    if (!value) {
        status = mc_java_file_mkdir(source->directory, &value, &child);
        if (file_result(error, status, child, "xi", 50) != MC_CLIENT_STATS_OK)
            return error->status;
        writer->pc = 50; /* Source ignores the boolean. */
    }
    if (source->parent == NULL)
        return stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                               "xi null parent listFiles receiver", "xi", "<init>",
                               "(Lgr;Ljava/io/File;)V", 55);
    status = mc_java_file_list(source->parent, &source->listing, &child);
    if (file_result(error, status, child, "xi", 55) != MC_CLIENT_STATS_OK)
        return error->status;
    if (source->listing == NULL)
        return stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE, "xi null listFiles result",
                               "xi", "<init>", "(Lgr;Ljava/io/File;)V", 62);
    writer->pc = 62;
    for (source->listing_index = 0;
         source->listing_index < mc_java_file_array_length(source->listing);
         ++source->listing_index) {
        mc_java_file *entry = mc_java_file_array_at(source->listing, source->listing_index);
        if (entry_name(writer, entry, 84, error) != MC_CLIENT_STATS_OK)
            return error->status;
        bool selected = prefix(mc_world_info_name_text(source->pending_name));
        clear_name(source);
        if (!selected)
            continue;
        if (entry_name(writer, entry, 97, error) != MC_CLIENT_STATS_OK)
            return error->status;
        selected = suffix(mc_world_info_name_text(source->pending_name));
        clear_name(source);
        if (!selected)
            continue;
        if (raw_file(writer, "xi", 108, error) != MC_CLIENT_STATS_OK)
            return error->status;
        if (entry_name(writer, entry, 115, error) != MC_CLIENT_STATS_OK)
            return error->status;
        status = mc_java_file_construct_child(source->pending_file, source->directory,
                                              source->pending_name, &child);
        if (file_result(error, status, child, "xi", 118) != MC_CLIENT_STATS_OK)
            return error->status;
        clear_name(source);
        status = mc_java_file_exists(source->pending_file, &value, &child);
        if (file_result(error, status, child, "xi", 125) != MC_CLIENT_STATS_OK)
            return error->status;
        if (!value) {
            if (builder_begin(writer, "xi", 134, error) != MC_CLIENT_STATS_OK)
                return error->status;
            if (stats_ascii(source->builder, "Relocating ", error) != MC_CLIENT_STATS_OK) {
                error->class_name = "xi";
                error->method = "<init>";
                error->descriptor = "(Lgr;Ljava/io/File;)V";
                error->bytecode = 143;
                return error->status;
            }
            if (entry_name(writer, entry, 148, error) != MC_CLIENT_STATS_OK)
                return error->status;
            const mc_nbt_string name = mc_world_info_name_text(source->pending_name);
            if (stats_append(source->builder, name.units, name.length, error) !=
                MC_CLIENT_STATS_OK) {
                error->class_name = "xi";
                error->method = "<init>";
                error->descriptor = "(Lgr;Ljava/io/File;)V";
                error->bytecode = 151;
                return error->status;
            }
            clear_name(source);
            if (builder_name(writer, "xi", 154, error) != MC_CLIENT_STATS_OK)
                return error->status;
            ++report->diagnostics;
            /* No source rename164 or finally is invented after this real
             * unfinished invocation. All actual File/String effects survive. */
            return stats_fail(error, MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY,
                              "actual PrintStream.println(String) relocation diagnostic", "xi",
                              "<init>", "(Lgr;Ljava/io/File;)V", 157);
        }
        status = mc_java_file_release(&source->pending_file, &child);
        if (file_result(error, status, child, "xi", 128) != MC_CLIENT_STATS_OK)
            return error->status;
    }
    writer->pc = 174;
    return MC_CLIENT_STATS_OK;
}

static mc_client_stats_status filename(mc_client_stats_writer *writer, mc_player_session *session,
                                       const char *tail, bool lower, unsigned int builder_pc,
                                       unsigned int prefix_pc, unsigned int session_pc,
                                       unsigned int lower_pc, unsigned int name_pc,
                                       unsigned int tail_pc, unsigned int result_pc,
                                       mc_client_stats_error *error) {
    stats_source *source = writer->memory->source;
    if (builder_begin(writer, "cj", builder_pc, error) != MC_CLIENT_STATS_OK)
        return error->status;
    if (stats_ascii(source->builder, "stats_", error) != MC_CLIENT_STATS_OK) {
        error->class_name = "cj";
        error->method = "<init>";
        error->descriptor = "(Lgr;Lxi;Ljava/io/File;)V";
        error->bytecode = prefix_pc;
        return error->status;
    }
    if (session == NULL)
        return stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                               "CJ null Session field receiver", "cj", "<init>",
                               "(Lgr;Lxi;Ljava/io/File;)V", session_pc);
    mc_player_string_view name;
    const bool present = mc_player_string_read(session->username, &name);
    if (!present && lower)
        return stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                               "CJ null username lowercase receiver", "cj", "<init>",
                               "(Lgr;Lxi;Ljava/io/File;)V", lower_pc);
    if (lower && writer->options.name_case != MC_CLIENT_STATS_NAME_ENGLISH_ASCII)
        return stats_fail(error, MC_CLIENT_STATS_UNSUPPORTED_PROFILE, "CJ default Locale lowercase",
                          "cj", "<init>", "(Lgr;Lxi;Ljava/io/File;)V", lower_pc);
    if (!present) {
        if (stats_ascii(source->builder, "null", error) != MC_CLIENT_STATS_OK) {
            error->class_name = "cj";
            error->method = "<init>";
            error->descriptor = "(Lgr;Lxi;Ljava/io/File;)V";
            error->bytecode = name_pc;
            return error->status;
        }
    } else
        for (size_t i = 0; i < name.length; ++i) {
            uint16_t unit = name.units[i];
            if (lower && unit > 127)
                return stats_fail(error, MC_CLIENT_STATS_UNSUPPORTED_PROFILE,
                                  "CJ nonASCII Locale lowercase", "cj", "<init>",
                                  "(Lgr;Lxi;Ljava/io/File;)V", lower_pc);
            if (lower && unit >= 'A' && unit <= 'Z')
                unit = (uint16_t)(unit + ('a' - 'A'));
            if (stats_append(source->builder, &unit, 1, error) != MC_CLIENT_STATS_OK) {
                error->class_name = "cj";
                error->method = "<init>";
                error->descriptor = "(Lgr;Lxi;Ljava/io/File;)V";
                error->bytecode = name_pc;
                return error->status;
            }
        }
    if (stats_ascii(source->builder, tail, error) != MC_CLIENT_STATS_OK) {
        error->class_name = "cj";
        error->method = "<init>";
        error->descriptor = "(Lgr;Lxi;Ljava/io/File;)V";
        error->bytecode = tail_pc;
        return error->status;
    }
    return builder_name(writer, "cj", result_pc, error);
}

mc_client_stats_status stats_source_make_files(mc_client_stats_writer *writer,
                                               mc_player_session *session,
                                               mc_client_stats_error *error) {
    stats_source *source = writer->memory->source;
    static const char *const tails[6] = {"_unsent.dat", ".dat",        "_unsent.old",
                                         ".old",        "_unsent.tmp", ".tmp"};
    static const unsigned int raw_pc[6] = {30, 72, 114, 156, 198, 240};
    static const unsigned int builder_pc[6] = {35, 77, 119, 161, 203, 245};
    static const unsigned int prefix_pc[6] = {44, 86, 128, 170, 212, 254};
    static const unsigned int name_pc[6] = {54, 96, 138, 180, 222, 264};
    static const unsigned int tail_pc[6] = {59, 101, 143, 185, 227, 269};
    static const unsigned int session_pc[6] = {48, 90, 132, 174, 216, 258};
    static const unsigned int lower_pc[6] = {51, 93, 135, 177, 219, 261};
    static const unsigned int result_pc[6] = {62, 104, 146, 188, 230, 272};
    static const unsigned int ctor_pc[6] = {65, 107, 149, 191, 233, 275};
    static const unsigned int stores[6] = {68, 110, 152, 194, 236, 278};
    for (size_t i = 0; i < 6; ++i) {
        if (raw_file(writer, "cj", raw_pc[i], error) != MC_CLIENT_STATS_OK)
            return error->status;
        if (filename(writer, session, tails[i], true, builder_pc[i], prefix_pc[i], session_pc[i],
                     lower_pc[i], name_pc[i], tail_pc[i], result_pc[i],
                     error) != MC_CLIENT_STATS_OK)
            return error->status;
        mc_java_file_error child = {0};
        const mc_java_file_status status = mc_java_file_construct_child(
            source->pending_file, source->directory, source->pending_name, &child);
        if (file_result(error, status, child, "cj", ctor_pc[i]) != MC_CLIENT_STATS_OK)
            return error->status;
        source->children[i] = source->pending_file;
        source->pending_file = NULL;
        clear_name(source);
        clear_builder(source);
        writer->retained_syncher->pc = stores[i];
    }
    mc_player_string_view name;
    (void)mc_player_string_read(session->username, &name);
    bool different = false;
    for (size_t i = 0; i < name.length; ++i)
        if (name.units[i] >= 'A' && name.units[i] <= 'Z')
            different = true;
    if (different)
        for (size_t i = 0; i < 6; ++i) {
            static const unsigned int builders[6] = {300, 336, 372, 408, 444, 480};
            static const unsigned int prefixes[6] = {309, 345, 381, 417, 453, 489};
            static const unsigned int names[6] = {316, 352, 388, 424, 460, 496};
            static const unsigned int tailsites[6] = {321, 357, 393, 429, 465, 501};
            static const unsigned int sessions[6] = {313, 349, 385, 421, 457, 493};
            static const unsigned int strings[6] = {324, 360, 396, 432, 468, 504};
            static const unsigned int calls[6] = {331, 367, 403, 439, 475, 511};
            if (filename(writer, session, tails[i], false, builders[i], prefixes[i], sessions[i], 0,
                         names[i], tailsites[i], strings[i], error) != MC_CLIENT_STATS_OK)
                return error->status;
            mc_java_file_error child = {0};
            mc_java_file_status status =
                mc_java_file_allocate(source->files, &source->pending_file, &child);
            if (status != MC_JAVA_FILE_OK) {
                stats_source_file_error(error, status, child, "cj", "a",
                                        "(Ljava/io/File;Ljava/lang/String;Ljava/io/File;)V", 0);
                stats_source_frame(error, "cj", "<init>", "(Lgr;Lxi;Ljava/io/File;)V", calls[i]);
                return error->status;
            }
            status = mc_java_file_construct_child(source->pending_file, source->directory,
                                                  source->pending_name, &child);
            if (status != MC_JAVA_FILE_OK) {
                stats_source_file_error(error, status, child, "cj", "a",
                                        "(Ljava/io/File;Ljava/lang/String;Ljava/io/File;)V", 6);
                stats_source_frame(error, "cj", "<init>", "(Lgr;Lxi;Ljava/io/File;)V", calls[i]);
                return error->status;
            }
            clear_name(source);
            clear_builder(source);
            bool exists = false, is_directory = false, target = false;
            unsigned int reached = 13;
            status = mc_java_file_exists(source->pending_file, &exists, &child);
            if (status == MC_JAVA_FILE_OK && exists) {
                reached = 21;
                status = mc_java_file_is_directory(source->pending_file, &is_directory, &child);
            }
            if (status == MC_JAVA_FILE_OK && exists && !is_directory) {
                reached = 28;
                status = mc_java_file_exists(source->children[i], &target, &child);
            }
            if (status == MC_JAVA_FILE_OK && exists && !is_directory && !target) {
                bool ignored = false;
                reached = 37;
                status = mc_java_file_rename_to(source->pending_file, source->children[i], &ignored,
                                                &child);
            }
            if (status != MC_JAVA_FILE_OK) {
                stats_source_file_error(error, status, child, "cj", "a",
                                        "(Ljava/io/File;Ljava/lang/String;Ljava/io/File;)V",
                                        reached);
                stats_source_frame(error, "cj", "<init>", "(Lgr;Lxi;Ljava/io/File;)V", calls[i]);
                return error->status;
            }
            status = mc_java_file_release(&source->pending_file, &child);
            if (file_result(error, status, child, "cj", calls[i]) != MC_CLIENT_STATS_OK)
                return error->status;
        }
    writer->retained_syncher->pc = 514;
    return MC_CLIENT_STATS_OK;
}

bool stats_owner_file_exists(mc_client_stats_writer *writer, size_t index,
                             mc_client_stats_error *error) {
    stats_source *source = writer->memory->source;
    if (source == NULL)
        return stats_file_exists(writer->retained_syncher->files[index], error);
    mc_java_file_error child = {0};
    bool exists = false;
    mc_java_file_status status = mc_java_file_exists(source->children[index], &exists, &child);
    if (status != MC_JAVA_FILE_OK)
        stats_source_file_error(error, status, child, NULL, NULL, NULL, 0);
    return status == MC_JAVA_FILE_OK && exists;
}

mc_client_stats_status stats_source_caught(mc_client_stats_writer *writer,
                                           mc_client_stats_report *report,
                                           mc_client_stats_error *error, unsigned int pc) {
    (void)writer;
    const mc_client_stats_error cause = *error;
    if (report != NULL) {
        ++report->diagnostics;
        report->file_exception_caught = true;
        report->caught = cause;
    }
    stats_source_cause_set(error, &cause);
    const mc_client_stats_status status =
        stats_fail(error, MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY,
                   "actual Throwable.printStackTrace() source dependency", "cj", "a",
                   "(Ljava/io/File;)Ljava/util/Map;", pc);
    return status;
}

mc_client_stats_status stats_source_diagnostic(mc_client_stats_writer *writer,
                                               const mc_client_stats_diagnostic *event,
                                               mc_client_stats_report *report,
                                               mc_client_stats_error *error) {
    (void)writer;
    unsigned int pc = 0;
    if (report != NULL) {
        ++report->diagnostics;
        if (event->kind == MC_CLIENT_STATS_UNKNOWN_STATISTIC)
            ++report->unknown_ids;
        if (event->kind == MC_CLIENT_STATS_CHECKSUM_MISMATCH)
            report->checksum_mismatch = true;
        if (event->kind == MC_CLIENT_STATS_CAUGHT_EXCEPTION)
            report->caught = event->cause;
    }
    switch (event->kind) {
        case MC_CLIENT_STATS_UNKNOWN_STATISTIC:
            pc = 179;
            break;
        case MC_CLIENT_STATS_CHECKSUM_MISMATCH:
            pc = 280;
            break;
        case MC_CLIENT_STATS_CAUGHT_EXCEPTION:
            stats_source_cause_set(error, &event->cause);
            pc = 290;
            break;
        default:
            return stats_fail(error, MC_CLIENT_STATS_INVALID_PHASE,
                              "typed Stats diagnostic call requires its concrete source prefix",
                              NULL, NULL, NULL, 0);
    }
    const mc_client_stats_status status =
        stats_fail(error, MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY,
                   event->kind == MC_CLIENT_STATS_CAUGHT_EXCEPTION
                       ? "actual xe.printStackTrace() source dependency"
                       : "actual PrintStream.println(String) source dependency",
                   "xi", "a", "(Ljava/lang/String;)Ljava/util/Map;", pc);
    return status;
}

static mc_client_stats_status cleanup_file(mc_java_file **file, mc_client_stats_error *error) {
    mc_java_file_error child = {0};
    const mc_java_file_status status = mc_java_file_release(file, &child);
    return status == MC_JAVA_FILE_OK
               ? MC_CLIENT_STATS_OK
               : stats_source_file_error(error, status, child, NULL, NULL, NULL, 0);
}

static const char write_descriptor[] =
    "(Ljava/util/Map;Ljava/io/File;Ljava/io/File;Ljava/io/File;)V";

static mc_client_stats_status write_result(stats_source *source, mc_java_writer_status status,
                                           unsigned int pc, mc_client_stats_error *error) {
    const mc_java_typed_file_writer_error *cause = &source->write_error;
    error->writer = status;
    error->system_error = cause->writer.system_error != 0 ? cause->writer.system_error
                                                          : cause->output.cause.system_error;
    if (cause->source_exception)
        return stats_exception(error,
                               cause->output.status == MC_JAVA_OUTPUT_SOURCE_NULL
                                   ? MC_CLIENT_STATS_NULL_REFERENCE
                                   : MC_CLIENT_STATS_SOURCE_IO,
                               cause->operation, "cj", "a", write_descriptor, pc);
    return stats_fail(
        error,
        status == MC_JAVA_WRITER_BUSY                  ? MC_CLIENT_STATS_BUSY
        : status == MC_JAVA_WRITER_LIMIT               ? MC_CLIENT_STATS_LIMIT
        : status == MC_JAVA_WRITER_ALLOCATION_FAILED   ? MC_CLIENT_STATS_ALLOCATION_FAILED
        : status == MC_JAVA_WRITER_UNSUPPORTED_CHARSET ? MC_CLIENT_STATS_UNSUPPORTED_PROFILE
                                                       : MC_CLIENT_STATS_IO_FAILED,
        cause->operation, "cj", "a", write_descriptor, pc);
}

static mc_client_stats_status write_dispose(stats_source *source, mc_client_stats_error *error) {
    const mc_java_writer_status status =
        mc_java_typed_file_writer_retire(&source->write_output, &source->write_retirement);
    if (status != MC_JAVA_WRITER_OK) {
        error->writer = status;
        error->system_error = source->write_retirement.writer.system_error;
        return stats_fail(
            error, status == MC_JAVA_WRITER_BUSY ? MC_CLIENT_STATS_BUSY : MC_CLIENT_STATS_IO_FAILED,
            "native Stats typed writer retirement", NULL, NULL, NULL, 0);
    }
    mc_client_stats_text_retire(&source->write_text);
    return MC_CLIENT_STATS_OK;
}

mc_client_stats_status stats_source_write_file_set(mc_client_stats_writer *writer,
                                                   const mc_client_stats_map *map, size_t dat_index,
                                                   mc_client_stats_report *report,
                                                   mc_client_stats_error *error) {
    stats_source *source = writer->memory->source;
    if (dat_index > 1u || source == NULL)
        return stats_fail(error, MC_CLIENT_STATS_INVALID_ARGUMENT, "Stats typed File set", NULL,
                          NULL, NULL, 0);
    if (mtx_lock(&writer->mutex) != thrd_success)
        return stats_fail(error, MC_CLIENT_STATS_THREAD_FAILED, "Stats typed save admission lock",
                          NULL, NULL, NULL, 0);
    if (source->write_active || (source->write_output != NULL && !source->write_complete)) {
        (void)mtx_unlock(&writer->mutex);
        return stats_fail(error, MC_CLIENT_STATS_BUSY, "Stats retained/active typed save prefix",
                          NULL, NULL, NULL, 0);
    }
    source->write_active = true;
    (void)mtx_unlock(&writer->mutex);
    mc_client_stats_status status = write_dispose(source, error);
    if (status != MC_CLIENT_STATS_OK)
        goto done;
    source->write_complete = false;
    source->write_error = (mc_java_typed_file_writer_error){0};
    source->write_retirement = (mc_java_typed_file_writer_error){0};
    source->write_pc = 0;
    mc_java_writer_status ws = mc_java_typed_file_writer_allocate(
        source->files, NULL, &source->write_output, &source->write_error);
    if (ws != MC_JAVA_WRITER_OK) {
        status = write_result(source, ws, 0, error);
        goto done;
    }
    const mc_java_writer_profile profile = {writer->options.charset, MC_JAVA_WRITER_PRINT,
#ifdef _WIN32
                                            MC_JAVA_WRITER_CRLF
#else
                                            MC_JAVA_WRITER_LF
#endif
    };
    source->write_pc = 10;
    ws = mc_java_typed_file_writer_construct(source->write_output, source->children[dat_index + 4u],
                                             &profile, &source->write_error);
    if (ws != MC_JAVA_WRITER_OK) {
        status = write_result(source, ws, 10, error);
        goto done;
    }
    source->write_pc = 16; /* Completed PW(FileWriter(File,false)) local. */
    stats_syncher *syncher = writer->retained_syncher;
    if (syncher->session == NULL) {
        status = stats_exception(error, MC_CLIENT_STATS_NULL_REFERENCE,
                                 "Stats source null Session username receiver", "cj", "a",
                                 write_descriptor, 24);
    } else {
        mc_player_string_view username;
        const bool present = mc_player_string_read(syncher->session->username, &username);
        static const uint16_t local[] = {'l', 'o', 'c', 'a', 'l'};
        source->write_pc = 30;
        status =
            map == NULL
                ? stats_exception(
                      error, MC_CLIENT_STATS_NULL_REFERENCE, "Stats serializer null Map", "xi", "a",
                      "(Ljava/lang/String;Ljava/lang/String;Ljava/util/Map;)Ljava/lang/String;", 89)
                : mc_client_stats_serialize(
                      map,
                      (mc_client_stats_string){present ? username.units : NULL,
                                               present ? username.length : 0, !present},
                      (mc_client_stats_string){local, 5, false}, &source->write_text, error);
    }
    if (status == MC_CLIENT_STATS_OK) {
        source->write_pc = 33;
        const mc_java_properties_string_view view = {source->write_text->units,
                                                     source->write_text->length};
        ws = mc_java_typed_file_writer_print(source->write_output, &view, &source->write_error);
        if (ws != MC_JAVA_WRITER_OK)
            status = write_result(source, ws, 33, error);
    }
    /* Only a completed source operation or actual source Throwable reaches
     * close38/finally48. Native policy stops preserve the unclosed prefix. */
    if (status == MC_CLIENT_STATS_OK || status == MC_CLIENT_STATS_SOURCE_EXCEPTION) {
        const mc_client_stats_error prior = *error;
        if (status == MC_CLIENT_STATS_SOURCE_EXCEPTION)
            stats_source_cause_set(error, &prior);
        source->write_pc = status == MC_CLIENT_STATS_OK ? 38u : 48u;
        ws = mc_java_typed_file_writer_source_close(source->write_output, &source->write_error);
        if (ws != MC_JAVA_WRITER_OK)
            status = write_result(source, ws, source->write_pc, error);
        else
            *error = prior;
    }
    mc_java_typed_file_writer_state output;
    if (mc_java_typed_file_writer_inspect(source->write_output, &output) &&
        output.writer_state_present && output.writer_state.trouble && report != NULL)
        report->print_trouble = true;
    if (status != MC_CLIENT_STATS_OK)
        goto done;
    mc_java_file *dat = source->children[dat_index], *old = source->children[dat_index + 2u],
                 *temporary = source->children[dat_index + 4u];
    mc_java_file_error child = {0};
    bool exists = false, ignored = false;
#define SOURCE_FILE_CALL(pc, call)                                                                 \
    do {                                                                                           \
        source->write_pc = (pc);                                                                   \
        const mc_java_file_status fs = (call);                                                     \
        if (fs != MC_JAVA_FILE_OK) {                                                               \
            status = stats_source_file_error(error, fs, child, "cj", "a", write_descriptor, (pc)); \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)
    SOURCE_FILE_CALL(56, mc_java_file_exists(old, &exists, &child));
    if (exists)
        SOURCE_FILE_CALL(64, mc_java_file_delete(old, &ignored, &child));
    SOURCE_FILE_CALL(69, mc_java_file_exists(dat, &exists, &child));
    if (exists)
        SOURCE_FILE_CALL(78, mc_java_file_rename_to(dat, old, &ignored, &child));
    SOURCE_FILE_CALL(84, mc_java_file_rename_to(temporary, dat, &ignored, &child));
#undef SOURCE_FILE_CALL
    source->write_pc = 88;
    source->write_complete = true;
done:
    if (mtx_lock(&writer->mutex) != thrd_success)
        return stats_fail(error, MC_CLIENT_STATS_THREAD_FAILED, "Stats typed save completion lock",
                          NULL, NULL, NULL, 0);
    source->write_active = false;
    (void)mtx_unlock(&writer->mutex);
    return status;
}

mc_client_stats_status stats_source_save_caught(mc_client_stats_report *report,
                                                mc_client_stats_error *error) {
    const mc_client_stats_error cause = *error;
    if (report != NULL) {
        ++report->diagnostics;
        report->file_exception_caught = true;
        report->caught = cause;
    }
    stats_source_cause_set(error, &cause);
    return stats_fail(error, MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY,
                      "actual synchronous save Exception.printStackTrace dependency", "cj", "b",
                      "(Ljava/util/Map;)V", 66);
}

mc_client_stats_status stats_source_retire(mc_client_stats_writer *writer,
                                           mc_client_stats_error *error) {
    stats_source *source = writer->memory->source;
    if (source == NULL)
        return MC_CLIENT_STATS_OK;
    source->retiring = true;
    if (write_dispose(source, error) != MC_CLIENT_STATS_OK) {
        source->cleanup_failure = stats_source_snapshot(error);
        return error->status;
    }
    /* Caller already joined every real worker. No foreground name occurrence
     * change or final File destruction races the handed-off worker. */
    while (source->readers != NULL) {
        stats_source_reader *reader = source->readers;
        /* CoreReader borrows this context/FIS; destroy its arena before the
         * final native stream occurrence. No source close is synthesized. */
        reader->reader = NULL;
        stats_arena_clear(&reader->arena);
        stats_builder_clear(&reader->text);
        mc_client_stats_map_retire(&reader->result);
        mc_java_file_error child = {0};
        const mc_java_file_status status = mc_java_file_stream_release(&reader->input.file, &child);
        if (status != MC_JAVA_FILE_OK) {
            stats_source_file_error(error, status, child, NULL, NULL, NULL, 0);
            source->cleanup_failure = stats_source_snapshot(error);
            return error->status;
        }
        source->readers = reader->next;
        if (source->current_reader == reader)
            source->current_reader = source->readers;
        stats_free(reader);
    }
    mc_client_stats_map_retire(&source->parse_candidate);
    if (cleanup_file(&source->pending_file, error) != MC_CLIENT_STATS_OK)
        goto failed;
    for (size_t i = 0; i < 6; ++i)
        if (cleanup_file(&source->children[i], error) != MC_CLIENT_STATS_OK)
            goto failed;
    {
        mc_java_file_error child = {0};
        const mc_java_file_status status = mc_java_file_array_release(&source->listing, &child);
        if (status != MC_JAVA_FILE_OK) {
            stats_source_file_error(error, status, child, NULL, NULL, NULL, 0);
            goto failed;
        }
    }
    if (cleanup_file(&source->directory, error) != MC_CLIENT_STATS_OK)
        goto failed;
    if (cleanup_file(&source->parent, error) != MC_CLIENT_STATS_OK)
        goto failed;
    clear_name(source);
    clear_builder(source);
    if (mc_client_stats_classes_release(&source->classes, error) != MC_CLIENT_STATS_OK)
        goto failed;
    {
        mc_java_file_error child = {0};
        const mc_java_file_status status = mc_java_files_release(&source->files, &child);
        if (status != MC_JAVA_FILE_OK) {
            stats_source_file_error(error, status, child, NULL, NULL, NULL, 0);
            goto failed;
        }
    }
    writer->memory->source = NULL;
    stats_free(source);
    return MC_CLIENT_STATS_OK;
failed:
    source->cleanup_failure = stats_source_snapshot(error);
    return error->status;
}

mc_client_stats_status stats_source_worker_caught(mc_client_stats_writer *writer,
                                                  mc_client_stats_report *report,
                                                  mc_client_stats_error *error) {
    (void)writer;
    const mc_client_stats_error cause = *error;
    if (report != NULL) {
        ++report->diagnostics;
        report->file_exception_caught = true;
        report->caught = cause;
    }
    stats_source_cause_set(error, &cause);
    const mc_client_stats_status status = stats_fail(
        error, MC_CLIENT_STATS_UNIMPLEMENTED_DEPENDENCY,
        "actual worker Exception.printStackTrace() source dependency", "vk", "run", "()V", 111);
    return status;
}
