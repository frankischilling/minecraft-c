#include "source_pause_screen_internal.h"

#include "../application_state.h"
#include "mc/core/java/java.h"
#include "mc/world/runtime/construction.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static bool fail(mc_client_source_oz *o, mc_client_screen_error *e, mc_client_screen_status status,
                 const char *operation, const char *method, unsigned int pc, bool native) {
    mc_screen_fail(e, status, operation, "oz", method, "()V", pc);
    e->native_policy = native;
    if (o != NULL) {
        o->last_pc = pc;
        o->failure = *e;
    }
    return false;
}

static bool child(mc_client_source_oz *o, mc_client_screen_error *e, const char *method,
                  unsigned int pc) {
    o->last_pc = pc;
    e->parent_site = (mc_client_screen_site){"oz", method, "()V", pc};
    o->failure = *e;
    return false;
}

mc_client_screen_status mc_client_screen_pause_allocate(mc_client_screen_classes *classes,
                                                        const mc_client_screen_options *options,
                                                        mc_client_source_oz **out,
                                                        mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "oz native output", "<init>", 0, true);
    else if (mc_screen_lock(classes, &e)) {
        if (classes->retiring || classes->holds == SIZE_MAX ||
            (options != NULL && options->maximum_bytes < sizeof(mc_client_source_oz)))
            fail(NULL, &e, classes->retiring ? MC_CLIENT_SCREEN_BUSY : MC_CLIENT_SCREEN_LIMIT,
                 "oz native namespace/owner budget", "<init>", 0, true);
        else {
            mc_client_source_oz *o = calloc(1, sizeof(*o));
            if (o == NULL)
                fail(NULL, &e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "oz RAW owner allocation",
                     "<init>", 0, true);
            else {
                o->base.classes = classes;
                o->base.holds = 1;
                o->base.kind = MC_CLIENT_SCREEN_PAUSE;
                mc_screen_memory_init(&o->base.memory, options, sizeof(*o));
                ++classes->holds;
                *out = o;
            }
        }
        (void)mtx_unlock(&classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_pause_construct(mc_client_source_oz *o,
                                                         mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "oz constructor receiver", "<init>", 0,
             true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        if (o->base.kind != MC_CLIENT_SCREEN_PAUSE || o->base.phase != MC_CLIENT_SCREEN_RAW ||
            o->base.classes->retiring)
            fail(o, &e, MC_CLIENT_SCREEN_INVALID_PHASE, "oz one-shot constructor", "<init>", 0,
                 true);
        else {
            o->base.phase = MC_CLIENT_SCREEN_CONSTRUCTING;
            if (!mc_screen_base_construct_locked(&o->base, &e)) {
                (void)child(o, &e, "<init>", 1);
                o->base.phase = MC_CLIENT_SCREEN_FAILED;
                o->base.failure = e;
            } else {
                o->save_pass = 0;
                mc_screen_store(&o->base, "oz", 6);
                o->ticks = 0;
                mc_screen_store(&o->base, "oz", 11);
                o->last_pc = 14;
                o->base.phase = MC_CLIENT_SCREEN_READY;
                e.source_returned = true;
                e.site = (mc_client_screen_site){"oz", "<init>", "()V", 14};
            }
        }
        (void)mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static bool literal(mc_client_source_oz *o, const char *ascii, mc_client_screen_text *out,
                    unsigned int pc, mc_client_screen_error *e) {
    mc_player_string *value = NULL;
    if (!mc_screen_literal_locked(o->base.classes, ascii, &value, e))
        return child(o, e, "b", pc);
    *out = (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, value, NULL};
    return true;
}

static bool translated(mc_client_source_oz *o, const char *key, mc_client_screen_text *out,
                       unsigned int pc, mc_client_screen_error *e) {
    mc_client_screen_text actual_key;
    if (!literal(o, key, &actual_key, pc - 2u, e))
        return false;
    mc_registry_classes *registry = o->base.classes->registry;
    if (registry == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                    "oz actual do/nh translation namespace", "b", pc, true);
    if (mc_registry_classes_initialize_translation_singleton(registry, &e->registry) !=
        MC_REGISTRY_OK)
        return fail(o, e, MC_CLIENT_SCREEN_REGISTRY_FAILED,
                    "oz actual translation singleton active use", "b", pc, true);
    mc_registry_translation_view view;
    if (mc_registry_classes_inspect_translation(registry, &view, &e->registry) != MC_REGISTRY_OK)
        return fail(o, e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "oz actual translation singleton", "b",
                    pc, true);
    if (view.singleton_a == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "oz null translation receiver",
                    "b", pc, false);
    mc_player_string_view key_view;
    if (!mc_client_screen_text_read(actual_key, &key_view))
        return fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "oz literal key String view", "b", pc,
                    true);
    const mc_registry_string *value = NULL;
    if (mc_translation_property(view.singleton_a, key_view.units, key_view.length, &value,
                                &e->registry) != MC_REGISTRY_OK)
        return fail(o, e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "oz actual translation lookup", "b", pc,
                    true);
    *out = value == NULL ? actual_key
                         : (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_REGISTRY, NULL, value};
    return true;
}

static bool clear(mc_client_source_oz *o, mc_client_screen_error *e) {
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    if (list == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "oz actual List.clear receiver",
                    "b", 9, false);
    for (size_t index = 0; index < list->size; ++index) {
        mc_client_source_ke *button = list->entries[index];
        if (button != NULL && (button->classes != o->base.classes || button->holds < 2))
            return fail(o, e, MC_CLIENT_SCREEN_INVALID_PHASE, "oz native List occurrence invariant",
                        "b", 9, true);
    }
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    const size_t size = list->size;
    list->size = 0;
    for (size_t index = 0; index < size; ++index) {
        mc_client_source_ke *button = list->entries[index];
        list->entries[index] = NULL;
        if (!mc_screen_button_release_locked(&button, e))
            return child(o, e, "b", 9);
    }
    return true;
}

static bool add(mc_client_source_oz *o, mc_client_screen_list *list, mc_client_source_ke *button,
                unsigned int pc, mc_client_screen_error *e) {
    if (list == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "oz actual List.add receiver",
                    "b", pc, false);
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    if (list->size >= (size_t)INT32_MAX)
        return fail(o, e, MC_CLIENT_SCREEN_LIMIT, "oz native List size after modCount", "b", pc,
                    true);
    if (list->size == list->capacity) {
        size_t next = list->capacity == 0 ? 10u : list->capacity + list->capacity / 2u;
        if (next <= list->size)
            next = list->size + 1u;
        if (next > (size_t)INT32_MAX || next > SIZE_MAX / sizeof(*list->entries))
            return fail(o, e, MC_CLIENT_SCREEN_LIMIT, "oz native List capacity", "b", pc, true);
        void **entries = mc_screen_allocate(&o->base.memory, next * sizeof(*entries));
        if (entries == NULL)
            return fail(o, e, o->base.memory.allocation_status,
                        "oz native List growth after modCount", "b", pc, true);
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

static bool button(mc_client_source_oz *o, mc_pause_initialization *record, int32_t id,
                   int32_t x_offset, int32_t y_offset, int32_t width, const char *label,
                   bool translate, unsigned int new_pc, unsigned int text_pc,
                   unsigned int constructor_pc, unsigned int add_pc, mc_client_screen_error *e) {
    /* Actual List operand is captured before NEW, argument evaluation and label lookup. */
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    mc_client_source_ke *value = mc_screen_button_allocate_locked(o->base.classes, e);
    if (value == NULL)
        return child(o, e, "b", new_pc);
    record->buttons[record->button_count++] = value;
    const int32_t x = mc_java_i32_add(o->base.fields.c / 2, x_offset);
    const int32_t y = mc_java_i32_add(mc_java_i32_add(o->base.fields.d / 4, y_offset), -16);
    mc_client_screen_text text;
    if (translate ? !translated(o, label, &text, text_pc, e)
                  : !literal(o, label, &text, text_pc, e))
        return false;
    if (!mc_screen_button_construct_locked(value, id, x, y, width, 20, text, e))
        return child(o, e, "b", constructor_pc);
    return add(o, list, value, add_pc, e);
}

bool mc_screen_pause_initialize_locked(mc_client_source_oz *o, mc_client_screen_error *e) {
    if (o == NULL || o->base.kind != MC_CLIENT_SCREEN_PAUSE ||
        o->base.phase != MC_CLIENT_SCREEN_READY || o->base.classes->retiring ||
        o->base.active_calls != 0)
        return fail(o, e, MC_CLIENT_SCREEN_INVALID_PHASE, "oz initialization owner", "b", 0, true);
    mc_pause_initialization *record = mc_screen_allocate(&o->base.memory, sizeof(*record));
    if (record == NULL)
        return fail(o, e, o->base.memory.allocation_status, "oz retained initialization", "b", 0,
                    true);
    if (o->last_initialization == NULL)
        o->initializations = record;
    else
        o->last_initialization->next = record;
    o->last_initialization = record;
    ++o->initialization_count;
    o->save_pass = 0; /* Actual a2; i is deliberately not reset. */
    o->last_pc = 2;
    if (!clear(o, e))
        goto failed;
    if (!button(o, record, 1, -100, 120, 200, "Save and quit to title", false, 21, 46, 48, 51, e))
        goto failed;
    if (o->base.fields.b == NULL) {
        fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "oz actual Minecraft.l null receiver",
             "b", 61, false);
        goto failed;
    }
    if (o->base.fields.b->active_world != NULL) {
        mc_world_runtime_fields world;
        if (mc_world_runtime_fields_get(o->base.fields.b->active_world, &world) !=
            MC_WORLD_STORAGE_OK) {
            fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "oz actual Minecraft.l World.B field", "b", 61,
                 true);
            goto failed;
        }
        record->multiplayer = world.B;
    }
    if (record->multiplayer) {
        mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
        if (list == NULL || list->size == 0) {
            fail(o, e,
                 list == NULL ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE
                              : MC_CLIENT_SCREEN_SOURCE_INDEX,
                 "oz actual first Button List.get receiver/index", "b", 72, false);
            goto failed;
        }
        mc_client_source_ke *first = list->entries[0];
        mc_client_screen_text text;
        if (!literal(o, "Disconnect", &text, 80, e))
            goto failed;
        if (first == NULL) {
            fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "oz Disconnect Button receiver", "b",
                 82, false);
            goto failed;
        }
        if (mc_player_string_retain((mc_player_string *)text.player) != MC_PLAYER_OK) {
            fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "oz Disconnect label occurrence", "b", 82,
                 true);
            goto failed;
        }
        const mc_client_screen_text old = first->fields.e;
        first->fields.e = text;
        if (old.kind == MC_CLIENT_SCREEN_TEXT_PLAYER)
            mc_player_string_release((mc_player_string *)old.player);
    }
    if (!button(o, record, 4, -100, 24, 200, "Back to game", false, 89, 114, 116, 119, e) ||
        !button(o, record, 0, -100, 96, 200, "Options...", false, 129, 154, 156, 159, e) ||
        !button(o, record, 5, -100, 48, 98, "gui.achievements", true, 169, 200, 203, 206, e) ||
        !button(o, record, 6, 2, 48, 98, "gui.stats", true, 216, 247, 250, 253, e))
        goto failed;
    record->returned = true;
    o->last_pc = 259;
    e->source_returned = true;
    e->site = (mc_client_screen_site){"oz", "b", "()V", 259};
    return true;
failed:
    record->failure = *e;
    o->failure = *e;
    return false;
}

bool mc_screen_pause_cleanup_locked(mc_client_source_oz *o, mc_client_screen_error *e) {
    for (mc_pause_initialization *record = o->initializations; record != NULL;
         record = record->next)
        for (unsigned int index = 0; index < record->button_count; ++index)
            if (!mc_screen_button_release_locked(&record->buttons[index], e))
                return false;
    return true;
}

void mc_screen_pause_tick_locked(mc_client_source_oz *o, mc_client_screen_error *e) {
    /* Actual invokespecial da.a1 is a literal no-op, then Java-wrapping i11. */
    o->ticks = mc_java_i32_add(o->ticks, 1);
    o->last_pc = 14;
    e->site = (mc_client_screen_site){"oz", "a", "()V", 14};
    e->source_returned = true;
}

mc_client_source_da *mc_client_screen_pause_base(mc_client_source_oz *o) {
    return o == NULL ? NULL : &o->base;
}

mc_client_screen_status mc_client_screen_pause_initialize(mc_client_source_oz *o,
                                                          mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "oz initialization receiver", "b", 0,
             true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        (void)mc_screen_pause_initialize_locked(o, &e);
        (void)mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_pause_tick(mc_client_source_oz *o,
                                                    mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "oz tick receiver", "a", 0, true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        mc_client_source_da *base = &o->base;
        if (base->kind != MC_CLIENT_SCREEN_PAUSE || base->classes->retiring ||
            base->phase != MC_CLIENT_SCREEN_READY || base->selected_cleanup != NULL ||
            base->active_calls != 0) {
            mc_screen_fail(&e, MC_CLIENT_SCREEN_BUSY, "oz idle READY native tick owner", "oz", "a",
                           "()V", 0);
            e.native_policy = true;
        } else {
            ++base->active_calls;
            mc_screen_pause_tick_locked(o, &e);
            --base->active_calls;
        }
        (void)mtx_unlock(&base->classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_pause_inspect(mc_client_source_oz *o,
                                                       mc_client_pause_screen_state *out,
                                                       mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "oz inspection owner/output", "b", 0,
             true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        *out = (mc_client_pause_screen_state){
            o->base.phase,
            o->base.holds,
            o->initialization_count,
            o->save_pass,
            o->ticks,
            o->last_pc,
            o->last_initialization == NULL ? 0 : o->last_initialization->button_count,
            o->last_initialization != NULL && o->last_initialization->returned,
            o->last_initialization != NULL && o->last_initialization->multiplayer,
            &o->failure};
        (void)mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static mc_client_screen_status release(mc_client_source_oz **slot, bool strict,
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

mc_client_screen_status mc_client_screen_pause_release(mc_client_source_oz **slot,
                                                       mc_client_screen_error *error) {
    return release(slot, false, error);
}

mc_client_screen_status mc_client_screen_pause_retire(mc_client_source_oz **slot,
                                                      mc_client_screen_error *error) {
    return release(slot, true, error);
}
