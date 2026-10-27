#ifndef MC_CLIENT_STARTUP_APPLICATION_OWNER_INTERNAL_H
#define MC_CLIENT_STARTUP_APPLICATION_OWNER_INTERNAL_H

#include "../application_state.h"
#include "application_owner.h"

#include <stdatomic.h>
#include <threads.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif

struct mc_client_application_classes {
    mtx_t mutex;
    mc_client_application_class_phase phase;
    uint8_t *b;
    int64_t *E, *F;
    int32_t G;
    int64_t H;
    client_application *a;
    void *af;
    size_t holds, array_bytes, array_allocations, allocation_attempts;
    size_t native_app_borrows;
    mc_player_string *timer_name;
    mc_player_string *applet_true, *applet_standalone;
    unsigned int last_global_store_pc;
    mc_client_application_class_options options;
    unsigned int last_store_pc;
    mc_client_application_owner_error failure;
};

struct mc_client_timer_hack {
#ifdef _WIN32
    SRWLOCK mutex;
    CONDITION_VARIABLE changed;
#else
    pthread_mutex_t mutex;
    pthread_cond_t changed;
#endif
    thrd_t thread;
    mc_client_timer_hack_phase phase;
    client_application *a;
    mc_player_string *name;
    bool daemon, started, sleeping, completed, joined;
    bool interrupted, native_cancel_requested, native_cancel_observed;
    bool source_J_false_observed, retiring, condition_destroyed;
    size_t J_reads, sleep_calls, interrupted_exceptions, native_spurious_wakes;
    unsigned int last_constructor_pc, last_run_pc;
    mc_client_application_owner_error failure;
};

mc_client_application_owner_status application_owner_fail(mc_client_application_owner_error *,
                                                          mc_client_application_owner_status,
                                                          const char *, const char *, const char *,
                                                          const char *, unsigned int, int);
mc_client_application_owner_status application_worker_hold(client_application *,
                                                           mc_client_application_owner_error *);
void application_worker_release(client_application *);
mc_client_application_owner_status application_static_hold(client_application *,
                                                           mc_client_application_owner_error *);
void application_static_release(client_application *);

/* Concrete synchronization used by both the real worker and authored native
 * tests. Signaling without setting interrupt/cancel is a true spurious wake. */
int timer_hack_lock(mc_client_timer_hack *);
int timer_hack_unlock(mc_client_timer_hack *);
int timer_hack_signal(mc_client_timer_hack *);

#endif
