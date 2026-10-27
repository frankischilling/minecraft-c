#ifndef MC_CLIENT_DISPLAY_STAGED_H
#define MC_CLIENT_DISPLAY_STAGED_H

#include "display.h"
#include "mc/entity/player/player.h"
#include "sdk_icons.h"
#include "sdk_input.h"
#include "sdk_update.h"

typedef struct mc_client_display_mode mc_client_display_mode;
typedef struct mc_client_sdk_input mc_client_sdk_input;
typedef struct mc_client_sdk_update mc_client_sdk_update;

typedef enum mc_client_display_phase {
    MC_CLIENT_DISPLAY_RAW,
    MC_CLIENT_DISPLAY_CREATING,
    MC_CLIENT_DISPLAY_READY,
    MC_CLIENT_DISPLAY_FAILED,
    MC_CLIENT_DISPLAY_RETIRING
} mc_client_display_phase;

typedef enum mc_client_display_failure_kind {
    MC_CLIENT_DISPLAY_FAILURE_NONE,
    MC_CLIENT_DISPLAY_NATIVE_POLICY,
    MC_CLIENT_DISPLAY_NATIVE_ALLOCATION,
    MC_CLIENT_DISPLAY_NATIVE_PLATFORM,
    MC_CLIENT_DISPLAY_SOURCE_NULL_POINTER,
    MC_CLIENT_DISPLAY_SOURCE_ILLEGAL_STATE,
    MC_CLIENT_DISPLAY_SOURCE_LWJGL_EXCEPTION
} mc_client_display_failure_kind;

typedef struct mc_client_display_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_client_display_site;

typedef struct mc_client_display_stage_error {
    mc_client_display_failure_kind kind;
    const char *operation;
    mc_client_display_site site, child;
    mc_client_display_error display;
    mc_client_display_error cleanup_display;
    mc_player_status string;
    /* Complete value remains valid across partial or terminal retirement;
     * no returned cleanup error points into an already-freed native child. */
    mc_client_sdk_input_error input;
    bool has_input;
    mc_client_sdk_icons_error icons;
    mc_client_sdk_update_error update;
    mc_client_sdk_platform_error platform;
    bool current_mode_stored, title_stored, window_created;
    bool native_owner_retained;
} mc_client_display_stage_error;

typedef struct mc_client_display_mode_state {
    size_t holds;
    bool constructed, fullscreen_capable;
    int32_t width, height, bits_per_pixel, frequency, last_store_pc;
} mc_client_display_mode_state;

typedef struct mc_client_display_stage_state {
    mc_client_display_phase phase;
    const mc_client_display_mode *initial_mode, *current_mode;
    const mc_player_string *title;
    bool active, class_initialized, window_created;
    int32_t last_store_pc;
    const mc_client_display_stage_error *failure;
} mc_client_display_stage_state;

/* Real counted immutable Mode candidate. Allocate models source new; construct
 * implements DisplayMode(II)->(IIIIZ), with bpp/frequency0 and false capability.
 * ALL signed dimensions are stored without validation/clamping. Native window
 * bounds are admitted only when reached by create_staged. */
bool mc_client_display_mode_allocate(mc_client_display_mode **out, mc_client_display_stage_error *);
bool mc_client_display_mode_construct(mc_client_display_mode *, int32_t width, int32_t height,
                                      mc_client_display_stage_error *);
bool mc_client_display_mode_retain(mc_client_display_mode *);
void mc_client_display_mode_release(mc_client_display_mode **);
bool mc_client_display_mode_inspect(const mc_client_display_mode *, mc_client_display_mode_state *);

/* RAW allocation has no SDL/window/context/input/SDK active-use effects. The
 * first stage method initializes the concrete linked SDL/platform namespace.
 * It is not credited at Mode allocation/construction. Inputs/output/error live
 * disjoint; all calls require the real main/video thread and exclusive owners. */
bool mc_client_display_stage_allocate(mc_client_display **out, mc_client_display_stage_error *);
/* Native imported prerequisite, before first SDK active use. Retains the same
 * READY pixel owner; never opens a JAR/class or mirrors an App/source flag.
 * Missing binding is a typed native resource failure, not source setIcon
 * success. The caller's occurrence stays independent of Display retirement. */
bool mc_client_display_stage_bind_icons(mc_client_display *, mc_client_sdk_icons *,
                                        mc_client_display_stage_error *);
/* Source setDisplayMode stores SAME counted Mode25 before checking created28.
 * The admitted no-window branch has no window work. Reconfiguration of an
 * already-created window remains explicit until its full source body exists. */
bool mc_client_display_stage_set_mode(mc_client_display *, mc_client_display_mode *,
                                      mc_client_display_stage_error *);
/* Source setTitle stores SAME immutable String14 before live-window setter29.
 * NULL resolves the actual class-owned empty String; caller's String lifetime
 * is independent. No source field is mirrored from App.d/e/native flags. */
bool mc_client_display_stage_set_title(mc_client_display *, mc_player_string *,
                                       mc_client_display_stage_error *);
/* Same owner receives the actual window/context/resources and concrete input
 * attachment. hidden is a native diagnostic option, not a source field.
 * Failure retains its reached prefix for terminal retirement; no failure is
 * consumed or reported as success. Native SDL/policy errors do NOT fabricate
 * Java LWJGLException and must not be caught as one by the startup caller. */
bool mc_client_display_create_staged(mc_client_display *, bool hidden,
                                     mc_client_display_stage_error *);
bool mc_client_display_stage_inspect(const mc_client_display *, mc_client_display_stage_state *);
/* Terminal native retirement. BUSY/failed temporary cleanup preserves pointer
 * and every still-owned child. If final destructive cleanup completed while
 * reporting an earlier GL cleanup error, the slot is NULL even on false;
 * native_owner_retained and the actual slot distinguish retryable ownership.
 * No Source save/flag/whole-ctor completion. */
bool mc_client_display_stage_retire(mc_client_display **, mc_client_display_stage_error *);
mc_client_sdk_input *mc_client_display_inputs(mc_client_display *);
mc_client_sdk_update *mc_client_display_update_owner(mc_client_display *);

#endif
