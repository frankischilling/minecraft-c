#ifndef MC_CLIENT_SOUND_SOUND_MANAGER_H
#define MC_CLIENT_SOUND_SOUND_MANAGER_H

#include "mc/client/settings/game_settings.h"
#include "mc/client/sound/sound_pool.h"
#include "mc/core/math/math.h"

typedef struct mc_sound_manager_shared mc_sound_manager_shared;
typedef struct mc_sound_manager mc_sound_manager;

typedef enum mc_sound_manager_pool_kind {
    MC_SOUND_MANAGER_EFFECTS,
    MC_SOUND_MANAGER_STREAMING,
    MC_SOUND_MANAGER_MUSIC,
    MC_SOUND_MANAGER_POOL_COUNT
} mc_sound_manager_pool_kind;

typedef enum mc_sound_manager_configuration {
    MC_SOUND_MANAGER_LIBRARY_OPENAL,
    MC_SOUND_MANAGER_CODEC_OGG,
    MC_SOUND_MANAGER_CODEC_MUS,
    MC_SOUND_MANAGER_CODEC_WAV
} mc_sound_manager_configuration;

typedef enum mc_sound_manager_command_kind {
    MC_SOUND_MANAGER_NEW_SOURCE,
    MC_SOUND_MANAGER_NEW_STREAMING_SOURCE,
    MC_SOUND_MANAGER_BACKGROUND_MUSIC,
    MC_SOUND_MANAGER_SET_PITCH,
    MC_SOUND_MANAGER_SET_VOLUME,
    MC_SOUND_MANAGER_PLAY,
    MC_SOUND_MANAGER_STOP,
    MC_SOUND_MANAGER_LISTENER_POSITION,
    MC_SOUND_MANAGER_LISTENER_ORIENTATION,
    MC_SOUND_MANAGER_CLEANUP
} mc_sound_manager_command_kind;

typedef struct mc_sound_manager_command {
    mc_sound_manager_command_kind kind;
    const char *source_name;
    const mc_sound_pool_entry *entry;
    bool priority, looping;
    float x, y, z;
    int32_t attenuation;
    float distance, value;
    float orientation[6];
} mc_sound_manager_command;

/* Actual SoundSystem public-call boundaries, not device-playback results.
 * BACKGROUND_MUSIC is its distinct wrapper, which submits QUICK_PLAY and PLAY.
 * playing queries current backend state without draining queued commands.
 * Calls remain separate; backend work may execute between them. The backend
 * must retain its own copies of borrowed command/name/entry data before return.
 * SOURCE_EXCEPTION represents a source Throwable; native errors stay distinct.
 * construct publishes a nonnull owned handle on OK and preserves out on failure.
 * retire releases native ownership without implicit source cleanup. Context and
 * callbacks outlive shared. Callbacks must not mutate/reenter managers/settings. */
typedef struct mc_sound_manager_backend {
    void *context;
    mc_sound_status (*configure)(void *context, mc_sound_manager_configuration configuration,
                                 mc_sound_error *error);
    mc_sound_status (*construct)(void *context, void **out, mc_sound_error *error);
    mc_sound_status (*command)(void *context, void *backend,
                               const mc_sound_manager_command *command, mc_sound_error *error);
    mc_sound_status (*playing)(void *context, void *backend, const char *source_name, bool *out,
                               mc_sound_error *error);
    mc_sound_status (*retire)(void *context, void **backend, mc_sound_error *error);
} mc_sound_manager_backend;

typedef enum mc_sound_manager_source_exception {
    MC_SOUND_MANAGER_EXCEPTION_NONE,
    MC_SOUND_MANAGER_EXCEPTION_NULL_SETTINGS,
    MC_SOUND_MANAGER_EXCEPTION_NULL_BACKEND,
    MC_SOUND_MANAGER_EXCEPTION_BACKEND_THROWABLE
} mc_sound_manager_source_exception;

typedef struct mc_sound_manager_setup_report {
    bool entered, volumes_zeroed, first_save_reached, backend_published;
    bool volumes_restored, second_save_reached, throwable_caught;
    bool native_terminal;
    int32_t source_pc;
    mc_sound_manager_source_exception exception;
    mc_game_settings_status settings_status;
    mc_game_settings_error settings_error;
    mc_sound_error backend_error;
    mc_game_settings_report saves[2];
} mc_sound_manager_setup_report;

typedef struct mc_sound_manager_report {
    int32_t source_pc;
    mc_sound_manager_source_exception exception;
    mc_sound_manager_setup_report setup;
} mc_sound_manager_report;

typedef struct mc_sound_manager_state {
    bool shared_initialized, backend_present, active;
    int32_t sound_counter, music_delay;
    mc_java_random random;
    mc_game_settings *settings;
} mc_sound_manager_state;

typedef struct mc_sound_manager_listener {
    float previous_yaw, yaw;
    double previous_x, previous_y, previous_z;
    double x, y, z;
} mc_sound_manager_listener;

/* Shared owns yo's static SoundSystem and initialized fields. New managers do
 * not reset them. Native retirement requires all managers retired first and
 * invokes backend retirement, separately from source cleanup. */
mc_sound_status mc_sound_manager_shared_create(const mc_sound_manager_backend *backend,
                                               mc_sound_manager_shared **out,
                                               mc_sound_error *error);
mc_sound_status mc_sound_manager_shared_retire(mc_sound_manager_shared **shared,
                                               mc_sound_error *error);

/* Construct effects/streaming/music pools, fourth ordinary Random, then
 * nextInt(12000), in source order. Each pool gets its declared memory budget.
 * No settings, digit-policy change, backend setup or I/O occurs. Failure
 * consumes reached Random-profile prefixes, preserves out and frees native
 * allocations. Shared must outlive manager. */
mc_sound_status mc_sound_manager_create(mc_sound_manager_shared *shared,
                                        mc_java_random_profile *profile,
                                        size_t pool_allocation_bytes, mc_sound_manager **out,
                                        mc_sound_error *error);
mc_sound_status mc_sound_manager_retire(mc_sound_manager **manager, mc_sound_error *error);
bool mc_sound_manager_read_state(const mc_sound_manager *manager, mc_sound_manager_state *out);
const mc_sound_pool *mc_sound_manager_pool(const mc_sound_manager *manager,
                                           mc_sound_manager_pool_kind kind);
mc_sound_status mc_sound_manager_add_file(mc_sound_manager *manager,
                                          mc_sound_manager_pool_kind kind,
                                          mc_sound_text logical_name, const char *native_utf8_path,
                                          const mc_sound_pool_entry **out,
                                          mc_sound_pool_report *report, mc_sound_error *error);

/* Bind sets streaming.strip_digits=false before retaining settings. Null
 * settings enters setup when uninitialized, reaches its caught dereference,
 * then latches initialized. Both exact zero volumes suppress setup. Setup
 * zeroes volumes, saves, registers library/codecs, constructs/publishes,
 * restores and saves. No restoring finally exists. Native failures preserve
 * effects separately from caught source Throwables. A native setup failure is
 * terminal for shared: later setup requests return that failure without replay,
 * overwriting a published backend, restoring volumes or repeating saves.
 * Real saves emit diagnostics.
 * Binding does not attach this manager to GameSettings option actions; use
 * mc_game_settings_attach_sound_manager explicitly and detach before retirement. */
mc_sound_status mc_sound_manager_bind(mc_sound_manager *manager, mc_game_settings *settings,
                                      const mc_game_settings_diagnostics *diagnostics,
                                      mc_sound_manager_report *report, mc_sound_error *error);
mc_sound_status mc_sound_manager_options_changed(mc_sound_manager *manager,
                                                 const mc_game_settings_diagnostics *diagnostics,
                                                 mc_sound_manager_report *report,
                                                 mc_sound_error *error);
mc_sound_status mc_sound_manager_cleanup(mc_sound_manager *manager, mc_sound_manager_report *report,
                                         mc_sound_error *error);
mc_sound_status mc_sound_manager_music_tick(mc_sound_manager *manager,
                                            mc_sound_manager_report *report, mc_sound_error *error);
mc_sound_status mc_sound_manager_listener_update(mc_sound_manager *manager,
                                                 const mc_sound_manager_listener *listener,
                                                 float partial_tick, const mc_math_trig *trig,
                                                 mc_sound_manager_report *report,
                                                 mc_sound_error *error);
mc_sound_status mc_sound_manager_stream(mc_sound_manager *manager, mc_sound_text name, float x,
                                        float y, float z, float volume, float pitch,
                                        mc_sound_manager_report *report, mc_sound_error *error);
mc_sound_status mc_sound_manager_effect(mc_sound_manager *manager, mc_sound_text name, float x,
                                        float y, float z, float volume, float pitch,
                                        mc_sound_manager_report *report, mc_sound_error *error);
mc_sound_status mc_sound_manager_ui(mc_sound_manager *manager, mc_sound_text name, float volume,
                                    float pitch, mc_sound_manager_report *report,
                                    mc_sound_error *error);

/* External exclusive access to manager/shared/pools/settings is required.
 * Native guards remain held across callbacks. Read-only views may inspect busy
 * owners. Retirement never saves, source-cleans backend or resets initialized. */

#endif
