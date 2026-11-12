#include "internal.h"

#include <stdlib.h>
#include <string.h>

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

mc_client_save_status mc_client_save_list_sort(mc_client_save_list *list, mc_client_save_error *e) {
    if (list == NULL)
        return save_result(e, MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE, "Collections.sort null List",
                           "java/util/Collections", "sort(Ljava/util/List;)V", 0);
    if (list->active || list->retiring)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "save List sort native active/retiring owner",
                           NULL, NULL, -1);
    if (list->count > list->capacity || (list->count != 0 && list->items == NULL))
        return save_result(e, MC_CLIENT_SAVE_STATE_ERROR, "save List native storage invariant",
                           NULL, NULL, -1);
    if (list->count < 2)
        return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
    if (list->count > SIZE_MAX / (2u * sizeof(*list->items)) ||
        list->count * (2u * sizeof(*list->items)) > list->maximum_bytes)
        return save_result(e, MC_CLIENT_SAVE_LIMIT, "save List stable-sort native scratch bound",
                           NULL, NULL, -1);
    mc_client_save_selection **work = malloc(list->count * 2u * sizeof(*work));
    if (work == NULL)
        return save_result(e, MC_CLIENT_SAVE_ALLOCATION_FAILED,
                           "save List stable-sort native scratch", NULL, NULL, -1);
    list->active = true;
    mc_client_save_selection **from = work, **to = work + list->count;
    memcpy(from, list->items, list->count * sizeof(*from));
    mc_client_save_status status = MC_CLIENT_SAVE_OK;
    /* Native stable-sort kernel over canonical vb objects. Left-first equality
     * preserves ties; success commits into the SAME List storage without
     * replacing any List/row identity or changing native occurrence counts.
     * Scratch/failed-comparator cleanup preserves that storage. This kernel
     * does not claim the selected JDK's internal comparison/failure schedule. */
    for (size_t width = 1; width < list->count;) {
        for (size_t start = 0; start < list->count;) {
            const size_t middle = list->count - start < width ? list->count : start + width;
            const size_t end = list->count - middle < width ? list->count : middle + width;
            size_t left = start, right = middle, next = start;
            while (left < middle && right < end) {
                int32_t order = 0;
                if (from[left] == NULL) {
                    status = save_result(e, MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE,
                                         "save List sort null Comparable receiver",
                                         "java/util/Collections", "sort(Ljava/util/List;)V", 0);
                    goto complete;
                }
                status = mc_client_save_selection_compare(from[left], from[right], &order, e);
                if (status != MC_CLIENT_SAVE_OK)
                    goto complete;
                to[next++] = order <= 0 ? from[left++] : from[right++];
            }
            while (left < middle)
                to[next++] = from[left++];
            while (right < end)
                to[next++] = from[right++];
            start = end;
        }
        mc_client_save_selection **swap = from;
        from = to;
        to = swap;
        if (width > list->count / 2u)
            break;
        width *= 2u;
    }
    memcpy(list->items, from, list->count * sizeof(*from));
complete:
    list->active = false;
    free(work);
    if (status == MC_CLIENT_SAVE_OK)
        return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
    return status;
}
