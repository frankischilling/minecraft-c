#ifndef MC_WORLD_RUNTIME_RAILS_ERRORS_H
#define MC_WORLD_RUNTIME_RAILS_ERRORS_H

#include <stdbool.h>
#include <stdint.h>

/* Plain detector/lifecycle/connection/search sites hold borrowed diagnostic
 * context. No mutation, scheduler, World, callback or owning error type is
 * included here. */
typedef struct mc_world_detector_site {
    bool entered;
    const char *declaring_class;
    const char *method;
    unsigned int bytecode;
    int32_t x, y, z;
} mc_world_detector_site;

typedef struct mc_rail_lifecycle_site {
    bool entered;
    const char *declaring_class, *method;
    unsigned int bytecode;
    int32_t x, y, z;
} mc_rail_lifecycle_site;

typedef struct mc_rail_connection_site {
    bool entered;
    const char *class_name;
    const char *method;
    unsigned int bytecode;
} mc_rail_connection_site;

typedef struct mc_rail_power_search_site {
    bool entered;
    const char *declaring_class, *method;
    unsigned int bytecode;
    int32_t x, y, z, metadata, depth, axis;
    bool forward;
} mc_rail_power_search_site;

/* Selection is explicit and independent of entered. A selected-but-unentered
 * invocation, including a nullable receiver or failed class active use, retains
 * its actual class/method. PC0 and all-zero context never imply selection.
 * The bits correspond to flat fields of mc_world_mutation_error. */
enum mc_world_rail_error_site_bit {
    MC_WORLD_RAIL_SITE_DETECTOR_CONTACT = UINT32_C(1) << 0,
    MC_WORLD_RAIL_SITE_DETECTOR_TICK = UINT32_C(1) << 1,
    MC_WORLD_RAIL_SITE_DETECTOR_UPDATE = UINT32_C(1) << 2,
    MC_WORLD_RAIL_SITE_DETECTOR_CAUSE = UINT32_C(1) << 3,
    MC_WORLD_RAIL_SITE_ADDED = UINT32_C(1) << 4,
    MC_WORLD_RAIL_SITE_SHAPE = UINT32_C(1) << 5,
    MC_WORLD_RAIL_SITE_NEIGHBOR = UINT32_C(1) << 6,
    MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE = UINT32_C(1) << 7,
    MC_WORLD_RAIL_SITE_CONNECTION = UINT32_C(1) << 8,
    MC_WORLD_RAIL_SITE_CONNECTION_CAUSE = UINT32_C(1) << 9,
    MC_WORLD_RAIL_SITE_PREDICATE = UINT32_C(1) << 10,
    MC_WORLD_RAIL_SITE_PREDICATE_CAUSE = UINT32_C(1) << 11,
    MC_WORLD_RAIL_SITE_ACCESSOR = UINT32_C(1) << 12,
    MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE = UINT32_C(1) << 13,
    MC_WORLD_RAIL_SITE_SEARCH = UINT32_C(1) << 14,
    MC_WORLD_RAIL_SITE_VERIFY = UINT32_C(1) << 15,
    MC_WORLD_RAIL_SITE_SEARCH_CAUSE = UINT32_C(1) << 16
};

#endif
