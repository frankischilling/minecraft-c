#ifndef MC_CLIENT_RENDER_ENGINE_SOURCE_H
#define MC_CLIENT_RENDER_ENGINE_SOURCE_H
#include "../display/display.h"

typedef enum mc_client_graphics_status {
    MC_CLIENT_GRAPHICS_OK,
    MC_CLIENT_GRAPHICS_INVALID_ARGUMENT,
    MC_CLIENT_GRAPHICS_INVALID_PHASE,
    MC_CLIENT_GRAPHICS_LIMIT,
    MC_CLIENT_GRAPHICS_ALLOCATION_FAILED,
    MC_CLIENT_GRAPHICS_BUSY,
    MC_CLIENT_GRAPHICS_NULL_REFERENCE,
    MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_ARGUMENT,
    MC_CLIENT_GRAPHICS_SOURCE_ILLEGAL_STATE,
    MC_CLIENT_GRAPHICS_ARRAY_INDEX,
    MC_CLIENT_GRAPHICS_BUFFER_OVERFLOW,
    MC_CLIENT_GRAPHICS_CLASS_FAILED,
    MC_CLIENT_GRAPHICS_UNSUPPORTED_FP,
    MC_CLIENT_GRAPHICS_DISPLAY_FAILED,
    MC_CLIENT_GRAPHICS_UNIMPLEMENTED_DEPENDENCY
} mc_client_graphics_status;

typedef struct mc_client_graphics_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_client_graphics_site;

typedef struct mc_client_graphics_cause {
    mc_client_graphics_status status;
    const char *operation;
    mc_client_graphics_site caller, site;
    bool native_policy;
} mc_client_graphics_cause;

typedef struct mc_client_graphics_error {
    mc_client_graphics_status status;
    const char *operation;
    mc_client_graphics_site caller, site, entry, parent;
    mc_client_graphics_cause class_cause;
    mc_client_graphics_cause dependency_class_cause;
    mc_client_display_error display;
    mc_client_display_error cleanup_display;
    int32_t actual_index;
    size_t retained_count;
    bool native_policy, generated, first_append, second_append, cleanup_failed;
} mc_client_graphics_error;

static inline mc_client_graphics_status
mc_client_graphics_fail(mc_client_graphics_error *e, mc_client_graphics_status status,
                        const char *operation, mc_client_graphics_site site, bool native_policy) {
    e->status = status;
    e->operation = operation;
    e->site = site;
    e->native_policy = native_policy;
    return status;
}
#endif
