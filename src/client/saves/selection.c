#include "internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

mc_client_save_status mc_client_save_selection_allocate(size_t maximum,
                                                        mc_client_save_selection **out,
                                                        mc_client_save_error *e) {
    if (out == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "vb native output", NULL, NULL, -1);
    if (maximum < sizeof(mc_client_save_selection))
        return save_result(e, MC_CLIENT_SAVE_LIMIT, "vb native allocation bound", NULL, NULL, -1);
    mc_client_save_selection *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return save_result(e, MC_CLIENT_SAVE_ALLOCATION_FAILED, "vb native allocation", NULL, NULL,
                           -1);
    s->holds = 1;
    s->fields.phase = MC_CLIENT_SAVE_RAW;
    *out = s;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

static mc_client_save_status hold(mc_world_info_name *name, mc_client_save_error *e, int pc) {
    if (name == NULL)
        return MC_CLIENT_SAVE_OK;
    mc_world_info_status s = mc_world_info_name_retain(name);
    return s == MC_WORLD_INFO_OK
               ? MC_CLIENT_SAVE_OK
               : save_result(e,
                             s == MC_WORLD_INFO_LIMIT ? MC_CLIENT_SAVE_LIMIT
                                                      : MC_CLIENT_SAVE_STATE_ERROR,
                             "vb String field hold", "vb",
                             "<init>(Ljava/lang/String;Ljava/lang/String;JJZ)V", pc);
}

mc_client_save_status mc_client_save_selection_construct(mc_client_save_selection *s,
                                                         mc_world_info_name *filename,
                                                         mc_world_info_name *display,
                                                         int64_t played, int64_t size, bool convert,
                                                         mc_client_save_error *e) {
    if (s == NULL || s->fields.phase != MC_CLIENT_SAVE_RAW || s->active)
        return save_result(e, MC_CLIENT_SAVE_STATE_ERROR, "vb constructor receiver", NULL, NULL,
                           -1);
    s->active = true;
    s->fields.phase = MC_CLIENT_SAVE_CONSTRUCTING;
    mc_client_save_status status = hold(filename, e, 6);
    if (status != MC_CLIENT_SAVE_OK)
        goto failed;
    s->fields.filename = filename;
    s->fields.last_store_pc = 6;
    status = hold(display, e, 11);
    if (status != MC_CLIENT_SAVE_OK)
        goto failed;
    s->fields.display_name = display;
    s->fields.last_store_pc = 11;
    s->fields.last_played = played;
    s->fields.last_store_pc = 16;
    s->fields.size_on_disk = size;
    s->fields.last_store_pc = 22;
    s->fields.requires_conversion = convert;
    s->fields.last_store_pc = 28;
    s->fields.phase = MC_CLIENT_SAVE_READY;
    s->active = false;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
failed:
    s->fields.phase = MC_CLIENT_SAVE_FAILED;
    s->active = false;
    return status;
}

mc_client_save_status mc_client_save_selection_retain(mc_client_save_selection *s,
                                                      mc_client_save_error *e) {
    if (s == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "vb retain", NULL, NULL, -1);
    if (s->active || s->retiring)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "vb active/terminal retain", NULL, NULL, -1);
    if (s->holds == SIZE_MAX)
        return save_result(e, MC_CLIENT_SAVE_LIMIT, "vb holds", NULL, NULL, -1);
    ++s->holds;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_selection_release(mc_client_save_selection **slot,
                                                       mc_client_save_error *e) {
    if (slot == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "vb retirement slot", NULL, NULL,
                           -1);
    mc_client_save_selection *s = *slot;
    if (s == NULL)
        return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
    if (s->active)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "vb active retirement", NULL, NULL, -1);
    if (s->holds > 1) {
        --s->holds;
        *slot = NULL;
        return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
    }
    s->active = true;
    s->retiring = true;
    s->fields.phase = MC_CLIENT_SAVE_RETIRING;
    mc_world_info_name_release(s->fields.filename);
    mc_world_info_name_release(s->fields.display_name);
    *slot = NULL;
    free(s);
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_selection_read(const mc_client_save_selection *s,
                                                    mc_client_save_selection_fields *out,
                                                    mc_client_save_error *e) {
    if (s == NULL || out == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "vb fields", NULL, NULL, -1);
    *out = s->fields;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_selection_compare(const mc_client_save_selection *a,
                                                       const mc_client_save_selection *b,
                                                       int32_t *out, mc_client_save_error *e) {
    if (a == NULL || out == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "vb compare receiver/output", NULL,
                           NULL, -1);
    if (b == NULL)
        return save_result(e, MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE, "vb compare null other", "vb",
                           "a(Lvb;)I", 5);
    if (a->active || b->active || a->retiring || b->retiring)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "vb compare native admission", NULL, NULL, -1);
    int32_t result;
    if (a->fields.last_played < b->fields.last_played)
        result = 1;
    else if (a->fields.last_played > b->fields.last_played)
        result = -1;
    else {
        if (a->fields.filename == NULL || b->fields.filename == NULL)
            return save_result(e, MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE,
                               "vb filename compare null String", "vb", "a(Lvb;)I", 36);
        mc_nbt_string x = mc_world_info_name_text(a->fields.filename),
                      y = mc_world_info_name_text(b->fields.filename);
        if (x.length > INT32_MAX || y.length > INT32_MAX)
            return save_result(e, MC_CLIENT_SAVE_LIMIT, "vb native String length", "vb", "a(Lvb;)I",
                               36);
        size_t n = x.length < y.length ? x.length : y.length;
        result = 0;
        for (size_t i = 0; i < n; ++i)
            if (x.units[i] != y.units[i]) {
                result = (int32_t)x.units[i] - (int32_t)y.units[i];
                break;
            }
        if (result == 0)
            result = (int32_t)x.length - (int32_t)y.length;
    }
    *out = result;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status save_list_allocate(mc_client_save_format *f, mc_client_save_error *e) {
    if (f->classes->options.max_owner_bytes < sizeof(mc_client_save_list))
        return save_result(e, MC_CLIENT_SAVE_LIMIT, "ArrayList native owner bound", "hi",
                           "b()Ljava/util/List;", 0);
    mc_client_save_list *list = calloc(1, sizeof(*list));
    if (list == NULL)
        return save_result(e, MC_CLIENT_SAVE_ALLOCATION_FAILED, "ArrayList native owner", "hi",
                           "b()Ljava/util/List;", 0);
    list->holds = 1;
    list->maximum = f->classes->options.max_entries;
    list->maximum_bytes = f->classes->options.max_owner_bytes;
    f->state.pending_list = list;
    return MC_CLIENT_SAVE_OK;
}

mc_client_save_status save_list_append(mc_client_save_list *list, mc_client_save_selection *s,
                                       mc_client_save_error *e) {
    if (list == NULL || s == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "ArrayList add arguments", NULL,
                           NULL, -1);
    if (list->active || list->retiring)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "ArrayList add native admission", NULL, NULL,
                           -1);
    if (list->count >= list->maximum || list->count >= INT32_MAX)
        return save_result(e, MC_CLIENT_SAVE_LIMIT, "ArrayList retained count", "hi",
                           "b()Ljava/util/List;", 135);
    list->active = true;
    if (list->count == list->capacity) {
        size_t next = list->capacity == 0 ? 10u : list->capacity + list->capacity / 2u;
        if (next <= list->capacity || next > SIZE_MAX / sizeof(*list->items) ||
            next * sizeof(*list->items) > list->maximum_bytes) {
            list->active = false;
            return save_result(e, MC_CLIENT_SAVE_LIMIT, "ArrayList growth bound", "hi",
                               "b()Ljava/util/List;", 135);
        }
        mc_client_save_selection **items = malloc(next * sizeof(*items));
        if (items == NULL) {
            list->active = false;
            return save_result(e, MC_CLIENT_SAVE_ALLOCATION_FAILED, "ArrayList growth", "hi",
                               "b()Ljava/util/List;", 135);
        }
        if (list->count != 0)
            memcpy(items, list->items, list->count * sizeof(*items));
        mc_client_save_selection **old = list->items;
        list->items = items;
        list->capacity = next;
        free(old);
    }
    mc_client_save_status status = mc_client_save_selection_retain(s, e);
    if (status == MC_CLIENT_SAVE_OK)
        list->items[list->count++] = s;
    list->active = false;
    return status;
}

size_t mc_client_save_list_size(const mc_client_save_list *list) {
    return list == NULL ? 0 : list->count;
}

mc_client_save_selection *mc_client_save_list_at(const mc_client_save_list *list, size_t i) {
    return list != NULL && i < list->count ? list->items[i] : NULL;
}

mc_client_save_status mc_client_save_list_retain(mc_client_save_list *list,
                                                 mc_client_save_error *e) {
    if (list == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "ArrayList retain", NULL, NULL, -1);
    if (list->active || list->retiring)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "ArrayList active/terminal retain", NULL, NULL,
                           -1);
    if (list->holds == SIZE_MAX)
        return save_result(e, MC_CLIENT_SAVE_LIMIT, "ArrayList holds", NULL, NULL, -1);
    ++list->holds;
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}

mc_client_save_status mc_client_save_list_release(mc_client_save_list **slot,
                                                  mc_client_save_error *e) {
    if (slot == NULL)
        return save_result(e, MC_CLIENT_SAVE_INVALID_ARGUMENT, "ArrayList retirement slot", NULL,
                           NULL, -1);
    mc_client_save_list *list = *slot;
    if (list == NULL)
        return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
    if (list->active)
        return save_result(e, MC_CLIENT_SAVE_BUSY, "ArrayList active retirement", NULL, NULL, -1);
    if (list->holds > 1) {
        --list->holds;
        *slot = NULL;
        return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
    }
    list->retiring = true;
    list->active = true;
    for (size_t i = 0; i < list->count; ++i) {
        mc_client_save_status s = mc_client_save_selection_release(&list->items[i], e);
        if (s != MC_CLIENT_SAVE_OK) {
            list->active = false;
            return s;
        }
    }
    free(list->items);
    *slot = NULL;
    free(list);
    return save_result(e, MC_CLIENT_SAVE_OK, NULL, NULL, NULL, -1);
}
