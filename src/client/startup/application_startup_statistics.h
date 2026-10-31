#ifndef MC_CLIENT_STARTUP_APPLICATION_STATISTICS_H
#define MC_CLIENT_STARTUP_APPLICATION_STATISTICS_H

#include "../statistics/application.h"

typedef struct mc_client_startup_stats_call mc_client_startup_stats_call;

/* Passive native trace and complete diagnostics. Actual k/I and callbacks
 * remain on their canonical App/Registry owners; this record adds no stores. */
struct mc_client_startup_stats_call {
    bool active, entered, source_calls_completed, I_stored, callback_installed, retired;
    /* I_stored/callback_installed record reached stores, not live ownership.
     * Borrowed writer identities below are comparison-only and refresh after
     * native cleanup; never dereference them after external retirement. */
    unsigned int last_pc;
    mc_client_stats_classes *classes;              /* Borrowed from the actual MC namespace. */
    mc_client_stats_writer *candidate, *published; /* Borrowed identity guards. */
    mc_client_stats_report report;
    mc_client_stats_error classes_error;
    mc_client_statistics_owner_error failure, retirement;
};

#endif
