#include "source_create_world_internal.h"

#include "../application_state.h"
#include "../display/staged.h"
#include "bitmap_font_internal.h"
#include "loading_internal.h"
#include "mc/core/java/integer_parse.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct mc_create_world_string {
    struct mc_create_world_string *next;
    mc_player_string *value;
};

typedef struct mc_create_world_query {
    struct mc_create_world_query *next;
    mc_world_info_name *argument;
    mc_world_info *info;
} mc_create_world_query;

struct mc_create_world_naming {
    struct mc_create_world_naming *next;
    mc_client_save_format *format;
    mc_create_world_query *queries;
    mc_client_screen_text local;
};

struct mc_create_world_seed {
    struct mc_create_world_seed *next;
    mc_java_random random;
    bool constructed, drawn, parsed, hashed;
    int64_t initial, selected;
};

static const char *descriptor(const char *method) {
    if (strcmp(method, "<init>") == 0)
        return "(Lda;)V";
    if (strcmp(method, "key") == 0)
        return "(CI)V";
    if (strcmp(method, "mouse") == 0)
        return "(III)V";
    if (strcmp(method, "button") == 0)
        return "(Lke;)V";
    return "()V";
}

static const char *source_method(const char *method) {
    return strcmp(method, "key") == 0 || strcmp(method, "mouse") == 0 ||
                   strcmp(method, "button") == 0
               ? "a"
               : method;
}

static void reached(mc_client_source_fj *o, const char *method, unsigned int pc) {
    if (o != NULL) {
        o->method = method;
        o->last_pc = o->base.last_pc = pc;
    }
}

static bool fail(mc_client_source_fj *o, mc_client_screen_error *e, mc_client_screen_status status,
                 const char *operation, const char *method, unsigned int pc, bool native) {
    mc_screen_fail(e, status, operation, "fj", source_method(method), descriptor(method), pc);
    e->native_policy = native;
    reached(o, method, pc);
    if (o != NULL)
        o->failure = *e;
    return false;
}

static bool child(mc_client_source_fj *o, mc_client_screen_error *e, const char *method,
                  unsigned int pc) {
    reached(o, method, pc);
    e->parent_site = (mc_client_screen_site){"fj", source_method(method), descriptor(method), pc};
    o->failure = *e;
    return false;
}

static bool returned(mc_client_source_fj *o, mc_client_screen_error *e, const char *method,
                     unsigned int pc) {
    reached(o, method, pc);
    e->site = (mc_client_screen_site){"fj", source_method(method), descriptor(method), pc};
    e->source_returned = true;
    return true;
}

static bool literal(mc_client_source_fj *o, const char *value, mc_client_screen_text *out,
                    const char *method, unsigned int pc, mc_client_screen_error *e) {
    if (value[0] == '\0') {
        *out =
            (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, mc_player_empty_literal(), NULL};
        return true;
    }
    mc_player_string *s = NULL;
    if (!mc_screen_literal_locked(o->base.classes, value, &s, e))
        return child(o, e, method, pc);
    *out = (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, s, NULL};
    return true;
}

static bool text(mc_client_source_fj *o, mc_client_screen_text value, mc_player_string_view *out,
                 const char *method, unsigned int pc, mc_client_screen_error *e) {
    if (value.kind == MC_CLIENT_SCREEN_TEXT_NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ actual String receiver",
                    method, pc, false);
    return mc_client_screen_text_read(value, out) ||
           fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "FJ actual UTF16 String view", method, pc,
                true);
}

static bool new_string(mc_client_source_fj *o, const uint16_t *units, size_t count,
                       mc_client_screen_text *out, const char *method, unsigned int pc,
                       mc_client_screen_error *e) {
    mc_create_world_string *s = mc_screen_allocate(&o->base.memory, sizeof(*s));
    if (s == NULL)
        return fail(o, e, o->base.memory.allocation_status, "FJ retained String result", method, pc,
                    true);
    e->string = mc_player_string_create(units, count, &s->value);
    if (e->string != MC_PLAYER_OK)
        return fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "FJ actual String result", method, pc,
                    true);
    s->next = o->strings;
    o->strings = s;
    *out = (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, s->value, NULL};
    return true;
}

static bool folder_store(mc_client_source_fj *o, mc_client_screen_text value, unsigned int pc,
                         mc_client_screen_error *e) {
    mc_player_string *held =
        value.kind == MC_CLIENT_SCREEN_TEXT_PLAYER ? (mc_player_string *)value.player : NULL;
    if (held != NULL && mc_player_string_retain(held) != MC_PLAYER_OK)
        return fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "FJ folder String occurrence", "k", pc,
                    true);
    mc_player_string *old = o->folder_hold;
    o->fields.l = value;
    o->folder_hold = held;
    mc_player_string_release(old);
    reached(o, "k", pc);
    return true;
}

mc_client_source_fj *mc_screen_create_world_allocate_locked(mc_client_screen_classes *c,
                                                            const mc_client_screen_options *options,
                                                            mc_client_screen_error *e) {
    if (c == NULL || c->retiring || c->holds == SIZE_MAX ||
        (options != NULL && options->maximum_bytes < sizeof(mc_client_source_fj))) {
        fail(NULL, e,
             c == NULL     ? MC_CLIENT_SCREEN_INVALID_ARGUMENT
             : c->retiring ? MC_CLIENT_SCREEN_BUSY
                           : MC_CLIENT_SCREEN_LIMIT,
             "FJ native namespace/owner budget", "<init>", 0, true);
        return NULL;
    }
    mc_client_source_fj *o = calloc(1, sizeof(*o));
    if (o == NULL) {
        fail(NULL, e, MC_CLIENT_SCREEN_ALLOCATION_FAILED, "FJ RAW owner allocation", "<init>", 0,
             true);
        return NULL;
    }
    o->base.classes = c;
    o->base.holds = 1;
    o->base.kind = MC_CLIENT_SCREEN_CREATE_WORLD;
    mc_screen_memory_init(&o->base.memory, options, sizeof(*o));
    ++c->holds;
    return o;
}

bool mc_screen_create_world_construct_locked(mc_client_source_fj *o, mc_client_source_da *parent,
                                             mc_client_screen_error *e) {
    if (o == NULL || o->base.phase != MC_CLIENT_SCREEN_RAW || o->base.classes->retiring)
        return fail(o, e, MC_CLIENT_SCREEN_INVALID_PHASE, "FJ one-shot constructor", "<init>", 0,
                    true);
    o->base.phase = MC_CLIENT_SCREEN_CONSTRUCTING;
    bool success = mc_screen_base_construct_locked(&o->base, e);
    if (!success)
        child(o, e, "<init>", 1);
    else if (parent != NULL && parent->classes != o->base.classes)
        success = fail(o, e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "FJ actual parent namespace",
                       "<init>", 6, true);
    else if (parent != NULL && !mc_screen_instance_hold_locked(parent, e))
        success = child(o, e, "<init>", 6);
    else {
        o->parent_held = parent != NULL;
        o->fields.a = parent;
        mc_screen_store(&o->base, "fj", 6);
        returned(o, e, "<init>", 9);
    }
    o->base.phase = success ? MC_CLIENT_SCREEN_READY : MC_CLIENT_SCREEN_FAILED;
    if (!success)
        o->base.failure = o->failure = *e;
    return success;
}

static bool translation(mc_client_source_fj *o, const mc_translation **out,
                        mc_client_screen_error *e) {
    mc_registry_classes *r = o->base.classes->registry;
    if (r == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "FJ actual NH namespace", "b",
                    0, true);
    if (mc_registry_classes_initialize_translation_singleton(r, &e->registry) != MC_REGISTRY_OK)
        return fail(o, e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "FJ actual NH active use", "b", 0,
                    true);
    mc_registry_translation_view v;
    if (mc_registry_classes_inspect_translation(r, &v, &e->registry) != MC_REGISTRY_OK)
        return fail(o, e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "FJ actual NH singleton", "b", 0, true);
    *out = v.singleton_a;
    return true;
}

static bool translated(mc_client_source_fj *o, const mc_translation *owner, const char *key,
                       unsigned int literal_pc, unsigned int pc, mc_client_screen_text *out,
                       mc_client_screen_error *e) {
    mc_client_screen_text argument;
    if (!literal(o, key, &argument, "b", literal_pc, e))
        return false;
    if (owner == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ null NH property receiver",
                    "b", pc, false);
    mc_player_string_view view;
    if (!text(o, argument, &view, "b", pc, e))
        return false;
    const mc_registry_string *value = NULL;
    if (mc_translation_property(owner, view.units, view.length, &value, &e->registry) !=
        MC_REGISTRY_OK)
        return fail(o, e, MC_CLIENT_SCREEN_REGISTRY_FAILED, "FJ actual NH property", "b", pc, true);
    *out = value == NULL ? argument
                         : (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_REGISTRY, NULL, value};
    return true;
}

static bool repeat(mc_client_source_fj *o, bool enabled, mc_client_screen_error *e) {
    const char *method = enabled ? "b" : "h";
    const unsigned int pc = enabled ? 5u : 1u;
    client_application *app = o->base.fields.b;
    mc_client_sdk_input *input = app == NULL ? NULL : mc_client_display_inputs(app->display);
    if (mc_client_sdk_keyboard_enable_repeat(input, enabled, &o->input) != MC_CLIENT_SDK_INPUT_OK)
        return fail(o, e, MC_CLIENT_SCREEN_INPUT_FAILED, o->input.operation, method, pc,
                    o->input.native_policy);
    return true;
}

static bool clear(mc_client_source_fj *o, mc_client_screen_error *e) {
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    if (list == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ actual List.clear receiver",
                    "b", 12, false);
    for (size_t i = 0; i < list->size; ++i) {
        mc_client_source_ke *button = list->entries[i];
        if (button != NULL && (button->classes != o->base.classes || button->holds < 2))
            return fail(o, e, MC_CLIENT_SCREEN_INVALID_PHASE, "FJ native Button List occurrence",
                        "b", 12, true);
    }
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    const size_t count = list->size;
    list->size = 0;
    for (size_t i = 0; i < count; ++i) {
        mc_client_source_ke *button = list->entries[i];
        list->entries[i] = NULL;
        if (!mc_screen_button_release_locked(&button, e))
            return child(o, e, "b", 12);
    }
    return true;
}

static bool button(mc_client_source_fj *o, mc_create_world_initialization *r,
                   const mc_translation *translation_owner, int32_t id, const char *key,
                   mc_client_screen_error *e) {
    const unsigned int new_pc = id == 0 ? 21u : 66u, ctor = id == 0 ? 53u : 98u,
                       add_pc = id == 0 ? 56u : 101u;
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    mc_client_source_ke *b = mc_screen_button_allocate_locked(o->base.classes, e);
    if (b == NULL)
        return child(o, e, "b", new_pc);
    r->buttons[r->button_count++] = b;
    const int32_t x = mc_java_i32_sub(o->base.fields.c / 2, 100);
    const int32_t y =
        mc_java_i32_add(mc_java_i32_add(o->base.fields.d / 4, id == 0 ? 96 : 120), 12);
    mc_client_screen_text label;
    if (!translated(o, translation_owner, key, id == 0 ? 48u : 93u, id == 0 ? 50u : 95u, &label, e))
        return false;
    if (!mc_screen_button_construct_locked(b, id, x, y, 200, 20, label, e))
        return child(o, e, "b", ctor);
    if (list == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ actual List.add receiver",
                    "b", add_pc, false);
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    if (list->size >= (size_t)INT32_MAX)
        return fail(o, e, MC_CLIENT_SCREEN_LIMIT, "FJ native List size after modCount", "b", add_pc,
                    true);
    if (list->size == list->capacity) {
        size_t next = list->capacity == 0 ? 10u : list->capacity + list->capacity / 2u;
        if (next <= list->size || next > (size_t)INT32_MAX ||
            next > SIZE_MAX / sizeof(*list->entries))
            return fail(o, e, MC_CLIENT_SCREEN_LIMIT, "FJ native List capacity", "b", add_pc, true);
        void **values = mc_screen_allocate(&o->base.memory, next * sizeof(*values));
        if (values == NULL)
            return fail(o, e, o->base.memory.allocation_status, "FJ native List growth", "b",
                        add_pc, true);
        if (list->size != 0)
            memcpy(values, list->entries, list->size * sizeof(*values));
        list->entries = values;
        list->capacity = next;
    }
    if (!mc_screen_button_hold_locked(b, e))
        return child(o, e, "b", add_pc);
    list->entries[list->size++] = b;
    return true;
}

bool mc_screen_create_world_initialize_locked(mc_client_source_fj *o, mc_client_screen_error *e) {
    if (o == NULL || o->base.phase != MC_CLIENT_SCREEN_READY || o->base.classes->retiring ||
        o->base.active_calls != 0)
        return fail(o, e, MC_CLIENT_SCREEN_INVALID_PHASE, "FJ initialization owner", "b", 0, true);
    mc_create_world_initialization *r = mc_screen_allocate(&o->base.memory, sizeof(*r));
    if (r == NULL)
        return fail(o, e, o->base.memory.allocation_status, "FJ retained initialization", "b", 0,
                    true);
    if (o->last_initialization == NULL)
        o->initializations = r;
    else
        o->last_initialization->next = r;
    o->last_initialization = r;
    ++o->initialization_count;
    const mc_translation *translations;
    if (!translation(o, &translations, e) || !repeat(o, true, e) || !clear(o, e) ||
        !button(o, r, translations, 0, "selectWorld.create", e) ||
        !button(o, r, translations, 1, "gui.cancel", e))
        goto failed;
    r->name = mc_screen_text_field_allocate_locked(&o->base, e);
    if (r->name == NULL) {
        child(o, e, "b", 108);
        goto failed;
    }
    mc_client_bitmap_font *font = o->base.fields.g;
    const int32_t x = mc_java_i32_sub(o->base.fields.c / 2, 100);
    mc_client_screen_text name;
    if (!translated(o, translations, "selectWorld.newWorld", 134, 136, &name, e))
        goto failed;
    if (!mc_screen_text_field_construct_locked(r->name, &o->base, font, x, 60, 200, 20, name, e)) {
        child(o, e, "b", 139);
        goto failed;
    }
    o->fields.i = r->name;
    reached(o, "b", 142);
    o->fields.i->fields.a = true; /* Actual raw focus store150, no RO.a(Z) call. */
    if (!mc_screen_text_field_set_maximum_locked(o->fields.i, 32, e)) {
        child(o, e, "b", 159);
        goto failed;
    }
    r->seed = mc_screen_text_field_allocate_locked(&o->base, e);
    if (r->seed == NULL) {
        child(o, e, "b", 163);
        goto failed;
    }
    font = o->base.fields.g;
    const int32_t seed_x = mc_java_i32_sub(o->base.fields.c / 2, 100);
    mc_client_screen_text empty;
    if (!literal(o, "", &empty, "b", 188, e))
        goto failed;
    if (!mc_screen_text_field_construct_locked(r->seed, &o->base, font, seed_x, 116, 200, 20, empty,
                                               e)) {
        child(o, e, "b", 190);
        goto failed;
    }
    o->fields.j = r->seed;
    reached(o, "b", 193);
    if (!mc_screen_create_world_name_locked(o, e)) {
        child(o, e, "b", 197);
        goto failed;
    }
    r->returned = true;
    return returned(o, e, "b", 200);
failed:
    r->failure = o->failure = *e;
    return false;
}

static bool mapping(mc_client_source_fj *o, mc_client_screen_error *e) {
    /* Resolve a native capability to this same already existing GUI class
     * namespace, without constructing a substitute FP or reading its statics. */
    if (o->mapping == NULL && o->base.fields.g != NULL && o->base.fields.g->gui != NULL)
        o->mapping = o->base.fields.g->gui->mapping;
    if (o->mapping == NULL && o->base.fields.b != NULL &&
        o->base.fields.b->loading_screen != NULL) {
        mc_client_loading_classes *gui =
            mc_client_loading_screen_classes(o->base.fields.b->loading_screen);
        if (gui != NULL)
            o->mapping = gui->mapping;
    }
    const mc_player_string *unused = NULL;
    if (o->mapping == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY, "FJ actual FP class namespace",
                    "k", 14, true);
    if (mc_client_font_mapping_active_use(o->mapping, &unused, &o->mapping_error) !=
        MC_CLIENT_FONT_MAPPING_OK)
        return fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, o->mapping_error.operation, "k", 14,
                    true);
    return true;
}

static bool unique_failure(mc_client_source_fj *o, mc_client_screen_error *e,
                           mc_client_screen_status status, const char *operation, unsigned int pc,
                           bool native) {
    (void)fail(o, e, status, operation, "k", 82, native);
    e->parent_site = e->site;
    e->site = (mc_client_screen_site){"fj", "a", "(Lnl;Ljava/lang/String;)Ljava/lang/String;", pc};
    o->failure = *e;
    return false;
}

static bool unique(mc_client_source_fj *o, mc_client_screen_text input, mc_client_save_format *f,
                   mc_client_screen_text *out, mc_client_screen_error *e) {
    mc_create_world_naming *r = mc_screen_allocate(&o->base.memory, sizeof(*r));
    if (r == NULL)
        return fail(o, e, o->base.memory.allocation_status, "FJ retained naming invocation", "k",
                    82, true);
    r->local = input;
    if (o->last_naming == NULL)
        o->namings = r;
    else
        o->last_naming->next = r;
    o->last_naming = r;
    ++o->naming_calls;
    if (f == NULL)
        return unique_failure(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE,
                              "FJ actual NL metadata receiver", 2, false);
    if (mc_client_save_format_retain(f, &o->save) != MC_CLIENT_SAVE_OK)
        return fail(o, e, MC_CLIENT_SCREEN_APP_FAILED, "FJ metadata Format occurrence", "k", 82,
                    true);
    r->format = f;
    for (;;) {
        mc_create_world_query *query = mc_screen_allocate(&o->base.memory, sizeof(*query));
        if (query == NULL)
            return fail(o, e, o->base.memory.allocation_status, "FJ metadata query receipt", "k",
                        82, true);
        query->next = r->queries;
        r->queries = query;
        mc_player_string_view view;
        if (!text(o, r->local, &view, "k", 82, e))
            return false;
        if (mc_world_info_name_copy((mc_nbt_string){view.units, view.length}, SIZE_MAX,
                                    &query->argument) != MC_WORLD_INFO_OK)
            return fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "FJ metadata String native adapter",
                        "k", 82, true);
        ++o->metadata_queries;
        if (mc_client_save_format_read_info(f, query->argument, &query->info, &o->save) !=
            MC_CLIENT_SAVE_OK)
            return unique_failure(o, e, MC_CLIENT_SCREEN_APP_FAILED,
                                  "FJ actual NL.b metadata lookup", 2,
                                  o->save.status != MC_CLIENT_SAVE_SOURCE_NULL_REFERENCE);
        if (query->info == NULL) {
            *out = r->local;
            return true;
        }
        if (view.length == SIZE_MAX || view.length + 1 > SIZE_MAX / sizeof(uint16_t))
            return fail(o, e, MC_CLIENT_SCREEN_LIMIT, "FJ naming String length", "k", 82, true);
        uint16_t *units = mc_screen_allocate(&o->base.memory, (view.length + 1) * sizeof(*units));
        if (units == NULL)
            return fail(o, e, o->base.memory.allocation_status, "FJ appended naming String", "k",
                        82, true);
        if (view.length != 0)
            memcpy(units, view.units, view.length * sizeof(*units));
        units[view.length] = '-';
        if (!new_string(o, units, view.length + 1, &r->local, "k", 82, e))
            return false;
    }
}

bool mc_screen_create_world_name_locked(mc_client_source_fj *o, mc_client_screen_error *e) {
    if (o->fields.i == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ name RO null receiver", "k",
                    5, false);
    mc_client_screen_text value = o->fields.i->fields.h;
    mc_player_string_view view;
    if (!text(o, value, &view, "k", 8, e))
        return false;
    size_t begin = 0, end = view.length;
    while (begin < end && view.units[begin] <= 32)
        ++begin;
    while (end > begin && view.units[end - 1] <= 32)
        --end;
    if (begin != 0 || end != view.length) {
        if (begin == end)
            value = (mc_client_screen_text){MC_CLIENT_SCREEN_TEXT_PLAYER, mc_player_empty_literal(),
                                            NULL};
        else if (!new_string(o, view.units + begin, end - begin, &value, "k", 8, e))
            return false;
    }
    if (!folder_store(o, value, 11, e) || !mapping(o, e))
        return false;
    mc_client_font_mapping_state state;
    if (!mc_client_font_mapping_inspect(o->mapping, &state) || state.b == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_STRING_FAILED, "FJ actual FP.b char array", "k", 19,
                    true);
    for (size_t index = 0; index < 15; ++index) {
        value = o->fields.l;
        if (!text(o, value, &view, "k", 42, e))
            return false;
        const uint16_t prohibited = state.b[index];
        bool changed = false;
        for (size_t i = 0; i < view.length; ++i)
            changed |= view.units[i] == prohibited;
        if (changed) {
            uint16_t *units = mc_screen_allocate(&o->base.memory, view.length * sizeof(*units));
            if (units == NULL)
                return fail(o, e, o->base.memory.allocation_status, "FJ replace String storage",
                            "k", 42, true);
            for (size_t i = 0; i < view.length; ++i)
                units[i] = view.units[i] == prohibited ? '_' : view.units[i];
            if (!new_string(o, units, view.length, &value, "k", 42, e))
                return false;
        }
        if (!folder_store(o, value, 45, e))
            return false;
    }
    if (!text(o, o->fields.l, &view, "k", 58, e))
        return false;
    if (view.length == 0) {
        if (!literal(o, "World", &value, "k", 65, e) || !folder_store(o, value, 67, e))
            return false;
    }
    client_application *app = o->base.fields.b;
    if (app == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ actual Minecraft.c receiver",
                    "k", 75, false);
    if (!unique(o, o->fields.l, app->aa, &value, e) || !folder_store(o, value, 85, e))
        return false;
    return returned(o, e, "k", 88);
}

bool mc_screen_create_world_tick_locked(mc_client_source_fj *o, mc_client_screen_error *e) {
    if (!mc_screen_text_field_tick_locked(o->fields.i, e))
        return child(o, e, "a", 4);
    if (!mc_screen_text_field_tick_locked(o->fields.j, e))
        return child(o, e, "a", 11);
    return returned(o, e, "a", 14);
}

bool mc_screen_create_world_closed_locked(mc_client_source_fj *o, mc_client_screen_error *e) {
    return repeat(o, false, e) && returned(o, e, "h", 4);
}

bool mc_screen_create_world_tab_locked(mc_client_source_fj *o, mc_client_screen_error *e) {
    if (o->fields.i == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ tab name RO receiver", "j", 4,
                    false);
    const bool focused = o->fields.i->fields.a;
    if (!mc_screen_text_field_focus_locked(o->fields.i, !focused, e))
        return child(o, e, "j", focused ? 15u : 34u);
    if (!mc_screen_text_field_focus_locked(o->fields.j, focused, e))
        return child(o, e, "j", focused ? 23u : 42u);
    return returned(o, e, "j", 45);
}

typedef struct key_bridge {
    mc_client_source_fj *owner;
    const mc_client_text_field_hooks *hooks;
} key_bridge;

static bool tab(void *context, mc_client_source_da *parent, mc_client_screen_error *e) {
    key_bridge *b = context;
    if (parent != &b->owner->base)
        return fail(b->owner, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                    "FJ actual RO parent.j receiver", "key", 16, true);
    return mc_screen_create_world_tab_locked(b->owner, e);
}

static bool clipboard(void *context, mc_player_string **out, mc_client_screen_error *e) {
    key_bridge *b = context;
    if (b->hooks == NULL || b->hooks->clipboard == NULL)
        return fail(b->owner, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                    "FJ actual DA clipboard bridge", "key", 16, true);
    return b->hooks->clipboard(b->hooks->context, out, e);
}

static mc_client_source_ke *first(mc_client_source_fj *o, unsigned int pc,
                                  mc_client_screen_error *e) {
    mc_client_screen_list *list = (mc_client_screen_list *)o->base.fields.e;
    if (list == NULL || list->size == 0 || list->entries == NULL) {
        fail(o, e,
             list == NULL ? MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE : MC_CLIENT_SCREEN_SOURCE_INDEX,
             "FJ actual Button List.get(0)", "key", pc, false);
        return NULL;
    }
    mc_client_source_ke *button_owner = list->entries[0];
    if (button_owner == NULL)
        fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ null first Button receiver", "key",
             pc == 43 ? 51u : 85u, false);
    return button_owner;
}

bool mc_screen_create_world_key_locked(mc_client_source_fj *o, uint16_t character, int32_t key,
                                       const mc_client_text_field_hooks *hooks,
                                       mc_create_world_enter_fn enter, void *context,
                                       mc_client_screen_error *e) {
    if (o->fields.i == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ key name focus receiver",
                    "key", 4, false);
    const bool focused = o->fields.i->fields.a;
    key_bridge bridge = {o, hooks};
    const mc_client_text_field_hooks actual = {hooks == NULL ? o->mapping : hooks->mapping, &bridge,
                                               tab, clipboard};
    if (!mc_screen_text_field_key_locked(focused ? o->fields.i : o->fields.j, character, key,
                                         &actual, e))
        return child(o, e, "key", focused ? 16u : 28u);
    if (character == 13) {
        mc_client_source_ke *b = first(o, 43, e);
        if (b == NULL)
            return false;
        if (enter == NULL)
            return fail(o, e, MC_CLIENT_SCREEN_UNIMPLEMENTED_DEPENDENCY,
                        "FJ actual synchronous Button action bridge", "key", 51, true);
        if (!enter(context, o, b, e))
            return child(o, e, "key", 51);
    }
    mc_client_source_ke *b = first(o, 59, e);
    if (b == NULL)
        return false;
    if (o->fields.i == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ key RO text receiver", "key",
                    71, false);
    mc_player_string_view view;
    if (!text(o, o->fields.i->fields.h, &view, "key", 74, e))
        return false;
    b->fields.g = view.length > 0;
    reached(o, "key", 85);
    if (!mc_screen_create_world_name_locked(o, e))
        return child(o, e, "key", 89);
    return returned(o, e, "key", 92);
}

bool mc_screen_create_world_mouse_after_base_locked(mc_client_source_fj *o, int32_t x, int32_t y,
                                                    int32_t button_value,
                                                    mc_client_screen_error *e) {
    if (!mc_screen_text_field_mouse_locked(o->fields.i, x, y, button_value, e))
        return child(o, e, "mouse", 14);
    if (!mc_screen_text_field_mouse_locked(o->fields.j, x, y, button_value, e))
        return child(o, e, "mouse", 24);
    return returned(o, e, "mouse", 27);
}

bool mc_screen_create_world_seed_locked(mc_client_source_fj *o, int64_t *out,
                                        mc_client_screen_error *e) {
    if (out == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "FJ seed native output", "button", 58,
                    true);
    mc_create_world_seed *r = mc_screen_allocate(&o->base.memory, sizeof(*r));
    if (r == NULL)
        return fail(o, e, o->base.memory.allocation_status, "FJ actual Random native storage",
                    "button", 58, true);
    if (o->last_seed == NULL)
        o->seeds = r;
    else
        o->last_seed->next = r;
    o->last_seed = r;
    reached(o, "button", 62);
    e->random = mc_java_random_construct_default(o->base.classes->profile, &r->random);
    if (e->random != MC_JAVA_RANDOM_PROFILE_OK)
        return fail(o, e, MC_CLIENT_SCREEN_RANDOM_FAILED, "FJ actual new Random constructor",
                    "button", 62, true);
    r->constructed = true;
    r->initial = r->selected = mc_java_random_next_i64(&r->random);
    r->drawn = true;
    if (o->fields.j == NULL)
        return fail(o, e, MC_CLIENT_SCREEN_SOURCE_NULL_REFERENCE, "FJ seed RO receiver", "button",
                    73, false);
    mc_client_screen_text seed = o->fields.j->fields.h;
    /* IN.a(String) accepts NULL and empty before Long.parseLong. */
    if (seed.kind != MC_CLIENT_SCREEN_TEXT_NULL) {
        mc_player_string_view view;
        if (!text(o, seed, &view, "button", 80, e))
            return false;
        if (view.length != 0) {
            const mc_java_integer_parse_text argument = {view.units, view.length};
            int64_t parsed = 0;
            const mc_java_integer_parse_status status = mc_java_long_parse(&argument, &parsed);
            if (status == MC_JAVA_INTEGER_PARSE_OK) {
                r->parsed = true;
                if (parsed != 0)
                    r->selected = parsed;
            } else if (status == MC_JAVA_INTEGER_PARSE_NUMBER_FORMAT) {
                uint32_t hash = 0;
                for (size_t i = 0; i < view.length; ++i)
                    hash = hash * UINT32_C(31) + view.units[i];
                const int32_t signed_hash =
                    hash <= INT32_MAX ? (int32_t)hash : -1 - (int32_t)(UINT32_MAX - hash);
                r->selected = signed_hash;
                r->hashed = true;
            } else
                return fail(o, e, MC_CLIENT_SCREEN_LIMIT, "FJ Long.parseLong native text bound",
                            "button", 88, true);
        }
    }
    *out = r->selected;
    /* This is an entered method segment ending before the real NEW OS119. */
    reached(o, "button", 115);
    return true;
}

bool mc_screen_create_world_cleanup_locked(mc_client_source_fj *o, mc_client_screen_error *e) {
    for (mc_create_world_initialization *r = o->initializations; r != NULL; r = r->next) {
        if (!mc_screen_text_field_release_locked(&r->name, e) ||
            !mc_screen_text_field_release_locked(&r->seed, e))
            return false;
        for (unsigned int i = 0; i < r->button_count; ++i)
            if (!mc_screen_button_release_locked(&r->buttons[i], e))
                return false;
    }
    o->fields.i = o->fields.j = NULL;
    for (mc_create_world_naming *r = o->namings; r != NULL; r = r->next) {
        if (r->format != NULL &&
            mc_client_save_format_release(&r->format, &o->save) != MC_CLIENT_SAVE_OK)
            return fail(o, e, MC_CLIENT_SCREEN_BUSY, "FJ retained metadata Format retirement", "k",
                        82, true);
        for (mc_create_world_query *q = r->queries; q != NULL; q = q->next) {
            mc_world_info_release(q->info);
            q->info = NULL;
            mc_world_info_name_release(q->argument);
            q->argument = NULL;
        }
    }
    mc_player_string_release(o->folder_hold);
    o->folder_hold = NULL;
    o->fields.l = (mc_client_screen_text){0};
    for (mc_create_world_string *s = o->strings; s != NULL; s = s->next) {
        mc_player_string_release(s->value);
        s->value = NULL;
    }
    if (o->parent_held) {
        if (!mc_screen_instance_release_locked(&o->fields.a, e))
            return false;
        o->parent_held = false;
    }
    return true;
}

mc_client_screen_status
mc_client_screen_create_world_allocate(mc_client_screen_classes *c,
                                       const mc_client_screen_options *options,
                                       mc_client_source_fj **out, mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (out == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "FJ allocation output", "<init>", 0,
             true);
    else if (mc_screen_lock(c, &e)) {
        mc_client_source_fj *o = mc_screen_create_world_allocate_locked(c, options, &e);
        if (o != NULL)
            *out = o;
        (void)mtx_unlock(&c->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_create_world_construct(mc_client_source_fj *o,
                                                                mc_client_source_da *parent,
                                                                mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "FJ constructor receiver", "<init>", 0,
             true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        (void)mc_screen_create_world_construct_locked(o, parent, &e);
        (void)mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_source_da *mc_client_screen_create_world_base(mc_client_source_fj *o) {
    return o == NULL ? NULL : &o->base;
}

mc_client_screen_status mc_client_screen_create_world_initialize(mc_client_source_fj *o,
                                                                 mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "FJ initialization receiver", "b", 0,
             true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        (void)mc_screen_create_world_initialize_locked(o, &e);
        (void)mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

mc_client_screen_status mc_client_screen_create_world_inspect(mc_client_source_fj *o,
                                                              mc_client_create_world_state *out,
                                                              mc_client_screen_error *error) {
    mc_client_screen_error e = {0};
    if (o == NULL || out == NULL)
        fail(NULL, &e, MC_CLIENT_SCREEN_INVALID_ARGUMENT, "FJ inspection output", "<init>", 0,
             true);
    else if (mc_screen_lock(o->base.classes, &e)) {
        *out = (mc_client_create_world_state){.phase = o->base.phase,
                                              .holds = o->base.holds,
                                              .initialization_count = o->initialization_count,
                                              .naming_calls = o->naming_calls,
                                              .metadata_queries = o->metadata_queries,
                                              .fields = o->fields,
                                              .method = o->method,
                                              .last_pc = o->last_pc,
                                              .failure = &o->failure,
                                              .save = &o->save};
        if (o->last_seed != NULL) {
            out->random_constructed = o->last_seed->constructed;
            out->random_drawn = o->last_seed->drawn;
            out->seed_parsed = o->last_seed->parsed;
            out->seed_hashed = o->last_seed->hashed;
            out->default_seed = o->last_seed->initial;
            out->selected_seed = o->last_seed->selected;
        }
        (void)mtx_unlock(&o->base.classes->mutex);
    }
    return mc_screen_publish(&e, error);
}

static mc_client_screen_status release(mc_client_source_fj **slot, bool strict,
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

mc_client_screen_status mc_client_screen_create_world_release(mc_client_source_fj **o,
                                                              mc_client_screen_error *e) {
    return release(o, false, e);
}

mc_client_screen_status mc_client_screen_create_world_retire(mc_client_source_fj **o,
                                                             mc_client_screen_error *e) {
    return release(o, true, e);
}
