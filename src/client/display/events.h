#ifndef MC_CLIENT_DISPLAY_EVENTS_H
#define MC_CLIENT_DISPLAY_EVENTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct SDL_Window SDL_Window;
typedef union SDL_Event SDL_Event;

typedef struct mc_client_event_port mc_client_event_port;
typedef struct mc_client_event_record mc_client_event_record;

typedef enum mc_client_events_status {
    MC_CLIENT_EVENTS_OK,
    MC_CLIENT_EVENTS_INVALID_ARGUMENT,
    MC_CLIENT_EVENTS_BUSY,
    MC_CLIENT_EVENTS_ALLOCATION_FAILED,
    MC_CLIENT_EVENTS_LIMIT,
    MC_CLIENT_EVENTS_SDL_FAILED,
    MC_CLIENT_EVENTS_UNSUPPORTED_PAYLOAD
} mc_client_events_status;

typedef struct mc_client_events_error {
    mc_client_events_status status;
    const char *operation;
    size_t records_retained, bytes_retained, records_transferred;
    size_t records_retired;
    uint32_t pending_type;
    bool pump_completed;
    char detail[256];
} mc_client_events_error;

typedef struct mc_client_events_state {
    size_t ports, records, bytes, tickets;
    uint64_t pump_generation;
    bool active, dirty, resized;
    int width, height;
    size_t port_tickets;
} mc_client_events_state;

/* A single native SDL queue owner on the main/video thread. Attach only actual
 * live windows; several idle Displays share this same owner. No other module
 * may Pump/Poll/GET/flush/filter-remove SDL events. SDL pushers may append.
 * The first attach fixes the native byte budget (default8MiB); this is not the
 * original SDK's 200-record EventQueue or its overflow policy. */
mc_client_events_status mc_client_events_attach(SDL_Window *, size_t max_bytes,
                                                mc_client_event_port **out,
                                                mc_client_events_error *);
/* One explicit pump, followed by a bounded snapshot of pending SDL records.
 * Deep copy/reserve precedes GET. On a later failure, earlier records and the
 * still-queued SDL head remain owned. No input consumer runs here. */
mc_client_events_status mc_client_events_pump(mc_client_event_port *, mc_client_events_error *);
/* Same ownership transfer, without PumpEvents. Use before a native API whose
 * documented internal flush would otherwise destroy still-pending records.
 * This is not a source processMessages call and does not advance generation. */
mc_client_events_status mc_client_events_retain_pending(mc_client_event_port *,
                                                        mc_client_events_error *);
/* No pump. Take the first matching type for this window or global event.
 * all_windows is native terminal-drain only and requires this sole port.
 * Success with NULL *out means empty. A returned occurrence owns all copied
 * payloads until release; its actual SDL_Event is a borrowed immutable view. */
mc_client_events_status mc_client_events_take(mc_client_event_port *, uint32_t first, uint32_t last,
                                              bool all_windows, mc_client_event_record **out,
                                              mc_client_events_error *);
const SDL_Event *mc_client_event_read(const mc_client_event_record *);
/* Logical window extent at this record's retained queue position. A real
 * preceding resize updates it; stale/global/foreign-window records have none. */
bool mc_client_event_window_extent(const mc_client_event_record *, int *width, int *height);
void mc_client_event_release(mc_client_event_record **);
bool mc_client_events_inspect(const mc_client_event_port *, mc_client_events_state *);
bool mc_client_events_take_dirty(mc_client_event_port *);
bool mc_client_events_take_resized(mc_client_event_port *);
bool mc_client_events_take_close(mc_client_event_port *);
/* No implicit event discard. Active use, tickets and queued records for this
 * port (or any records for the last port) preserve its occurrence on BUSY. */
mc_client_events_status mc_client_events_detach(mc_client_event_port **, mc_client_events_error *);
/* Explicit terminal native-owner cleanup. Unlike detach, consumes only this
 * retiring window's queued occurrences (all remaining records for the final
 * port), reports their count, and preserves every other port's records. No
 * ordinary update/input call uses this operation. Tickets/active use are BUSY. */
mc_client_events_status mc_client_events_retire(mc_client_event_port **, mc_client_events_error *);

#endif
