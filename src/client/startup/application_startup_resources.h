#ifndef MC_CLIENT_STARTUP_RESOURCES_H
#define MC_CLIENT_STARTUP_RESOURCES_H

#include "../resources/download_thread.h"
#include "application_gl_output.h"

typedef struct mc_client_startup_resources_call {
    bool active, U_stored, constructed, started, caught_exception, post_returned, retired;
    mc_client_download_classes *classes;
    mc_client_download_thread *candidate;
    mc_client_download_error download, cleanup;
    mc_client_gl_output_call *diagnostic;
    mc_client_gl_output_error graphics, graphics_cleanup;
} mc_client_startup_resources_call;

#endif
