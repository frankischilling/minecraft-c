#ifndef MC_CLIENT_STARTUP_APPLICATION_OWNER_H
#define MC_CLIENT_STARTUP_APPLICATION_OWNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct client_application client_application;
typedef struct mc_player_string mc_player_string;
typedef struct mc_client_application_classes mc_client_application_classes;
typedef struct mc_client_timer_hack mc_client_timer_hack;
typedef struct mc_java_file mc_java_file;
typedef struct mc_java_file_error mc_java_file_error;

typedef struct mc_client_stats_error mc_client_stats_error;

typedef struct mc_java_print_error mc_java_print_error;
typedef struct mc_client_cx_error mc_client_cx_error;

#define MC_CLIENT_APPLICATION_RESERVE_BYTES 10485760u
#define MC_CLIENT_APPLICATION_HISTORY_LENGTH 512u
#define MC_CLIENT_TIMER_HACK_SLEEP_MILLIS UINT32_C(2147483647)

typedef enum mc_client_application_owner_status {
    MC_CLIENT_APPLICATION_OWNER_OK,
    MC_CLIENT_APPLICATION_OWNER_INVALID_ARGUMENT,
    MC_CLIENT_APPLICATION_OWNER_INVALID_PHASE,
    MC_CLIENT_APPLICATION_OWNER_BUSY,
    MC_CLIENT_APPLICATION_OWNER_LIMIT,
    MC_CLIENT_APPLICATION_OWNER_ALLOCATION_FAILED,
    MC_CLIENT_APPLICATION_OWNER_THREAD_FAILED,
    MC_CLIENT_APPLICATION_OWNER_CLOCK_FAILED,
    MC_CLIENT_APPLICATION_OWNER_STRING_FAILED,
    MC_CLIENT_APPLICATION_OWNER_NULL_THREAD_NAME,
    MC_CLIENT_APPLICATION_OWNER_CLASS_ERRONEOUS,
    MC_CLIENT_APPLICATION_OWNER_FILE_FAILED,
    MC_CLIENT_APPLICATION_OWNER_STATS_FAILED,
    MC_CLIENT_APPLICATION_OWNER_OUTPUT_FAILED,
    MC_CLIENT_APPLICATION_OWNER_CX_FAILED
} mc_client_application_owner_status;

typedef struct mc_client_application_owner_site {
    const char *class_name, *method, *descriptor;
    unsigned int pc;
} mc_client_application_owner_site;

typedef struct mc_client_application_owner_error {
    mc_client_application_owner_status status;
    const char *operation;
    mc_client_application_owner_site site;
    int native_error;
    /* Complete typed cause retained by the failed namespace; borrowed through
     * retry and invalidated only on successful final namespace retirement. */
    const mc_java_file_error *file;
    const mc_client_stats_error *statistics;
    const mc_java_print_error *standard_output;
    const mc_client_cx_error *cx; /* Namespace-owned; re-inspect after the next class mutation. */
} mc_client_application_owner_error;

/* Native actual-App constructor state, separate from source J/o and the
 * component frontend. Source operations are unavailable in RETIRING. */
typedef enum mc_client_application_constructor_phase {
    MC_CLIENT_APP_RAW = 0,
    MC_CLIENT_APP_CONSTRUCTING,
    MC_CLIENT_APP_READY,
    MC_CLIENT_APP_FAILED,
    MC_CLIENT_APP_RETIRING
} mc_client_application_constructor_phase;

typedef enum mc_client_application_class_phase {
    MC_CLIENT_APPLICATION_CLASS_COLD,
    MC_CLIENT_APPLICATION_CLASS_INITIALIZING,
    MC_CLIENT_APPLICATION_CLASS_READY,
    MC_CLIENT_APPLICATION_CLASS_ERRONEOUS
} mc_client_application_class_phase;

typedef struct mc_client_application_class_options {
    /* Native retained array-payload budget, separate from JVM heap accounting.
     * Owner/synchronization setup is not one of the three source allocations. */
    size_t maximum_array_bytes, maximum_array_allocations;
} mc_client_application_class_options;

typedef struct mc_client_application_class_state {
    mc_client_application_class_phase phase;
    const uint8_t *b;
    const int64_t *E, *F;
    int32_t G;
    int64_t H;
    client_application *a;
    const mc_java_file *af; /* Actual canonical counted nullable File identity. */
    bool native_retiring;
    const mc_java_file_error *file_retirement;
    size_t b_length, E_length, F_length;
    size_t native_holds, array_bytes, array_allocations, allocation_attempts;
    unsigned int last_store_pc, last_global_store_pc;
    size_t native_app_borrows;
    const mc_player_string *timer_name, *applet_true, *applet_standalone;
    mc_client_application_owner_error failure;
} mc_client_application_class_state;

/* Cold native class-loader storage only. First active use executes the actual
 * Minecraft initializer, with three distinct zeroed arrays in source order.
 * Failed active use preserves completed stores and latches the native cause.
 * The latch implements class activation; no source CountDownLatch is invented.
 * Repeated erroneous use returns CLASS_ERRONEOUS with the first cause/site.
 * Success does not allocate or publish an App and does not activate children. */
mc_client_application_owner_status
mc_client_application_classes_create(const mc_client_application_class_options *,
                                     mc_client_application_classes **out,
                                     mc_client_application_owner_error *);
mc_client_application_owner_status
mc_client_application_classes_active_use(mc_client_application_classes *,
                                         mc_client_application_owner_error *);
mc_client_application_owner_status
mc_client_application_classes_retain(mc_client_application_classes *);
/* A successful occurrence release always consumes its pointer, including when
 * another native hold remains. BUSY/error preserves the pointer for retry.
 * Final native af File cleanup runs outside the class mutex and can retain a
 * terminal prefix: native_retiring then rejects new source use/retains, while
 * inspection and final-release retry stay available. No source af/a/J store. */
mc_client_application_owner_status
mc_client_application_classes_release(mc_client_application_classes **,
                                      mc_client_application_owner_error *);
bool mc_client_application_classes_inspect(mc_client_application_classes *,
                                           mc_client_application_class_state *);

typedef enum mc_client_application_lifetime_phase {
    MC_CLIENT_APPLICATION_LIFETIME_RAW,
    MC_CLIENT_APPLICATION_LIFETIME_REGISTERED,
    MC_CLIENT_APPLICATION_LIFETIME_RETIRED
} mc_client_application_lifetime_phase;

typedef struct mc_client_application_volatile_state {
    bool J, o;
    size_t native_worker_holds, native_static_holds, native_callback_holds;
    size_t native_effect_borrows;
    size_t native_resource_thread_borrows;
    size_t native_screen_borrows;
} mc_client_application_volatile_state;

/* Register the unique real App storage before any atomic access. The App may
 * be stack-owned or heap-owned; this does not allocate or assume a refcounted
 * App owner. Its native phase must have its real zero/default initialization.
 * atomic_init establishes actual source-default false J/o, not constructor
 * stores. The constructor/run later perform their actual atomic stores.
 * Owner registration/retirement/worker construction require exclusive App use.
 * The borrowed App must remain allocated until retirement succeeds. */
mc_client_application_owner_status
mc_client_application_lifetime_register(client_application *, mc_client_application_owner_error *);
/* No source J/o store. BUSY preserves the registered App while a retained kg
 * owner or Registry-owned kh can still access it. Successful retirement is one-shot and allocation
 * free; source field values remain inspectable in their live atomic storage. */
mc_client_application_owner_status
mc_client_application_lifetime_retire(client_application *, mc_client_application_owner_error *);
bool mc_client_application_volatile_inspect(const client_application *,
                                            mc_client_application_volatile_state *);

/* Native namespace borrows are NOT counted class ownership. Every actual
 * RAW/READY/FAILED App keeps one until RETIRING ends all source/worker use.
 * Final class release stays BUSY (without consuming its final occurrence) while
 * any borrow remains, including a displaced earlier App. External runtime keeps
 * the namespace through all source work; caller operations are exclusive. */
mc_client_application_owner_status
mc_client_application_namespace_attach(mc_client_application_classes *, client_application *,
                                       mc_client_application_owner_error *);
mc_client_application_owner_status
mc_client_application_namespace_detach(client_application *, mc_client_application_owner_error *);
/* Borrow the one lazily resolved class literal at actual ldc211. No Thread,
 * source App field, clock, Random or general String intern table is created. */
mc_client_application_owner_status
mc_client_application_classes_timer_name(mc_client_application_classes *, mc_player_string **out,
                                         mc_client_application_owner_error *);
/* Same class-literal namespace, reached only on actual nonnull-Applet branch.
 * Resolve immutable 'true'244 then 'stand-alone'247 in that source order. */
mc_client_application_owner_status
mc_client_application_classes_applet_true(mc_client_application_classes *, mc_player_string **out,
                                          mc_client_application_owner_error *);
mc_client_application_owner_status mc_client_application_classes_applet_standalone(
    mc_client_application_classes *, mc_player_string **out, mc_client_application_owner_error *);
/* Actual Minecraft.<init> putstatic a264. Acquire new real App static hold,
 * store, then release only old static hold (same identity included). Earlier
 * failures preserve old a. Only the real constructing registered App on this
 * namespace is admitted. No native teardown writes source a=NULL or J=false. */
mc_client_application_owner_status
mc_client_application_classes_publish(mc_client_application_classes *, client_application *,
                                      mc_client_application_owner_error *);

typedef enum mc_client_timer_hack_phase {
    MC_CLIENT_TIMER_HACK_RAW,
    MC_CLIENT_TIMER_HACK_CONSTRUCTING,
    MC_CLIENT_TIMER_HACK_READY,
    MC_CLIENT_TIMER_HACK_FAILED,
    MC_CLIENT_TIMER_HACK_RETIRING
} mc_client_timer_hack_phase;

typedef struct mc_client_timer_hack_state {
    mc_client_timer_hack_phase phase;
    client_application *a;
    const mc_player_string *name;
    bool daemon, started, sleeping, completed, joined;
    bool interrupted, native_cancel_requested, native_cancel_observed;
    bool source_J_false_observed;
    size_t J_reads, sleep_calls, interrupted_exceptions, native_spurious_wakes;
    unsigned int last_constructor_pc, last_run_pc;
    uint32_t sleep_millis;
    mc_client_application_owner_error failure;
} mc_client_timer_hack_state;

/* RAW storage and native wait setup only. The one-shot normal kg body takes
 * the real registered App, stores a2, owns the supplied Thread-name identity
 * through the lower Thread(String)7 role, stores daemon12 and starts a genuine
 * joinable native worker at16. Name may be any existing immutable String.
 * Null name reports the reached Thread(String) boundary; no dummy name is used.
 * No source App field receives kg; its discarded expression is retained only
 * by the native attempt until allocation-free retirement completes. */
mc_client_application_owner_status
mc_client_timer_hack_allocate(mc_client_timer_hack **, mc_client_application_owner_error *);
mc_client_application_owner_status
mc_client_timer_hack_construct(mc_client_timer_hack *, client_application *, mc_player_string *name,
                               mc_client_application_owner_error *);
/* Native controlled interrupt of an actually started worker. Its real sleep
 * clears the interrupt and follows kg.run's handler19 before loading J again.
 * This finite seam does not claim general java.lang.Thread interoperability. */
mc_client_application_owner_status
mc_client_timer_hack_interrupt(mc_client_timer_hack *, mc_client_application_owner_error *);
bool mc_client_timer_hack_inspect(mc_client_timer_hack *, mc_client_timer_hack_state *);
/* Native cancellation is separate from source J/o and source interruption.
 * It wakes and joins the worker before releasing the actual App borrow and
 * String. BUSY/failure preserves all remaining owner state for retry. Source
 * daemon=true never detaches a native thread whose lifetime must be joined.
 * stop leaves the joined owner and its source fields/holds inspectable.
 * External operations on one worker require exclusive caller access; its real
 * worker thread is the only concurrently executing owner operation. */
mc_client_application_owner_status mc_client_timer_hack_stop(mc_client_timer_hack *,
                                                             mc_client_application_owner_error *);
mc_client_application_owner_status mc_client_timer_hack_retire(mc_client_timer_hack **,
                                                               mc_client_application_owner_error *);

#endif
