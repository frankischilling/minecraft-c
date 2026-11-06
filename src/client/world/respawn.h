#ifndef MC_CLIENT_WORLD_RESPAWN_H
#define MC_CLIENT_WORLD_RESPAWN_H

#include "load_world.h"

typedef struct mc_client_respawn_call mc_client_respawn_call;
typedef struct mc_world_respawn_error mc_world_respawn_error;
typedef struct mc_client_display_screen_call mc_client_display_screen_call;
typedef struct mc_world_entity_tick_error mc_world_entity_tick_error;

typedef enum mc_client_respawn_status {
    MC_CLIENT_RESPAWN_OK,
    MC_CLIENT_RESPAWN_ARGUMENT,
    MC_CLIENT_RESPAWN_BUSY,
    MC_CLIENT_RESPAWN_LIMIT,
    MC_CLIENT_RESPAWN_ALLOCATION,
    MC_CLIENT_RESPAWN_NULL_REFERENCE,
    MC_CLIENT_RESPAWN_CHILD_FAILED,
    MC_CLIENT_RESPAWN_UNIMPLEMENTED,
    MC_CLIENT_RESPAWN_CLEANUP_FAILED
} mc_client_respawn_status;

typedef struct mc_client_respawn_error {
    mc_client_respawn_status status;
    const char *operation;
    mc_client_world_load_site site, child;
    bool native_policy;
    const mc_world_respawn_error *world;
    const mc_world_entity_tick_error *entities;
    const mc_singleplayer_controller_error *controller;
    const mc_player_spawn_error *spawn;
    const mc_player_input_error *player_input;
    const mc_world_player_load_error *player_load;
    const mc_movement_input_error *input;
    const mc_client_world_prepare_state *terrain;
    const mc_client_display_screen_call *display;
} mc_client_respawn_error;

typedef struct mc_client_respawn_state {
    bool active, retiring, source_returned, wrapper_entered, wrapper_returned;
    bool force, old_player_removed, player_created, player_stored, view_stored;
    bool spawn_returned, prepared, player_inserted, input_stored, id_stored;
    bool player_finished, terrain_returned, death_screen_closed;
    int32_t dimension, previous_id;
    uintptr_t previous_player_identity, player_identity;
    const mc_client_respawn_error *failure, *cleanup;
} mc_client_respawn_state;

/* Actual Minecraft.a(ZI)V on the active Load's SAME App/World graph. The
 * DC.p_ wrapper captures its actual App and invokes false/zero. Both run
 * synchronously, preserve every reached store on failure and return a retained
 * call whenever admission allocated one. No source action is replayed by
 * cleanup. The actual final instanceof-CH branch invokes display(NULL)369;
 * CH's own handler still performs its separate later display(NULL)30.
 *
 * The active Load owns published Player/Input occurrences, including failed
 * replacement prefixes. Its inspectors expose the current graph after a
 * replacement; earlier borrowed snapshots require their own retained owners
 * across a subsequent source mutation. Failed constructor owners stay with the
 * actual Controller. These calls neither invent a World nor reload a save.
 */
mc_client_respawn_status mc_client_respawn_begin(client_application *, mc_client_world_load_call *,
                                                 bool force, int32_t dimension,
                                                 mc_client_respawn_call **,
                                                 mc_client_respawn_error *);
mc_client_respawn_status mc_client_respawn_player_begin(mc_entity *actual_player,
                                                        mc_client_world_load_call *,
                                                        mc_client_respawn_call **,
                                                        mc_client_respawn_error *);
bool mc_client_respawn_inspect(const mc_client_respawn_call *, mc_client_respawn_state *);
/* End this invocation's native child leases before Load terminal admission.
 * A failed cleanup preserves the same output slot for retry; success consumes
 * the call and invalidates its borrowed diagnostic/child views.
 * No screen, World/Player store, inventory, save or source retry occurs. */
mc_client_respawn_status mc_client_respawn_retire(mc_client_respawn_call **,
                                                  mc_client_respawn_error *);

#endif
