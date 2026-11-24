#ifndef MC_CLIENT_WORLD_PREPARE_WORLD_H
#define MC_CLIENT_WORLD_PREPARE_WORLD_H
#include "../../world/runtime/spawn_internal.h"
#include "../gui/loading_screen.h"
#include "mc/world/runtime/acquisition_error.h"
#include "mc/world/runtime/propagation.h"
#include "mc/world/runtime/saving/errors.h"

typedef struct mc_client_world_prepare_call mc_client_world_prepare_call;

typedef enum mc_client_world_prepare_status {
    MC_CLIENT_WORLD_PREPARE_OK,
    MC_CLIENT_WORLD_PREPARE_INVALID_ARGUMENT,
    MC_CLIENT_WORLD_PREPARE_ALLOCATION_FAILED,
    MC_CLIENT_WORLD_PREPARE_LIMIT,
    MC_CLIENT_WORLD_PREPARE_BUSY,
    MC_CLIENT_WORLD_PREPARE_NULL_REFERENCE,
    MC_CLIENT_WORLD_PREPARE_STRING_FAILED,
    MC_CLIENT_WORLD_PREPARE_LOADING_FAILED,
    MC_CLIENT_WORLD_PREPARE_WORLD_FAILED,
    MC_CLIENT_WORLD_PREPARE_UNIMPLEMENTED,
    MC_CLIENT_WORLD_PREPARE_CLEANUP_FAILED
} mc_client_world_prepare_status;

typedef enum mc_client_world_prepare_phase {
    MC_CLIENT_WORLD_PREPARE_ACTIVE,
    MC_CLIENT_WORLD_PREPARE_RETURNED,
    MC_CLIENT_WORLD_PREPARE_FAILED,
    MC_CLIENT_WORLD_PREPARE_RETIRING
} mc_client_world_prepare_phase;

typedef struct mc_client_world_prepare_sample {
    int32_t dx, dz, progress, x, z;
    bool progress_called, progress_returned, block_called, block_returned;
    size_t light_calls, light_true_returns;
    const mc_world_runtime *block_world;
} mc_client_world_prepare_sample;

typedef struct mc_client_world_prepare_state {
    mc_client_world_prepare_phase phase;
    mc_client_world_prepare_status status, cleanup_status;
    const char *operation, *cleanup_operation;
    unsigned int source_pc; /* Minecraft.d(String); nested causes retain their own PCs. */
    int native_thread_error;
    bool source_returned, spawn_constructed, player_x_stored, player_z_stored;
    int32_t radius, count, total, dx, dz, last_progress, unused_simulation_count;
    size_t progress_calls, progress_returns, block_samples, light_calls, light_true_returns;
    size_t loading_calls, samples_started;
    const client_application *app;
    const mc_player_string *text;
    const mc_player_coordinates *spawn; /* Same actual owned br, including RAW/failed prefix. */
    const mc_chunk_provider *provider;  /* Captured actual fd.w44 value. */
    const mc_world_runtime *provider_world, *spawn_world, *last_world;
    const mc_client_world_prepare_sample *samples; /* Fixed 289 native prefix records. */
    const mc_client_loading_error *loading, *loading_cleanup;
    const mc_world_storage_error *world;
    const mc_world_spawn_error *spawn_error;
    const mc_world_runtime_acquisition_error *acquisition;
    const mc_world_light_operation_error *light;
    const mc_world_runtime_lifecycle_error *maintenance;
    mc_player_status string_status;
} mc_client_world_prepare_state;

/* Actual Minecraft.d(String): Loading start5/message14, captured provider44,
 * owned fd.u/br53, actual Player override75/88 and concrete kx test; x-major
 * 289 progress159 BEFORE fresh App.f block184 and fd.j192 until false; final
 * message219, dead local2000, real finish_maintenance231 and return234.
 *
 * This synchronous call executes once. No resume/replay API, diagnostic World,
 * fake clock, shortened successful grid, capped light loop or 2000 ticks. Existing
 * lower native policy failures retain their completed source prefix.
 * Caller holds the complete actual App/World/provider/Player/Loading/graphics
 * graph and class namespace live/exclusive through successful checked retire,
 * including every failed call. World references are source borrows; this owner
 * publishes no App fields. It owns the String argument, local br and native
 * captured Loading occurrences. Constants resolve in the real MC namespace.
 *
 * *out initially NULL. Native admission/allocation failure preserves it; once
 * allocated, success or failure transfers the retained call to *out. Errors and
 * views are heap-owned by that call. Retire pending lower calls before caller
 * graph retirement; BUSY/failure preserves this same slot and remaining rights. */
mc_client_world_prepare_status mc_client_world_prepare_begin(client_application *,
                                                             mc_player_string *nullable_text,
                                                             mc_client_world_prepare_call **out);
bool mc_client_world_prepare_inspect(const mc_client_world_prepare_call *,
                                     mc_client_world_prepare_state *);
mc_client_world_prepare_status mc_client_world_prepare_retire(mc_client_world_prepare_call **);
#endif
