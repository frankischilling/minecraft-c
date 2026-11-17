#include "../application_state.h"
#include "display_screen.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/construction.h"
#include "source_sign_screen_internal.h"

#include <limits.h>
#include <string.h>

static void reached(mc_client_source_yc *o, const char *method, unsigned int pc) {
    o->method = method;
    o->last_pc = pc;
}

static bool returned(mc_client_source_yc *o, mc_client_screen_error *e, const char *method,
                     const char *descriptor, unsigned int pc) {
    reached(o, method, pc);
    e->site = (mc_client_screen_site){"yc", method, descriptor, pc};
    e->source_returned = true;
    return true;
}

static bool repeat(mc_client_source_yc *o, bool enabled, mc_client_screen_error *e) {
    const char *method = enabled ? "b" : "h";
    const unsigned int pc = enabled ? 10u : 1u;
    reached(o, method, pc);
    if (mc_client_sdk_keyboard_enable_repeat(o->input, enabled, &o->detail.input) !=
        MC_CLIENT_SDK_INPUT_OK) {
        o->detail.child = (mc_client_screen_site){
            o->detail.input.site.class_name, o->detail.input.site.method,
            o->detail.input.site.descriptor,
            o->detail.input.site.pc < 0 ? 0u : (unsigned int)o->detail.input.site.pc};
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_INPUT_FAILED, o->detail.input.operation,
                                   method, "()V", pc, o->detail.input.native_policy);
    }
    return true;
}

bool mc_screen_sign_closed_locked(mc_client_source_yc *o, mc_client_screen_error *e) {
    ++o->source_calls;
    if (!repeat(o, false, e))
        return false;
    client_application *app = o->base.fields.b; /* Actual fresh getfield b5. */
    if (app == NULL)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                   "YC null App World field receiver", "h", "()V", 8, false);
    mc_world_runtime *world = app->active_world;
    if (world == NULL)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                   "YC null World remote field receiver", "h", "()V", 11, false);
    mc_world_runtime_fields fields;
    if (mc_world_runtime_fields_get(world, &fields) != MC_WORLD_STORAGE_OK)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_APP_FAILED,
                                   "YC actual World native field profile", "h", "()V", 11, true);
    if (fields.B)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                                   "YC actual remote Minecraft.s network child", "h", "()V", 21,
                                   true);
    return returned(o, e, "h", "()V", 62);
}

static bool clear(mc_client_source_yc *o, mc_client_screen_error *e) {
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    if (list == NULL)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                   "YC actual List.clear receiver", "b", "()V", 4, false);
    for (size_t i = 0; i < list->size; ++i) {
        mc_client_source_ke *button = list->entries[i];
        if (button != NULL && (button->classes != o->base.classes || button->holds < 2))
            return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_INVALID_PHASE,
                                       "YC native List Button occurrence", "b", "()V", 4, true);
    }
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    const size_t count = list->size;
    list->size = 0;
    for (size_t i = 0; i < count; ++i) {
        mc_client_source_ke *button = list->entries[i];
        list->entries[i] = NULL;
        if (!mc_screen_button_release_locked(&button, e))
            return false;
    }
    return true;
}

bool mc_screen_sign_initialize_locked(mc_client_source_yc *o, mc_client_screen_error *e) {
    ++o->source_calls;
    reached(o, "b", 4);
    if (!clear(o, e) || !repeat(o, true, e))
        return false;
    /* Native retention record is allocated only after the reached repeat store.
     * It owns the distinct NEW KE even if constructor/add later fails. */
    mc_sign_initialization *r = mc_screen_allocate(&o->base.memory, sizeof(*r));
    if (r == NULL)
        return mc_screen_sign_fail(o, e, o->base.memory.allocation_status,
                                   "YC retained Button construction record", "b", "()V", 17, true);
    r->next = o->initializations;
    o->initializations = r;
    ++o->initialization_count;
    r->button = mc_screen_button_allocate_locked(o->base.classes, e);
    if (r->button == NULL) {
        e->parent_site = (mc_client_screen_site){"yc", "b", "()V", 17};
        return false;
    }
    const int32_t x = mc_java_i32_sub(o->base.fields.c / 2, 100);
    const int32_t y = mc_java_i32_add(o->base.fields.d / 4, 120);
    mc_player_string *label = NULL;
    if (!mc_screen_literal_locked(o->base.classes, "Done", &label, e)) {
        e->parent_site = (mc_client_screen_site){"yc", "b", "()V", 40};
        return false;
    }
    if (!mc_screen_button_construct_locked(
            r->button, 0, x, y, 200, 20,
            (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, label, NULL}, e)) {
        e->parent_site = (mc_client_screen_site){"yc", "b", "()V", 42};
        return false;
    }
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    if (list == NULL)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                   "YC actual List.add receiver", "b", "()V", 45, false);
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    if (list->size >= (size_t)INT32_MAX)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_LIMIT, "YC List size after modCount", "b",
                                   "()V", 45, true);
    if (list->size == list->capacity) {
        size_t next = list->capacity == 0 ? 10u : list->capacity + list->capacity / 2u;
        if (next <= list->size || next > (size_t)INT32_MAX ||
            next > SIZE_MAX / sizeof(*list->entries))
            return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_LIMIT, "YC List growth bound", "b",
                                       "()V", 45, true);
        void **entries = mc_screen_allocate(&o->base.memory, next * sizeof(*entries));
        if (entries == NULL)
            return mc_screen_sign_fail(o, e, o->base.memory.allocation_status,
                                       "YC actual List native growth", "b", "()V", 45, true);
        if (list->size != 0)
            memcpy(entries, list->entries, list->size * sizeof(*entries));
        list->entries = entries;
        list->capacity = next;
    }
    if (!mc_screen_button_hold_locked(r->button, e))
        return false;
    list->entries[list->size++] = r->button;
    return returned(o, e, "b", "()V", 51);
}

static bool row(mc_client_source_yc *o, unsigned int field_pc, unsigned int array_pc,
                unsigned int length_pc, mc_tile_entity_name **out, mc_tile_entity_name_view *view,
                mc_client_screen_error *e) {
    if (o->tile == NULL)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                   "YC null Sign line-array field receiver", "a", "(CI)V", field_pc,
                                   false);
    o->detail.tile = mc_tile_entity_sign_get_line(o->tile, o->row, out);
    if (o->detail.tile != MC_TILE_ENTITY_OK)
        return mc_screen_sign_fail(o, e,
                                   o->detail.tile == MC_TILE_ENTITY_INVALID_SLOT
                                       ? MC_CLIENT_SCREEN_SOURCE_INDEX
                                       : MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                                   "YC actual constructor-owned String array profile", "a", "(CI)V",
                                   array_pc, o->detail.tile != MC_TILE_ENTITY_INVALID_SLOT);
    (void)mc_tile_entity_name_get_view(*out, view);
    if (view->is_null) {
        mc_tile_entity_name_release(*out);
        *out = NULL;
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                   "YC null row String.length receiver", "a", "(CI)V", length_pc,
                                   false);
    }
    return true;
}

static bool set_row(mc_client_source_yc *o, mc_tile_entity_name *name, unsigned int pc,
                    mc_client_screen_error *e) {
    o->detail.tile = mc_tile_entity_sign_set_line(o->tile, o->row, name);
    if (o->detail.tile != MC_TILE_ENTITY_OK)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_APP_FAILED,
                                   "YC reached Sign array native replacement", "a", "(CI)V", pc,
                                   true);
    ++o->line_stores;
    reached(o, "a", pc);
    return true;
}

bool mc_screen_sign_key_locked(mc_client_source_yc *o, uint16_t character, int32_t key,
                               mc_client_screen_error *e) {
    ++o->source_calls;
    reached(o, "a", 0);
    if (key == 200) {
        o->row = mc_java_i32_sub(o->row, 1) & 3;
        ++o->row_stores;
        reached(o, "a", 16);
    }
    if (key == 208 || key == 28) {
        o->row = mc_java_i32_add(o->row, 1) & 3;
        ++o->row_stores;
        reached(o, "a", 41);
    }
    if (key == 14) {
        mc_tile_entity_name *old = NULL;
        mc_tile_entity_name_view view;
        if (!row(o, 54, 61, 62, &old, &view, e))
            return false;
        const bool nonempty = view.text.length != 0;
        mc_tile_entity_name_release(old);
        if (nonempty) {
            /* Each actual array read is fresh even though this finite body has
             * no callback between String operations. Keep prefix and length
             * references independently held through the replacement. */
            mc_tile_entity_name *prefix = NULL, *length = NULL, *replacement = NULL;
            mc_tile_entity_name_view prefix_view, length_view;
            if (!row(o, 83, 90, 109, &prefix, &prefix_view, e))
                return false;
            if (!row(o, 96, 103, 104, &length, &length_view, e)) {
                mc_tile_entity_name_release(prefix);
                return false;
            }
            const size_t result_length = length_view.text.length - 1u;
            if (length_view.text.length == 0 || result_length > prefix_view.text.length) {
                mc_tile_entity_name_release(length);
                mc_tile_entity_name_release(prefix);
                mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_SOURCE_INDEX,
                                    "YC actual String.substring bounds", "a", "(CI)V", 109, false);
                e->exception_class = "java/lang/StringIndexOutOfBoundsException";
                o->base.failure = *e;
                return false;
            }
            if (result_length == 0)
                o->detail.tile = mc_tile_entity_classes_literal(o->literal_classes, "",
                                                                &replacement, &o->detail.literal);
            else
                o->detail.tile =
                    mc_tile_entity_name_create(prefix_view.text.units, result_length, &replacement);
            mc_tile_entity_name_release(length);
            mc_tile_entity_name_release(prefix);
            if (o->detail.tile != MC_TILE_ENTITY_OK)
                return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_APP_FAILED,
                                           "YC actual substring native String/literal owner", "a",
                                           "(CI)V", 109, true);
            const bool stored = set_row(o, replacement, 112, e);
            mc_tile_entity_name_release(replacement);
            if (!stored)
                return false;
        }
    }
    const mc_player_string *accepted = o->base.classes->sign_accepted;
    if (accepted == NULL)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                   "YC null m String.indexOf receiver", "a", "(CI)V", 117, false);
    mc_player_string_view allowed;
    if (!mc_player_string_read(accepted, &allowed))
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED,
                                   "YC captured accepted String native view", "a", "(CI)V", 117,
                                   true);
    bool found = false;
    for (size_t i = 0; i < allowed.length; ++i)
        if (allowed.units[i] == character) {
            found = true;
            break;
        }
    if (found) {
        mc_tile_entity_name *old = NULL;
        mc_tile_entity_name_view view;
        if (!row(o, 127, 134, 135, &old, &view, e))
            return false;
        if (view.text.length < 15) {
            uint16_t units[15];
            if (view.text.length != 0)
                memcpy(units, view.text.units, view.text.length * sizeof(*units));
            units[view.text.length] = character;
            mc_tile_entity_name *replacement = NULL;
            o->detail.tile = mc_tile_entity_name_create(units, view.text.length + 1u, &replacement);
            mc_tile_entity_name_release(old);
            if (o->detail.tile != MC_TILE_ENTITY_OK)
                return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_APP_FAILED,
                                           "YC actual concat fresh String owner", "a", "(CI)V", 170,
                                           true);
            const bool stored = set_row(o, replacement, 173, e);
            mc_tile_entity_name_release(replacement);
            if (!stored)
                return false;
        } else
            mc_tile_entity_name_release(old);
    }
    return returned(o, e, "a", "(CI)V", 174);
}

bool mc_screen_sign_tick_locked(mc_client_source_yc *o, mc_client_screen_error *e) {
    ++o->source_calls;
    o->tick = mc_java_i32_add(o->tick, 1);
    ++o->tick_stores;
    return returned(o, e, "a", "()V", 10);
}

static bool admit(mc_client_source_yc *o, mc_client_screen_error *e) {
    if (o == NULL) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "YC native component receiver", NULL,
                       NULL, NULL, 0);
        return false;
    }
    if (!mc_screen_lock(o->base.classes, e))
        return false;
    if (o->base.phase != MC_CLIENT_SCREEN_READY || o->base.classes->retiring ||
        o->base.active_calls != 0) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "YC native component admission", NULL, NULL, NULL,
                       0);
        mtx_unlock(&o->base.classes->mutex);
        return false;
    }
    return true;
}

mc_client_screen_status mc_client_screen_sign_initialize(mc_client_source_yc *o,
                                                         mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (admit(o, &e)) {
        (void)mc_screen_sign_initialize_locked(o, &e);
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_sign_key(mc_client_source_yc *o, uint16_t character,
                                                  int32_t key, mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (admit(o, &e)) {
        (void)mc_screen_sign_key_locked(o, character, key, &e);
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_sign_tick(mc_client_source_yc *o,
                                                   mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (admit(o, &e)) {
        (void)mc_screen_sign_tick_locked(o, &e);
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static void dirty_capture_failure(mc_client_source_yc *o, mc_world_storage_status status) {
    if (o->cleanup_failure.status != MC_CLIENT_SCREEN_OK)
        return;
    mc_screen_fail(&o->cleanup_failure,
                   status == MC_WORLD_STORAGE_LIMIT  ? MC_CLIENT_SCREEN_LIMIT
                   : status == MC_WORLD_STORAGE_BUSY ? MC_CLIENT_SCREEN_BUSY
                                                     : MC_CLIENT_SCREEN_APP_FAILED,
                   "YC dirty invocation native receipt capture", NULL, NULL, NULL, 0);
}

static void dirty_capture(mc_client_source_yc *o, mc_sign_dirty_call *call,
                          mc_world_access_failure *borrowed, bool *source_exception,
                          const char **exception_class) {
    const char *actual_class = NULL;
    if (mc_world_access_failure_source_exception(borrowed, &actual_class)) {
        *source_exception = true;
        *exception_class = actual_class;
    }
    if (borrowed->receipt == NULL) {
        if (borrowed->receiver != NULL || borrowed->render_global != NULL) {
            dirty_capture_failure(o, MC_WORLD_STORAGE_UNSUPPORTED_STATE);
            borrowed->receiver = NULL;
            borrowed->render_global = NULL;
        }
        return;
    }
    for (size_t i = 0; i < call->count; ++i)
        if (call->captures[i].receipt == borrowed->receipt)
            return;
    const mc_world_storage_status status =
        mc_world_access_failure_retain(borrowed, &call->captures[call->count]);
    if (status == MC_WORLD_STORAGE_OK)
        ++call->count;
    else {
        dirty_capture_failure(o, status);
        /* Preserve scalar/source-null evidence, but never publish a producer
         * borrow for which native custody was refused. The actual Source
         * envelope and any already captured prefix remain unchanged. */
        borrowed->receiver = NULL;
        borrowed->render_global = NULL;
        borrowed->receipt = NULL;
    }
}

bool mc_screen_sign_button_prefix_locked(mc_client_source_yc *o, mc_client_source_ke *button,
                                         bool *display, client_application **app,
                                         mc_client_screen_error *e) {
    ++o->source_calls;
    *display = false;
    if (button == NULL)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                   "YC null Button enabled receiver", "a", "(Lke;)V", 1, false);
    if (button->classes != o->base.classes || button->phase != MC_CLIENT_SCREEN_READY)
        return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_INVALID_ARGUMENT,
                                   "YC native actual Button operand", "a", "(Lke;)V", 1, true);
    if (button->fields.g && button->fields.f == 0) {
        if (o->tile == NULL)
            return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                       "YC null Sign dirty receiver", "a", "(Lke;)V", 19, false);
        mc_sign_dirty_call *call = mc_screen_allocate(&o->base.memory, sizeof(*call));
        if (call == NULL)
            return mc_screen_sign_fail(o, e, o->base.memory.allocation_status,
                                       "YC retained dirty invocation", "a", "(Lke;)V", 19, true);
        call->next = o->dirty_calls;
        o->dirty_calls = call;
        o->detail.dirty_returned = false;
        o->detail.tile = mc_tile_entity_dirty_detailed(o->tile, &o->detail.dirty);
        bool source_exception = false;
        const char *exception_class = NULL;
        dirty_capture(o, call, &o->detail.dirty.world_access_failure, &source_exception,
                      &exception_class);
        dirty_capture(o, call, &o->detail.dirty.publication.world_access_failure, &source_exception,
                      &exception_class);
        dirty_capture(o, call, &o->detail.dirty.batch.publication.world_access_failure,
                      &source_exception, &exception_class);
        if (o->detail.tile != MC_TILE_ENTITY_OK) {
            (void)mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_APP_FAILED,
                                      "YC actual Sign dirty child", "a", "(Lke;)V", 19, true);
            if (source_exception) {
                e->native_policy = false;
                e->source_exception = true;
                e->exception_class = exception_class;
                o->base.failure = *e;
            }
            return false;
        }
        o->detail.dirty_returned = true;
        *app = o->base.fields.b; /* Actual fresh b23, after dirty return. */
        if (*app == NULL)
            return mc_screen_sign_fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                                       "YC null App display receiver", "a", "(Lke;)V", 27, false);
        *display = true;
        reached(o, "a", 27);
    } else
        reached(o, "a", button->fields.g ? 30u : 7u);
    return true;
}

static const char *stats_exception_class(mc_client_stats_exception exception) {
    /* Current typed Stats paths: Argo classes are pinned as xe/ao. The checksum
     * INDEX case is hf38 MessageDigest.update(byte[],0,String.length), whose
     * selected JDK17 guard throws IllegalArgumentException for a short buffer.
     * SOURCE_IO aggregates IO failures without recording their dynamic subtype. */
    switch (exception) {
        case MC_CLIENT_STATS_NULL_REFERENCE:
            return "java/lang/NullPointerException";
        case MC_CLIENT_STATS_NUMBER_FORMAT:
            return "java/lang/NumberFormatException";
        case MC_CLIENT_STATS_JSON_SYNTAX:
            return "xe";
        case MC_CLIENT_STATS_JSON_PATH:
            return "ao";
        case MC_CLIENT_STATS_ILLEGAL_STATE:
            return "java/lang/IllegalStateException";
        case MC_CLIENT_STATS_NO_SUCH_ELEMENT:
            return "java/util/NoSuchElementException";
        case MC_CLIENT_STATS_INDEX_OUT_OF_BOUNDS:
            return "java/lang/IllegalArgumentException";
        case MC_CLIENT_STATS_SOURCE_IO:
        case MC_CLIENT_STATS_EXCEPTION_NONE:
            return NULL;
    }
    return NULL;
}

bool mc_screen_sign_display_failed(mc_client_source_yc *o,
                                   const mc_client_display_screen_call *call,
                                   mc_client_screen_error *e) {
    const mc_client_display_screen_call *leaf = call;
    const mc_client_screen_error *cause = NULL;
    bool initialization = false;
    while (leaf != NULL) {
        if (leaf->screen.status != MC_CLIENT_SCREEN_OK) {
            cause = &leaf->screen;
            break;
        }
        if (leaf->initialization.screen.status != MC_CLIENT_SCREEN_OK) {
            cause = &leaf->initialization.screen;
            initialization = true;
            break;
        }
        if (leaf->focus_display == NULL ||
            leaf->focus_display->status == MC_CLIENT_DISPLAY_SCREEN_OK)
            break;
        leaf = leaf->focus_display;
    }
    const mc_client_display_screen_status status =
        call == NULL ? MC_CLIENT_DISPLAY_SCREEN_ALLOCATION : call->status;
    const mc_client_screen_status mapped =
        status == MC_CLIENT_DISPLAY_SCREEN_BUSY ? MC_CLIENT_SCREEN_BUSY
        : status == MC_CLIENT_DISPLAY_SCREEN_UNIMPLEMENTED
            ? MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY
        : status == MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                                                            : MC_CLIENT_SCREEN_APP_FAILED;
    const char *operation =
        call == NULL ? "YC actual Minecraft display native allocation" : call->operation;
    (void)mc_screen_sign_fail(o, e, mapped, operation, "a", "(Lke;)V", 27,
                              status != MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE);
    if (call != NULL)
        o->detail.child = call->site;
    if (cause != NULL) {
        /* The full value includes class failures and typed diagnostic borrows.
         * Only the YC wrapper envelope changes; the child remains caller-owned. */
        const mc_client_screen_site enclosing = e->site;
        *e = *cause;
        e->status = mapped;
        e->operation = operation;
        e->site = enclosing;
        e->parent_site = cause->site;
        if (initialization)
            e->initialization = &leaf->initialization;
    } else if (leaf != NULL) {
        e->parent_site = leaf->child.class_name == NULL ? leaf->site : leaf->child;
        if (leaf->status == MC_CLIENT_DISPLAY_SCREEN_NULL_REFERENCE ||
            leaf->gui.status == MC_CLIENT_GUI_INGAME_SOURCE_NULL_REFERENCE ||
            leaf->resolution_error.status == MC_CLIENT_LOADING_NULL_REFERENCE) {
            e->native_policy = false;
            e->source_exception = true;
            e->exception_class = "java/lang/NullPointerException";
        } else if (leaf->statistics.status == MC_CLIENT_STATS_SOURCE_EXCEPTION) {
            e->native_policy = false;
            e->source_exception = true;
            e->exception_class = stats_exception_class(leaf->statistics.exception);
        } else if (leaf->update.status == MC_CLIENT_SDK_UPDATE_SOURCE_ILLEGAL_STATE) {
            e->native_policy = false;
            e->source_exception = true;
            e->exception_class = "java/lang/IllegalStateException";
        } else if (leaf->mouse.status != MC_CLIENT_MOUSE_HELPER_OK && !leaf->mouse.native_policy &&
                   leaf->mouse.exception_class != NULL) {
            e->native_policy = false;
            e->source_exception = true;
            e->exception_class = leaf->mouse.exception_class;
        } else if (leaf->resolution_error.status == MC_CLIENT_LOADING_STOPPED_ERROR) {
            e->native_policy = false;
            e->source_error = true;
            e->exception_class = "xx";
        }
    }
    e->source_returned = false;
    e->sign = &o->detail;
    o->base.failure = *e;
    return false;
}

mc_client_screen_status mc_client_screen_sign_button(mc_client_source_yc *o,
                                                     mc_client_source_ke *button,
                                                     mc_client_display_screen_call **out,
                                                     mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL || *out != NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "YC display-child native output",
                       NULL, NULL, NULL, 0);
    else if (admit(o, &e)) {
        bool display = false;
        client_application *app = NULL;
        if (mc_screen_sign_button_prefix_locked(o, button, &display, &app, &e)) {
            if (!display)
                (void)returned(o, &e, "a", "(Lke;)V", o->last_pc);
            else {
                /* Enclosing call owns the live receiver; the real display
                 * program recursively calls old YC.h under the same monitor. */
                o->detail.display_entered = true;
                ++o->base.active_calls;
                mtx_unlock(&o->base.classes->mutex);
                const mc_client_display_screen_status status =
                    mc_client_display_screen_run(app, o->base.classes, NULL, out);
                if (mtx_lock(&o->base.classes->mutex) != thrd_success) {
                    o->button_monitor_lost = true;
                    return mc_screen_fail(error == NULL ? &e : error,
                                          MC_CLIENT_SCREEN_THREAD_FAILED,
                                          "YC display monitor reacquisition", NULL, NULL, NULL, 0);
                }
                --o->base.active_calls;
                if (status == MC_CLIENT_DISPLAY_SCREEN_OK) {
                    o->detail.display_returned = true;
                    (void)returned(o, &e, "a", "(Lke;)V", 30);
                } else {
                    (void)mc_screen_sign_display_failed(o, *out, &e);
                }
            }
        }
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}
