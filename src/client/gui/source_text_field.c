#include "source_text_field_internal.h"

#include "mc/core/java/java.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static void at(mc_client_source_ro *o, const char *method, const char *descriptor,
               unsigned int pc) {
    o->site = (mc_client_screen_site){"ro", method, descriptor, pc};
}

static bool fail(mc_client_source_ro *o, mc_client_screen_error *e, mc_client_screen_status status,
                 const char *operation, const char *method, const char *descriptor,
                 unsigned int pc) {
    mc_client_screen_error local = {0};
    mc_client_screen_error *error = e == NULL ? &local : e;
    *error = (mc_client_screen_error){0};
    mc_screen_fail(error, status, operation, "ro", method, descriptor, pc);
    error->native_policy =
        status != MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE && status != MC_CLIENT_SCREEN_SOURCE_INDEX;
    if (o != NULL) {
        o->failure = *error;
        at(o, method, descriptor, pc);
    }
    return false;
}

static bool admitted(mc_client_source_ro *o, const char *method, const char *descriptor,
                     mc_client_screen_error *e) {
    if (o == NULL || o->native_owner == NULL || o->retired)
        return fail(o, e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "RO live native owner", method,
                    descriptor, 0);
    if (e != NULL)
        *e = (mc_client_screen_error){0};
    if (!o->active)
        o->failure = (mc_client_screen_error){0};
    at(o, method, descriptor, 0);
    return true;
}

static bool returned(mc_client_source_ro *o, const char *method, const char *descriptor,
                     unsigned int pc, mc_client_screen_error *e) {
    at(o, method, descriptor, pc);
    o->failure = (mc_client_screen_error){0};
    if (e != NULL) {
        *e = (mc_client_screen_error){0};
        e->source_returned = true;
    }
    return true;
}

mc_client_source_ro *mc_screen_text_field_allocate_locked(mc_client_source_da *owner,
                                                          mc_client_screen_error *e) {
    if (owner == NULL) {
        fail(NULL, e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "RO enclosing native arena", "<init>",
             "(Lda;Lsj;IIIILjava/lang/String;)V", 0);
        return NULL;
    }
    mc_client_source_ro *o = mc_screen_allocate(&owner->memory, sizeof(*o));
    if (o == NULL) {
        fail(NULL, e, owner->memory.allocation_status, "RO native raw allocation", "<init>",
             "(Lda;Lsj;IIIILjava/lang/String;)V", 0);
        return NULL;
    }
    o->native_owner = owner;
    if (e != NULL)
        *e = (mc_client_screen_error){0};
    return o;
}

static bool text_store(mc_client_source_ro *o, mc_client_screen_text text, const char *descriptor,
                       unsigned int pc, mc_client_screen_error *e) {
    mc_player_string *held = NULL;
    bool valid = false;
    if (text.kind == MC_CLIENT_SCREEN_TEXT_NULL)
        valid = text.player == NULL && text.registry == NULL;
    else if (text.kind == MC_CLIENT_SCREEN_TEXT_PLAYER && text.player != NULL &&
             text.registry == NULL) {
        held = (mc_player_string *)text.player;
        const mc_player_status status = mc_player_string_retain(held);
        if (status != MC_PLAYER_OK) {
            fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "RO actual String occurrence", "a",
                 descriptor, pc);
            if (e != NULL)
                e->string = status;
            o->failure.string = status;
            return false;
        }
        valid = true;
    } else if (text.kind == MC_CLIENT_SCREEN_TEXT_REGISTRY)
        valid =
            text.player == NULL && text.registry != NULL && o->native_owner->classes->registry_held;
    if (!valid)
        return fail(o, e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "RO actual source String reference",
                    "a", descriptor, pc);
    mc_player_string *old = o->text_hold;
    o->fields.h = text;
    o->text_hold = held;
    ++o->text_stores;
    mc_player_string_release(old);
    return true;
}

bool mc_screen_text_field_set_text_locked(mc_client_source_ro *o, mc_client_screen_text text,
                                          mc_client_screen_error *e) {
    const char *const descriptor = "(Ljava/lang/String;)V";
    return admitted(o, "a", descriptor, e) && text_store(o, text, descriptor, 2, e) &&
           returned(o, "a", descriptor, 5, e);
}

bool mc_screen_text_field_construct_locked(mc_client_source_ro *o, mc_client_source_da *parent,
                                           mc_client_bitmap_font *font, int32_t x, int32_t y,
                                           int32_t width, int32_t height,
                                           mc_client_screen_text text, mc_client_screen_error *e) {
    const char *const descriptor = "(Lda;Lsj;IIIILjava/lang/String;)V";
    if (!admitted(o, "<init>", descriptor, e))
        return false;
    if (o->phase != MC_CLIENT_SCREEN_RAW || o->active)
        return fail(o, e, MC_CLIENT_SCREEN_INVALID_PHASE, "RO raw constructor entry", "<init>",
                    descriptor, 0);
    o->phase = MC_CLIENT_SCREEN_CONSTRUCTING;
    o->fields.a = false;
    o->fields.b = true;
    o->fields.l = parent;
    o->fields.c = font;
    o->fields.d = x;
    o->fields.e = y;
    o->fields.f = width;
    o->fields.g = height;
    if (!mc_screen_text_field_set_text_locked(o, text, e)) {
        o->phase = MC_CLIENT_SCREEN_FAILED;
        if (e != NULL)
            e->parent_site = (mc_client_screen_site){"ro", "<init>", descriptor, 50};
        return false;
    }
    o->phase = MC_CLIENT_SCREEN_READY;
    return returned(o, "<init>", descriptor, 53, e);
}

bool mc_screen_text_field_set_maximum_locked(mc_client_source_ro *o, int32_t maximum,
                                             mc_client_screen_error *e) {
    if (!admitted(o, "a", "(I)V", e))
        return false;
    o->fields.i = maximum;
    return returned(o, "a", "(I)V", 5, e);
}

bool mc_screen_text_field_tick_locked(mc_client_source_ro *o, mc_client_screen_error *e) {
    if (!admitted(o, "b", "()V", e))
        return false;
    o->fields.j = mc_java_i32_add(o->fields.j, 1);
    return returned(o, "b", "()V", 10, e);
}

bool mc_screen_text_field_focus_locked(mc_client_source_ro *o, bool focus,
                                       mc_client_screen_error *e) {
    if (!admitted(o, "a", "(Z)V", e))
        return false;
    if (focus && !o->fields.a)
        o->fields.j = 0;
    o->fields.a = focus;
    return returned(o, "a", "(Z)V", 21, e);
}

bool mc_screen_text_field_mouse_locked(mc_client_source_ro *o, int32_t x, int32_t y, int32_t unused,
                                       mc_client_screen_error *e) {
    (void)unused;
    if (!admitted(o, "a", "(III)V", e))
        return false;
    const bool hit = o->fields.b && x >= o->fields.d &&
                     x < mc_java_i32_add(o->fields.d, o->fields.f) && y >= o->fields.e &&
                     y < mc_java_i32_add(o->fields.e, o->fields.g);
    return mc_screen_text_field_focus_locked(o, hit, e) && returned(o, "a", "(III)V", 62, e);
}

static bool text_read(mc_client_source_ro *o, mc_player_string_view *out, unsigned int pc,
                      mc_client_screen_error *e) {
    at(o, "a", "(CI)V", pc);
    if (o->fields.h.kind == MC_CLIENT_SCREEN_TEXT_NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "RO String.length null receiver",
                    "a", "(CI)V", pc);
    if (!mc_client_screen_text_read(o->fields.h, out) || out->length > INT32_MAX)
        return fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "RO actual UTF16 String view", "a",
                    "(CI)V", pc);
    return true;
}

static bool edited(mc_client_source_ro *o, mc_player_string_view first,
                   mc_player_string_view second, unsigned int pc, mc_client_screen_error *e) {
    if (first.length > MC_PLAYER_MAX_TEXT_UNITS ||
        second.length > MC_PLAYER_MAX_TEXT_UNITS - first.length)
        return fail(o, e, MC_CLIENT_SCREEN_LIMIT, "RO edited String native unit ceiling", "a",
                    "(CI)V", pc);
    const size_t length = first.length + second.length;
    if (length == 0)
        return text_store(
            o,
            (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, mc_player_empty_literal(), NULL},
            "(CI)V", pc, e);
    uint16_t *units = length == 0 ? NULL : malloc(length * sizeof(*units));
    if (length != 0 && units == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "RO edited String native scratch",
                    "a", "(CI)V", pc);
    if (first.length != 0)
        memcpy(units, first.units, first.length * sizeof(*units));
    if (second.length != 0)
        memcpy(units + first.length, second.units, second.length * sizeof(*units));
    mc_player_string *value = NULL;
    const mc_player_status status = mc_player_string_create(units, length, &value);
    free(units);
    if (status != MC_PLAYER_OK) {
        fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "RO edited actual String allocation", "a",
             "(CI)V", pc);
        if (e != NULL)
            e->string = status;
        o->failure.string = status;
        return false;
    }
    const bool stored = text_store(
        o, (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, value, NULL}, "(CI)V", pc, e);
    mc_player_string_release(value);
    return stored;
}

static bool bridge_failed(mc_client_source_ro *o, mc_client_screen_error *e,
                          mc_client_screen_error child, unsigned int pc) {
    if (child.status == MC_CLIENT_SCREEN_OK)
        return fail(o, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                    "RO source-call bridge did not return", "a", "(CI)V", pc);
    child.parent_site = (mc_client_screen_site){"ro", "a", "(CI)V", pc};
    o->failure = child;
    if (e != NULL)
        *e = child;
    at(o, "a", "(CI)V", pc);
    return false;
}

static bool key_body(mc_client_source_ro *o, uint16_t ch, int32_t key,
                     const mc_client_text_field_hooks *hooks, mc_client_screen_error *e) {
    if (!o->fields.b || !o->fields.a)
        return returned(o, "a", "(CI)V", 14, e);
    if (ch == 9) {
        at(o, "a", "(CI)V", 25);
        if (o->fields.l == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "RO Tab null parent receiver",
                        "a", "(CI)V", 25);
        if (hooks == NULL || hooks->tab == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                        "RO actual parent Tab source-call bridge", "a", "(CI)V", 25);
        mc_client_screen_error child = {0};
        ++o->tab_calls;
        if (!hooks->tab(hooks->context, o->fields.l, &child))
            return bridge_failed(o, e, child, 25);
    }
    if (ch == 22) {
        at(o, "a", "(CI)V", 34);
        if (hooks == NULL || hooks->clipboard == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                        "RO actual Clipboard source-call bridge", "a", "(CI)V", 34);
        mc_player_string *clipboard = NULL;
        mc_client_screen_error child = {0};
        ++o->clipboard_calls;
        if (!hooks->clipboard(hooks->context, &clipboard, &child)) {
            mc_player_string_release(clipboard);
            return bridge_failed(o, e, child, 34);
        }
        mc_player_string_view current, pasted = {0};
        bool ok = text_read(o, &current, 51, e);
        if (ok && clipboard != NULL && !mc_player_string_read(clipboard, &pasted))
            ok = fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "RO actual Clipboard String view", "a",
                      "(CI)V", 60);
        if (ok) {
            int32_t count = mc_java_i32_sub(32, (int32_t)current.length);
            if (count > (int32_t)pasted.length)
                count = (int32_t)pasted.length;
            if (count > 0) {
                pasted.length = (size_t)count;
                ok = edited(o, current, pasted, 105, e);
            }
        }
        mc_player_string_release(clipboard);
        if (!ok)
            return false;
    }
    if (key == 14) {
        mc_player_string_view current;
        if (!text_read(o, &current, 118, e))
            return false;
        if (current.length != 0) {
            --current.length; /* String.substring removes one UTF16 code unit. */
            if (!edited(o, current, (mc_player_string_view){0}, 142, e))
                return false;
        }
    }
    at(o, "a", "(CI)V", 145);
    if (hooks == NULL || hooks->mapping == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "RO actual FP class namespace",
                    "a", "(CI)V", 145);
    const mc_player_string *allowed = NULL;
    if (mc_client_font_mapping_active_use(hooks->mapping, &allowed, &o->mapping) !=
        MC_CLIENT_FONT_MAPPING_OK)
        return fail(o, e, MC_CLIENT_SCREEN_FONT_FAILED, o->mapping.operation, "a", "(CI)V", 145);
    at(o, "a", "(CI)V", 149);
    if (allowed == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "RO FP.a null indexOf receiver",
                    "a", "(CI)V", 149);
    mc_player_string_view whitelist;
    if (!mc_player_string_read(allowed, &whitelist))
        return fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "RO actual FP.a String view", "a",
                    "(CI)V", 149);
    for (size_t i = 0; i < whitelist.length; ++i)
        if (whitelist.units[i] == ch) {
            mc_player_string_view current;
            if (!text_read(o, &current, 159, e))
                return false;
            if ((int32_t)current.length < o->fields.i || o->fields.i == 0)
                if (!edited(o, current, (mc_player_string_view){&ch, 1}, 198, e))
                    return false;
            break;
        }
    return returned(o, "a", "(CI)V", 201, e);
}

bool mc_screen_text_field_key_locked(mc_client_source_ro *o, uint16_t ch, int32_t key,
                                     const mc_client_text_field_hooks *hooks,
                                     mc_client_screen_error *e) {
    if (!admitted(o, "a", "(CI)V", e))
        return false;
    if (o->active)
        return fail(o, e, MC_CLIENT_SCREEN_BUSY, "RO active keyboard invocation", "a", "(CI)V", 0);
    o->active = true;
    o->mapping = (mc_client_font_mapping_error){0};
    const bool ok = key_body(o, ch, key, hooks, e);
    o->active = false;
    return ok;
}

bool mc_client_text_field_inspect(const mc_client_source_ro *o, mc_client_text_field_state *out) {
    if (o == NULL || out == NULL || o->retired)
        return false;
    *out = (mc_client_text_field_state){
        o->phase,   o->fields,  o->active,    o->retired,         o->site,
        o->failure, o->mapping, o->tab_calls, o->clipboard_calls, o->text_stores};
    return true;
}

bool mc_screen_text_field_release_locked(mc_client_source_ro **slot, mc_client_screen_error *e) {
    if (slot == NULL)
        return fail(NULL, e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "RO native retirement slot", NULL,
                    NULL, 0);
    mc_client_source_ro *o = *slot;
    if (o == NULL)
        return true;
    if (o->active)
        return fail(o, e, MC_CLIENT_SCREEN_BUSY, "RO active native retirement", NULL, NULL, 0);
    mc_player_string_release(o->text_hold);
    o->text_hold = NULL;
    o->retired = true;
    o->phase = MC_CLIENT_SCREEN_RETIRING;
    *slot = NULL;
    return true;
}
