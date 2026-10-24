#include "../../block/registry/registry_internal.h"
#include "../../platform/text/locale.h"
#include "translation_internal.h"

#include <limits.h>

typedef struct format_arena {
    mc_registry_classes *classes;
    bool failed;
} format_arena;

static void *allocate(void *context, size_t bytes) {
    format_arena *arena = context;
    void *storage = mc_registry_allocate(arena->classes, bytes);
    if (storage == NULL)
        arena->failed = true;
    return storage;
}

static mc_registry_status own_error(mc_registry_classes *classes) {
    classes->last_error.constructor_class =
        classes->options.side == MC_REGISTRY_CLIENT ? "nh" : "ii";
    classes->last_error.constructor_pc = 12;
    return classes->last_error.status;
}

static mc_registry_status fail(mc_registry_classes *classes, mc_registry_status status,
                               const char *message) {
    (void)mc_registry_fail(classes, status, message, NULL);
    return own_error(classes);
}

mc_registry_status mc_registry_translate_formatted(mc_registry_classes *classes,
                                                   const mc_registry_string *key,
                                                   const mc_registry_string *const *arguments,
                                                   size_t argument_count,
                                                   const mc_registry_string **out) {
    if (classes == NULL)
        return MC_REGISTRY_INVALID_ARGUMENT;
    if (out == NULL || (arguments == NULL && argument_count != 0))
        return mc_registry_fail(classes, MC_REGISTRY_INVALID_ARGUMENT,
                                "Formatted Translation arguments", NULL);
    const mc_registry_string *pattern = NULL;
    mc_registry_status status = mc_registry_translate(classes, key, &pattern);
    if (status != MC_REGISTRY_OK)
        return status;
    if (argument_count > (size_t)INT32_MAX ||
        argument_count > SIZE_MAX / sizeof(mc_java_format_text) ||
        argument_count > SIZE_MAX / sizeof(mc_java_format_text *))
        return fail(classes, MC_REGISTRY_LIMIT, "Formatted Translation argument count");

    const mc_java_format_text *empty_arguments = NULL;
    const mc_java_format_text **references = arguments == NULL ? NULL : &empty_arguments;
    if (argument_count != 0) {
        mc_java_format_text *views = mc_registry_allocate(classes, argument_count * sizeof(*views));
        if (views == NULL)
            return own_error(classes);
        references = mc_registry_allocate(classes, argument_count * sizeof(*references));
        if (references == NULL)
            return own_error(classes);
        for (size_t i = 0; i < argument_count; ++i) {
            if (arguments[i] == NULL)
                references[i] = NULL;
            else {
                views[i] = (mc_java_format_text){.units = arguments[i]->units,
                                                 .length = arguments[i]->length};
                references[i] = &views[i];
            }
        }
    }

    mc_text_number_locale locale = MC_TEXT_NUMBER_LOCALE_UNKNOWN;
    const mc_text_locale_status queried = mc_text_number_locale_query(&locale);
    if (queried != MC_TEXT_LOCALE_OK)
        return fail(classes,
                    queried == MC_TEXT_LOCALE_ALLOCATION_FAILED ? MC_REGISTRY_ALLOCATION_FAILED
                                                                : MC_REGISTRY_IO_FAILED,
                    "Formatted Translation native format locale");
    static const uint16_t newline[] = {
#ifdef _WIN32
        '\r',
#endif
        '\n'};
    format_arena arena = {.classes = classes};
    const mc_java_format_options options = {
        .allocator = {.context = &arena, .allocate = allocate},
        .locale = locale == MC_TEXT_NUMBER_LOCALE_EN_US || locale == MC_TEXT_NUMBER_LOCALE_EN
                      ? MC_JAVA_FORMAT_LOCALE_ENGLISH
                      : MC_JAVA_FORMAT_LOCALE_UNSUPPORTED,
        .line_separator = {.units = newline, .length = sizeof(newline) / sizeof(*newline)}};
    const mc_java_format_text text = {.units = pattern->units, .length = pattern->length};
    mc_java_format_text formatted;
    mc_java_format_error detail = {0};
    const mc_java_format_status result =
        mc_java_format_strings(&options, &text, references, argument_count, &formatted, &detail);
    if (result != MC_JAVA_FORMAT_OK) {
        if (arena.failed)
            status = own_error(classes);
        else {
            const mc_registry_status mapped =
                result == MC_JAVA_FORMAT_LIMIT                ? MC_REGISTRY_LIMIT
                : result == MC_JAVA_FORMAT_ALLOCATION_FAILED  ? MC_REGISTRY_ALLOCATION_FAILED
                : result == MC_JAVA_FORMAT_UNSUPPORTED_LOCALE ? MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY
                : result == MC_JAVA_FORMAT_INVALID_ARGUMENT   ? MC_REGISTRY_INVALID_ARGUMENT
                                                              : MC_REGISTRY_ILLEGAL_ARGUMENT;
            status = fail(classes, mapped, "Translation String.format at12 failed");
        }
        classes->last_error.format_error = detail;
        return status;
    }
    if (formatted.length == 0) {
        status = mc_registry_literal_ascii(classes, "", out);
        return status == MC_REGISTRY_OK ? status : own_error(classes);
    }
    mc_registry_string *string = mc_registry_allocate(classes, sizeof(*string));
    if (string == NULL)
        return own_error(classes);
    *string = (mc_registry_string){.units = formatted.units, .length = formatted.length};
    *out = string;
    return MC_REGISTRY_OK;
}
