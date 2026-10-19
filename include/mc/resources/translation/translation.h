#ifndef MC_RESOURCES_TRANSLATION_TRANSLATION_H
#define MC_RESOURCES_TRANSLATION_TRANSLATION_H

#include "mc/block/registry/registry.h"
#include "mc/core/java/properties.h"

typedef struct mc_translation mc_translation;

/* Borrowed class-loader resource source. Names are the actual absolute game
 * resource names, such as /lang/en_US.lang. OK supplies a live byte stream;
 * NULL_REFERENCE represents an absent resource, whose null stream is used at
 * the original subsequent load call. Other statuses report native failures.
 * The source owns all opened stream contexts and their eventual retirement.
 * Translation construction does not close them. Callbacks must not mutate or
 * retire the admitted class owner or its borrowed dependencies. */
struct mc_registry_resources {
    void *context;
    mc_registry_status (*open)(void *context, const char *name, mc_java_properties_input *out);
};

typedef struct mc_registry_translation_view {
    mc_registry_phase singleton_phase, wrapper_phase;
    int32_t singleton_initializer_pc, wrapper_initializer_pc;
    const mc_translation *singleton_a;
    const mc_translation *wrapper_a;
} mc_registry_translation_view;

/* Execute the actual do/ch wrapper and nh/ii singleton initialization on the
 * same class owner used by Block/Item. Resource reads occur only when reached.
 * The singleton is published after both loads succeed. Formatted lookup and
 * caught-I/O stack-trace output remain explicit unfinished dependencies until
 * implemented; neither may be replaced with a successful empty callback. */
mc_registry_status mc_registry_classes_initialize_translation(mc_registry_classes *classes,
                                                              mc_registry_error *error);
mc_registry_status mc_registry_classes_inspect_translation(const mc_registry_classes *classes,
                                                           mc_registry_translation_view *out,
                                                           mc_registry_error *error);

/* Read an actual loaded String property without class initialization. Absence
 * succeeds with NULL; the source game's getProperty(key,key) fallback belongs
 * to its wrapper call. Returned String identities remain live through later
 * operations until the common class owner is freed. No lookup allocation or
 * map iteration order is observable through this interface. */
mc_registry_status mc_translation_property(const mc_translation *translation, const uint16_t *key,
                                           size_t length, const mc_registry_string **out,
                                           mc_registry_error *error);
size_t mc_translation_property_count(const mc_translation *translation);

#endif
