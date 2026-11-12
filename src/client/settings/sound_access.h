#ifndef MC_CLIENT_SETTINGS_SOUND_ACCESS_H
#define MC_CLIENT_SETTINGS_SOUND_ACCESS_H

#include "mc/client/settings/game_settings.h"

struct mc_sound_manager;

/* Only the concrete SoundManager setup uses these two field/save operations.
 * During an option action, access is restricted to its attached SoundManager.
 * Public mutation and diagnostic reentry retain ordinary BUSY admission. */
mc_game_settings_status mc_game_settings_sound_set_volumes(mc_game_settings *settings,
                                                           const struct mc_sound_manager *manager,
                                                           float music, float sound,
                                                           mc_game_settings_error *error);
mc_game_settings_status mc_game_settings_sound_save(mc_game_settings *settings,
                                                    const struct mc_sound_manager *manager,
                                                    const mc_game_settings_diagnostics *diagnostics,
                                                    mc_game_settings_report *report,
                                                    mc_game_settings_error *error);

#endif
