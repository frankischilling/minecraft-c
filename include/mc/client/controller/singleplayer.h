#ifndef MC_CLIENT_CONTROLLER_SINGLEPLAYER_H
#define MC_CLIENT_CONTROLLER_SINGLEPLAYER_H

#include "mc/client/gui/achievement_notice.h"
#include "mc/entity/player/player.h"
#include "mc/world/dimension/world_provider.h"

typedef struct mc_client_singleplayer_controller mc_client_singleplayer_controller;

typedef enum mc_singleplayer_controller_status {
    MC_SINGLEPLAYER_CONTROLLER_OK,
    MC_SINGLEPLAYER_CONTROLLER_INVALID_ARGUMENT,
    MC_SINGLEPLAYER_CONTROLLER_INVALID_PHASE,
    MC_SINGLEPLAYER_CONTROLLER_BUSY,
    MC_SINGLEPLAYER_CONTROLLER_LIMIT,
    MC_SINGLEPLAYER_CONTROLLER_ALLOCATION_FAILED,
    MC_SINGLEPLAYER_CONTROLLER_NULL_REFERENCE,
    MC_SINGLEPLAYER_CONTROLLER_ENTITY_FAILED,
    MC_SINGLEPLAYER_CONTROLLER_WORLD_FAILED,
    MC_SINGLEPLAYER_CONTROLLER_PLAYER_FAILED
} mc_singleplayer_controller_status;

typedef enum mc_singleplayer_controller_phase {
    MC_SINGLEPLAYER_CONTROLLER_RAW,
    MC_SINGLEPLAYER_CONTROLLER_READY
} mc_singleplayer_controller_phase;

typedef struct mc_singleplayer_controller_error {
    mc_singleplayer_controller_status status;
    const char *operation;
    mc_player_site site;
    mc_entity_error entity;
    mc_player_error player;
    mc_world_storage_status world;
    mc_world_provider_status provider;
} mc_singleplayer_controller_error;

typedef struct mc_singleplayer_controller_snapshot {
    mc_singleplayer_controller_phase phase;
    client_application *a; /* Actual borrowed ob.a, nullable. */
    bool b;
    int32_t c, d, e;
    float f, g, h;
    int32_t i;
    unsigned int base_constructor_pc, constructor_pc;
    mc_entity *failed_player; /* Borrowed retained candidate; never App.h. */
    size_t native_holds;
    bool entered;
} mc_singleplayer_controller_snapshot;

/* Allocate the canonical concrete os owner without constructing source fields.
 * Bind the exact client Entity namespace/profile; neither class initialization
 * nor Random construction occurs. max_bytes==0 selects1MiB and charges this
 * owner only. Entity, Inventory and diagnostic budgets remain independent.
 * Source construction stores inherited b=false/App and own -1/+0 defaults in
 * actual ob/os order. The actual App identity is borrowed and may be NULL;
 * keep a nonnull App and its live fields valid through all controller use.
 * No App field, World, Player, tick, rendering or audio work is implied. */
mc_singleplayer_controller_status
mc_client_singleplayer_controller_allocate(mc_entity_classes *, mc_java_random_profile *,
                                           size_t max_bytes, mc_client_singleplayer_controller **,
                                           mc_singleplayer_controller_error *);
mc_singleplayer_controller_status mc_client_singleplayer_controller_construct(
    mc_client_singleplayer_controller *, client_application *, mc_singleplayer_controller_error *);
bool mc_client_singleplayer_controller_read(const mc_client_singleplayer_controller *,
                                            mc_singleplayer_controller_snapshot *);

/* Actual os.a(fd), including its invokespecial of literal-empty ob.a(fd).
 * The World argument is not dereferenced. Actual inherited ob.b(gs) is also
 * literal-empty; NULL Player succeeds. These are completed selected bodies,
 * not successful substitutes for os.c(), mining or other controller methods. */
mc_singleplayer_controller_status mc_client_singleplayer_controller_change_world(
    mc_client_singleplayer_controller *, mc_world_runtime *, mc_singleplayer_controller_error *);
mc_singleplayer_controller_status
mc_client_singleplayer_controller_finish_player(mc_client_singleplayer_controller *, mc_entity *,
                                                mc_singleplayer_controller_error *);

/* Actual inherited ob.b(fd): allocate genuine dc before loading App.k13,
 * then World.t17/provider.g20, then invoke its real constructor23. The current
 * actual App Session is captured on each call. NULL App/World/provider fail at
 * those reached sites. NULL Session enters dc and preserves its real failure.
 * Native same-namespace/profile/game-World admission and temporary holds add
 * no source fields, substitutes or callback. Output is unchanged on failure.
 * A successful returned Player has one caller hold and borrows the actual App.
 * No reset/input/World insertion or App.h/i publication occurs here.
 *
 * Exactly one failed allocated candidate may be retained for prefix inspection.
 * Inspect it, optionally retain it independently, then clear it promptly. A
 * pending failed candidate rejects another attempt before effects. Clear
 * releases only this owner's hold, including RAW and failed constructors;
 * external holds keep the candidate alive normally. No unbounded failure List.
 * A per-call complete error frame is private heap scratch and is released on
 * every return; caller-provided error storage must be live and disjoint.
 * Clear performs no allocation and reports directly into optional output. */
mc_singleplayer_controller_status
mc_client_singleplayer_controller_create_player(mc_client_singleplayer_controller *,
                                                mc_world_runtime *, const mc_inventory_limits *,
                                                mc_entity **, mc_singleplayer_controller_error *);
mc_singleplayer_controller_status
mc_client_singleplayer_controller_clear_failed_player(mc_client_singleplayer_controller *,
                                                      mc_singleplayer_controller_error *);

/* Actual os.a(gs): store only aS/yaw=-180 at3. Preserve previous yaw and every
 * other field, including RAW/failed source prefixes. Canonical LocalPlayer is
 * the selected native virtual closure; NULL reaches the actual putfield3.
 * No READY-only public mutation admission is changed. */
mc_singleplayer_controller_status
mc_client_singleplayer_controller_prepare_player(mc_client_singleplayer_controller *, mc_entity *,
                                                 mc_singleplayer_controller_error *);

mc_singleplayer_controller_status
mc_client_singleplayer_controller_retain(mc_client_singleplayer_controller *);
mc_singleplayer_controller_status
mc_client_singleplayer_controller_release(mc_client_singleplayer_controller *);
/* Native free requires the sole owner hold and no entered operation. It clears
 * a retained failed candidate, then releases its class binding. Source empty
 * hooks, reset/save/input/tick and App retirement are not invoked. Free performs
 * no allocation, including RAW/failed-candidate, null-owner and BUSY paths. */
mc_singleplayer_controller_status
mc_client_singleplayer_controller_free(mc_client_singleplayer_controller **,
                                       mc_singleplayer_controller_error *);

#endif
