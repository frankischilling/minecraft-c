#ifndef MC_CLIENT_SETTINGS_SOURCE_INPUT_H
#define MC_CLIENT_SETTINGS_SOURCE_INPUT_H

#include "mc/client/settings/game_settings.h"

/* Native access to the SAME source fields at Minecraft.k's literal stores,
 * not new KV Java methods. Null/active/non-READY typed owners are native
 * admission errors; source caller null-receiver PCs stay with Minecraft.
 * Calls require the actual live/exclusive owner and disjoint optional outputs.
 * They do not save, copy Strings, allocate, dispatch options or call hooks. */
mc_game_settings_status
mc_game_settings_source_input_toggle_hide_gui(mc_game_settings *,
                                              mc_game_settings_error *); /* z, k668 */
mc_game_settings_status
mc_game_settings_source_input_toggle_debug(mc_game_settings *,
                                           mc_game_settings_error *); /* B, k698 */
mc_game_settings_status
mc_game_settings_source_input_toggle_third_person(mc_game_settings *,
                                                  mc_game_settings_error *); /* A, k728 */
mc_game_settings_status
mc_game_settings_source_input_toggle_smooth_camera(mc_game_settings *,
                                                   mc_game_settings_error *); /* E, k758 */
/* Literal run299 A=false after actual Player.L() succeeds. */
mc_game_settings_status mc_game_settings_source_render_clear_third_person(mc_game_settings *,
                                                                          mc_game_settings_error *);

typedef struct mc_game_settings_source_input_wheel_report {
    bool D_read, D, G_read, G_stored;
    int32_t normalized_wheel;
    unsigned int source_pc;
    float before_G, after_G;
} mc_game_settings_source_input_wheel_report;

/* Source post-Inventory wheel branch k367..418. Zero returns without reading
 * D/G; false D at386 returns without float work. True D normalizes sign, reads
 * G409 then uses Java binary32 multiply0.25/add and stores G418. It never adds
 * a clamp or save. FP admission is required only when arithmetic is reached;
 * refusal is native STATE_ERROR and leaves G unchanged while reporting the
 * reached D/G reads. Earlier native owner rejection preserves report. */
mc_game_settings_status
mc_game_settings_source_input_wheel(mc_game_settings *, int32_t wheel,
                                    mc_game_settings_source_input_wheel_report *,
                                    mc_game_settings_error *);

#endif
