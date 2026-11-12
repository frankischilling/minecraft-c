#ifndef MC_CLIENT_RENDER_CX_H
#define MC_CLIENT_RENDER_CX_H

#include "../display/occlusion.h"

typedef struct mc_client_cx_classes mc_client_cx_classes;
typedef struct mc_client_source_cx mc_client_source_cx;

typedef enum mc_client_cx_status {
    MC_CLIENT_CX_OK = 0,
    MC_CLIENT_CX_INVALID_ARGUMENT,
    MC_CLIENT_CX_INVALID_PHASE,
    MC_CLIENT_CX_ALLOCATION_FAILED,
    MC_CLIENT_CX_LIMIT,
    MC_CLIENT_CX_BUSY,
    MC_CLIENT_CX_THREAD_FAILED,
    MC_CLIENT_CX_DISPLAY_FAILED
} mc_client_cx_status;

typedef enum mc_client_cx_class_phase {
    MC_CLIENT_CX_CLASS_COLD = 0,
    MC_CLIENT_CX_CLASS_READY
} mc_client_cx_class_phase;

typedef enum mc_client_cx_phase { MC_CLIENT_CX_RAW = 0, MC_CLIENT_CX_READY } mc_client_cx_phase;

typedef struct mc_client_cx_error {
    mc_client_cx_status status;
    const char *operation, *source_class, *source_method, *source_descriptor;
    unsigned int source_pc;
    bool source_returned, capability_read;
    int native_thread_error;
    mc_client_display_error display;
} mc_client_cx_error;

typedef struct mc_client_cx_class_state {
    mc_client_cx_class_phase phase;
    size_t holds;
    bool enabled; /* Actual private static cx.a; false before <clinit>. */
    unsigned int last_store;
} mc_client_cx_class_state;

typedef struct mc_client_cx_state {
    mc_client_cx_phase phase;
    size_t holds;
    unsigned int constructor_pc;
} mc_client_cx_state;

/* A separate counted native namespace. Create is cold and does not activate a
 * Display. Active use performs exactly cx.<clinit>: true store1, return4.
 * Allocate performs that active use before the RAW instance allocation; Object
 * constructor1 and return4 occur only in construct. No App association is made.
 * Instances retain this exact namespace. Read/inspect are passive native APIs.
 * Error/value/output/slot storage must be live and disjoint from owners. */
mc_client_cx_status mc_client_cx_classes_create(mc_client_cx_classes **, mc_client_cx_error *);
mc_client_cx_status mc_client_cx_classes_active_use(mc_client_cx_classes *, mc_client_cx_error *);
mc_client_cx_status mc_client_cx_classes_retain(mc_client_cx_classes *, mc_client_cx_error *);
mc_client_cx_status mc_client_cx_classes_release(mc_client_cx_classes **, mc_client_cx_error *);
mc_client_cx_status mc_client_cx_classes_inspect(mc_client_cx_classes *, mc_client_cx_class_state *,
                                                 mc_client_cx_error *);
mc_client_cx_status mc_client_cx_allocate(mc_client_cx_classes *, mc_client_source_cx **,
                                          mc_client_cx_error *);
mc_client_cx_status mc_client_cx_construct(mc_client_source_cx *, mc_client_cx_error *);
mc_client_cx_status mc_client_cx_retain(mc_client_source_cx *, mc_client_cx_error *);
mc_client_cx_status mc_client_cx_release(mc_client_source_cx **, mc_client_cx_error *);
mc_client_cx_status mc_client_cx_inspect(mc_client_source_cx *, mc_client_cx_state *,
                                         mc_client_cx_error *);

/* Complete actual cx.a()Z. Read the same namespace's static flag0. False returns
 * false20 without touching Display. True obtains that SAME already-current
 * Display's actual capabilities6 and reads GL_ARB_occlusion_query9, return20.
 * The Display is borrowed exclusively only for this call; it is not retained in
 * cx. There is no version shortcut, SDL hint override, GL error drain, event
 * pump, query generation or source flag setter. Failure preserves value.
 * Calls own a live counted occurrence and are exclusive per namespace. Native
 * monitor contention/reentry is BUSY; no source result is invented. */
mc_client_cx_status mc_client_cx_supported(mc_client_source_cx *, mc_client_display *, bool *value,
                                           mc_client_cx_error *);

#endif
