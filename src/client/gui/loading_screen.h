#ifndef MC_CLIENT_GUI_LOADING_SCREEN_H
#define MC_CLIENT_GUI_LOADING_SCREEN_H

#include "../application.h"
#include "../display/display.h"
#include "bitmap_font.h"
#include "font_mapping.h"
#include "mc/client/render_engine/render_engine.h"
#include "mc/entity/player/player.h"
#include "millis.h"

typedef struct client_application client_application;
typedef struct mc_client_loading_classes mc_client_loading_classes;
typedef struct mc_client_loading_screen mc_client_loading_screen;

typedef enum mc_client_loading_status {
    MC_CLIENT_LOADING_OK,
    MC_CLIENT_LOADING_INVALID_ARGUMENT,
    MC_CLIENT_LOADING_INVALID_PHASE,
    MC_CLIENT_LOADING_BUSY,
    MC_CLIENT_LOADING_LIMIT,
    MC_CLIENT_LOADING_ALLOCATION_FAILED,
    MC_CLIENT_LOADING_NULL_REFERENCE,
    MC_CLIENT_LOADING_STOPPED_ERROR, /* actual xx extends Error */
    MC_CLIENT_LOADING_CLOCK_FAILED,
    MC_CLIENT_LOADING_SETTINGS_FAILED,
    MC_CLIENT_LOADING_UNSUPPORTED_FP,
    MC_CLIENT_LOADING_DISPLAY_FAILED,
    MC_CLIENT_LOADING_FONT_FAILED,
    MC_CLIENT_LOADING_TEXTURE_FAILED,
    MC_CLIENT_LOADING_UNIMPLEMENTED_DEPENDENCY
} mc_client_loading_status;

typedef enum mc_client_loading_phase {
    MC_CLIENT_LOADING_RAW,
    MC_CLIENT_LOADING_CONSTRUCTING,
    MC_CLIENT_LOADING_READY,
    MC_CLIENT_LOADING_FAILED
} mc_client_loading_phase;

typedef struct mc_client_loading_site {
    const char *class_name, *method, *descriptor;
    int32_t pc;
} mc_client_loading_site;

/* Private drawing diagnostics are retained by the concrete lower owner and
 * copied through its typed interface. This record is the complete mk layer:
 * outer invocation, actual failed child site and actual Display/clock cause.
 * The normal draw extension adds its full typed font/texture causes, rather
 * than casting a smaller record into an unrelated error layout. */
typedef struct mc_client_loading_error {
    mc_client_loading_status status;
    const char *operation;
    mc_client_loading_site site, child, parent;
    mc_platform_millis_status clock;
    mc_client_display_error display;
    mc_client_font_mapping_error fp;
    mc_client_graphics_error graphics;
    mc_client_bitmap_font_error font;
    mc_client_render_texture_error texture;
    int32_t qq_width, qq_height, qq_factor;
    bool stopped_allowed, sampled_clock_committed;
} mc_client_loading_error;

typedef struct mc_client_loading_fields {
    const mc_player_string *a;
    client_application *b;
    const mc_player_string *c;
    int64_t d;
    bool e;
} mc_client_loading_fields;

typedef struct mc_client_loading_state {
    mc_client_loading_phase phase;
    mc_client_loading_fields fields;
    int32_t last_store_pc;
    size_t holds;
    bool active;
} mc_client_loading_state;

/* One class-loader lifetime supplies the shared empty literal. It is allocated
 * as an existing immutable String owner before any mk source constructor and
 * is shared by both a7/c13 and every mk on these classes. Other equal empty
 * String owners remain distinct. No clock, App dereference, resource active
 * use or GL call occurs here. The class owner needs no source initializer. */
mc_client_loading_status mc_client_loading_classes_create(mc_client_render_engine_classes *,
                                                          mc_client_loading_classes **out);
/* Native class-loader binding only; no resource active use or fp initializer.
 * Bind once after the real resource source exists. It stays borrowed through
 * all retained GUI/failed constructor lifetimes. */
mc_client_loading_status mc_client_loading_classes_bind_resources(mc_client_loading_classes *,
                                                                  const mc_registry_resources *,
                                                                  mc_client_loading_error *);
mc_client_render_engine_classes *
mc_client_loading_screen_graphics(const mc_client_loading_screen *);
/* Borrow the one actual canonical empty literal; no allocation, source active
 * use, clock or App access. Caller may retain its own String occurrence at the
 * actual source store. The class owner remains live through this operation. */
mc_player_string *mc_client_loading_classes_empty(const mc_client_loading_classes *);
mc_client_loading_status mc_client_loading_classes_retain(mc_client_loading_classes *);
mc_client_loading_status mc_client_loading_classes_release(mc_client_loading_classes *);

/* App is the real nullable source App, borrowed until the final failed/ready
 * mk hold is released. Keep that actual storage alive independently of worker
 * holds. Before a nonnull App reaches source J4, register its native lifetime
 * with the canonical application owner. RAW/RETIRED App returns native
 * INVALID_PHASE before any atomic access. Only actual atomic J is loaded at4;
 * native running/captured do not mirror J/o. Source constructor/run stores and
 * prepared-state fixture stores are explicit and separate from registration.
 * allocate publishes only RAW zeroed native storage;
 * construct executes a7,c13,clock17,d20,e25,b30. Failure is not retryable.
 * Inputs/errors/outputs are live and disjoint. Calls require exclusive owners.
 * Immutable String arguments are borrowed through a call; field publication
 * gains an independent native hold, including when both fields alias. */
mc_client_loading_status mc_client_loading_screen_allocate(mc_client_loading_classes *,
                                                           mc_client_loading_screen **out);
mc_client_loading_status mc_client_loading_screen_construct(mc_client_loading_screen *,
                                                            client_application *,
                                                            mc_client_loading_error *);
mc_client_loading_status mc_client_loading_screen_retain(mc_client_loading_screen *);
/* Native final release preserves the owner/field holds on BUSY. Retire actual
 * namespace GPU resources after ending font calls/leases, then retry. */
mc_client_loading_status mc_client_loading_screen_release(mc_client_loading_screen *);
bool mc_client_loading_screen_inspect(const mc_client_loading_screen *, mc_client_loading_state *);

mc_client_loading_status mc_client_loading_screen_start(mc_client_loading_screen *,
                                                        mc_player_string *nullable_text,
                                                        mc_client_loading_error *); /* a(String) */
mc_client_loading_status mc_client_loading_screen_allow_stopped(mc_client_loading_screen *,
                                                                mc_player_string *ignored,
                                                                mc_client_loading_error *); /* b */
mc_client_loading_status mc_client_loading_screen_title(mc_client_loading_screen *,
                                                        mc_player_string *nullable_text,
                                                        mc_client_loading_error *); /* c */
mc_client_loading_status mc_client_loading_screen_message(mc_client_loading_screen *,
                                                          mc_player_string *nullable_text,
                                                          mc_client_loading_error *); /* d */
mc_client_loading_status mc_client_loading_screen_progress(mc_client_loading_screen *,
                                                           int32_t progress,
                                                           mc_client_loading_error *); /* a(int) */
#endif
