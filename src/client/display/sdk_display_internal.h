#ifndef MC_CLIENT_DISPLAY_SDK_DISPLAY_INTERNAL_H
#define MC_CLIENT_DISPLAY_SDK_DISPLAY_INTERNAL_H

#include "display.h"
#include "events.h"

typedef struct mc_client_sdk_input mc_client_sdk_input;
typedef struct mc_client_sdk_update mc_client_sdk_update;

/* Canonical owner facts, never inferred from an arbitrary nonnull SDL window.
 * Helpers are private to the real Display/update/input composition. */
bool mc_client_display_sdk_created(const mc_client_display *);
bool mc_client_display_sdk_live(const mc_client_display *);
bool mc_client_display_sdk_operation_busy(const mc_client_display *);
bool mc_client_display_sdk_external_busy(const mc_client_display *);
bool mc_client_display_sdk_resized(mc_client_display *, bool *, mc_client_display_error *);
bool mc_client_display_sdk_extent(mc_client_display *, bool width, int32_t *,
                                  mc_client_display_error *);
bool mc_client_display_sdk_source_extent(const mc_client_display *, bool width, int32_t *);
bool mc_client_display_sdk_input_candidate(const mc_client_display *, mc_client_sdk_input **);
mc_client_sdk_input *mc_client_display_inputs(mc_client_display *);
mc_client_sdk_update *mc_client_display_update_owner(mc_client_display *);
bool mc_client_display_sdk_update_candidate(const mc_client_display *, mc_client_sdk_update **);
bool mc_client_display_sdk_swap(mc_client_display *, bool *completed, mc_client_display_error *);
mc_client_event_port *mc_client_display_event_port(mc_client_display *);

#endif
