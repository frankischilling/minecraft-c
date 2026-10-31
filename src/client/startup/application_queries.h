#ifndef MC_CLIENT_STARTUP_APPLICATION_QUERIES_H
#define MC_CLIENT_STARTUP_APPLICATION_QUERIES_H

#include "application_owner.h"

typedef enum mc_client_application_query_status {
    MC_CLIENT_APPLICATION_QUERY_OK,
    MC_CLIENT_APPLICATION_QUERY_INVALID_ARGUMENT,
    MC_CLIENT_APPLICATION_QUERY_NATIVE_FAILED,
    MC_CLIENT_APPLICATION_QUERY_SOURCE_NULL
} mc_client_application_query_status;

typedef struct mc_client_application_query_error {
    mc_client_application_query_status status;
    const char *operation;
    mc_client_application_owner_site site;
    bool native_policy, source_returned;
    mc_client_application_owner_error owner;
} mc_client_application_query_error;

/* Actual static Minecraft.v()Z. Active use belongs to the supplied canonical
 * namespace, independently of any renderer App. The actual static a is read
 * at0 and reread at6; a NULL first read returns false, while a reached NULL
 * Settings fails at12. No construction, publication or Settings default is
 * supplied. Source return is23. The namespace, published Apps and Settings
 * stay live/exclusive through this synchronous call; ordinary out is unchanged
 * on failure. Output/error storage must be disjoint from those owners. */
mc_client_application_query_status
mc_client_application_ambient_occlusion(mc_client_application_classes *, bool *out,
                                        mc_client_application_query_error *);

#endif
