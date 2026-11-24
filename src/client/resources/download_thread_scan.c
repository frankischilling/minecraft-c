#include "download_thread_internal.h"
#include <stdlib.h>

static mc_client_download_status scan_source(mc_client_download_error *e, int pc,
                                             const char *exception, const char *op) {
    mc_download_fail(e, MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION, "cz", "a",
                     "(Ljava/io/File;Ljava/lang/String;)V", pc, op);
    e->exception_class = exception;
    return e->status;
}

static mc_client_download_status scan_file_error(mc_client_download_error *e, mc_java_file_error f,
                                                 int pc, const char *method) {
    mc_client_download_status s = mc_download_file_error(e, f, pc, method);
    if (pc >= 0)
        e->site.descriptor = "(Ljava/io/File;Ljava/lang/String;)V";
    return s;
}

static mc_client_download_status pop(mc_client_download_thread *w, mc_client_download_error *e) {
    download_frame *f = &w->frames[w->depth - 1u];
    mc_java_file_error fe = {0};
    if (mc_java_file_array_release(&f->array, &fe) != MC_JAVA_FILE_OK)
        return scan_file_error(e, fe, -1, NULL);
    mc_world_info_name_release(f->prefix);
    mc_world_info_name_release(f->name);
    *f = (download_frame){0};
    --w->depth;
    return MC_CLIENT_DOWNLOAD_OK;
}

static mc_client_download_status failed_leaf(mc_client_download_thread *w, download_frame *f,
                                             mc_java_file *file, mc_client_download_error *e) {
    if (w->caught_count++ == 0)
        w->caught = *e;
    *e = (mc_client_download_error){0};
    /* Static System.out105 is independent of nullable source App b. */
    if (w->classes->system == NULL)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_UNIMPLEMENTED, "cz", "a",
                                "(Ljava/io/File;Ljava/lang/String;)V", 105,
                                "unbound actual System.out namespace");
    if (mc_java_system_output_get(w->classes->system, &w->call.print, &e->print) !=
        MC_JAVA_PRINT_OK)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, "cz", "a",
                                "(Ljava/io/File;Ljava/lang/String;)V", 105,
                                "actual System.out field read");
    mc_world_info_name *literal = NULL;
    if (mc_download_literal(w->classes, 3, &literal, e) != MC_CLIENT_DOWNLOAD_OK) {
        mc_download_caller(e, "cz", "a", "(Ljava/io/File;Ljava/lang/String;)V", 115);
        return e->status;
    }
    mc_world_info_name *head = NULL;
    mc_client_download_status status = mc_download_concat(
        mc_world_info_name_text(literal), mc_world_info_name_text(f->prefix), "", &head, e);
    mc_world_info_name_release(literal);
    if (status != MC_CLIENT_DOWNLOAD_OK)
        return status;
    /* Source handler performs a FRESH child.getName128, not an answer reused
     * from the throwing registration's earlier getName84. */
    mc_world_info_name *name = NULL;
    mc_java_file_error fe = {0};
    if (mc_java_file_get_name(file, &name, &fe) != MC_JAVA_FILE_OK) {
        mc_world_info_name_release(head);
        return scan_file_error(e, fe, 128, "a");
    }
    status = mc_download_concat(mc_world_info_name_text(head), mc_world_info_name_text(name), "",
                                &w->call.message, e);
    mc_world_info_name_release(head);
    mc_world_info_name_release(name);
    if (status != MC_CLIENT_DOWNLOAD_OK)
        return status;
    if (w->call.print == NULL)
        return scan_source(e, 137, "java/lang/NullPointerException",
                           "actual null println receiver in leaf handler");
    mc_nbt_string message = mc_world_info_name_text(w->call.message);
    mc_java_properties_string_view view = {message.units, message.length};
    if (mc_java_print_stream_println(w->call.print, &view, &e->print) != MC_JAVA_PRINT_OK)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_NATIVE, "cz", "a",
                                "(Ljava/io/File;Ljava/lang/String;)V", 137,
                                "actual leaf handler PrintStream.println");
    return MC_CLIENT_DOWNLOAD_OK;
}

static mc_client_download_status scan_body(mc_client_download_thread *w,
                                           mc_client_download_error *e) {
    if (w->frames == NULL) {
        w->frames = calloc(w->options.max_depth, sizeof(*w->frames));
        if (w->frames == NULL)
            return mc_download_fail(e, MC_CLIENT_DOWNLOAD_ALLOCATION, NULL, NULL, NULL, -1,
                                    "cz native traversal stack");
    }
    if (w->depth != 0)
        return mc_download_fail(e, MC_CLIENT_DOWNLOAD_STATE, NULL, NULL, NULL, -1,
                                "cz retained failed scan cannot replay");
    if (mc_download_call_retire(w, e) != MC_CLIENT_DOWNLOAD_OK)
        return e->status;
    w->frames[0] = (download_frame){.directory = w->a, .prefix = mc_world_info_name_empty()};
    w->depth = 1;
    while (w->depth != 0) {
        if (atomic_load_explicit(&w->stop, memory_order_acquire))
            return mc_download_fail(e, MC_CLIENT_DOWNLOAD_CANCELLED_NATIVE, NULL, NULL, NULL, -1,
                                    "cz native scan retirement stop");
        download_frame *f = &w->frames[w->depth - 1u];
        mc_java_file_error fe = {0};
        if (!f->initialized) {
            if (f->directory == NULL)
                return scan_source(e, 1, "java/lang/NullPointerException",
                                   "actual null listFiles receiver");
            if (mc_java_file_list(f->directory, &f->array, &fe) != MC_JAVA_FILE_OK)
                return scan_file_error(e, fe, 1, "a");
            f->initialized = true;
            if (f->array == NULL)
                return scan_source(e, 11, "java/lang/NullPointerException",
                                   "actual null listFiles arraylength");
            f->count = mc_java_file_array_length(f->array);
        }
        if (f->index == f->count) {
            if (pop(w, e) != MC_CLIENT_DOWNLOAD_OK)
                return e->status;
            continue;
        }
        if (w->local_visits == w->options.max_visits)
            return mc_download_fail(e, MC_CLIENT_DOWNLOAD_LIMIT, NULL, NULL, NULL, -1,
                                    "cz local visit bound");
        ++w->local_visits;
        if (mc_download_call_retire(w, e) != MC_CLIENT_DOWNLOAD_OK)
            return e->status;
        mc_java_file *file = mc_java_file_array_at(f->array, f->index);
        if (file == NULL)
            return scan_source(e, 19, "java/lang/NullPointerException",
                               "actual child isDirectory receiver");
        bool directory = false;
        if (mc_java_file_is_directory(file, &directory, &fe) != MC_JAVA_FILE_OK)
            return scan_file_error(e, fe, 19, "a");
        mc_world_info_name_release(f->name);
        f->name = NULL;
        if (directory) {
            if (mc_java_file_get_name(file, &f->name, &fe) != MC_JAVA_FILE_OK)
                return scan_file_error(e, fe, 45, "a");
            if (w->depth == w->options.max_depth)
                return mc_download_fail(e, MC_CLIENT_DOWNLOAD_LIMIT, NULL, NULL, NULL, -1,
                                        "cz recursive scan depth bound");
            mc_world_info_name *next = NULL;
            if (mc_download_concat(mc_world_info_name_text(f->prefix),
                                   mc_world_info_name_text(f->name), "/", &next,
                                   e) != MC_CLIENT_DOWNLOAD_OK)
                return e->status;
            ++f->index;
            w->frames[w->depth++] = (download_frame){.directory = file, .prefix = next};
            continue;
        }
        /* Capture the actual App before constructing the String argument.
         * Native borrow policy keeps that same App stable through the worker. */
        client_application *captured = w->b;
        if (mc_java_file_get_name(file, &f->name, &fe) != MC_JAVA_FILE_OK)
            return scan_file_error(e, fe, 84, "a");
        if (mc_download_concat(mc_world_info_name_text(f->prefix), mc_world_info_name_text(f->name),
                               "", &w->call.key, e) != MC_CLIENT_DOWNLOAD_OK)
            return e->status;
        mc_client_download_status status = mc_download_register(w, captured, w->call.key, file, e);
        if (status == MC_CLIENT_DOWNLOAD_SOURCE_EXCEPTION) {
            mc_download_caller(e, "cz", "a", "(Ljava/io/File;Ljava/lang/String;)V", 97);
            status = failed_leaf(w, f, file, e);
        }
        if (status != MC_CLIENT_DOWNLOAD_OK)
            return status;
        ++f->index;
    }
    return MC_CLIENT_DOWNLOAD_OK;
}

mc_client_download_status mc_download_scan(mc_client_download_thread *w,
                                           mc_client_download_error *e) {
    mc_client_download_status s = scan_body(w, e);
    if (s != MC_CLIENT_DOWNLOAD_OK)
        for (size_t i = 1; i < w->depth; ++i)
            mc_download_caller(e, "cz", "a", "(Ljava/io/File;Ljava/lang/String;)V", 59);
    return s;
}
