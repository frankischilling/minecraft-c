#include "gui_ingame_chat.h"
#include "../application_state.h"
#include "block/registry/registry_internal.h"
#include "block/registry/runtime_internal.h"
#include "gui_ingame_chat_internal.h"
#include "mc/block/registry/runtime.h"
#include "mc/core/java/java.h"
#include <stdlib.h>
#include <string.h>

typedef struct chat_temporary {
    struct chat_temporary *next;
    mc_player_string *value;
} chat_temporary;

struct mc_client_gui_ingame_chat_call {
    mc_client_source_uq *owner;
    mc_registry_classes *registry;
    const mc_player_string *argument;
    mc_registry_classes *argument_registry;
    const mc_registry_string *registry_argument;
    bool argument_registry_held;
    bool registry_held, active, translating;
    mc_client_gui_ingame_chat_state state;
    mc_client_gui_ingame_chat_error failure, cleanup;
    mc_client_bitmap_font *fonts[64];
    size_t font_count;
    chat_temporary *temporaries;
    mc_gui_chat_message *pending;
};

static const char *const descriptor = "(Ljava/lang/String;)V";

static mc_client_gui_ingame_chat_status publish(mc_client_gui_ingame_chat_error *out,
                                                const mc_client_gui_ingame_chat_error *value) {
    if (out != NULL)
        *out = *value;
    return value->status;
}

static bool fail(mc_client_gui_ingame_chat_call *c, mc_client_gui_ingame_chat_status status,
                 const char *operation, const char *method, unsigned int pc, bool native) {
    c->failure.status = status;
    c->failure.operation = operation;
    c->failure.site = (mc_client_gui_ingame_site){"uq", method, descriptor, pc};
    c->failure.native_policy = native;
    c->state.source_pc = pc;
    return false;
}

static bool translation_native_policy(const mc_registry_classes *registry,
                                      mc_registry_status status, const mc_registry_error *error) {
    const mc_registry_error *cause = error;
    if (status == MC_REGISTRY_FAILED_INITIALIZATION &&
        registry->translation_state.phase == MC_REGISTRY_FAILED)
        cause = &registry->translation_state.failure;
    return !(cause->status == MC_REGISTRY_NULL_REFERENCE && cause->constructor_class != NULL &&
             strcmp(cause->constructor_class, "nh") == 0 &&
             (cause->constructor_pc == 26 || cause->constructor_pc == 40) &&
             cause->active_initializer_class != NULL &&
             strcmp(cause->active_initializer_class, "nh") == 0 &&
             cause->active_initializer_pc == 4);
}

static void *allocate(mc_client_gui_ingame_chat_call *c, size_t bytes, unsigned int pc) {
    mc_client_source_uq *o = c->owner;
    ++o->attempts;
    if (o->allocations >= o->options.maximum_allocations || o->bytes > o->options.maximum_bytes ||
        bytes > o->options.maximum_bytes - o->bytes) {
        fail(c, MC_CLIENT_GUI_INGAME_CHAT_LIMIT, "GuiIngame native chat allocation quota", "a", pc,
             true);
        return NULL;
    }
    void *value = calloc(1, bytes);
    if (value == NULL) {
        fail(c, MC_CLIENT_GUI_INGAME_CHAT_ALLOCATION, "GuiIngame native chat allocation", "a", pc,
             true);
        return NULL;
    }
    ++o->allocations;
    o->bytes += bytes;
    return value;
}

static bool measure(mc_client_gui_ingame_chat_call *c, const mc_player_string_view *text,
                    unsigned int app_pc, unsigned int pc, int32_t *out) {
    client_application *app = c->owner->fields.g;
    if (app == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL, "UQ actual App.q null receiver", "a",
                    app_pc, false);
    mc_client_bitmap_font *font = app->font_renderer;
    if (font == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL,
                    "UQ actual Font.measure null receiver", "a", pc, false);
    size_t i = 0;
    while (i < c->font_count && c->fonts[i] != font)
        ++i;
    if (i == c->font_count) {
        if (i == sizeof(c->fonts) / sizeof(c->fonts[0]))
            return fail(c, MC_CLIENT_GUI_INGAME_CHAT_LIMIT, "UQ native captured Font bound", "a",
                        pc, true);
        if (mc_client_bitmap_font_retain(font) != MC_CLIENT_BITMAP_FONT_OK)
            return fail(c, MC_CLIENT_GUI_INGAME_CHAT_BUSY, "UQ native Font occurrence", "a", pc,
                        true);
        c->fonts[c->font_count++] = font;
    }
    const size_t length = text == NULL ? 0 : text->length;
    if (length > 16777216u - c->state.scanned_units)
        return fail(c, MC_CLIENT_GUI_INGAME_CHAT_LIMIT, "UQ native measured UTF16 work bound", "a",
                    pc, true);
    c->state.scanned_units += length;
    ++c->state.widths;
    if (mc_client_bitmap_font_measure_view(font, text, out, &c->failure.font) !=
        MC_CLIENT_BITMAP_FONT_OK) {
        c->failure.child =
            (mc_client_gui_ingame_site){"sj", c->failure.font.method, c->failure.font.descriptor,
                                        (unsigned int)c->failure.font.pc};
        return fail(c, MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED, c->failure.font.operation, "a", pc,
                    c->failure.font.native_policy);
    }
    return true;
}

/* Temporary substring views used only by sj measurement retain the original
 * immutable parent. Escaping substring results get distinct canonical String
 * owners, except the JDK17 full-range and empty substring identities. */
static bool substring(mc_client_gui_ingame_chat_call *c, mc_gui_chat_text text, size_t begin,
                      size_t end, unsigned int pc, mc_gui_chat_text *out) {
    mc_player_string_view view;
    if (!mc_gui_chat_text_read(text, &view) || begin > end || end > view.length)
        return fail(c, MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT, "UQ native substring source invariant",
                    "a", pc, true);
    ++c->state.substrings;
    if (begin == 0 && end == view.length) {
        *out = text;
        return true;
    }
    if (begin == end) {
        *out = (mc_gui_chat_text){.player = mc_player_empty_literal()};
        return true;
    }
    chat_temporary *temporary = allocate(c, sizeof(*temporary), pc);
    if (temporary == NULL)
        return false;
    temporary->next = c->temporaries;
    c->temporaries = temporary;
    c->failure.string = mc_player_string_create(view.units + begin, end - begin, &temporary->value);
    if (c->failure.string != MC_PLAYER_OK)
        return fail(c, MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED, "UQ actual substring String owner",
                    "a", pc, true);
    *out = (mc_gui_chat_text){.player = temporary->value};
    return true;
}

static bool list_valid(mc_client_gui_ingame_chat_call *c, mc_client_gui_ingame_list *list,
                       unsigned int pc) {
    if (list == NULL)
        return fail(c, MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL, "UQ actual chat List null receiver",
                    "a", pc, false);
    if (list->size > list->capacity || list->size > INT32_MAX ||
        (list->capacity != 0 && list->entries == NULL))
        return fail(c, MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT, "UQ native List storage invariant", "a",
                    pc, true);
    return true;
}

static bool insert(mc_client_gui_ingame_chat_call *c, mc_gui_chat_text text) {
    /* Captured e77 may be null: NEWsw81 and constructor86 precede List.add89. */
    mc_client_gui_ingame_list *list = c->owner->fields.e;
    mc_gui_chat_message *message = allocate(c, sizeof(*message), 81);
    if (message == NULL)
        return false;
    c->pending = message;
    if (text.registry != NULL) {
        c->failure.registry.status = mc_registry_classes_retain(
            text.registry_owner, MC_REGISTRY_CLIENT, text.registry_owner->options.random_profile,
            &c->failure.registry);
        if (c->failure.registry.status != MC_REGISTRY_OK)
            return fail(c, MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED,
                        "SW native actual translated String namespace occurrence", "a", 86, true);
        message->registry_held = true;
    } else if (text.player != NULL) {
        c->failure.string = mc_player_string_retain((mc_player_string *)text.player);
        if (c->failure.string != MC_PLAYER_OK)
            return fail(c, MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED,
                        "SW native actual String occurrence", "a", 86, true);
    }
    message->a = text; /* Actual SW.a6, then b0 store11. */
    message->b = 0;
    if (!list_valid(c, list, 89))
        return false;
    list->mod_count = mc_java_i32_add(list->mod_count, 1);
    if (list->size == list->capacity) {
        const size_t capacity = list->capacity == 0 ? 10 : list->capacity + list->capacity / 2;
        if (capacity <= list->size || capacity > INT32_MAX ||
            capacity > SIZE_MAX / sizeof(*list->entries))
            return fail(c, MC_CLIENT_GUI_INGAME_CHAT_LIMIT, "UQ native ArrayList growth bound", "a",
                        89, true);
        void **entries = allocate(c, capacity * sizeof(*entries), 89);
        if (entries == NULL)
            return false;
        if (list->size != 0)
            memcpy(entries, list->entries, list->size * sizeof(*entries));
        c->owner->bytes -= list->capacity * sizeof(*entries);
        free(list->entries);
        list->entries = entries;
        list->capacity = capacity;
    }
    memmove(list->entries + 1, list->entries, list->size * sizeof(*list->entries));
    list->entries[0] = message;
    ++list->size;
    message->next = c->owner->messages;
    c->owner->messages = message;
    c->pending = NULL;
    ++c->state.messages;
    for (;;) {
        list = c->owner->fields.e; /* Fresh e95/98 after add89. */
        if (!list_valid(c, list, 98))
            return false;
        if (list->size <= 50)
            break;
        mc_client_gui_ingame_list *receiver = c->owner->fields.e; /* e109 */
        mc_client_gui_ingame_list *sized = c->owner->fields.e;    /* e113 */
        if (!list_valid(c, sized, 116) || !list_valid(c, receiver, 123))
            return false;
        const size_t index = sized->size - 1;
        if (index >= receiver->size)
            return fail(c, MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT, "UQ native remove index invariant",
                        "a", 123, true);
        receiver->mod_count = mc_java_i32_add(receiver->mod_count, 1);
        memmove(receiver->entries + index, receiver->entries + index + 1,
                (receiver->size - index - 1) * sizeof(*receiver->entries));
        receiver->entries[--receiver->size] = NULL;
        ++c->state.removals;
    }
    return true;
}

static bool add(mc_client_gui_ingame_chat_call *c, mc_gui_chat_text text, unsigned int depth) {
    if (depth >= 64)
        return fail(c, MC_CLIENT_GUI_INGAME_CHAT_LIMIT, "UQ native recursive prefix bound", "a", 64,
                    true);
    for (;;) {
        mc_player_string_view view = {0};
        const bool null = text.player == NULL && text.registry == NULL;
        if (!null && !mc_gui_chat_text_read(text, &view))
            return fail(c, MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT, "UQ native String view", "a", 8,
                        true);
        int32_t width;
        if (!measure(c, null ? NULL : &view, 4, 8, &width))
            return false;
        if (width <= 320)
            return insert(c, text);
        size_t split = 1;
        while (split < view.length) {
            const mc_player_string_view candidate = {view.units, split + 1};
            ++c->state.substrings; /* Actual ephemeral String.substring39. */
            if (!measure(c, &candidate, 31, 42, &width))
                return false;
            if (width > 320)
                break;
            ++split;
        }
        mc_gui_chat_text prefix, suffix;
        if (!substring(c, text, 0, split, 61, &prefix) || !add(c, prefix, depth + 1) ||
            !substring(c, text, split, view.length, 69, &suffix))
            return false;
        text = suffix;
    }
}

static mc_client_gui_ingame_chat_status
begin(mc_client_source_uq *owner, mc_registry_classes *registry, const mc_player_string *argument,
      mc_registry_classes *key_owner, const mc_registry_string *registry_key, bool translating,
      mc_client_gui_ingame_chat_call **out, mc_client_gui_ingame_chat_error *error) {
    mc_client_gui_ingame_chat_error admission = {0};
    admission.native_policy = true;
    if (owner == NULL || owner->classes == NULL || out == NULL || *out != NULL ||
        (translating && registry == NULL)) {
        admission.status = MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT;
        admission.operation = "chat native owner/namespace/output";
        return publish(error, &admission);
    }
    if (mtx_trylock(&owner->classes->mutex) != thrd_success) {
        admission.status = MC_CLIENT_GUI_INGAME_CHAT_BUSY;
        admission.operation = "chat native class monitor";
        return publish(error, &admission);
    }
    if (owner->draw_call != NULL || owner->chat_call != NULL || owner->classes->retiring ||
        owner->phase == MC_CLIENT_GUI_INGAME_CONSTRUCTING ||
        owner->phase == MC_CLIENT_GUI_INGAME_RETIRING || owner->holds == SIZE_MAX) {
        admission.status = MC_CLIENT_GUI_INGAME_CHAT_BUSY;
        admission.operation = "chat native active/retiring owner";
        (void)mtx_unlock(&owner->classes->mutex);
        return publish(error, &admission);
    }
    mc_client_gui_ingame_chat_call *c = calloc(1, sizeof(*c));
    if (c == NULL) {
        admission.status = MC_CLIENT_GUI_INGAME_CHAT_ALLOCATION;
        admission.operation = "chat native retained call";
        (void)mtx_unlock(&owner->classes->mutex);
        return publish(error, &admission);
    }
    c->owner = owner;
    c->registry = registry;
    c->translating = translating;
    c->active = true;
    c->state.failure = &c->failure;
    c->state.cleanup = &c->cleanup;
    owner->chat_call = c;
    ++owner->holds;
    *out = c;
    (void)mtx_unlock(&owner->classes->mutex);
    mc_gui_chat_text text = {.player = argument};
    if (registry_key != NULL) {
        if (argument != NULL || key_owner == NULL) {
            fail(c, MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT, "chat native literal key shape", "c", 0,
                 true);
            goto done;
        }
        c->argument_registry = key_owner;
        const mc_registry_status held = mc_registry_classes_retain(
            key_owner, MC_REGISTRY_CLIENT, key_owner->options.random_profile, &c->failure.registry);
        if (held != MC_REGISTRY_OK) {
            fail(c, MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED,
                 "chat native literal key namespace hold", "c", 0, true);
            goto done;
        }
        c->argument_registry_held = true;
        if (!mc_registry_runtime_literal_owned(key_owner, registry_key)) {
            fail(c, MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT, "chat native literal key owner/type", "c",
                 0, true);
            goto done;
        }
        c->registry_argument = registry_key;
        text = (mc_gui_chat_text){.registry = registry_key, .registry_owner = key_owner};
    }
    if (argument != NULL) {
        c->failure.string = mc_player_string_retain((mc_player_string *)argument);
        if (c->failure.string != MC_PLAYER_OK) {
            fail(c, MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED, "chat native argument String hold",
                 translating ? "c" : "a", 0, true);
            goto done;
        }
        c->argument = argument;
    }
    if (translating) {
        c->failure.registry.status = mc_registry_classes_retain(
            registry, MC_REGISTRY_CLIENT, owner->classes->profile, &c->failure.registry);
        if (c->failure.registry.status != MC_REGISTRY_OK) {
            fail(c, MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED, "chat native NH namespace occurrence",
                 "c", 0, true);
            goto done;
        }
        c->registry_held = true;
        const mc_registry_status initialized =
            mc_registry_classes_initialize_translation_singleton(registry, &c->failure.registry);
        if (initialized != MC_REGISTRY_OK) {
            fail(c, MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED, c->failure.registry.message, "c", 0,
                 translation_native_policy(registry, initialized, &c->failure.registry));
            goto done;
        }
        mc_registry_translation_view singleton;
        if (mc_registry_classes_inspect_translation(registry, &singleton, &c->failure.registry) !=
                MC_REGISTRY_OK ||
            singleton.singleton_a == NULL) {
            fail(c, MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED, "actual NH singleton publication", "c",
                 0, true);
            goto done;
        }
        if (argument == NULL && registry_key == NULL) {
            c->failure.child =
                (mc_client_gui_ingame_site){"nh", "a", "(Ljava/lang/String;)Ljava/lang/String;", 6};
            fail(c, MC_CLIENT_GUI_INGAME_CHAT_SOURCE_NULL, "NH actual getProperty null key", "c", 6,
                 false);
            goto done;
        }
        mc_player_string_view key;
        if (!mc_gui_chat_text_read(text, &key)) {
            fail(c, MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT, "NH native key String view", "c", 6, true);
            goto done;
        }
        const mc_registry_string *translated = NULL;
        if (mc_translation_property(singleton.singleton_a, key.units, key.length, &translated,
                                    &c->failure.registry) != MC_REGISTRY_OK) {
            fail(c, MC_CLIENT_GUI_INGAME_CHAT_CHILD_FAILED, c->failure.registry.message, "c", 6,
                 true);
            goto done;
        }
        if (translated != NULL)
            text = (mc_gui_chat_text){.registry = translated, .registry_owner = registry};
        c->state.translated = true;
    }
    if (add(c, text, 0)) {
        c->state.returned = c->failure.source_returned = true;
        c->state.source_pc = translating ? 15 : 132;
        c->failure.site = (mc_client_gui_ingame_site){"uq", translating ? "c" : "a", descriptor,
                                                      c->state.source_pc};
    }
done:
    c->active = false;
    return publish(error, &c->failure);
}

mc_client_gui_ingame_chat_status mc_client_gui_ingame_chat_translate_begin(
    mc_client_source_uq *owner, mc_registry_classes *registry, const mc_player_string *key,
    mc_client_gui_ingame_chat_call **out, mc_client_gui_ingame_chat_error *error) {
    return begin(owner, registry, key, NULL, NULL, true, out, error);
}

mc_client_gui_ingame_chat_status mc_client_gui_ingame_chat_translate_registry_begin(
    mc_client_source_uq *owner, mc_registry_classes *translation_owner,
    mc_registry_classes *key_owner, const mc_registry_string *key,
    mc_client_gui_ingame_chat_call **out, mc_client_gui_ingame_chat_error *error) {
    return begin(owner, translation_owner, NULL, key_owner, key, true, out, error);
}

mc_client_gui_ingame_chat_status
mc_client_gui_ingame_chat_add_begin(mc_client_source_uq *owner, const mc_player_string *text,
                                    mc_client_gui_ingame_chat_call **out,
                                    mc_client_gui_ingame_chat_error *error) {
    return begin(owner, NULL, text, NULL, NULL, false, out, error);
}

bool mc_client_gui_ingame_chat_inspect(const mc_client_gui_ingame_chat_call *c,
                                       mc_client_gui_ingame_chat_state *out) {
    if (c == NULL || out == NULL || c->active)
        return false;
    *out = c->state;
    return true;
}

mc_client_gui_ingame_chat_status
mc_client_gui_ingame_chat_retire(mc_client_gui_ingame_chat_call **slot,
                                 mc_client_gui_ingame_chat_error *error) {
    mc_client_gui_ingame_chat_error empty = {0};
    if (slot == NULL) {
        empty.status = MC_CLIENT_GUI_INGAME_CHAT_ARGUMENT;
        empty.operation = "chat native retirement slot";
        empty.native_policy = true;
        return publish(error, &empty);
    }
    if (*slot == NULL)
        return publish(error, &empty);
    mc_client_gui_ingame_chat_call *c = *slot;
    if (c->active || mtx_trylock(&c->owner->classes->mutex) != thrd_success) {
        empty.status = MC_CLIENT_GUI_INGAME_CHAT_BUSY;
        empty.operation = "chat native active retirement";
        empty.native_policy = true;
        return publish(error, &empty);
    }
    c->active = true;
    memset(&c->cleanup, 0, sizeof(c->cleanup));
    while (c->font_count != 0) {
        if (mc_client_bitmap_font_release(&c->fonts[c->font_count - 1], &c->cleanup.font) !=
            MC_CLIENT_BITMAP_FONT_OK)
            goto busy;
        --c->font_count;
    }
    if (c->pending != NULL) {
        if (!mc_gui_chat_message_release(c->pending, &c->cleanup.registry))
            goto busy;
        c->owner->bytes -= sizeof(*c->pending);
        c->pending = NULL;
    }
    if (c->registry_held) {
        if (mc_registry_classes_release(c->registry, &c->cleanup.registry) != MC_REGISTRY_OK)
            goto busy;
        c->registry_held = false;
    }
    mc_client_gui_ingame_error collection = {0};
    if (!mc_gui_chat_collect(c->owner, &collection)) {
        c->cleanup.registry = collection.registry;
        goto busy;
    }
    if (c->argument_registry_held) {
        if (mc_registry_classes_release(c->argument_registry, &c->cleanup.registry) !=
            MC_REGISTRY_OK)
            goto busy;
        c->argument_registry_held = false;
        c->argument_registry = NULL;
        c->registry_argument = NULL;
    }
    while (c->temporaries != NULL) {
        chat_temporary *temporary = c->temporaries;
        c->temporaries = temporary->next;
        mc_player_string_release(temporary->value);
        c->owner->bytes -= sizeof(*temporary);
        free(temporary);
    }
    mc_player_string_release((mc_player_string *)c->argument);
    mc_client_source_uq *owner = c->owner;
    owner->chat_call = NULL;
    --owner->holds;
    c->active = false;
    free(c);
    *slot = NULL;
    (void)mtx_unlock(&owner->classes->mutex);
    return publish(error, &empty);
busy:
    c->active = false;
    c->cleanup.status = MC_CLIENT_GUI_INGAME_CHAT_BUSY;
    c->cleanup.operation = "chat retained native child retirement";
    c->cleanup.native_policy = true;
    (void)mtx_unlock(&c->owner->classes->mutex);
    return publish(error, &c->cleanup);
}
