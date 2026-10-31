#ifndef MC_CLIENT_GUI_GUI_MAIN_MENU_H
#define MC_CLIENT_GUI_GUI_MAIN_MENU_H

#include "../display/display.h"
#include "../resources/assets.h"
#include "mc/client/gui/scaled_resolution.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct mc_gui_main_menu mc_gui_main_menu;

enum { MC_GUI_MAIN_MENU_MAX_BUTTONS = 32 };

typedef enum mc_gui_main_menu_status {
    MC_GUI_MAIN_MENU_OK = 0,
    MC_GUI_MAIN_MENU_INVALID_ARGUMENT,
    MC_GUI_MAIN_MENU_ALLOCATION_FAILED,
    MC_GUI_MAIN_MENU_LIMIT,
    MC_GUI_MAIN_MENU_UNSUPPORTED_STATE,
    MC_GUI_MAIN_MENU_ASSETS_FAILED,
    MC_GUI_MAIN_MENU_FONT_FAILED,
    MC_GUI_MAIN_MENU_DISPLAY_FAILED,
    MC_GUI_MAIN_MENU_CLOCK_FAILED
} mc_gui_main_menu_status;

typedef struct mc_gui_main_menu_error {
    mc_gui_main_menu_status status;
    const char *operation;
    mc_client_assets_error assets;
    mc_font_renderer_status font;
    mc_client_display_error display;
} mc_gui_main_menu_error;

/* Read-only ordinary GuiButton state. NULL label is a Java null String;
 * nonnull labels may be empty. Identity/action routing remains caller-owned. */
typedef struct mc_gui_button {
    int32_t id, x, y, width, height;
    const mc_font_text *label;
    bool enabled, visible;
    float z_level;
} mc_gui_button;

typedef struct mc_gui_main_menu_frame {
    mc_scaled_resolution resolution;
    const mc_font_text *splash;
    const mc_gui_button *buttons;
    size_t button_count;
    mc_gui_mouse_position mouse;
    float z_level;
    float partial_tick; /* Unread by the source fu/ordinary-ke draw path. */
} mc_gui_main_menu_frame;

/* Called once after background/logo drawing and before pulse arithmetic, at
 * the source fu.a(IIF)V PC133 position. Failure leaves *out unchanged. The
 * callback may not reenter/retire the menu or its borrowed owners. A caller
 * may supply an explicit conditional clock input; the renderer never derives
 * time from pixels or accepts a precomputed pulse/scale. */
typedef bool (*mc_gui_main_menu_millis_fn)(void *context, int64_t *out);

/* Narrow source Font bridge. Text is a borrowed nullable UTF-16 view and the
 * callback performs its real source measurement. The native glyph kernel must
 * produce the same width; a disagreement stops drawing at that measurement.
 * Callback/owners must not reenter or mutate any borrowed drawing state. */
typedef bool (*mc_gui_main_menu_measure_fn)(void *context, const mc_font_text *, int32_t *out);

/* Default-pack, no-World GuiMainMenu draw owner. Borrow assets
 * and display, which must outlive it and remain exclusively usable here.
 * Creation loads/uploads the genuine font image at this caller-selected point;
 * App must place it at its existing font-image step before title Random/splash
 * construction. Font.txt parsing, FontRenderer creation and trig initialization
 * remain lazy inside draw after background/logo and the real clock call.
 * The initial anaglyph flag selects the real texture RGB conversion and font
 * palette construction. It stays fixed for this owner; live texture refresh
 * and replacement after an options action remain separate operations.
 * No label translation, splash selection, calendar, event or frame-clock owner.
 * GL IDs belong to display; decoded images belong to assets. out is unchanged
 * on failure. Retire any prior owner before successful replacement. An uploaded
 * prefix remains display-owned after a later failure. out/error storage is
 * disjoint from owners and their dependencies. */
bool mc_gui_main_menu_create(mc_client_display *display, mc_client_assets *assets, bool anaglyph,
                             mc_gui_main_menu **out, mc_gui_main_menu_error *error);
void mc_gui_main_menu_free(mc_gui_main_menu *menu); /* NULL is harmless; no GL call. */

/* One shared production body for App and a conditional native probe. Borrowed
 * frame/text/button storage remains unchanged and valid through the call. Its
 * storage and error are disjoint from owners/dependencies. button_count is
 * bounded to32; NULL buttons requires zero count. Ordinary ke visibility,
 * enabled, half-open hover and Java int arithmetic are preserved in array order.
 * Nonzero z-level rendering is unfinished and fails at its reached draw point.
 * This does not dispatch arbitrary GuiButton subclasses or a World gradient.
 * Exact positive projection extents and positive screen dimensions are required.
 * Display begin establishes the current physical viewport/depth state. No swap,
 * capture, alpha normalization, input polling, timer sampling or sleep occurs.
 * Failure preserves the reached resource/GL/font prefix; it is not a rollback.
 * error resets on success and records a reached cause on failure. */
bool mc_gui_main_menu_draw(mc_gui_main_menu *menu, const mc_gui_main_menu_frame *frame,
                           mc_gui_main_menu_millis_fn read_millis, void *clock_context,
                           mc_gui_main_menu_error *error);

/* Same drawing body with canonical source Font measurement at each reached
 * measurement point. The original diagnostic entry above selects no callback.
 * This does not replace labels, positions, Random, clock or menu state. */
bool mc_gui_main_menu_draw_measured(mc_gui_main_menu *, const mc_gui_main_menu_frame *,
                                    mc_gui_main_menu_millis_fn, void *clock_context,
                                    mc_gui_main_menu_measure_fn, void *measure_context,
                                    mc_gui_main_menu_error *);
bool mc_gui_main_menu_same_source(const mc_gui_main_menu *, const mc_client_display *,
                                  const mc_registry_resources *, const mc_registry_classes *,
                                  bool anaglyph);

/* Shared no-World GUI kernels, without FU logo/splash/version/copyright.
 * Begin owns one exclusive drawing scope and the existing GL3.3 projection;
 * every successful begin requires end, including a later failed draw. End is
 * native scope cleanup only and never rolls back GL effects. Text measures the
 * same captured source Font before using the established glyph kernels.
 * Quad vertices retain source order; background selects the real dirt image,
 * false selects untextured vertex colors. These kernels do not sample input,
 * clocks, select rows, swap, or claim arbitrary fixed-function GL state. */
bool mc_gui_canvas_begin(mc_gui_main_menu *, const mc_scaled_resolution *,
                         mc_gui_main_menu_measure_fn, void *measure_context,
                         mc_gui_main_menu_error *);
bool mc_gui_canvas_quad(mc_gui_main_menu *, bool background, const mc_client_vertex[4], bool blend,
                        bool alpha_test, mc_gui_main_menu_error *);
bool mc_gui_canvas_text(mc_gui_main_menu *, const mc_font_text *, int32_t x, int32_t y,
                        uint32_t color, bool centered, mc_gui_main_menu_error *);
bool mc_gui_canvas_button(mc_gui_main_menu *, const mc_gui_button *, mc_gui_mouse_position,
                          mc_gui_main_menu_error *);
void mc_gui_canvas_end(mc_gui_main_menu *);

#endif
