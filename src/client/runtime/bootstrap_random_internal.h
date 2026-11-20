#ifndef MC_CLIENT_RUNTIME_BOOTSTRAP_RANDOM_INTERNAL_H
#define MC_CLIENT_RUNTIME_BOOTSTRAP_RANDOM_INTERNAL_H

#include "../../core/java/random_profile_internal.h"
#include "bootstrap.h"

typedef enum mc_client_bootstrap_random_mode {
    MC_CLIENT_BOOTSTRAP_RANDOM_REPLAY = 0,
    MC_CLIENT_BOOTSTRAP_RANDOM_RECORD
} mc_client_bootstrap_random_mode;

/* Value-only terminal receipt, separate from Source/open/close success. The
 * caller initializes present/profile_retired false and keeps this disjoint
 * output writable until the retained bootstrap is finally consumed. It is
 * written only at actual profile retirement, after the bootstrap's dependent
 * World/App/startup/SDK/service/worker/class owners have drained. A later close
 * refusal can retain the bootstrap after this same profile has been consumed;
 * retries never rewrite an already retired profile's receipt.
 *
 * Replay completion is checked here, after any real shutdown constructor. All
 * unused/exhausted/order failures stay visible; record capacity unused is not
 * replay input unused. No completion status changes the actual close result.
 * A harness must require present, profile_retired, successful binding/view/
 * completion statuses and its genuine Source/cleanup outcome independently.
 * A failed open before profile creation yields no profile-retirement receipt.
 */
typedef struct mc_client_bootstrap_random_terminal {
    bool present, profile_retired, binding_attempted;
    mc_client_bootstrap_random_mode mode;
    mc_java_random_profile_status binding_status, completion_status;
    mc_java_random_profile_status replay_view_status, record_view_status;
    mc_java_random_constructor_inputs_view replay;
    mc_java_random_constructor_records_view recording;
} mc_client_bootstrap_random_terminal;

typedef struct mc_client_bootstrap_random_config {
    mc_client_bootstrap_random_mode mode;

    union {
        struct {
            const mc_java_random_constructor_input *inputs;
            size_t count;
        } replay;

        struct {
            mc_java_random_constructor_record *records;
            size_t capacity;
        } record;
    } data;

    mc_client_bootstrap_random_terminal *terminal;
} mc_client_bootstrap_random_config;

/* Private observation entry points, the SAME entered bootstrap body as the
 * ordinary NULL-config calls. A nonNULL config must have a valid closed mode
 * and terminal output. Its selected binding is attempted immediately after
 * actual fresh profile_create, before any dependent service/App constructor.
 * Config itself is borrowed only through open; selected buffer/tape and the
 * terminal output remain caller-owned and live/disjoint through final close.
 * All profile binding/shape/quiescence contracts apply. Invalid config does
 * not silently fall back to normal startup. Failed native binding retains the
 * actual partial bootstrap for ordinary checked close; no input is repaired.
 *
 * Only Random constructor inputs are observed/replayed. Timer, frame clocks,
 * Source call order, constructor bodies, App/World fields and normal rendering
 * are unchanged. Source caller-role matching, all evolved RNG/Chunk/dirty-work
 * equality and any whole-frame attribution are separate, unfinished controls.
 * There is no global hook, callback, post-used binding, unbind or prefix mode.
 */
mc_client_bootstrap_status
mc_client_bootstrap_open_with_random_inputs(const mc_client_application_options *,
                                            const mc_client_bootstrap_random_config *,
                                            mc_client_bootstrap **);
mc_client_bootstrap_status
mc_client_bootstrap_open_standalone_with_random_inputs(const mc_client_application_options *,
                                                       const mc_client_bootstrap_random_config *,
                                                       mc_client_bootstrap **);

#endif
