#ifndef MC_TEST_PRINT_STREAM_REBIND_OBSERVER_H
#define MC_TEST_PRINT_STREAM_REBIND_OBSERVER_H
#include "mc/core/java/print_stream.h"

/* Qualification-only, real native writes followed by a genuine namespace
 * mutation. Callback can attempt retirement/reentry of its real source caller. */
void print_observer_install(mc_java_print_stream *, mc_java_system_output *, mc_java_print_stream *,
                            unsigned int write_number, void (*observe)(void *), void *);
bool print_observer_rebound(void);
void print_observer_clear(mc_java_print_stream *);

typedef struct print_observer_monitor_hold print_observer_monitor_hold;
/* A real separate thread locks the actual canonical owner's monitor until
 * release joins it. These borrow the owner; the caller must keep every owner
 * occurrence live through release. No source/GL operation runs on the worker. */
bool print_observer_hold_stream(mc_java_print_stream *, print_observer_monitor_hold **);
bool print_observer_hold_system(mc_java_system_output *, print_observer_monitor_hold **);
bool print_observer_release_monitor(print_observer_monitor_hold **);
#endif
