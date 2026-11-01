#ifndef MC_CLIENT_RUNTIME_MENU_LOOP_H
#define MC_CLIENT_RUNTIME_MENU_LOOP_H

#include "bootstrap.h"

typedef struct mc_client_menu_loop mc_client_menu_loop;
typedef struct mc_world_storage_error mc_world_storage_error;

/* Native copies of already reached source clock reads. No sample controls
 * gameplay, source fields or scheduling. Valid flags describe this frame's
 * prefix; zeros never stand for an unreached clock. The index is the number
 * of completed frames before this prefix. A native retirement refusal before
 * the next frame's reset leaves the preceding snapshot intact. */
typedef struct mc_client_loop_clock_observation {
    uint64_t frame_index;
    int64_t run_137, run_203, run_387;
    int32_t timer_ticks, tick_returns;
    bool frame_entered, frame_returned, timer_fields_read;
    bool run_137_read, run_203_read, run_387_read;
} mc_client_loop_clock_observation;

typedef struct mc_client_menu_loop_state {
    uint64_t frames, ticks, texture_ticks, account_checks_omitted;
    uint64_t world_draws, hud_draws;
    uint64_t gameplay_left_clicks, gameplay_swing_calls, gameplay_click_timer_stores;
    uint64_t gameplay_mining_starts, gameplay_mining_progress_calls, gameplay_mined_blocks;
    uint64_t gameplay_hit_particles_constructed, gameplay_hit_particles_appended;
    uint64_t gameplay_right_clicks, gameplay_block_uses, gameplay_air_uses;
    uint64_t gameplay_placements_admitted, gameplay_placed_blocks, gameplay_placement_sounds;
    uint64_t gameplay_used_statistics, gameplay_stack_count_stores;
    uint64_t gameplay_selected_slot_clears, gameplay_equip_resets;
    uint64_t gameplay_drop_calls, gameplay_drop_publications, gameplay_dropped_items;
    uint64_t notice_returns, notice_draws, notice_text_returns, notice_vertices;
    /* Native observations of completed source calls survive child retirement
     * and the next tick's fresh diagnostic record. They control no gameplay. */
    uint64_t world_tick_returns, random_chunk_calls, random_chunk_samples;
    uint64_t random_display_calls, random_display_samples, random_display_random_calls;
    size_t last_random_chunk_count;
    bool render_clock_stored, render_returned;
    int64_t render_clock; /* Existing World-branch px.b450 read / I453 store, before HUD506. */
    mc_client_loop_clock_observation clocks;
    size_t font_measurements, mouse_events, keyboard_events;
    bool failed, running;
    bool terminal_prepare_selected, terminal_prepared; /* Native cleanup progress only. */
    bool terminal_tail_entered;
    const char *retirement_operation; /* Native cleanup diagnosis, separate from source failure. */
    const mc_world_storage_error *retirement_world;
    mc_client_bootstrap_status retirement_bootstrap;
    const char *operation, *source_class, *source_method;
    int64_t source_pc; /* Source unsigned PCs and native -1 both retain their value. */
    /* Borrowed child diagnostics remain live until this loop retires. Gameplay
     * value diagnostics survive cleanup retry; released focus/monitor views
     * end with their successful native cleanup. */
    const struct mc_singleplayer_tick_state *controller_tick;
    const struct mc_client_display_screen_call *display_call;
    const struct mc_player_error *player;
    const struct mc_client_gameplay_input *gameplay_input;
    const struct mc_client_world_tick *world_tick;
    const struct mc_client_renderer_focus_state *renderer_focus;
    const struct mc_client_world_frame_state *world_frame;
    const struct mc_client_gui_ingame_draw_state *hud;
    const struct mc_client_notice_draw_state *notice;
} mc_client_menu_loop_state;

/* Native frame composition for the genuine constructed, initialized no-World
 * main menu and World selection. Borrows the complete bootstrap through retirement.
 * It uses the actual
 * App Timer, SDK, screen, statistics, sound and texture-effect owners. The
 * existing OpenGL3.3 GUI kernels present source-owned screen records. Unsupported
 * World-tick/gameplay-action/fullscreen/debug/screenshot routes retain a reached
 * diagnostic rather than entering the old inspection scene.
 *
 * Calls and callbacks require exclusive use of this whole graph on the video
 * thread. A failed frame is terminal; inspect remains available. Output stays
 * unchanged on ordinary create failure. A blocked setup cleanup returns its
 * retained failed loop for checked retirement. Retirement releases native renderer and transition
 * ownership without running source shutdown or restoring historical fields.
 * Source run's exception handlers and d() shutdown are separate dependencies. */
bool mc_client_menu_loop_create(mc_client_bootstrap *, mc_client_menu_loop **);
bool mc_client_menu_loop_frame(mc_client_menu_loop *);
bool mc_client_menu_loop_inspect(const mc_client_menu_loop *, mc_client_menu_loop_state *);
/* End inspection of a healthy completed frame's native Notice/HUD/World draw
 * records before direct component invocations. Requires an inactive loop and
 * at least one returned frame. Borrowed draw views expire; source fields,
 * frame/tick counts and clock observations remain unchanged. Idempotent after
 * success. A native retirement failure preserves its remaining owners and
 * selects the same terminal failure as the ordinary next-frame prefix. */
bool mc_client_menu_loop_retire_completed_draws(mc_client_menu_loop *);
/* End this loop's source admission and drain its owned call/World-load prefix,
 * retaining screens, App, full Worlds/Players/Inputs/handlers and all services.
 * This one-way operation performs no World container/cache/provider teardown
 * and does not end shared TH/LL classes. Repeated calls retry the same slots;
 * success means only this prefix completed. Before complete retire, the outer
 * owner must separately end dispatcher admissions/captures and admit every
 * World graph. Do not use this as source World clearing or resume frames after
 * the first attempt. NULL/active or previously entered legacy retire refuses. */
bool mc_client_menu_loop_terminal_prepare(mc_client_menu_loop *);
/* Normal native shutdown through this loop's original bootstrap association.
 * End the real prefix and dispatchers, then prove no storage was acquired or
 * admit the intact selected-load World graph before entering checked cleanup.
 * Unsupported/partial loads remain live for diagnosis; no legacy fallback is
 * selected. A retained setup failure uses only its never-entered setup cleanup.
 * Once the destructive tail starts, retries continue its remaining checked
 * owner slots instead of demanding an already consumed World. Keep the same
 * bootstrap and exclusive graph alive throughout. Source failure is preserved;
 * retirement_* fields report native cleanup separately. Success nulls *slot. */
bool mc_client_menu_loop_terminal_retire(mc_client_menu_loop **);
void mc_client_menu_loop_free(mc_client_menu_loop *);
/* Separate low-level native retirement. The caller supplies outer dispatcher
 * and storage admission; normal menu-run shutdown uses terminal_retire. */
bool mc_client_menu_loop_retire(mc_client_menu_loop **);
int mc_client_menu_run(const mc_client_application_options *);

#endif
