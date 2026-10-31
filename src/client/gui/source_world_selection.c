#include "../application_state.h"
#include "mc/core/java/java.h"
#include "source_world_list.h"
#include "source_world_selection_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static mc_client_screen_site rq_site(const char *method, unsigned int pc) {
    return (mc_client_screen_site){"rq", method, strcmp(method, "<init>") == 0 ? "(Lda;)V" : "()V",
                                   pc};
}

static void reached(mc_client_source_rq *o, const char *method, unsigned int pc) {
    o->method = method;
    o->last_pc = o->base.last_pc = pc;
}

static void stored(mc_client_source_rq *o, const char *method, unsigned int pc) {
    reached(o, method, pc);
    if (o->store_count < sizeof(o->stores) / sizeof(o->stores[0]))
        o->stores[o->store_count++] = rq_site(method, pc);
}

static bool failure(mc_client_source_rq *o, mc_client_screen_error *e,
                    mc_client_screen_status status, const char *operation, const char *method,
                    unsigned int pc, bool native) {
    reached(o, method, pc);
    mc_screen_fail(e, status, operation, "rq", method, rq_site(method, pc).descriptor, pc);
    e->native_policy = native;
    o->failure.screen = *e;
    return false;
}

static bool child_failure(mc_client_source_rq *o, mc_client_screen_error *e, const char *method,
                          unsigned int pc) {
    reached(o, method, pc);
    e->parent_site = rq_site(method, pc);
    o->failure.screen = *e;
    return false;
}

static bool literal(mc_client_source_rq *o, const char *ascii, mc_player_string **out,
                    const char *method, unsigned int pc, mc_client_screen_error *e) {
    reached(o, method, pc);
    return mc_screen_literal_locked(o->base.classes, ascii, out, e) ||
           child_failure(o, e, method, pc);
}

mc_client_source_rq *
mc_screen_world_selection_allocate_locked(mc_client_screen_classes *c,
                                          const mc_client_screen_options *options,
                                          mc_client_screen_error *e) {
    if (c == NULL || c->retiring || c->holds == SIZE_MAX) {
        mc_screen_fail(e,
                       c == NULL     ? MC_CLIENT_SCREEN_INVALID_ARGUMENT
                       : c->retiring ? MC_CLIENT_SCREEN_BUSY
                                     : MC_CLIENT_SCREEN_LIMIT,
                       "rq native screen namespace allocation", "rq", "<init>", "(Lda;)V", 0);
        return NULL;
    }
    if (options != NULL && options->maximum_bytes < sizeof(mc_client_source_rq)) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_LIMIT, "rq native owner budget", "rq", "<init>",
                       "(Lda;)V", 0);
        return NULL;
    }
    mc_client_source_rq *o = calloc(1, sizeof(*o));
    if (o == NULL) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "rq RAW native owner", "rq", "<init>",
                       "(Lda;)V", 0);
        return NULL;
    }
    o->base.classes = c;
    o->base.holds = 1;
    o->base.kind = MC_CLIENT_SCREEN_WORLD_SELECTION;
    mc_screen_memory_init(&o->base.memory, options, sizeof(*o));
    ++c->holds; /* No rq <clinit>, FU Random activation or source constructor work. */
    return o;
}

bool mc_screen_world_selection_construct_locked(mc_client_source_rq *o, mc_client_source_da *parent,
                                                mc_client_screen_error *e) {
    if (o == NULL || o->base.kind != MC_CLIENT_SCREEN_WORLD_SELECTION ||
        o->base.phase != MC_CLIENT_SCREEN_RAW || o->base.classes->retiring) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "rq one-shot constructor receiver", "rq",
                       "<init>", "(Lda;)V", 0);
        return false;
    }
    o->base.phase = MC_CLIENT_SCREEN_CONSTRUCTING;
    bool success = mc_screen_base_construct_locked(&o->base, e);
    if (!success) {
        (void)child_failure(o, e, "<init>", 1);
        goto complete;
    }
    reached(o, "<init>", 5);
    reached(o, "<init>", 9);
    o->failure.date.status =
        mc_platform_date_format_default_create(&o->pending_format, &o->failure.date);
    if (o->failure.date.status != MC_PLATFORM_DATE_FORMAT_OK) {
        (void)failure(o, e, MC_CLIENT_SCREEN_CALENDAR_FAILED,
                      "rq actual default SimpleDateFormat constructor", "<init>", 9, true);
        success = false;
        goto complete;
    }
    o->fields.j = o->pending_format;
    o->pending_format = NULL;
    stored(o, "<init>", 12);
    mc_player_string *title = NULL;
    if (!literal(o, "Select world", &title, "<init>", 16, e)) {
        success = false;
        goto complete;
    }
    o->fields.i = (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, title, NULL};
    stored(o, "<init>", 18);
    o->fields.l = false;
    stored(o, "<init>", 23);
    if (parent != NULL) {
        if (parent->classes != o->base.classes) {
            (void)failure(o, e, MC_CLIENT_SCREEN_INVALID_ARGUMENT,
                          "rq parent belongs to another screen namespace", "<init>", 28, true);
            success = false;
            goto complete;
        }
        if (!mc_screen_instance_hold_locked(parent, e)) {
            (void)child_failure(o, e, "<init>", 28);
            success = false;
            goto complete;
        }
        o->parent_held = true;
    }
    o->fields.a = parent;
    stored(o, "<init>", 28);
    reached(o, "<init>", 31);
    e->site = rq_site("<init>", 31);
    e->source_returned = true;
complete:
    o->base.phase = success ? MC_CLIENT_SCREEN_READY : MC_CLIENT_SCREEN_FAILED;
    if (!success) {
        o->base.failure = *e;
        o->failure.screen = *e;
    }
    return success;
}

static bool translation(mc_client_source_rq *o, const char *method, const mc_translation **out,
                        mc_client_screen_error *e) {
    reached(o, method, 0);
    if (o->base.classes->registry == NULL)
        return failure(o, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                       "rq actual nh class namespace", method, 0, true);
    e->registry.status = mc_registry_classes_initialize_translation_singleton(
        o->base.classes->registry, &e->registry);
    if (e->registry.status != MC_REGISTRY_OK)
        return failure(o, e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "rq actual nh singleton active use",
                       method, 0, true);
    mc_registry_translation_view view = {0};
    e->registry.status =
        mc_registry_classes_inspect_translation(o->base.classes->registry, &view, &e->registry);
    if (e->registry.status != MC_REGISTRY_OK)
        return failure(o, e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "rq actual nh singleton field",
                       method, 0, true);
    *out = view.singleton_a;
    return true;
}

static bool label(mc_client_source_rq *o, const mc_translation *translation_owner, const char *key,
                  unsigned int literal_pc, unsigned int invoke_pc, const char *method,
                  mc_client_screen_text *out, mc_client_screen_error *e) {
    mc_player_string *actual_key = NULL;
    if (!literal(o, key, &actual_key, method, literal_pc, e))
        return false;
    reached(o, method, invoke_pc);
    if (translation_owner == NULL)
        return failure(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                       "rq actual nh property receiver", method, invoke_pc, false);
    mc_player_string_view key_view;
    if (!mc_player_string_read(actual_key, &key_view))
        return failure(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "rq canonical key String view", method,
                       invoke_pc, true);
    const mc_registry_string *value = NULL;
    e->registry.status = mc_translation_property(translation_owner, key_view.units, key_view.length,
                                                 &value, &e->registry);
    if (e->registry.status != MC_REGISTRY_OK)
        return failure(o, e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "rq actual nh property lookup",
                       method, invoke_pc, true);
    *out = value == NULL ? (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, actual_key, NULL}
                         : (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_REGISTRY, NULL, value};
    return true;
}

static bool reload(mc_client_source_rq *o, mc_screen_world_selection_initialization *r,
                   mc_client_screen_error *e) {
    reached(o, "l", 4);
    client_application *app = o->base.fields.b;
    if (app == NULL)
        return failure(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "rq actual App.c receiver",
                       "l", 4, false);
    mc_client_save_format *format = app->aa; /* Actual Minecraft.c getfield aa1 / areturn4. */
    reached(o, "l", 10);
    if (format == NULL)
        return failure(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "rq actual nl.b receiver", "l",
                       10, false);
    r->failure.save.status = mc_client_save_format_list(format, &r->list, &r->failure.save);
    if (r->failure.save.status != MC_CLIENT_SAVE_OK) {
        o->failure.save = r->failure.save;
        return failure(o, e,
                       r->failure.save.status == MC_CLIENT_SAVE_BUSY ? MC_CLIENT_SCREEN_BUSY
                                                                     : MC_CLIENT_SCREEN_APP_FAILED,
                       "rq actual App.aa SaveFormat list", "l", 10, true);
    }
    o->fields.n = r->list; /* Publish the SAME returned List before Collections.sort. */
    stored(o, "l", 15);
    reached(o, "l", 22);
    r->failure.save.status = mc_client_save_list_sort(r->list, &r->failure.save);
    if (r->failure.save.status != MC_CLIENT_SAVE_OK) {
        o->failure.save = r->failure.save;
        return failure(o, e,
                       r->failure.save.status == MC_CLIENT_SAVE_BUSY ? MC_CLIENT_SCREEN_BUSY
                                                                     : MC_CLIENT_SCREEN_APP_FAILED,
                       "rq actual stable Collections.sort", "l", 22, true);
    }
    o->fields.m = -1;
    stored(o, "l", 27);
    reached(o, "l", 30);
    return true;
}

static bool list_add(mc_client_source_rq *o, mc_client_screen_list *list,
                     mc_client_source_ke *button, unsigned int pc, mc_client_screen_error *e) {
    reached(o, "k", pc);
    if (list == NULL)
        return failure(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "rq actual List.add receiver",
                       "k", pc, false);
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    if (list->size >= (size_t)INT32_MAX)
        return failure(o, e, MC_CLIENT_SCREEN_LIMIT, "rq native ArrayList size after modCount", "k",
                       pc, true);
    if (list->size == list->capacity) {
        size_t next = list->capacity == 0 ? 10u : list->capacity + list->capacity / 2u;
        if (next <= list->size)
            next = list->size + 1u;
        if (next > (size_t)INT32_MAX || next > SIZE_MAX / sizeof(*list->entries))
            return failure(o, e, MC_CLIENT_SCREEN_LIMIT, "rq native ArrayList capacity", "k", pc,
                           true);
        void **entries = mc_screen_allocate(&o->base.memory, next * sizeof(*entries));
        if (entries == NULL)
            return failure(o, e, o->base.memory.allocation_status,
                           "rq native ArrayList growth after modCount", "k", pc, true);
        if (list->size != 0)
            memcpy(entries, list->entries, list->size * sizeof(*entries));
        list->entries = entries;
        list->capacity = next;
    }
    if (!mc_screen_button_hold_locked(button, e))
        return child_failure(o, e, "k", pc);
    list->entries[list->size++] = button;
    return true;
}

static bool button(mc_client_source_rq *o, mc_screen_world_selection_initialization *r,
                   const mc_translation *tr, int32_t id, int32_t x_offset, int32_t y_offset,
                   int32_t width, const char *key, unsigned int new_pc, unsigned int lookup_pc,
                   unsigned int constructor_pc, unsigned int field_pc, unsigned int add_pc,
                   mc_client_source_ke **field, unsigned int field_index,
                   mc_client_screen_error *e) {
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    reached(o, "k", new_pc);
    mc_client_source_ke *b = mc_screen_button_allocate_locked(o->base.classes, e);
    if (b == NULL)
        return child_failure(o, e, "k", new_pc);
    r->buttons[r->button_count++] = b;
    int32_t half;
    if (!mc_java_i32_div(o->base.fields.c, 2, &half))
        return failure(o, e, MC_CLIENT_SCREEN_SOURCE_ARITHMETIC, "rq width division", "k", new_pc,
                       false);
    const int32_t x = mc_java_i32_add(half, x_offset);
    const int32_t y = mc_java_i32_add(o->base.fields.d, y_offset);
    mc_client_screen_text text;
    if (!label(o, tr, key, lookup_pc - 2u, lookup_pc, "k", &text, e))
        return false;
    reached(o, "k", constructor_pc);
    if (!mc_screen_button_construct_locked(b, id, x, y, width, 20, text, e))
        return child_failure(o, e, "k", constructor_pc);
    if (field != NULL) {
        if (!mc_screen_button_hold_locked(b, e))
            return child_failure(o, e, "k", field_pc);
        r->field_buttons[field_index] = b;
        *field = b;
        stored(o, "k", field_pc);
    }
    return list_add(o, list, b, add_pc, e);
}

static bool buttons(mc_client_source_rq *o, mc_screen_world_selection_initialization *r,
                    mc_client_screen_error *e) {
    const mc_translation *tr = NULL;
    if (!translation(o, "k", &tr, e))
        return false;
    if (!button(o, r, tr, 1, -154, -52, 150, "selectWorld.select", 9, 39, 42, 46, 49, &o->fields.t,
                0, e) ||
        !button(o, r, tr, 6, -154, -28, 70, "selectWorld.rename", 60, 90, 93, 97, 100, &o->fields.s,
                1, e) ||
        !button(o, r, tr, 2, -74, -28, 70, "selectWorld.delete", 111, 139, 142, 146, 149,
                &o->fields.u, 2, e) ||
        !button(o, r, tr, 3, 4, -52, 150, "selectWorld.create", 159, 187, 190, 0, 193, NULL, 0,
                e) ||
        !button(o, r, tr, 0, 4, -28, 150, "gui.cancel", 203, 231, 234, 0, 237, NULL, 0, e))
        return false;
    mc_client_source_ke *fields[] = {o->fields.t, o->fields.s, o->fields.u};
    const unsigned int pcs[] = {248, 256, 264};
    for (size_t i = 0; i < 3; ++i) {
        if (fields[i] == NULL)
            return failure(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                           "rq actual button disable receiver", "k", pcs[i], false);
        fields[i]->fields.g = false;
        stored(o, "k", pcs[i]);
    }
    reached(o, "k", 267);
    return true;
}

bool mc_screen_world_selection_initialize_locked(mc_client_source_rq *o,
                                                 mc_client_screen_error *e) {
    if (o != NULL && o->base.active_calls != 0) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_BUSY, "rq b native active source occurrence", "rq", "b",
                       "()V", 0);
        return false;
    }
    if (o == NULL || o->base.kind != MC_CLIENT_SCREEN_WORLD_SELECTION ||
        o->base.phase != MC_CLIENT_SCREEN_READY || o->base.classes->retiring) {
        mc_screen_fail(e, MC_CLIENT_SCREEN_INVALID_PHASE, "rq b native receiver", "rq", "b", "()V",
                       0);
        return false;
    }
    mc_screen_world_selection_initialization *r = mc_screen_allocate(&o->base.memory, sizeof(*r));
    if (r == NULL)
        return failure(o, e, o->base.memory.allocation_status, "rq retained initialization record",
                       "b", 0, true);
    if (o->last_initialization == NULL)
        o->initializations = r;
    else
        o->last_initialization->next = r;
    o->last_initialization = r;
    ++o->initialization_count;
    o->failure = (mc_client_world_selection_error){0};
    const mc_translation *tr = NULL;
    if (!translation(o, "b", &tr, e))
        goto failed;
    mc_client_screen_text text;
    if (!label(o, tr, "selectWorld.title", 6, 8, "b", &text, e))
        goto failed;
    o->fields.i = text;
    stored(o, "b", 11);
    if (!label(o, tr, "selectWorld.world", 16, 18, "b", &text, e))
        goto failed;
    o->fields.p = text;
    stored(o, "b", 21);
    if (!label(o, tr, "selectWorld.conversion", 26, 28, "b", &text, e))
        goto failed;
    o->fields.q = text;
    stored(o, "b", 31);
    if (!reload(o, r, e)) {
        e->parent_site = rq_site("b", 35);
        goto failed;
    }
    reached(o, "b", 39);
    r->scroll = mc_screen_world_list_allocate_locked(o, e);
    if (r->scroll == NULL) {
        (void)child_failure(o, e, "b", 39);
        goto failed;
    }
    reached(o, "b", 44);
    if (!mc_screen_world_list_construct_locked(r->scroll, e)) {
        (void)child_failure(o, e, "b", 44);
        goto failed;
    }
    o->fields.o = r->scroll;
    stored(o, "b", 47);
    reached(o, "b", 60);
    if (!mc_screen_world_list_buttons_locked(r->scroll, (mc_client_screen_list *)o->base.fields.e,
                                             4, 5, e)) {
        (void)child_failure(o, e, "b", 60);
        goto failed;
    }
    if (!buttons(o, r, e)) {
        e->parent_site = rq_site("b", 64);
        goto failed;
    }
    reached(o, "b", 67);
    r->returned = true;
    e->site = rq_site("b", 67);
    e->source_returned = true;
    return true;
failed:
    o->failure.screen = *e;
    r->failure = o->failure;
    return false;
}

bool mc_screen_world_selection_cleanup_locked(mc_client_source_rq *o, mc_client_screen_error *e) {
    for (mc_screen_world_selection_initialization *r = o->initializations; r != NULL; r = r->next) {
        if (!mc_screen_world_list_release_locked(&r->scroll, e))
            return false;
        r->failure.save.status = mc_client_save_list_release(&r->list, &r->failure.save);
        if (r->failure.save.status != MC_CLIENT_SAVE_OK) {
            o->failure.save = r->failure.save;
            return failure(o, e,
                           r->failure.save.status == MC_CLIENT_SAVE_BUSY
                               ? MC_CLIENT_SCREEN_BUSY
                               : MC_CLIENT_SCREEN_APP_FAILED,
                           "rq retained save List retirement", "b", 0, true);
        }
        for (size_t i = 0; i < 3; ++i)
            if (!mc_screen_button_release_locked(&r->field_buttons[i], e))
                return false;
        for (unsigned int i = 0; i < r->button_count; ++i)
            if (!mc_screen_button_release_locked(&r->buttons[i], e))
                return false;
    }
    o->failure.date.status = mc_platform_date_format_retire(&o->pending_format, &o->failure.date);
    if (o->failure.date.status == MC_PLATFORM_DATE_FORMAT_OK)
        o->failure.date.status = mc_platform_date_format_retire(&o->fields.j, &o->failure.date);
    if (o->failure.date.status != MC_PLATFORM_DATE_FORMAT_OK)
        return failure(o, e, MC_CLIENT_SCREEN_CALENDAR_FAILED, "rq DateFormat native retirement",
                       "<init>", 0, true);
    if (o->parent_held) {
        if (!mc_screen_instance_release_locked(&o->fields.a, e))
            return false;
        o->parent_held = false;
    }
    return true;
}

mc_client_screen_status mc_client_screen_world_selection_allocate(
    mc_client_screen_classes *c, const mc_client_screen_options *options, mc_client_source_rq **out,
    mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "rq allocation output", "rq",
                       "<init>", "(Lda;)V", 0);
    else if (mc_screen_lock(c, &e)) {
        mc_client_source_rq *o = mc_screen_world_selection_allocate_locked(c, options, &e);
        if (o != NULL)
            *out = o;
        (void)mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_world_selection_construct(mc_client_source_rq *o,
                                                                   mc_client_source_da *parent,
                                                                   mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "rq constructor receiver", "rq",
                       "<init>", "(Lda;)V", 0);
    else if (mc_screen_lock(o->base.classes, &e)) {
        (void)mc_screen_world_selection_construct_locked(o, parent, &e);
        (void)mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_source_da *mc_client_screen_world_selection_base(mc_client_source_rq *o) {
    return o == NULL ? NULL : &o->base;
}

mc_client_screen_status mc_client_screen_world_selection_initialize(mc_client_source_rq *o,
                                                                    mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "rq b receiver", "rq", "b", "()V", 0);
    else if (mc_screen_lock(o->base.classes, &e)) {
        (void)mc_screen_world_selection_initialize_locked(o, &e);
        (void)mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_world_selection_inspect(
    mc_client_source_rq *o, mc_client_world_selection_state *out, mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "rq inspection receiver/output", NULL,
                       NULL, NULL, 0);
    else if (mc_screen_lock(o->base.classes, &e)) {
        *out = (mc_client_world_selection_state){.phase = o->base.phase,
                                                 .holds = o->base.holds,
                                                 .initialization_count = o->initialization_count,
                                                 .fields = o->fields,
                                                 .method = o->method,
                                                 .last_pc = o->last_pc,
                                                 .store_count = o->store_count,
                                                 .failure = &o->failure};
        memcpy(out->stores, o->stores, sizeof(out->stores));
        (void)mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static mc_client_screen_status release(mc_client_source_rq **slot, bool strict,
                                       mc_client_screen_error *e) {
    if (slot == NULL)
        return strict ? mc_client_screen_retire(NULL, e) : mc_client_screen_release(NULL, e);
    mc_client_source_da *base = *slot == NULL ? NULL : &(*slot)->base;
    const mc_client_screen_status status =
        strict ? mc_client_screen_retire(&base, e) : mc_client_screen_release(&base, e);
    if (base == NULL)
        *slot = NULL;
    return status;
}

mc_client_screen_status mc_client_screen_world_selection_release(mc_client_source_rq **slot,
                                                                 mc_client_screen_error *e) {
    return release(slot, false, e);
}

mc_client_screen_status mc_client_screen_world_selection_retire(mc_client_source_rq **slot,
                                                                mc_client_screen_error *e) {
    return release(slot, true, e);
}
