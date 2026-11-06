#ifndef MC_CLIENT_STARTUP_APPLICATION_EFFECTS_H
#define MC_CLIENT_STARTUP_APPLICATION_EFFECTS_H

#include "../render/cx.h"
#include "../render_engine/effects.h"
#include "mc/client/sound/sound_manager.h"

#define MC_CLIENT_STARTUP_EFFECT_CALLS 9u

/* One actual operand/call prefix. engine owns a native occurrence until
 * cleanup; argument is borrowed, owned is a separate new-expression occurrence.
 * Historical snapshots contain scalars only and survive consumed owners. */
typedef struct mc_client_startup_effect_call {
    mc_client_render_engine *engine;
    mc_client_texture_effect *argument, *owned;
    mc_texture_effect_kind kind;
    unsigned int receiver_pc, new_pc, constructor_pc, invocation_pc;
    bool receiver_captured, allocated, constructed, registration_entered;
    bool null_receiver, container_retired, snapshot_valid;
    mc_texture_effect_phase observed_phase;
    int32_t observed_counter, observed_last_store;
    size_t observed_holds;
    mc_client_render_engine_status engine_hold_error, engine_release_error;
    mc_client_effect_registration_error registration, cleanup;
    mc_texture_effect_error construction, retirement;
} mc_client_startup_effect_call;

struct mc_client_startup_effects_call {
    bool active, entered, S_stored, sound_entered, sound_returned;
    bool sound_settings_borrow, sound_hook_attached, retired;
    bool null_sound_receiver, lava_trig_bound;
    unsigned int last_pc, registrations_completed;
    mc_client_cx_classes *cx_classes; /* Borrowed from SAME MC namespace. */
    mc_client_source_cx *cx_candidate;
    mc_client_cx_error cx, cx_cleanup;
    mc_sound_manager *sound;          /* Actual B481, constructor-owned, never retired here. */
    mc_game_settings *sound_settings; /* Actual z485, protected until detach. */
    mc_sound_manager_report sound_report;
    mc_sound_error sound_error, sound_cleanup;
    mc_game_settings_error sound_hook_error;
    mc_lava_texture_error lava;
    mc_client_startup_effect_call effects[MC_CLIENT_STARTUP_EFFECT_CALLS];
};

#endif
