#ifndef MC_CLIENT_APPLICATION_DIRECTORY_CLASSES_H
#define MC_CLIENT_APPLICATION_DIRECTORY_CLASSES_H

#include "application_startup.h"

typedef enum application_directory_literal_id {
    APPLICATION_DIRECTORY_MINECRAFT,
    APPLICATION_DIRECTORY_HOME_KEY,
    APPLICATION_DIRECTORY_DOT,
    APPLICATION_DIRECTORY_APPDATA_KEY,
    APPLICATION_DIRECTORY_MAC_PREFIX,
    APPLICATION_DIRECTORY_SAVES,
    APPLICATION_DIRECTORY_LITERAL_COUNT
} application_directory_literal_id;

typedef struct mc_client_directory_dispatch_state {
    mc_client_application_class_phase pd_phase, kn_phase;
    const void *pd[5]; /* five DISTINCT actual enum identities, in ordinal order */
    const mc_world_info_name *names[5];
    const void *values_identity, *values_entries[5];
    const int32_t *switch_map;
    size_t enum_allocations, values_allocations, clone_allocations, map_allocations;
    unsigned int last_pd_store, last_kn_store;
    const mc_client_startup_error *failure;
} mc_client_directory_dispatch_state;

bool mc_client_application_directory_dispatch_inspect(mc_client_application_classes *,
                                                      mc_client_directory_dispatch_state *);

/* Native narrow helpers: all require the externally live MC namespace. A
 * resolved literal is borrowed from it, not a new equal-text argument owner. */
mc_client_startup_status application_directory_literal(mc_client_application_classes *,
                                                       application_directory_literal_id, size_t,
                                                       mc_world_info_name **,
                                                       mc_client_startup_error *);
mc_client_startup_status application_directory_dispatch_activate(mc_client_application_classes *,
                                                                 size_t, mc_client_startup_error *);
mc_client_startup_status application_directory_dispatch_case(mc_client_application_classes *,
                                                             unsigned int ordinal, int32_t *,
                                                             mc_client_startup_error *);

#endif
