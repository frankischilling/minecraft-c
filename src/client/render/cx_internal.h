#ifndef MC_CLIENT_RENDER_CX_INTERNAL_H
#define MC_CLIENT_RENDER_CX_INTERNAL_H

#include "cx.h"
#include <threads.h>

struct mc_client_cx_classes {
    mtx_t mutex;
    size_t holds;
    mc_client_cx_class_phase phase;
    bool enabled;
    unsigned int last_store;
};

struct mc_client_source_cx {
    mc_client_cx_classes *classes;
    size_t holds;
    mc_client_cx_phase phase;
    unsigned int constructor_pc;
};

/* Shared CPU monitor/error kernels. The CPU object has no Display/SDL call or
 * link dependency; only cx_supported.c invokes the canonical device helper. */
mc_client_cx_status cx_publish(const mc_client_cx_error *, mc_client_cx_error *);
mc_client_cx_status cx_fail(mc_client_cx_error *, mc_client_cx_status, const char *, const char *,
                            unsigned int);
bool cx_instance_lock(mc_client_source_cx *, mc_client_cx_error *);

#endif
