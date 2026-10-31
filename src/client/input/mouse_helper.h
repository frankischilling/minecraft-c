#ifndef MC_CLIENT_INPUT_MOUSE_HELPER_H
#define MC_CLIENT_INPUT_MOUSE_HELPER_H

#include "../display/sdk_cursor.h"

typedef struct mc_client_source_canvas mc_client_source_canvas;
typedef struct mc_client_source_vy mc_client_source_vy;

/* Serial exclusive calls and exclusive shared ge namespace throughout. Cold
 * allocation and CPU failed prefixes need no video initialization; reached
 * cursor operations require the real SDL main thread. */

typedef enum mc_client_mouse_helper_status {
    MC_CLIENT_MOUSE_HELPER_OK,
    MC_CLIENT_MOUSE_HELPER_INVALID_ARGUMENT,
    MC_CLIENT_MOUSE_HELPER_INVALID_PHASE,
    MC_CLIENT_MOUSE_HELPER_BUSY,
    MC_CLIENT_MOUSE_HELPER_LIMIT,
    MC_CLIENT_MOUSE_HELPER_ALLOCATION_FAILED,
    MC_CLIENT_MOUSE_HELPER_GRAPHICS_FAILED,
    MC_CLIENT_MOUSE_HELPER_CURSOR_FAILED,
    MC_CLIENT_MOUSE_HELPER_SOURCE_EXCEPTION,
    MC_CLIENT_MOUSE_HELPER_UNIMPLEMENTED_DEPENDENCY,
    MC_CLIENT_MOUSE_HELPER_NULL_REFERENCE,
    MC_CLIENT_MOUSE_HELPER_INPUT_FAILED
} mc_client_mouse_helper_status;

typedef enum mc_client_mouse_helper_phase {
    MC_CLIENT_MOUSE_HELPER_RAW,
    MC_CLIENT_MOUSE_HELPER_CONSTRUCTING,
    MC_CLIENT_MOUSE_HELPER_READY,
    MC_CLIENT_MOUSE_HELPER_FAILED,
    MC_CLIENT_MOUSE_HELPER_RETIRING
} mc_client_mouse_helper_phase;

typedef struct mc_client_mouse_helper_options {
    size_t max_bytes, max_allocations;
    mc_client_sdk_cursor_limits cursor;
} mc_client_mouse_helper_options;

typedef struct mc_client_mouse_helper_error {
    mc_client_mouse_helper_status status;
    const char *operation, *exception_class;
    mc_client_graphics_site site, child;
    bool native_policy, diagnostic_frontier, source_returned;
    mc_client_graphics_error graphics;
    mc_client_sdk_cursor_error cursor;
    mc_client_render_engine_status classes;
    mc_client_sdk_input_error input;
} mc_client_mouse_helper_error;

typedef struct mc_client_mouse_helper_state {
    mc_client_mouse_helper_phase phase;
    size_t holds, native_bytes, native_allocations;
    bool active, source_c_stored, source_e_stored, source_d_stored;
    int32_t source_a, source_b, source_e, last_store_pc, pending_pc;
    mc_client_source_canvas *source_c;
    mc_client_sdk_cursor *source_d, *cursor_candidate;
    const mc_client_direct_int_buffer *delay_buffer, *image_buffer;
    const mc_client_mouse_helper_error *failure;
    const char *last_method;
} mc_client_mouse_helper_state;

mc_client_mouse_helper_options mc_client_mouse_helper_default_options(void);
/* Real RAW vy identity for Minecraft NEW369. Retains the actual shared ge/nw
 * class namespace; no ge active use, device, cursor, source field store or App
 * publication occurs. Outputs/errors/slots are live and disjoint. */
mc_client_mouse_helper_status
mc_client_mouse_helper_allocate(mc_client_render_engine_classes *,
                                const mc_client_mouse_helper_options *, mc_client_source_vy **out,
                                mc_client_mouse_helper_error *);
/* Complete vy(Component): e7, nullable c12, ge buffers16/34 and native SDK
 * Cursor54/d57. A nonnull actual Component is borrowed and must remain live
 * through retirement; this constructor never dereferences or invents Canvas.
 * The reached normal profile uses the current canonical SDK Display/input.
 * The caller publishes actual App.C380 only after OK. Failed instances retain
 * every buffer/cursor prefix and cannot retry their source constructor. A
 * genuine LWJGLException reaches the separate printStackTrace67 frontier;
 * native SDL/policy failures never masquerade as that source exception. */
mc_client_mouse_helper_status mc_client_mouse_helper_construct(mc_client_source_vy *,
                                                               mc_client_source_canvas *,
                                                               mc_client_display *,
                                                               mc_client_sdk_input *,
                                                               mc_client_mouse_helper_error *);
mc_client_mouse_helper_status mc_client_mouse_helper_retain(mc_client_source_vy *,
                                                            mc_client_mouse_helper_error *);
/* Actual vy.a()V: Mouse.setGrabbed(true)1, a=0 at6, b=0 at11, return14.
 * Actual vy.c()V independently consumes getDX1/store4 then getDY8/store11.
 * Both use the SAME canonical input passed to this owner's constructor. The
 * SDK entries retain their existing queue/pump behavior; read_motion adds no
 * poll, SDL event removal or shared read of the two accumulated deltas.
 * READY/exclusive owners and disjoint error storage are required. Native
 * cleanup never calls these source methods or changes the source grab state. */
mc_client_mouse_helper_status mc_client_mouse_helper_grab(mc_client_source_vy *,
                                                          mc_client_sdk_input *,
                                                          mc_client_mouse_helper_error *);
mc_client_mouse_helper_status mc_client_mouse_helper_read_motion(mc_client_source_vy *,
                                                                 mc_client_sdk_input *,
                                                                 mc_client_mouse_helper_error *);
/* Actual vy.b()V first dereferences c at Component.getWidth4. NULL fails there
 * before any Mouse operation. A nonnull Component remains an explicit owner
 * dependency; SDL window dimensions do not substitute for that source object. */
mc_client_mouse_helper_status mc_client_mouse_helper_ungrab(mc_client_source_vy *,
                                                            mc_client_sdk_input *,
                                                            mc_client_mouse_helper_error *);
mc_client_mouse_helper_status mc_client_mouse_helper_release(mc_client_source_vy **,
                                                             mc_client_mouse_helper_error *);
bool mc_client_mouse_helper_inspect(const mc_client_source_vy *, mc_client_mouse_helper_state *);
/* Inspection views expire at the next owner mutation. Generic Cursor
 * selection/animation/destroy are separate unimplemented bodies.
 * Native cleanup never calls those source bodies or changes the mouse grab. */

#endif
