#ifndef MC_CLIENT_SETTINGS_SOURCE_ACCESS_H
#define MC_CLIENT_SETTINGS_SOURCE_ACCESS_H

#include "mc/client/settings/game_option.h"

/* Actual kv.k field read. Admission matches the passive state view. */
bool mc_game_settings_read_ambient_occlusion(const mc_game_settings *, bool *);

/* Actual source Settings reads the SAME kv class arrays. Legacy owners leave
 * handled=false and keep the existing immutable-fact path unchanged. */
mc_game_option_status mc_game_settings_source_value_key(const mc_game_settings *,
                                                        const mc_game_option *, int32_t,
                                                        mc_game_settings_text *, bool *handled);

#endif
