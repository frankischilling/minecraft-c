#include "description_registry.h"
#include "../../block/registry/registry_internal.h"
#include "../../block/registry/runtime_internal.h"
#include "../../platform/text/locale.h"
#include <stdlib.h>
#include <string.h>

struct mc_achievement_description_callback {
    mc_registry_classes *classes;
    client_application *a;
};

struct mc_client_owned_inventory_callback {
    mc_achievement_description_callback source;
    struct mc_client_owned_inventory_callback *next;
    void (*release)(client_application *);
};

static struct mc_client_owned_inventory_callback *
owned_callback(const mc_registry_classes *c, const mc_achievement_description_callback *callback) {
    for (struct mc_client_owned_inventory_callback *node = c->owned_inventory_callbacks;
         node != NULL; node = node->next)
        if (&node->source == callback)
            return node;
    return NULL;
}

bool mc_client_inventory_callback_is_owned(const mc_registry_classes *c,
                                           const mc_achievement_description_callback *callback) {
    return c != NULL && callback != NULL && owned_callback(c, callback) != NULL;
}

void mc_client_inventory_callbacks_retire(mc_registry_classes *c) {
    while (c->owned_inventory_callbacks != NULL) {
        struct mc_client_owned_inventory_callback *node = c->owned_inventory_callbacks;
        c->owned_inventory_callbacks = node->next;
        void (*release)(client_application *) = node->release;
        node->release = NULL;
        if (release != NULL)
            release(node->source.a);
    }
}

mc_registry_status mc_client_notice_registry_retire_admit(const mc_registry_classes *c,
                                                          mc_registry_error *e) {
    return mc_registry_runtime_borrow_release_admit(c, e);
}

static mc_registry_status result(mc_registry_error *e, mc_registry_status s, const char *message) {
    if (e != NULL)
        *e = (mc_registry_error){.status = s,
                                 .message = message,
                                 .initializer_pc = -1,
                                 .block_id = -1,
                                 .constructor_pc = -1,
                                 .active_initializer_pc = -1,
                                 .item_argument = -1};
    return s;
}

static mc_registry_status admit(mc_registry_classes *c, mc_registry_error *e) {
    if (c == NULL || c->options.side != MC_REGISTRY_CLIENT)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "client achievement Registry");
    if (c->entered != 0)
        return result(e, MC_REGISTRY_BUSY, "client achievement Registry entered");
    return MC_REGISTRY_OK;
}

mc_registry_status mc_client_achievement_admit(mc_registry_classes *c, const mc_statistic *s,
                                               mc_registry_error *e) {
    mc_registry_status status = admit(c, e);
    if (status != MC_REGISTRY_OK)
        return status;
    if (s != NULL && (s->classes != c || s->kind != MC_STATISTIC_ACHIEVEMENT))
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "foreign/non-achievement descriptor");
    return result(e, MC_REGISTRY_OK, NULL);
}

mc_registry_status
mc_client_inventory_callback_create(mc_registry_classes *c, client_application *client,
                                    const mc_achievement_description_callback **out,
                                    mc_registry_error *e) {
    mc_registry_status status = admit(c, e);
    if (status != MC_REGISTRY_OK)
        return status;
    if (out == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "kh allocation output");
    mc_achievement_description_callback *p = mc_registry_allocate(c, sizeof(*p));
    if (p == NULL) {
        if (e != NULL)
            *e = c->last_error;
        return c->last_error.status;
    }
    p->classes = c;
    p->a = client;
    *out = p;
    return result(e, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_client_inventory_callback_owned_allocate(
    mc_registry_classes *c, const mc_achievement_description_callback **out, mc_registry_error *e) {
    mc_registry_status status = admit(c, e);
    if (status != MC_REGISTRY_OK)
        return status;
    if (out == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "owned kh allocation output");
    struct mc_client_owned_inventory_callback *node = mc_registry_allocate(c, sizeof(*node));
    if (node == NULL) {
        if (e != NULL)
            *e = c->last_error;
        return c->last_error.status;
    }
    node->source.classes = c;
    node->next = c->owned_inventory_callbacks;
    c->owned_inventory_callbacks = node;
    *out = &node->source;
    return result(e, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_client_inventory_callback_owned_finish(
    mc_registry_classes *c, const mc_achievement_description_callback *callback,
    client_application *client, void (*release)(client_application *), mc_registry_error *e) {
    mc_registry_status status = admit(c, e);
    if (status != MC_REGISTRY_OK)
        return status;
    struct mc_client_owned_inventory_callback *node = owned_callback(c, callback);
    if (node == NULL || client == NULL || release == NULL || node->release != NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "owned kh native borrow transfer");
    node->release = release;
    node->source.a = client; /* Actual kh constructor's a store. */
    return result(e, MC_REGISTRY_OK, NULL);
}

mc_registry_status
mc_client_achievement_callback_set(mc_registry_classes *c, const mc_statistic *s,
                                   const mc_achievement_description_callback *callback,
                                   mc_registry_error *e) {
    mc_registry_status status = mc_client_achievement_admit(c, s, e);
    if (status != MC_REGISTRY_OK)
        return status;
    if (callback != NULL && callback->classes != c)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "foreign kh owner");
    if (s == NULL)
        return result(e, MC_REGISTRY_NULL_REFERENCE, "ny.a(gt) putfield2 null receiver");
    /* Source public receiver fields are mutable; Registry still owns the object. */
    ((mc_statistic *)s)->achievement.client_callback = callback;
    return result(e, MC_REGISTRY_OK, NULL);
}

mc_registry_status
mc_client_inventory_callback_client(mc_registry_classes *c,
                                    const mc_achievement_description_callback *callback,
                                    client_application **out, mc_registry_error *e) {
    mc_registry_status status = admit(c, e);
    if (status != MC_REGISTRY_OK)
        return status;
    if (callback == NULL || callback->classes != c || out == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "kh owner");
    *out = callback->a;
    return result(e, MC_REGISTRY_OK, NULL);
}

typedef struct text_allocation {
    struct text_allocation *next;
    long double alignment;
} text_allocation;

struct mc_client_description_owner {
    mc_registry_string value;
    text_allocation *allocations;
    size_t bytes;
    mc_registry_status failure;
};

void mc_client_description_result_free(mc_client_description_result *result) {
    if (result == NULL)
        return;
    mc_client_description_owner *owner = result->owner;
    if (owner != NULL) {
        text_allocation *allocation = owner->allocations;
        while (allocation != NULL) {
            text_allocation *next = allocation->next;
            free(allocation);
            allocation = next;
        }
        free(owner);
    }
    *result = (mc_client_description_result){0};
}

static void *format_allocate(void *context, size_t bytes) {
    mc_client_description_owner *owner = context;
    const size_t maximum = 8u * 1024u * 1024u;
    if (bytes > SIZE_MAX - sizeof(text_allocation) ||
        sizeof(text_allocation) + bytes > maximum - owner->bytes) {
        owner->failure = MC_REGISTRY_LIMIT;
        return NULL;
    }
    text_allocation *allocation = malloc(sizeof(*allocation) + bytes);
    if (allocation == NULL) {
        owner->failure = MC_REGISTRY_ALLOCATION_FAILED;
        return NULL;
    }
    owner->bytes += sizeof(*allocation) + bytes;
    allocation->next = owner->allocations;
    owner->allocations = allocation;
    return allocation + 1;
}

mc_registry_status mc_client_description_format(mc_registry_classes *c,
                                                const mc_registry_string *pattern,
                                                const char *key_name,
                                                mc_client_description_result *out,
                                                mc_registry_error *e) {
    mc_registry_status status = admit(c, e);
    if (status != MC_REGISTRY_OK)
        return status;
    if (out == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "kh format output");
    uint16_t key[32];
    size_t length = key_name == NULL ? 0 : strlen(key_name);
    if (length > sizeof(key) / sizeof(key[0]))
        return result(e, MC_REGISTRY_LIMIT, "Keyboard name length");
    for (size_t i = 0; i < length; ++i)
        key[i] = (unsigned char)key_name[i];
    const mc_java_format_text argument = {key, length};
    const mc_java_format_text *arguments[1] = {key_name == NULL ? NULL : &argument};
    const mc_java_format_text text = {pattern == NULL ? NULL : pattern->units,
                                      pattern == NULL ? 0 : pattern->length};
    mc_text_number_locale locale = MC_TEXT_NUMBER_LOCALE_UNKNOWN;
    const mc_text_locale_status queried = mc_text_number_locale_query(&locale);
    if (queried != MC_TEXT_LOCALE_OK)
        return result(e,
                      queried == MC_TEXT_LOCALE_ALLOCATION_FAILED ? MC_REGISTRY_ALLOCATION_FAILED
                                                                  : MC_REGISTRY_IO_FAILED,
                      "kh format locale query");
    static const uint16_t newline[] = {
#ifdef _WIN32
        '\r',
#endif
        '\n'};
    mc_client_description_owner *owner = calloc(1, sizeof(*owner));
    if (owner == NULL)
        return result(e, MC_REGISTRY_ALLOCATION_FAILED, "kh runtime String owner");
    owner->bytes = sizeof(*owner);
    const mc_java_format_options options = {
        .allocator = {owner, format_allocate},
        .locale = (locale == MC_TEXT_NUMBER_LOCALE_EN || locale == MC_TEXT_NUMBER_LOCALE_EN_US)
                      ? MC_JAVA_FORMAT_LOCALE_ENGLISH
                      : MC_JAVA_FORMAT_LOCALE_UNSUPPORTED,
        .line_separator = {newline, sizeof(newline) / sizeof(newline[0])},
        .max_output_units = 1048576u};
    mc_java_format_text formatted;
    mc_java_format_error format = {0};
    mc_java_format_status fs = mc_java_format_strings(&options, pattern == NULL ? NULL : &text,
                                                      arguments, 1, &formatted, &format);
    mc_client_description_result produced = {.owner = owner};
    if (fs != MC_JAVA_FORMAT_OK) {
        status = owner->failure != MC_REGISTRY_OK          ? owner->failure
                 : fs == MC_JAVA_FORMAT_ALLOCATION_FAILED  ? MC_REGISTRY_ALLOCATION_FAILED
                 : fs == MC_JAVA_FORMAT_LIMIT              ? MC_REGISTRY_LIMIT
                 : fs == MC_JAVA_FORMAT_NULL_FORMAT        ? MC_REGISTRY_NULL_REFERENCE
                 : fs == MC_JAVA_FORMAT_UNSUPPORTED_LOCALE ? MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY
                                                           : MC_REGISTRY_ILLEGAL_ARGUMENT;
        mc_client_description_result_free(&produced);
        result(e, status, "kh String.format24");
        if (e != NULL)
            e->format_error = format;
        return status;
    }
    if (formatted.length == 0) {
        const mc_registry_string *empty = NULL;
        status = mc_registry_literal_ascii(c, "", &empty);
        mc_client_description_result_free(&produced);
        if (status != MC_REGISTRY_OK) {
            if (e != NULL)
                *e = c->last_error;
            return status;
        }
        *out = (mc_client_description_result){.string = empty};
    } else {
        owner->value = (mc_registry_string){.units = formatted.units, .length = formatted.length};
        produced.string = &owner->value;
        *out = produced;
    }
    return result(e, MC_REGISTRY_OK, NULL);
}

mc_registry_status mc_client_achievement_get_title(mc_registry_classes *c,
                                                   const mc_registry_string **out,
                                                   mc_registry_error *e) {
    mc_registry_status status = admit(c, e);
    if (status != MC_REGISTRY_OK)
        return status;
    if (out == NULL)
        return result(e, MC_REGISTRY_INVALID_ARGUMENT, "achievement title output");
    const mc_registry_string *key = NULL;
    status = mc_registry_literal_ascii(c, "achievement.get", &key);
    if (status == MC_REGISTRY_OK) {
        ++c->entered;
        status = mc_registry_translate(c, key, out);
        --c->entered;
    }
    if (status != MC_REGISTRY_OK) {
        if (e != NULL)
            *e = c->last_error;
        return status;
    }
    return result(e, MC_REGISTRY_OK, NULL);
}
