#include "source_screen_internal.h"

#include <stdlib.h>
#include <string.h>

void mc_screen_store(mc_client_source_da *o, const char *cl, unsigned int pc) {
    if (o->store_count < sizeof(o->stores) / sizeof(o->stores[0]))
        o->stores[o->store_count++] = (mc_client_screen_site){cl, "<init>", "()V", pc};
}

bool mc_screen_base_construct_locked(mc_client_source_da *o, mc_client_screen_error *e) {
    o->last_pc = 1;
    o->fields.k = 0.0f;
    mc_screen_store(o, "ub", 6);
    o->last_pc = 5;
    mc_client_screen_list *list = mc_screen_allocate(&o->memory, sizeof(*list));
    if (list == NULL) {
        mc_screen_fail(e, o->memory.allocation_status, "da actual new ArrayList", "da", "<init>",
                       "()V", 5);
        return false;
    }
    list->kind = MC_SCREEN_LIST_BUTTONS;
    list->native_live = true;
    o->fields.e = list;
    mc_screen_store(o, "da", 12);
    o->fields.f = false;
    mc_screen_store(o, "da", 17);
    o->fields.a = NULL;
    mc_screen_store(o, "da", 22);
    o->last_pc = 25;
    return true;
}

static mc_client_screen_status allocate_screen(mc_client_screen_classes *c,
                                               const mc_client_screen_options *options, bool menu,
                                               void **out, mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "screen allocation output", NULL,
                       NULL, NULL, 0);
    else if (mc_screen_lock(c, &e)) {
        const size_t size = menu ? sizeof(mc_client_source_fu) : sizeof(mc_client_source_da);
        if ((!menu || mc_screen_active_locked(c, &e)) && e.status == MC_CLIENT_SCREEN_OK) {
            if (c->retiring || c->holds == SIZE_MAX)
                mc_screen_fail(&e, c->retiring ? MC_CLIENT_SCREEN_BUSY : MC_CLIENT_SCREEN_LIMIT,
                               "screen class allocation hold", NULL, NULL, NULL, 0);
            else if (options != NULL && options->maximum_bytes < size)
                mc_screen_fail(&e, MC_CLIENT_SCREEN_LIMIT, "screen native owner budget", NULL, NULL,
                               NULL, 0);
            else {
                mc_client_source_da *o = calloc(1, size);
                if (o == NULL)
                    mc_screen_fail(&e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "screen RAW owner", NULL,
                                   NULL, NULL, 0);
                else {
                    o->classes = c;
                    o->holds = 1;
                    o->is_menu = menu;
                    mc_screen_memory_init(&o->memory, options, size);
                    ++c->holds;
                    *out = o;
                }
            }
        }
        (void)mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_allocate(mc_client_screen_classes *c,
                                                  const mc_client_screen_options *options,
                                                  mc_client_source_da **out,
                                                  mc_client_screen_error *error) {
    void *value = NULL;
    mc_client_screen_status s =
        allocate_screen(c, options, false, out == NULL ? NULL : &value, error);
    if (s == MC_CLIENT_SCREEN_OK)
        *out = value;
    return s;
}

mc_client_screen_status mc_client_screen_menu_allocate(mc_client_screen_classes *c,
                                                       const mc_client_screen_options *options,
                                                       mc_client_source_fu **out,
                                                       mc_client_screen_error *error) {
    void *value = NULL;
    mc_client_screen_status s =
        allocate_screen(c, options, true, out == NULL ? NULL : &value, error);
    if (s == MC_CLIENT_SCREEN_OK)
        *out = value;
    return s;
}

static mc_client_screen_status construct_screen(mc_client_source_da *o, bool menu,
                                                mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "screen constructor receiver", NULL,
                       NULL, NULL, 0);
    else if (mc_screen_lock(o->classes, &e)) {
        if (o->phase != MC_CLIENT_SCREEN_RAW || o->is_menu != menu || o->classes->retiring)
            mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_PHASE, "one-shot actual screen constructor",
                           menu ? "fu" : "da", "<init>", "()V", 0);
        else {
            o->phase = MC_CLIENT_SCREEN_CONSTRUCTING;
            bool success = mc_screen_base_construct_locked(o, &e);
            if (success && menu)
                success = mc_screen_menu_construct_locked((mc_client_source_fu *)o, &e);
            o->phase = success ? MC_CLIENT_SCREEN_READY : MC_CLIENT_SCREEN_FAILED;
            if (!success)
                o->failure = e;
            else
                e.source_returned = true;
        }
        (void)mtx_unlock(&o->classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_construct(mc_client_source_da *o,
                                                   mc_client_screen_error *e) {
    return construct_screen(o, false, e);
}

mc_client_screen_status mc_client_screen_menu_construct(mc_client_source_fu *o,
                                                        mc_client_screen_error *e) {
    return construct_screen(o == NULL ? NULL : &o->base, true, e);
}

mc_client_source_da *mc_client_screen_menu_base(mc_client_source_fu *o) {
    return o == NULL ? NULL : &o->base;
}

mc_client_screen_status mc_client_screen_retain(mc_client_source_da *o,
                                                mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "screen retain receiver", NULL, NULL,
                       NULL, 0);
    else if (mc_screen_lock(o->classes, &e)) {
        if (o->classes->retiring || o->phase == MC_CLIENT_SCREEN_CONSTRUCTING ||
            o->phase == MC_CLIENT_SCREEN_RETIRING || o->holds == SIZE_MAX)
            mc_screen_fail(&e,
                           o->holds == SIZE_MAX ? MC_CLIENT_SCREEN_LIMIT : MC_CLIENT_SCREEN_BUSY,
                           "screen native retain", NULL, NULL, NULL, 0);
        else
            ++o->holds;
        (void)mtx_unlock(&o->classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static mc_client_screen_status release_screen(mc_client_source_da **slot, bool strict,
                                              mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (slot == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "screen release slot", NULL, NULL,
                       NULL, 0);
    else if (*slot != NULL && mc_screen_lock((*slot)->classes, &e)) {
        mc_client_source_da *o = *slot;
        mc_client_screen_classes *c = o->classes;
        if (o->phase == MC_CLIENT_SCREEN_CONSTRUCTING || (strict && o->holds != 1))
            mc_screen_fail(&e, MC_CLIENT_SCREEN_BUSY, "screen strict retirement/active operation",
                           NULL, NULL, NULL, 0);
        else if (o->holds > 1) {
            --o->holds;
            *slot = NULL;
        } else {
            o->phase = MC_CLIENT_SCREEN_RETIRING;
            bool ready = !o->is_menu || mc_screen_menu_cleanup_locked((mc_client_source_fu *)o, &e);
            if (ready)
                ready = mc_screen_button_release_locked(&o->selected_cleanup, &e);
            if (ready)
                ready = mc_screen_button_release_locked(&o->fields.a, &e);
            if (ready)
                ready = mc_screen_initializations_prepare_locked(o, &e);
            if (ready && c->holds == 1)
                ready = mc_screen_class_final_locked(c, &e);
            if (ready)
                ready = mc_screen_initializations_release_apps_locked(o, &e);
            if (ready) {
                if (o->is_menu)
                    mc_screen_menu_strings_free((mc_client_source_fu *)o);
                mc_screen_memory_free(&o->memory);
                free(o);
                *slot = NULL;
                mc_screen_class_drop_locked(c);
                return mc_screen_publish(&e, error);
            }
        }
        (void)mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_release(mc_client_source_da **o,
                                                 mc_client_screen_error *e) {
    return release_screen(o, false, e);
}

mc_client_screen_status mc_client_screen_retire(mc_client_source_da **o,
                                                mc_client_screen_error *e) {
    return release_screen(o, true, e);
}

static mc_client_screen_status release_menu(mc_client_source_fu **slot, bool strict,
                                            mc_client_screen_error *error) {
    if (slot == NULL)
        return release_screen(NULL, strict, error);
    mc_client_source_da *base = *slot == NULL ? NULL : &(*slot)->base;
    const mc_client_screen_status status = release_screen(&base, strict, error);
    if (base == NULL)
        *slot = NULL;
    return status;
}

mc_client_screen_status mc_client_screen_menu_release(mc_client_source_fu **o,
                                                      mc_client_screen_error *e) {
    return release_menu(o, false, e);
}

mc_client_screen_status mc_client_screen_menu_retire(mc_client_source_fu **o,
                                                     mc_client_screen_error *e) {
    return release_menu(o, true, e);
}

mc_client_screen_status mc_client_screen_inspect(mc_client_source_da *o,
                                                 mc_client_screen_state *out,
                                                 mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL)
        mc_screen_fail(&e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "screen inspection receiver/output",
                       NULL, NULL, NULL, 0);
    else if (mc_screen_lock(o->classes, &e)) {
        mc_client_screen_state v = {0};
        v.phase = o->phase;
        v.holds = o->holds;
        v.arena_bytes = o->memory.bytes;
        v.allocations = o->memory.allocations;
        v.allocation_attempts = o->memory.attempts;
        v.is_menu = o->is_menu;
        v.da = o->fields;
        v.last_pc = o->last_pc;
        v.store_count = o->store_count;
        memcpy(v.stores, o->stores, sizeof(v.stores));
        v.failure = o->failure;
        if (o->is_menu) {
            const mc_client_source_fu *m = (const mc_client_source_fu *)o;
            v.fu = m->fields;
            v.caught_exception = m->caught;
            v.caught_pc = m->caught_pc;
            v.caught_cause = m->caught_cause;
            v.splash_count = m->splashes == NULL ? 0 : m->splashes->size;
            v.lines_read = m->lines_read;
            v.last_line = m->last_line;
            v.last_trimmed = m->last_trimmed;
            v.open_stream = m->stream;
            v.input_reader_constructed = m->input_reader != NULL && m->input_reader->constructed;
            v.buffered_reader_constructed =
                m->buffered_reader != NULL && m->buffered_reader->constructed;
        }
        if (o->phase == MC_CLIENT_SCREEN_RETIRING) {
            v.da.a = NULL;
            v.da.g = NULL;
            v.da.h = NULL;
            v.da.e = NULL;
            v.fu.l = NULL;
        }
        *out = v;
        (void)mtx_unlock(&o->classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

bool mc_client_screen_list_size(const mc_client_screen_list *list, size_t *out) {
    if (list == NULL || out == NULL || !list->native_live)
        return false;
    *out = list->size;
    return true;
}
