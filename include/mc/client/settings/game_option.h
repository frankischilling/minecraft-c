#ifndef MC_CLIENT_GAME_OPTION_H
#define MC_CLIENT_GAME_OPTION_H

#include "mc/client/settings/game_settings.h"

typedef struct mc_game_option mc_game_option;

typedef enum mc_game_option_id {
    MC_GAME_OPTION_MUSIC,
    MC_GAME_OPTION_SOUND,
    MC_GAME_OPTION_INVERT_MOUSE,
    MC_GAME_OPTION_SENSITIVITY,
    MC_GAME_OPTION_RENDER_DISTANCE,
    MC_GAME_OPTION_VIEW_BOBBING,
    MC_GAME_OPTION_ANAGLYPH,
    MC_GAME_OPTION_ADVANCED_OPENGL,
    MC_GAME_OPTION_FRAMERATE_LIMIT,
    MC_GAME_OPTION_DIFFICULTY,
    MC_GAME_OPTION_GRAPHICS,
    MC_GAME_OPTION_AMBIENT_OCCLUSION,
    MC_GAME_OPTION_GUI_SCALE,
    MC_GAME_OPTION_COUNT
} mc_game_option_id;

typedef struct mc_game_option_info {
    mc_game_option_id id;
    const char *name, *translation_key;
    bool is_float, is_boolean;
} mc_game_option_info;

typedef enum mc_game_option_status {
    MC_GAME_OPTION_OK,
    MC_GAME_OPTION_INVALID_ARGUMENT,
    MC_GAME_OPTION_SOURCE_NULL_OPTION,
    MC_GAME_OPTION_SOURCE_INDEX_OUT_OF_BOUNDS
} mc_game_option_status;

/* Original client EnumOptions alias ht. Stable immutable native descriptors,
 * in the original thirteen-constant ordinal order. An out-of-range signed ID
 * returns NULL, as ht.a(I) does; no default/clamp/modulo. Foreign descriptor
 * pointers are rejected by identity without dereferencing them. */
const mc_game_option *mc_game_option_by_id(int32_t id);
bool mc_game_option_read(const mc_game_option *option, mc_game_option_info *out);

/* Read the actual public owner projection. NULL option returns positive zero
 * from the float query; the boolean query reports the original null-ordinal
 * failure. GRAPHICS is not one of the five boolean getter cases. Native
 * invalid arguments are separate, and failures preserve out. Access requires
 * the settings owner's normal exclusive live-owner contract. No I/O/effects. */
mc_game_option_status mc_game_settings_option_float(const mc_game_settings *settings,
                                                    const mc_game_option *option, float *out);
mc_game_option_status mc_game_settings_option_boolean(const mc_game_settings *settings,
                                                      const mc_game_option *option, bool *out);

/* The four original J/K/L/M translation-key tables, with current raw indices.
 * Only RENDER_DISTANCE, DIFFICULTY, GUI_SCALE and FRAMERATE_LIMIT have tables;
 * other descriptors are invalid arguments to this table accessor. It does not
 * translate, initialize resources, clamp, or change settings. */
mc_game_option_status mc_game_option_value_key(const mc_game_option *option, int32_t index,
                                               const char **out);

/* Retained LWJGL2.9.0 KEY_ constant/name facts after normal completion of its
 * reflection initializer: 256 slots, 131 names, 125 NULL holes. Names are
 * immutable ASCII, including NONE at0 and LMETA/RMETA at219/220. A valid hole
 * succeeds with NULL; negative/256+ is the source array-index failure. This
 * neither opens a keyboard nor maps SDL/OS key codes, and does not claim Java
 * allocation/exceptional partial-initialization or monitor behavior. */
mc_game_option_status mc_game_key_name(int32_t code, const char **out);

#endif
