#include "internal.h"

static mc_client_save_status info_failure(mc_client_save_error *e, mc_world_info_status status,
                                          int pc) {
    mc_client_save_status mapped = status == MC_WORLD_INFO_LIMIT ? MC_CLIENT_SAVE_LIMIT
                                   : status == MC_WORLD_INFO_ALLOCATION_FAILED
                                       ? MC_CLIENT_SAVE_ALLOCATION_FAILED
                                       : MC_CLIENT_SAVE_INFO_ERROR;
    save_result(e, mapped, "actual ei field getter", "hi", "b()Ljava/util/List;", pc);
    if (e != NULL)
        e->info.status = status;
    return mapped;
}

/* One canonical reached hi row body, used after a real nonnull Info return.
 * No callback/fixture/parallel codec supplies Info to the public list route. */
static mc_client_save_status append_info(mc_client_save_format *f, mc_client_save_error *e) {
    mc_world_info *info = f->state.pending_info;
    int32_t version;
    mc_world_info_status is = mc_world_info_version_get(info, &version);
    if (is != MC_WORLD_INFO_OK)
        return info_failure(e, is, 71);
    bool convert = version != 19132;
    mc_world_info_name *display = NULL;
    is = mc_world_info_name_get(info, &display);
    if (is != MC_WORLD_INFO_OK)
        return info_failure(e, is, 89);
    bool empty = display == NULL;
    if (display != NULL) {
        if (f->classes->math == NULL)
            return save_result(e, MC_CLIENT_SAVE_UNIMPLEMENTED_DEPENDENCY,
                               "actual shared MathHelper class owner", "hi", "b()Ljava/util/List;",
                               101);
        mc_math_class_error child;
        mc_math_class_status ms = mc_math_classes_ensure(f->classes->math, &child);
        if (ms != MC_MATH_CLASS_OK) {
            mc_client_save_status s = ms == MC_MATH_CLASS_LIMIT ? MC_CLIENT_SAVE_LIMIT
                                      : ms == MC_MATH_CLASS_ALLOCATION_FAILED
                                          ? MC_CLIENT_SAVE_ALLOCATION_FAILED
                                      : ms == MC_MATH_CLASS_BUSY ? MC_CLIENT_SAVE_BUSY
                                                                 : MC_CLIENT_SAVE_MATH_ERROR;
            save_result(e, s, child.operation, "in", "<clinit>()V", child.source_pc);
            if (e != NULL)
                e->math = child;
            save_push(e, "hi", "b()Ljava/util/List;", 101);
            return s;
        }
        /* in.a checks length ONLY AFTER its real class active-use completes. */
        empty = mc_world_info_name_text(display).length == 0;
    }
    if (empty)
        display = f->state.pending_name;
    mc_client_save_status s = mc_client_save_selection_allocate(f->classes->options.max_owner_bytes,
                                                                &f->state.pending_selection, e);
    if (s != MC_CLIENT_SAVE_OK) {
        save_push(e, "hi", "b()Ljava/util/List;", 112);
        return s;
    }
    int64_t last_played, size;
    is = mc_world_info_last_played_get(info, &last_played);
    if (is != MC_WORLD_INFO_OK)
        return info_failure(e, is, 122);
    is = mc_world_info_size_get(info, &size);
    if (is != MC_WORLD_INFO_OK)
        return info_failure(e, is, 127);
    s = mc_client_save_selection_construct(f->state.pending_selection, f->state.pending_name,
                                           display, last_played, size, convert, e);
    if (s != MC_CLIENT_SAVE_OK) {
        save_push(e, "hi", "b()Ljava/util/List;", 132);
        return s;
    }
    s = save_list_append(f->state.pending_list, f->state.pending_selection, e);
    if (s != MC_CLIENT_SAVE_OK) {
        save_push(e, "hi", "b()Ljava/util/List;", 135);
        return s;
    }
    return mc_client_save_selection_release(&f->state.pending_selection, e);
}

mc_client_save_status mc_client_save_format_list(mc_client_save_format *f,
                                                 mc_client_save_list **out,
                                                 mc_client_save_error *e) {
    if (out == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "SaveFormat list output", NULL, NULL,
                           -1);
    mc_client_save_status s = save_ready(f, e);
    if (s != MC_CLIENT_SAVE_OK)
        return s;
    f->state.active = true;
    s = save_list_allocate(f, e);
    if (s != MC_CLIENT_SAVE_OK)
        goto failed;
    mc_java_file_error child;
    mc_java_file_status fs = mc_java_file_list(f->state.source_a, &f->state.pending_files, &child);
    if (fs != MC_JAVA_FILE_OK) {
        s = save_file_error(e, fs, child, "hi", "b()Ljava/util/List;", 12);
        goto failed;
    }
    if (f->state.pending_files == NULL) {
        s = save_result(e, MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE,
                        "File.listFiles returned null array", "hi", "b()Ljava/util/List;", 19);
        goto failed;
    }
    size_t count = mc_java_file_array_length(f->state.pending_files);
    for (size_t i = 0; i < count; ++i) {
        mc_java_file *directory = mc_java_file_array_at(f->state.pending_files, i);
        if (directory == NULL) {
            s = save_result(e, MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE, "null File array element",
                            "hi", "b()Ljava/util/List;", 40);
            goto failed;
        }
        bool is_directory = false;
        fs = mc_java_file_is_directory(directory, &is_directory, &child);
        if (fs != MC_JAVA_FILE_OK) {
            s = save_file_error(e, fs, child, "hi", "b()Ljava/util/List;", 40);
            goto failed;
        }
        if (!is_directory)
            continue;
        fs = mc_java_file_get_name(directory, &f->state.pending_name, &child);
        if (fs != MC_JAVA_FILE_OK) {
            s = save_file_error(e, fs, child, "hi", "b()Ljava/util/List;", 51);
            goto failed;
        }
        s = save_read_info(f, f->state.pending_name, &f->state.pending_info, e);
        if (s != MC_CLIENT_SAVE_OK) {
            save_push(e, "hi", "b()Ljava/util/List;", 59);
            goto failed;
        }
        if (f->state.pending_info != NULL) {
            s = append_info(f, e);
            if (s != MC_CLIENT_SAVE_OK)
                goto failed;
        }
        mc_world_info_release(f->state.pending_info);
        f->state.pending_info = NULL;
        mc_world_info_name_release(f->state.pending_name);
        f->state.pending_name = NULL;
    }
    fs = mc_java_file_array_release(&f->state.pending_files, &child);
    if (fs != MC_JAVA_FILE_OK) {
        s = save_file_error(e, fs, child, NULL, NULL, -1);
        goto failed;
    }
    *out = f->state.pending_list;
    f->state.pending_list = NULL;
    f->state.active = false;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
failed:
    f->state.active = false;
    return s;
}
