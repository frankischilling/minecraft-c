#ifndef MC_CLIENT_TIMER_CONSTRUCTION_H
#define MC_CLIENT_TIMER_CONSTRUCTION_H

#include "mc/client/timer/timer.h"

#include "../../platform/time/millis.h"
#include "../../platform/time/nanotime.h"

#include <stddef.h>

/* Private actual py constructor seam; this is not a public App wrapper. */
typedef enum mc_timer_native_phase {
    MC_TIMER_NATIVE_RAW = 0,
    MC_TIMER_NATIVE_CONSTRUCTING,
    MC_TIMER_NATIVE_READY,
    MC_TIMER_NATIVE_FAILED,
    MC_TIMER_NATIVE_RETIRING
} mc_timer_native_phase;

typedef enum mc_timer_constructor_origin {
    MC_TIMER_ORIGIN_RAW = 0,
    MC_TIMER_ORIGIN_LEGACY_SAMPLES,
    MC_TIMER_ORIGIN_SYSTEM,
    MC_TIMER_ORIGIN_OBSERVED
} mc_timer_constructor_origin;

typedef enum mc_timer_construction_status {
    MC_TIMER_CONSTRUCTION_OK = 0,
    MC_TIMER_CONSTRUCTION_INVALID_ARGUMENT,
    MC_TIMER_CONSTRUCTION_ALLOCATION_LIMIT,
    MC_TIMER_CONSTRUCTION_ALLOCATION_FAILED,
    MC_TIMER_CONSTRUCTION_WRONG_PHASE,
    MC_TIMER_CONSTRUCTION_WALL_FAILED,
    MC_TIMER_CONSTRUCTION_NANO_FAILED,
    MC_TIMER_CONSTRUCTION_BUSY
} mc_timer_construction_status;

typedef enum mc_timer_clock_stage {
    MC_TIMER_CLOCK_NONE = 0,
    MC_TIMER_CLOCK_WALL,
    MC_TIMER_CLOCK_NANO_INIT,
    MC_TIMER_CLOCK_NANO_SAMPLE
} mc_timer_clock_stage;

typedef struct mc_timer_construction_error {
    mc_timer_construction_status status;
    int32_t py_pc; /* -1 for native admission/allocation; actual call25/32 otherwise. */
    mc_timer_clock_stage stage;
    mc_platform_millis_status wall;
    mc_platform_nanotime_status nano;
    size_t allocation_bytes; /* Native policy only, not Java allocation size. */
} mc_timer_construction_error;

typedef struct mc_timer_constructor_fields {
    float a; /* rate */
    int32_t b;
    float c, d, e;
    double f;
    int64_t g, h, i;
    double j;
} mc_timer_constructor_fields;

typedef struct mc_timer_constructor_state {
    mc_timer_native_phase phase;
    mc_timer_constructor_origin origin;
    mc_timer_constructor_fields fields;
    int32_t last_store_pc;
    size_t store_count, clock_call_count, nano_init_calls;
    int32_t stores[6], clock_calls[2];
    bool nano_initialized_by_call;
    mc_timer_construction_error failure;
} mc_timer_constructor_state;

/* Declared primitive clock observations only. This form makes no OS query or
 * platform conversion/FP check. Status/value is read only at its actual25/32
 * call site; a failed wall call never reads the later nano observation. These
 * are native fixture inputs, not a claim to have sampled a real OS/JVM clock.
 * All status fields must be valid listed enum values. Failure values are not read. */
typedef struct mc_timer_observed_constructor_clocks {
    mc_platform_millis_status wall_status;
    int64_t wall_millis;
    mc_platform_nanotime_status nano_status;
    int64_t nano_time;
} mc_timer_observed_constructor_clocks;

/* Unique/exclusive owner, as with the legacy Timer. One zeroed allocation;
 * no clock, FP admission, source constructor store or child active use. A zero
 * limit admits the one native allocation. Output is required, unchanged on
 * failure, and must not overwrite another live owner occurrence on success. */
mc_timer_construction_status mc_timer_allocate_raw(size_t allocation_limit, mc_timer **out,
                                                   mc_timer_construction_error *error);

/* Actual source stores6/11/16/21; wall25/g28; nano32/ldiv38/h39. Construct once.
 * Production uses real platform clocks. The borrowed defined nanotime context
 * is examined only at32; a false initialized flag triggers the real native
 * prerequisite there, followed by one fresh sample. NULL reaches that late
 * native failure after g28. No context borrow survives the call. Initialization
 * effects are native prerequisites, not source System class initialization.
 * A context that may initialize is exclusively borrowed. An already initialized
 * context stays immutable, as required by the actual platform clock owner.
 * Source constructor arithmetic itself has no FP admission: production's first
 * native FP check is the reached nanotime init/sample, after wall25/g28.
 * Caller FP controls must stay stable through each real platform call.
 * Failure retains every preceding field/clock effect, including initialized
 * native clock state. It cannot retry; it remains independently inspectable. */
mc_timer_construction_status mc_timer_construct_system(mc_timer *, float rate,
                                                       mc_platform_nanotime *,
                                                       mc_timer_construction_error *);
mc_timer_construction_status
mc_timer_construct_observed(mc_timer *, float rate, const mc_timer_observed_constructor_clocks *,
                            mc_timer_construction_error *);

/* Full actual fields, including independent c/e. Inspect never samples clocks
 * or changes phases. Legacy create has READY/supplied-sample origin with no
 * manufactured ordered-call trace. Writable outputs are valid/aligned and
 * disjoint from owners, context, observations, input slots and each other. */
bool mc_timer_constructor_inspect(const mc_timer *, mc_timer_constructor_state *);

/* Clock/allocation-free retirement. Successful release consumes/nulls the
 * unique passed occurrence. NULL *slot is harmless. A live construction is
 * BUSY and preserves it. No concurrent/reentrant owner operations are admitted.
 * All other pointer borrows end before retirement. Old free retains its
 * exclusive unique-owner contract and also samples no clock. */
mc_timer_construction_status mc_timer_retire(mc_timer **, mc_timer_construction_error *);

#endif
