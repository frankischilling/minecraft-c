#include "internal.h"
#include <stdlib.h>
#include <string.h>

mc_client_save_status save_result(mc_client_save_error *e, mc_client_save_status s, const char *op,
                                  const char *owner, const char *desc, int pc) {
    if (e != NULL) {
        *e = (mc_client_save_error){.status = s, .operation = op};
        if (owner != NULL) {
            e->frames[0] = (mc_client_save_site){owner, desc, pc};
            e->frame_count = 1;
        }
    }
    return s;
}

void save_push(mc_client_save_error *e, const char *owner, const char *desc, int pc) {
    if (e == NULL)
        return;
    size_t n = e->frame_count < 4u ? e->frame_count : 3u;
    if (n != 0)
        memmove(e->frames + 1, e->frames, n * sizeof(*e->frames));
    e->frames[0] = (mc_client_save_site){owner, desc, pc};
    e->frame_count = n + 1u;
}

mc_client_save_status save_file_error(mc_client_save_error *e, mc_java_file_status s,
                                      mc_java_file_error child, const char *owner, const char *desc,
                                      int pc) {
    mc_client_save_status status =
        s == MC_JAVA_FILE_LIMIT               ? MC_CLIENT_SAVE_LIMIT
        : s == MC_JAVA_FILE_ALLOCATION_FAILED ? MC_CLIENT_SAVE_ALLOCATION_FAILED
        : s == MC_JAVA_FILE_BUSY              ? MC_CLIENT_SAVE_BUSY
        : s == MC_JAVA_FILE_SOURCE_NULL_STRING || s == MC_JAVA_FILE_SOURCE_NULL_FILE
            ? MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE
        : s == MC_JAVA_FILE_UNSUPPORTED_PROVIDER ? MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY
                                                 : MC_CLIENT_SAVE_FILE_ERROR;
    save_result(e, status, child.operation, owner, desc, pc);
    if (e != NULL)
        e->file = child;
    return status;
}

mc_client_save_options mc_client_save_default_options(void) {
    return (mc_client_save_options){16777216u, 1048576u};
}

mc_client_save_status mc_client_save_formats_create(mc_java_files *files, mc_math_classes *math,
                                                    const mc_client_save_options *options,
                                                    mc_client_save_formats **out,
                                                    mc_client_save_error *e) {
    if (files == NULL || out == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat namespace arguments",
                           NULL, NULL, -1);
    mc_client_save_options o = options == NULL ? mc_client_save_default_options() : *options;
    if (o.max_owner_bytes < sizeof(mc_client_save_formats))
        return save_result(e, MC_CLIENT_SAVE_LIMIT, "SaveFormat namespace allocation bound", NULL,
                           NULL, -1);
    mc_client_save_formats *c = calloc(1, sizeof(*c));
    if (c == NULL)
        return save_result(e, MC_CLIENT_SAVE_ALLOCATION_FAILED, "SaveFormat namespace allocation",
                           NULL, NULL, -1);
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_files_retain(files, &fe);
    if (fs != MC_JAVA_FILE_OK) {
        free(c);
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    }
    c->files = files;
    c->math = math;
    c->holds = 1;
    c->options = o;
    if (math != NULL) {
        mc_math_class_error me;
        mc_math_class_status ms = mc_math_classes_retain(math, &me);
        if (ms != MC_MATH_CLASS_OK) {
            (void)mc_java_files_release(&c->files, NULL);
            free(c);
            save_result(e,
                        ms == MC_MATH_CLASS_BUSY ? MC_CLIENT_SAVE_BUSY : MC_CLIENT_SAVE_MATH_ERROR,
                        "SaveFormat MathHelper hold", NULL, NULL, -1);
            if (e != NULL)
                e->math = me;
            return ms == MC_MATH_CLASS_BUSY ? MC_CLIENT_SAVE_BUSY : MC_CLIENT_SAVE_MATH_ERROR;
        }
    }
    *out = c;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_formats_retain(mc_client_save_formats *c,
                                                    mc_client_save_error *e) {
    if (c == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat namespace retain", NULL,
                           NULL, -1);
    if (c->active || c->retiring)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "SaveFormat namespace terminal/active hold",
                           NULL, NULL, -1);
    if (c->holds == SIZE_MAX)
        return save_result(e, MC_CLIENT_SAVE_LIMIT, "SaveFormat namespace holds", NULL, NULL, -1);
    ++c->holds;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_formats_release(mc_client_save_formats **slot,
                                                     mc_client_save_error *e) {
    if (slot == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat namespace slot", NULL,
                           NULL, -1);
    mc_client_save_formats *c = *slot;
    if (c == NULL)
        return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
    if (c->active)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "SaveFormat namespace active retirement", NULL,
                           NULL, -1);
    if (c->holds > 1) {
        --c->holds;
        *slot = NULL;
        return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
    }
    c->retiring = true;
    c->active = true;
    mc_math_class_error me;
    mc_math_class_status ms = mc_math_classes_release(&c->math, &me);
    if (ms != MC_MATH_CLASS_OK) {
        c->active = false;
        save_result(e, MC_CLIENT_SAVE_BUSY, "SaveFormat MathHelper retirement", NULL, NULL, -1);
        if (e != NULL)
            e->math = me;
        return MC_CLIENT_SAVE_BUSY;
    }
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_files_release(&c->files, &fe);
    if (fs != MC_JAVA_FILE_OK) {
        c->active = false;
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    }
    mc_world_info_name_release(c->label);
    mc_world_info_name_release(c->current_name);
    mc_world_info_name_release(c->old_name);
    *slot = NULL;
    free(c);
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_format_allocate(mc_client_save_formats *c,
                                                     mc_client_save_format **out,
                                                     mc_client_save_error *e) {
    if (c == NULL || out == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT,
                           "SaveFormat native allocation arguments", NULL, NULL, -1);
    if (c->options.max_owner_bytes < sizeof(mc_client_save_format))
        return save_result(e, MC_CLIENT_SAVE_LIMIT, "SaveFormat native allocation bound", NULL,
                           NULL, -1);
    mc_client_save_format *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return save_result(e, MC_CLIENT_SAVE_ALLOCATION_FAILED, "SaveFormat native allocation",
                           NULL, NULL, -1);
    mc_client_save_status s = mc_client_save_formats_retain(c, e);
    if (s != MC_CLIENT_SAVE_OK) {
        free(f);
        return s;
    }
    f->classes = c;
    f->state.holds = 1;
    f->state.phase = MC_CLIENT_SAVE_RAW;
    *out = f;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_format_construct(mc_client_save_format *f, mc_java_file *input,
                                                      mc_client_save_error *e) {
    if (f == NULL || f->state.phase != MC_CLIENT_SAVE_RAW || f->state.active)
        return save_result(e, MC_CLIENT_SAVE_STATE_ERROR, "hi constructor receiver", NULL, NULL,
                           -1);
    f->state.active = true;
    f->state.phase = MC_CLIENT_SAVE_CONSTRUCTING;
    mc_client_save_status status;
    mc_java_file_error fe;
    mc_java_file_status fs;
    if (input == NULL) {
        status = save_result(e, MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE, "tq null File exists", "tq",
                             "<init>(Ljava/io/File;)V", 5);
        goto failed;
    }
    if (mc_java_file_namespace(input) != f->classes->files) {
        status = save_result(e, MC_CLIENT_SAVE_STATE_ERROR, "tq File namespace", "tq",
                             "<init>(Ljava/io/File;)V", 5);
        goto failed;
    }
    fs = mc_java_file_retain(input, &fe);
    if (fs != MC_JAVA_FILE_OK) {
        status = save_file_error(e, fs, fe, "tq", "<init>(Ljava/io/File;)V", 5);
        goto failed;
    }
    f->state.pending_argument = input;
    bool exists = false;
    fs = mc_java_file_exists(input, &exists, &fe);
    if (fs != MC_JAVA_FILE_OK) {
        status = save_file_error(e, fs, fe, "tq", "<init>(Ljava/io/File;)V", 5);
        goto failed;
    }
    if (!exists) {
        bool ignored = false;
        fs = mc_java_file_mkdirs(input, &ignored, &fe);
        if (fs != MC_JAVA_FILE_OK) {
            status = save_file_error(e, fs, fe, "tq", "<init>(Ljava/io/File;)V", 12);
            goto failed;
        }
    }
    f->state.source_a = f->state.pending_argument;
    f->state.pending_argument = NULL;
    f->state.last_store_pc = 18;
    f->state.phase = MC_CLIENT_SAVE_READY;
    f->state.active = false;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
failed:
    f->state.phase = MC_CLIENT_SAVE_FAILED;
    f->state.active = false;
    save_push(e, "hi", "<init>(Ljava/io/File;)V", 2);
    return status;
}

mc_client_save_status mc_client_save_format_retain(mc_client_save_format *f,
                                                   mc_client_save_error *e) {
    if (f == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat retain", NULL, NULL, -1);
    if (f->state.active || f->state.retiring)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "SaveFormat active/terminal hold", NULL, NULL,
                           -1);
    if (f->state.holds == SIZE_MAX)
        return save_result(e, MC_CLIENT_SAVE_LIMIT, "SaveFormat holds", NULL, NULL, -1);
    ++f->state.holds;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

bool save_pending(const mc_client_save_format *f) {
    return f->state.pending_reader != NULL || f->state.pending_document != NULL ||
           f->state.pending_data != NULL || f->state.pending_gzip != NULL ||
           f->state.pending_argument != NULL || f->state.pending_world != NULL ||
           f->state.pending_metadata != NULL || f->state.pending_candidate != NULL ||
           f->state.pending_stream != NULL || f->state.pending_files != NULL ||
           f->state.pending_list != NULL || f->state.pending_selection != NULL ||
           f->state.pending_info != NULL || f->state.pending_name != NULL;
}

mc_client_save_status save_ready(mc_client_save_format *f, mc_client_save_error *e) {
    if (f == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat receiver", NULL, NULL,
                           -1);
    if (f->state.active)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "SaveFormat active operation", NULL, NULL, -1);
    if (f->state.phase != MC_CLIENT_SAVE_READY || f->state.retiring)
        return save_result(e, MC_CLIENT_SAVE_STATE_ERROR, "SaveFormat nonready operation", NULL,
                           NULL, -1);
    if (save_pending(f))
        return save_result(e, MC_CLIENT_SAVE_BUSY, "SaveFormat retained failed operation", NULL,
                           NULL, -1);
    return MC_CLIENT_SAVE_OK;
}

mc_client_save_status save_clear(mc_client_save_format *f, mc_client_save_error *e) {
    mc_client_save_status decoded = save_clear_decode(f, e);
    if (decoded != MC_CLIENT_SAVE_OK)
        return decoded;
    mc_java_file_error fe;
    mc_java_file_status fs;
    mc_client_save_status s = mc_client_save_selection_release(&f->state.pending_selection, e);
    if (s != MC_CLIENT_SAVE_OK)
        return s;
    s = mc_client_save_list_release(&f->state.pending_list, e);
    if (s != MC_CLIENT_SAVE_OK)
        return s;
    mc_world_info_release(f->state.pending_info);
    f->state.pending_info = NULL;
    fs = mc_java_file_release(&f->state.pending_candidate, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    fs = mc_java_file_release(&f->state.pending_metadata, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    fs = mc_java_file_release(&f->state.pending_world, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    fs = mc_java_file_array_release(&f->state.pending_files, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    fs = mc_java_file_release(&f->state.pending_argument, &fe);
    if (fs != MC_JAVA_FILE_OK)
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    mc_world_info_name_release(f->state.pending_name);
    f->state.pending_name = NULL;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_format_clear_failure(mc_client_save_format *f,
                                                          mc_client_save_error *e) {
    if (f == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat failure cleanup", NULL,
                           NULL, -1);
    if (f->state.active)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "SaveFormat active failure cleanup", NULL, NULL,
                           -1);
    f->state.active = true;
    mc_client_save_status s = save_clear(f, e);
    f->state.active = false;
    return s;
}

mc_client_save_status mc_client_save_format_release(mc_client_save_format **slot,
                                                    mc_client_save_error *e) {
    if (slot == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat retirement slot", NULL,
                           NULL, -1);
    mc_client_save_format *f = *slot;
    if (f == NULL)
        return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
    if (f->state.active)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "SaveFormat active retirement", NULL, NULL, -1);
    if (f->state.holds > 1) {
        --f->state.holds;
        *slot = NULL;
        return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
    }
    f->state.retiring = true;
    f->state.phase = MC_CLIENT_SAVE_RETIRING;
    f->state.active = true;
    mc_client_save_status s = save_clear(f, e);
    if (s != MC_CLIENT_SAVE_OK) {
        f->state.active = false;
        return s;
    }
    mc_java_file_error fe;
    mc_java_file_status fs = mc_java_file_release(&f->state.source_a, &fe);
    if (fs != MC_JAVA_FILE_OK) {
        f->state.active = false;
        return save_file_error(e, fs, fe, NULL, NULL, -1);
    }
    s = mc_client_save_formats_release(&f->classes, e);
    if (s != MC_CLIENT_SAVE_OK) {
        f->state.active = false;
        return s;
    }
    *slot = NULL;
    free(f);
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_format_read_state(const mc_client_save_format *f,
                                                       mc_client_save_format_state *out,
                                                       mc_client_save_error *e) {
    if (f == NULL || out == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat state", NULL, NULL, -1);
    *out = f->state;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status save_literal(mc_client_save_formats *c, const char *text,
                                   mc_world_info_name **slot, mc_client_save_error *e) {
    if (*slot != NULL)
        return MC_CLIENT_SAVE_OK;
    size_t n = strlen(text);
    if (n > 32u)
        return save_result(e, MC_CLIENT_SAVE_LIMIT, "SaveFormat literal size", NULL, NULL, -1);
    uint16_t u[32];
    for (size_t i = 0; i < n; ++i)
        u[i] = (uint16_t)(unsigned char)text[i];
    c->active = true;
    mc_world_info_status s =
        mc_world_info_name_copy((mc_nbt_string){u, n}, c->options.max_owner_bytes, slot);
    c->active = false;
    if (s != MC_WORLD_INFO_OK)
        return save_result(
            e, s == MC_WORLD_INFO_LIMIT ? MC_CLIENT_SAVE_LIMIT : MC_CLIENT_SAVE_ALLOCATION_FAILED,
            "SaveFormat literal ownership", NULL, NULL, -1);
    return MC_CLIENT_SAVE_OK;
}

mc_client_save_status mc_client_save_format_name(mc_client_save_format *f, mc_world_info_name **out,
                                                 mc_client_save_error *e) {
    if (f == NULL || out == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat name arguments", NULL,
                           NULL, -1);
    if (f->state.active || f->state.retiring)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "SaveFormat active name", NULL, NULL, -1);
    f->state.active = true;
    mc_client_save_status s =
        save_literal(f->classes, "Scaevolus' McRegion", &f->classes->label, e);
    if (s == MC_CLIENT_SAVE_OK) {
        mc_world_info_status held = mc_world_info_name_retain(f->classes->label);
        if (held != MC_WORLD_INFO_OK)
            s = save_result(e, MC_CLIENT_SAVE_LIMIT, "SaveFormat label hold", "hi",
                            "a()Ljava/lang/String;", 0);
        else
            *out = f->classes->label;
    }
    if (s != MC_CLIENT_SAVE_OK && e != NULL && e->frame_count == 0)
        save_push(e, "hi", "a()Ljava/lang/String;", 0);
    f->state.active = false;
    return s == MC_CLIENT_SAVE_OK ? save_result(e, s, NULL, NULL, NULL, -1) : s;
}
