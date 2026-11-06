#ifndef MC_CLIENT_WORLD_VIEW_H
#define MC_CLIENT_WORLD_VIEW_H

#include "assets.h"
#include "mesher.h"
#include "renderer.h"

#include "mc/world/runtime/propagation.h"

typedef struct mc_client_world_view mc_client_world_view;

typedef struct mc_client_world_view_error {
    const char *operation;
    mc_client_assets_error assets;
    mc_client_display_error display;
    mc_client_world_mesher_error mesher;
    mc_chunk_acquire_status acquisition;
    mc_world_storage_error world;
    mc_world_light_operation_error light;
    mc_client_camera_error camera;
} mc_client_world_view_error;

/* Finite native World inspection: actual generated/populated sections, textures
 * and GL buffers. This is not a Player, world tick or RenderGlobal scheduler.
 * Build a fixed three-by-three column view around the actual World spawn.
 * Acquire its padded ChunkCache footprint and drain actual World lighting
 * before building any section, so later population cannot stale earlier meshes.
 * World/profile/Registry/Assets remain exclusive during creation. The finished
 * owner retains only Display and copied GPU geometry; no World view survives. */
bool mc_client_world_view_create(mc_world_runtime *world, mc_client_display *display,
                                 mc_client_assets *assets, const mc_math_trig *trig, bool anaglyph,
                                 bool ambient_occlusion, bool fancy_leaves,
                                 mc_client_world_view **out, mc_client_world_view_error *error);
bool mc_client_world_view_draw(mc_client_world_view *view, mc_client_display_error *error);
/* Finite geometry remains the same actual 3x3 view. This constructor retains
 * the Player's actual World; inspection creation above keeps its old lifetime.
 * No chunk streaming, geometry updates, visibility scheduling or tick is added.
 * The draw uses that World's genuine Player and live settings, reorders actual
 * translucent sections against the current raw render origin, and requires the
 * caller's fog. The nullable actual application World is a separate
 * source field; never infer it from Player or retained geometry. Release the view before retiring
 * its World. Actual application h/i publication and dimensions belong to gameplay composition. */
bool mc_client_world_view_create_for_player(mc_entity *local_player, mc_client_display *display,
                                            mc_client_assets *assets, const mc_math_trig *trig,
                                            bool anaglyph, bool ambient_occlusion,
                                            bool fancy_leaves, mc_client_world_view **out,
                                            mc_client_world_view_error *error);
bool mc_client_world_view_draw_first_person(
    mc_client_world_view *view, mc_entity *local_player, const mc_game_settings *settings,
    mc_world_runtime *application_world, mc_registry_classes *code_registry,
    const mc_math_trig *trig, int32_t width, int32_t height, float partial_tick, int32_t eye_pass,
    const mc_client_world_fog *fog, mc_client_world_view_error *error);
/* Actual App forwarding; caller supplies source fog, never inspection fallback.
 * h/i remain the selected actual LocalPlayer identity argument. The App World
 * and settings are loaded by the camera at their real late source points. */
bool mc_client_world_view_draw_application(mc_client_world_view *view,
                                           const client_application *application,
                                           mc_entity *selected_local_player,
                                           mc_registry_classes *code_registry,
                                           const mc_math_trig *trig, float partial_tick,
                                           int32_t eye_pass, const mc_client_world_fog *fog,
                                           mc_client_world_view_error *error);
bool mc_client_world_view_retire(mc_client_world_view **view, mc_client_display_error *error);

#endif
