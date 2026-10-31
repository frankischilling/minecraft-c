#ifndef MC_RESOURCES_TRANSLATION_INTERNAL_H
#define MC_RESOURCES_TRANSLATION_INTERNAL_H

#include "mc/resources/translation/translation.h"

typedef struct mc_translation_string_binding {
    const mc_java_properties_string *source;
    const mc_registry_string *string;
    struct mc_translation_string_binding *next;
} mc_translation_string_binding;

struct mc_translation {
    mc_registry_classes *classes;
    mc_java_properties *b;
    mc_translation_string_binding *strings;
    /* Native diagnostic constructor metadata, not another source field. */
    int32_t constructor_pc;
};

typedef struct mc_registry_translation_storage {
    mc_translation *singleton_a;
    mc_translation *wrapper_a;
    /* Retained unpublished native construction metadata; never a source
     * static publication or substitute for singleton_a. */
    mc_translation *pending;
} mc_registry_translation_storage;

mc_registry_status mc_registry_translation_active_use(mc_registry_classes *classes);
mc_registry_status mc_registry_translate(mc_registry_classes *classes,
                                         const mc_registry_string *key,
                                         const mc_registry_string **out);
mc_registry_status mc_registry_translate_formatted(mc_registry_classes *classes,
                                                   const mc_registry_string *key,
                                                   const mc_registry_string *const *arguments,
                                                   size_t argument_count,
                                                   const mc_registry_string **out);

#endif
