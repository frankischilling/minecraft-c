#ifndef MC_CLIENT_WINDOW_CANVAS_H
#define MC_CLIENT_WINDOW_CANVAS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct SDL_Window SDL_Window;
typedef union SDL_Event SDL_Event;

typedef struct mc_client_source_canvas mc_client_source_canvas;
typedef struct mc_client_canvas_graphics mc_client_canvas_graphics;

typedef enum mc_client_canvas_status {
    MC_CLIENT_CANVAS_OK,
    MC_CLIENT_CANVAS_ARGUMENT,
    MC_CLIENT_CANVAS_NULL_REFERENCE,
    MC_CLIENT_CANVAS_BUSY,
    MC_CLIENT_CANVAS_LIMIT,
    MC_CLIENT_CANVAS_ALLOCATION,
    MC_CLIENT_CANVAS_NATIVE_STATE,
    MC_CLIENT_CANVAS_NATIVE_PLATFORM,
    MC_CLIENT_CANVAS_NATIVE_PROFILE
} mc_client_canvas_status;

typedef struct mc_client_canvas_error {
    mc_client_canvas_status status;
    const char *operation;
    unsigned long system_error;
    bool native_policy;
} mc_client_canvas_error;

typedef struct mc_client_canvas_state {
    size_t holds, graphics;
    int32_t width, height;
    uint32_t window_id;
    bool active, peer_bound;
} mc_client_canvas_state;

/* All Canvas domain calls, including cold allocation/retirement, are externally
 * serialized. Only counted live allocations may be dereferenced. Opaque foreign
 * identities remain storable by constructors; reached methods reject native
 * provenance without dereference. NULL retains its source classification.
 * Borrowed views expire on final release. External native/GL users retain the
 * Canvas until checked release after their own complete teardown. */
/* Cold Canvas identity with actual default width/height0 and no peer. No SDL
 * initialization, window creation or source Display active use occurs here. */
mc_client_canvas_status mc_client_source_canvas_allocate(mc_client_source_canvas **,
                                                         mc_client_canvas_error *);
mc_client_canvas_status mc_client_source_canvas_retain(mc_client_source_canvas *,
                                                       mc_client_canvas_error *);
/* Final release requires no active Graphics or host peer binding. A Frame or
 * peer owner must keep its SDL_Window live until checked detach completes. */
mc_client_canvas_status mc_client_source_canvas_release(mc_client_source_canvas **,
                                                        mc_client_canvas_error *);
/* Native host-layout adapter. Stores signed Component dimensions exactly;
 * application-side clamping belongs to Minecraft, not Component.getWidth. */
mc_client_canvas_status mc_client_source_canvas_store_size(mc_client_source_canvas *, int32_t,
                                                           int32_t, mc_client_canvas_error *);
/* Bind a real already-created peer. This owner neither creates nor destroys
 * that window; the separate Frame/Canvas-peer owner controls native lifetime. */
mc_client_canvas_status mc_client_source_canvas_bind_peer(mc_client_source_canvas *, SDL_Window *,
                                                          mc_client_canvas_error *);
mc_client_canvas_status mc_client_source_canvas_detach_peer(mc_client_source_canvas *,
                                                            mc_client_canvas_error *);
/* Called for this peer's actual retained logical resize record. No event pump,
 * queue take, Display flag mutation or implicit pixel-dimension conversion. */
mc_client_canvas_status mc_client_source_canvas_resize_event(mc_client_source_canvas *,
                                                             const SDL_Event *,
                                                             mc_client_canvas_error *);
mc_client_canvas_status mc_client_source_canvas_width(const mc_client_source_canvas *, int32_t *,
                                                      mc_client_canvas_error *);
mc_client_canvas_status mc_client_source_canvas_height(const mc_client_source_canvas *, int32_t *,
                                                       mc_client_canvas_error *);
mc_client_canvas_status mc_client_source_canvas_displayable(const mc_client_source_canvas *, bool *,
                                                            mc_client_canvas_error *);
bool mc_client_source_canvas_inspect(const mc_client_source_canvas *, mc_client_canvas_state *);
SDL_Window *mc_client_source_canvas_window(const mc_client_source_canvas *);

/* The presently reached startup Graphics subset: real peer acquisition,
 * Color.BLACK, clipped fillRect and dispose. A non-displayable Canvas returns
 * NULL Graphics successfully. A native failure never pretends this branch. */
mc_client_canvas_status mc_client_source_canvas_get_graphics(mc_client_source_canvas *,
                                                             mc_client_canvas_graphics **,
                                                             mc_client_canvas_error *);
mc_client_canvas_status mc_client_canvas_graphics_black(mc_client_canvas_graphics *,
                                                        mc_client_canvas_error *);
mc_client_canvas_status mc_client_canvas_graphics_fill_rect(mc_client_canvas_graphics *, int32_t,
                                                            int32_t, int32_t, int32_t,
                                                            mc_client_canvas_error *);
mc_client_canvas_status mc_client_canvas_graphics_dispose(mc_client_canvas_graphics **,
                                                          mc_client_canvas_error *);

typedef void (*mc_client_canvas_resize_listener)(void *);
mc_client_canvas_status mc_client_source_canvas_resize_listener(mc_client_source_canvas *,
                                                                mc_client_canvas_resize_listener,
                                                                void *, mc_client_canvas_error *);
#endif
