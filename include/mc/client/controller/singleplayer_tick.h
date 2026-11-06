#ifndef MC_CLIENT_CONTROLLER_SINGLEPLAYER_TICK_H
#define MC_CLIENT_CONTROLLER_SINGLEPLAYER_TICK_H

#include "mc/client/controller/singleplayer.h"

struct mc_sound_error;
struct mc_sound_manager;
struct mc_sound_manager_report;
typedef struct mc_singleplayer_tick_call mc_singleplayer_tick_call;

typedef enum mc_singleplayer_tick_status {
    MC_SINGLEPLAYER_TICK_OK,
    MC_SINGLEPLAYER_TICK_INVALID_ARGUMENT,
    MC_SINGLEPLAYER_TICK_INVALID_PHASE,
    MC_SINGLEPLAYER_TICK_BUSY,
    MC_SINGLEPLAYER_TICK_LIMIT,
    MC_SINGLEPLAYER_TICK_ALLOCATION_FAILED,
    MC_SINGLEPLAYER_TICK_CONTROLLER_FAILED,
    MC_SINGLEPLAYER_TICK_SOURCE_NULL_REFERENCE,
    MC_SINGLEPLAYER_TICK_SOUND_FAILED
} mc_singleplayer_tick_status;

/* Small value record. The complete YO children belong to this particular
 * retained call, rather than ambient manager/controller diagnostic storage.
 * A native policy failure is never an invented Java Throwable. child is empty
 * when the SoundManager rejected native admission before its source body. */
typedef struct mc_singleplayer_tick_error {
    mc_singleplayer_tick_status status;
    const char *operation;
    bool native_policy;
    mc_player_site site, child;
    mc_singleplayer_controller_status controller;
    const struct mc_sound_error *sound;
    const struct mc_sound_manager_report *sound_report;
} mc_singleplayer_tick_error;

typedef struct mc_singleplayer_tick_state {
    mc_singleplayer_tick_status status;
    unsigned int source_pc;
    bool active, g_stored, sound_called, sound_returned, source_returned;
    mc_client_singleplayer_controller *controller; /* Borrowed, retained by call. */
    client_application *app;                       /* Actual ob.a read at9. */
    struct mc_sound_manager *sound_manager;        /* Actual App.B read at12. */
    const mc_singleplayer_tick_error *error;
    const struct mc_sound_error *sound;
    const struct mc_sound_manager_report *sound_report;
} mc_singleplayer_tick_state;

/* Execute actual os.c()V once: copy f to g at5 without FP arithmetic, read
 * inherited App at9/B at12, invoke actual yo.c at15, return18. Null App/B fail
 * at12/15 after the g store. Allocation, READY/entered admission and one native
 * controller retain precede source effects. Once allocated, *owned_out receives
 * the call even on failure; inspect and retire it, never replay it to resume.
 * Before allocation, failure preserves *owned_out and fills admission_error.
 * On an allocated call admission_error receives its small terminal value too;
 * its borrowed children expire with that call. owned_out must start NULL;
 * a nonempty slot is rejected before source effects and remains unchanged.
 * All outputs must be disjoint.
 *
 * External exclusive access is required. The existing controller entered guard
 * spans SoundManager/backend callbacks. The call owns only its ordinary native
 * controller occurrence. Actual App/B/Settings/shared/backend remain borrowed:
 * the caller keeps the same complete graph live and exclusive through retire.
 * No App field, Random, SoundManager or backend owner is replaced or mirrored. */
mc_singleplayer_tick_status
mc_client_singleplayer_controller_tick_begin(mc_client_singleplayer_controller *,
                                             mc_singleplayer_tick_call **owned_out,
                                             mc_singleplayer_tick_error *admission_error);
bool mc_client_singleplayer_controller_tick_inspect(const mc_singleplayer_tick_call *,
                                                    mc_singleplayer_tick_state *);
/* Checked native cleanup only; no source tick/reset/save is invoked. An active
 * callback or entered controller leaves the call untouched and returns BUSY.
 * Other ordinary controller holds do not block dropping this call's occurrence.
 * Success consumes the call and invalidates all its borrowed diagnostic views.
 * Cleanup reports separately and never overwrites the immutable terminal error.
 * A NULL slot value is harmless. */
mc_singleplayer_tick_status
mc_client_singleplayer_controller_tick_retire(mc_singleplayer_tick_call **,
                                              mc_singleplayer_tick_error *cleanup_error);

/* Exact os.a()V: f=positive zero at2, i=0 at7, return10. This allocation-free
 * operation changes no other source field, including g/h and inherited App/b.
 * Native READY/entered admission precedes both stores. It neither reads App/B
 * nor calls SoundManager, and needs no heap or source-child diagnostic frame. */
mc_singleplayer_tick_status
mc_client_singleplayer_controller_reset(mc_client_singleplayer_controller *,
                                        mc_singleplayer_tick_error *);

#endif
