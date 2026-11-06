#include "source_death_screen_internal.h"

#include "../application_state.h"
#include "mc/core/java/java.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static bool fail(mc_client_source_ch *o, mc_client_screen_error *e, mc_client_screen_status status,
                 const char *operation, const char *method, unsigned int pc, bool native) {
    mc_screen_fail(e, status, operation, "ch", method, "()V", pc);
    e->native_policy = native;
    if (o != NULL) {
        o->last_pc = pc;
        o->failure = *e;
    }
    return false;
}

static bool child(mc_client_source_ch *o, mc_client_screen_error *e, const char *method,
                  unsigned int pc) {
    o->last_pc = pc;
    e->parent_site = (mc_client_screen_site){"ch", method, "()V", pc};
    o->failure = *e;
    return false;
}

mc_client_screen_status mc_client_screen_death_allocate(mc_client_screen_classes *classes,
                                                        const mc_client_screen_options *options,
                                                        mc_client_source_ch **out,
                                                        mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ch native output", "<init>", 0, true);
    else if (mc_screen_lock(classes, &e)) {
        if (classes->retiring || classes->holds == SIZE_MAX ||
            (options != NULL && options->maximum_bytes < sizeof(mc_client_source_ch)))
            fail(NULL, &e, classes->retiring ? MC_CLIENT_SCREEN_BUSY : MC_CLIENT_SCREEN_LIMIT,
                 "ch native namespace/owner budget", "<init>", 0, true);
        else {
            mc_client_source_ch *o = calloc(1, sizeof(*o));
            if (o == NULL)
                fail(NULL, &e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "ch RAW owner allocation",
                     "<init>", 0, true);
            else {
                o->base.classes = classes;
                o->base.holds = 1;
                o->base.kind = MC_CLIENT_SCREEN_DEATH;
                mc_screen_memory_init(&o->base.memory, options, sizeof(*o));
                ++classes->holds;
                *out = o;
            }
        }
        mtx_unlock(&classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_death_construct(mc_client_source_ch *o,
                                                         mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ch constructor receiver", "<init>", 0,
             true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        if (o->base.kind != MC_CLIENT_SCREEN_DEATH || o->base.phase != MC_CLIENT_SCREEN_RAW ||
            o->base.classes->retiring)
            fail(o, &e, MC_CLIENT_SCREEN_INVALID_PHASE, "ch one-shot constructor", "<init>", 0,
                 true);
        else {
            o->base.phase = MC_CLIENT_SCREEN_CONSTRUCTING;
            if (!mc_screen_base_construct_locked(&o->base, &e)) {
                child(o, &e, "<init>", 1);
                o->base.phase = MC_CLIENT_SCREEN_FAILED;
                o->base.failure = e;
            } else {
                o->last_pc = 4;
                o->base.phase = MC_CLIENT_SCREEN_READY;
                e.source_returned = true;
                e.site = (mc_client_screen_site){"ch", "<init>", "()V", 4};
            }
        }
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static bool clear(mc_client_source_ch *o, mc_client_screen_error *e) {
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    if (list == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ch actual List.clear receiver",
                    "b", 4, false);
    for (size_t i = 0; i < list->size; ++i) {
        const mc_client_source_ke *button = list->entries[i];
        if (button != NULL && (button->classes != o->base.classes || button->holds < 2))
            return fail(o, e, MC_CLIENT_SCREEN_INVALID_PHASE, "ch native List occurrence invariant",
                        "b", 4, true);
    }
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    const size_t count = list->size;
    list->size = 0;
    for (size_t i = 0; i < count; ++i) {
        mc_client_source_ke *button = list->entries[i];
        list->entries[i] = NULL;
        if (!mc_screen_button_release_locked(&button, e))
            return child(o, e, "b", 4);
    }
    return true;
}

static bool add(mc_client_source_ch *o, mc_client_screen_list *list, mc_client_source_ke *button,
                unsigned int pc, mc_client_screen_error *e) {
    if (list == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ch actual List.add receiver",
                    "b", pc, false);
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    if (list->size >= (size_t)INT32_MAX)
        return fail(o, e, MC_CLIENT_SCREEN_LIMIT, "ch native List size after modCount", "b", pc,
                    true);
    if (list->size == list->capacity) {
        size_t next = list->capacity == 0 ? 10u : list->capacity + list->capacity / 2u;
        if (next <= list->size)
            next = list->size + 1u;
        if (next > (size_t)INT32_MAX || next > SIZE_MAX / sizeof(*list->entries))
            return fail(o, e, MC_CLIENT_SCREEN_LIMIT, "ch native List capacity", "b", pc, true);
        void **entries = mc_screen_allocate(&o->base.memory, next * sizeof(*entries));
        if (entries == NULL)
            return fail(o, e, o->base.memory.allocation_status,
                        "ch native List growth after modCount", "b", pc, true);
        if (list->size != 0)
            memcpy(entries, list->entries, list->size * sizeof(*entries));
        list->entries = entries;
        list->capacity = next;
    }
    if (!mc_screen_button_hold_locked(button, e))
        return child(o, e, "b", pc);
    list->entries[list->size++] = button;
    return true;
}

static bool button(mc_client_source_ch *o, mc_death_initialization *record, int32_t id,
                   int32_t y_offset, const char *label, unsigned int new_pc,
                   unsigned int literal_pc, unsigned int constructor_pc, unsigned int add_pc,
                   mc_client_screen_error *e) {
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    mc_client_source_ke *value = mc_screen_button_allocate_locked(o->base.classes, e);
    if (value == NULL)
        return child(o, e, "b", new_pc);
    record->buttons[record->button_count++] = value;
    const int32_t x = mc_java_i32_sub(o->base.fields.c / 2, 100);
    const int32_t y = mc_java_i32_add(o->base.fields.d / 4, y_offset);
    mc_player_string *literal = NULL;
    if (!mc_screen_literal_locked(o->base.classes, label, &literal, e))
        return child(o, e, "b", literal_pc);
    const mc_client_screen_text text = {MC_CLIENT_SCREEN_TEXT_PLAYER, literal, NULL};
    if (!mc_screen_button_construct_locked(value, id, x, y, 200, 20, text, e))
        return child(o, e, "b", constructor_pc);
    return add(o, list, value, add_pc, e);
}

bool mc_screen_death_initialize_locked(mc_client_source_ch *o, mc_client_screen_error *e) {
    if (o == NULL || o->base.kind != MC_CLIENT_SCREEN_DEATH ||
        o->base.phase != MC_CLIENT_SCREEN_READY || o->base.classes->retiring ||
        o->base.active_calls != 0)
        return fail(o, e, MC_CLIENT_SCREEN_INVALID_PHASE, "ch initialization owner", "b", 0, true);
    mc_death_initialization *record = mc_screen_allocate(&o->base.memory, sizeof(*record));
    if (record == NULL)
        return fail(o, e, o->base.memory.allocation_status, "ch retained initialization", "b", 0,
                    true);
    if (o->last_initialization == NULL)
        o->initializations = record;
    else
        o->last_initialization->next = record;
    o->last_initialization = record;
    ++o->initialization_count;
    if (!clear(o, e) || !button(o, record, 1, 72, "Respawn", 13, 36, 38, 41, e) ||
        !button(o, record, 2, 96, "Title menu", 51, 74, 76, 79, e))
        goto failed;
    if (o->base.fields.b == NULL) {
        fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ch actual Minecraft.k null receiver",
             "b", 89, false);
        goto failed;
    }
    if (o->base.fields.b->session == NULL) {
        mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
        if (list == NULL || list->size <= 1) {
            fail(o, e,
                 list == NULL ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                              : MC_CLIENT_SCREEN_SOURCE_INDEX,
                 "ch actual title Button List.get receiver/index", "b", 100, false);
            goto failed;
        }
        mc_client_source_ke *title = list->entries[1];
        if (title == NULL) {
            fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "ch title Button.g null receiver",
                 "b", 109, false);
            goto failed;
        }
        title->fields.g = false;
        o->last_pc = 109;
        record->title_disabled = true;
    }
    record->returned = true;
    o->last_pc = 112;
    e->source_returned = true;
    e->site = (mc_client_screen_site){"ch", "b", "()V", 112};
    return true;
failed:
    record->failure = *e;
    o->failure = *e;
    return false;
}

bool mc_screen_death_cleanup_locked(mc_client_source_ch *o, mc_client_screen_error *e) {
    for (mc_death_initialization *r = o->initializations; r != NULL; r = r->next)
        for (unsigned int i = 0; i < r->button_count; ++i)
            if (!mc_screen_button_release_locked(&r->buttons[i], e))
                return false;
    return true;
}

mc_client_source_da *mc_client_screen_death_base(mc_client_source_ch *o) {
    return o == NULL ? NULL : &o->base;
}

mc_client_screen_status mc_client_screen_death_initialize(mc_client_source_ch *o,
                                                          mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ch initialization receiver", "b", 0,
             true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        (void)mc_screen_death_initialize_locked(o, &e);
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_death_inspect(mc_client_source_ch *o,
                                                       mc_client_death_screen_state *out,
                                                       mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "ch inspection receiver/output", "b", 0,
             true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        const mc_death_initialization *r = o->last_initialization;
        *out = (mc_client_death_screen_state){o->base.phase,
                                              o->base.holds,
                                              o->initialization_count,
                                              o->last_pc,
                                              r == NULL ? 0 : r->button_count,
                                              r != NULL && r->returned,
                                              r != NULL && r->title_disabled,
                                              &o->failure};
        mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static mc_client_screen_status release(mc_client_source_ch **slot, bool strict,
                                       mc_client_screen_error *error) {
    if (slot == NULL)
        return mc_client_screen_release(NULL, error);
    mc_client_source_da *base = *slot == NULL ? NULL : &(*slot)->base;
    const mc_client_screen_status status =
        strict ? mc_client_screen_retire(&base, error) : mc_client_screen_release(&base, error);
    if (base == NULL)
        *slot = NULL;
    return status;
}

mc_client_screen_status mc_client_screen_death_release(mc_client_source_ch **slot,
                                                       mc_client_screen_error *error) {
    return release(slot, false, error);
}

mc_client_screen_status mc_client_screen_death_retire(mc_client_source_ch **slot,
                                                      mc_client_screen_error *error) {
    return release(slot, true, error);
}
