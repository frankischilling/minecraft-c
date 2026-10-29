#ifndef MC_CLIENT_STARTUP_APPLICATION_OWNER_INTERNAL_H
#define MC_CLIENT_STARTUP_APPLICATION_OWNER_INTERNAL_H

#include "../application_state.h"
#include "../gui/source_screen.h"
#include "../render/cx.h"
#include "application_owner.h"
#include "mc/client/gui/gui_ingame.h"
#include "mc/client/statistics/writer.h"
#include "mc/core/java/print_stream.h"
#include "mc/world/storage/java_file.h"

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
    mc_java_file *af;
    bool native_retiring;
    mc_java_file_error file_retirement;
    mc_client_stats_classes *startup_stats_classes; /* SAME outer xi literal namespace. */
    mc_client_stats_error stats_retirement;
    /* Cold associations; actual NEW697 and later NEWfu trigger active use. */
    mc_client_gui_ingame_classes *startup_gui_ingame_classes;
    mc_client_render_classes *startup_gui_ingame_models;
    mc_java_random_profile *startup_gui_ingame_profile;
    mc_client_gui_ingame_error gui_ingame_retirement;
    mc_client_screen_classes *startup_screen_classes;
    mc_java_random_profile *startup_screen_profile;
    const mc_registry_resources *startup_screen_resources;
    mc_registry_classes *startup_screen_registry;
    mc_client_screen_error screen_retirement;
    /* One selected System-output bootstrap per actual native class namespace.
     * This is a native service association, never Minecraft's source clinit. */
    mc_java_system_output *system_output;
    bool system_output_attempted;
    mc_java_print_error system_output_failure, output_retirement;
    /* Native cold association only. Actual new cx470 performs active use. */
    mc_client_cx_classes *startup_cx_classes;
    bool startup_cx_attempted;
    mc_client_cx_error cx_service_failure, cx_retirement;
    mc_player_string *startup_gl_labels[2]; /* Actual MC ldc395 and ldc464. */
    mc_player_string *startup_post_label;   /* Actual MC ldc691. */
    mc_player_string *startup_title;
    mc_player_string *startup_font_resource; /* Lazy actual MC ldc250, namespace-owned. */
    mc_player_string *startup_logo_resource; /* Lazy actual MC.x ldc119, namespace-owned. */
    mc_player_string
        *startup_color_literals[3]; /* Lazy MC ldc266/278/290, same source identities. */
    struct application_directory_classes *directory_classes;
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

void application_directory_classes_destroy(struct application_directory_classes *);

/* Concrete synchronization used by both the real worker and authored native
 * tests. Signaling without setting interrupt/cancel is a true spurious wake. */
int timer_hack_lock(mc_client_timer_hack *);
int timer_hack_unlock(mc_client_timer_hack *);
int timer_hack_signal(mc_client_timer_hack *);

#endif
