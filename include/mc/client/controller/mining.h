#ifndef MC_CLIENT_CONTROLLER_MINING_H
#define MC_CLIENT_CONTROLLER_MINING_H

#include "mc/client/controller/singleplayer.h"
#include "mc/entity/player/mining.h"
#include "mc/inventory/player_current.h"

struct mc_world_auxiliary_error;
struct mc_world_mutation_error;
struct mc_player_statistic_error;
struct mc_sound_error;
struct mc_sound_manager_report;

typedef struct mc_singleplayer_mining_call mc_singleplayer_mining_call;

typedef enum mc_singleplayer_mining_action {
    MC_SINGLEPLAYER_MINING_START,
    MC_SINGLEPLAYER_MINING_PROGRESS,
    MC_SINGLEPLAYER_MINING_DESTROY
} mc_singleplayer_mining_action;

typedef enum mc_singleplayer_mining_status {
    MC_SINGLEPLAYER_MINING_OK,
    MC_SINGLEPLAYER_MINING_ARGUMENT,
    MC_SINGLEPLAYER_MINING_BUSY,
    MC_SINGLEPLAYER_MINING_LIMIT,
    MC_SINGLEPLAYER_MINING_ALLOCATION,
    MC_SINGLEPLAYER_MINING_SOURCE_NULL,
    MC_SINGLEPLAYER_MINING_REGISTRY,
    MC_SINGLEPLAYER_MINING_WORLD,
    MC_SINGLEPLAYER_MINING_PLAYER,
    MC_SINGLEPLAYER_MINING_SOUND,
    MC_SINGLEPLAYER_MINING_DEPENDENCY,
    MC_SINGLEPLAYER_MINING_FLOATING
} mc_singleplayer_mining_status;

typedef struct mc_singleplayer_mining_error {
    mc_singleplayer_mining_status status;
    const char *operation;
    mc_player_site site;
    bool native_policy;
    mc_registry_error registry;
    mc_world_storage_error world;
    const mc_player_mining_error *player;
    const mc_inventory_current_error *inventory;
    const struct mc_world_auxiliary_error *auxiliary;
    const struct mc_world_mutation_error *mutation;
    const struct mc_player_statistic_error *statistic;
    const struct mc_sound_error *sound;
    const struct mc_sound_manager_report *sound_report;
} mc_singleplayer_mining_error;

typedef struct mc_singleplayer_mining_state {
    mc_singleplayer_mining_status status;
    mc_singleplayer_mining_action action;
    mc_client_singleplayer_controller *controller;
    const mc_singleplayer_mining_error *error;
    int32_t x, y, z, face, block_id, metadata;
    float strength;
    unsigned int source_pc;
    bool active, returned, changed_coordinate, progress_stored, sound_returned;
    bool auxiliary_returned, removal_returned, removed, harvest_allowed;
    bool statistic_returned, drop_returned, cooldown_stored;
} mc_singleplayer_mining_state;

/* Actual OS start/progress/destroy bodies on the same canonical controller.
 * A retained per-invocation receipt owns its controller occurrence and reached
 * diagnostic storage. Caller keeps the App/World/Player/Registry graph live and
 * exclusive through retirement, including failed WorldAccess/particle children.
 * Once allocated, *out receives the receipt even on failure. Prior source
 * stores, sound/particle/publication/file effects are never rolled back.
 * Nonempty tools and distinct Block hook overrides retain reached frontiers.
 * Destroy's Boolean return is state.removed, and is meaningful only on success. */
mc_singleplayer_mining_status mc_client_singleplayer_mining_begin(
    mc_client_singleplayer_controller *, mc_singleplayer_mining_action, int32_t x, int32_t y,
    int32_t z, int32_t face, mc_singleplayer_mining_call **out, mc_singleplayer_mining_error *);
bool mc_client_singleplayer_mining_inspect(const mc_singleplayer_mining_call *,
                                           mc_singleplayer_mining_state *);
mc_singleplayer_mining_status mc_client_singleplayer_mining_retire(mc_singleplayer_mining_call **,
                                                                   mc_singleplayer_mining_error *);

#endif
