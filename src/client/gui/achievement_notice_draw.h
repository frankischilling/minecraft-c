#ifndef MC_CLIENT_GUI_ACHIEVEMENT_NOTICE_DRAW_H
#define MC_CLIENT_GUI_ACHIEVEMENT_NOTICE_DRAW_H
#include "../render/item_gui.h"
#include "../render_engine/lighting.h"
#include "../startup/application_owner.h"
#include "achievement_notice_internal.h"
#include "bitmap_font.h"
#include "mc/client/gui/scaled_resolution.h"
#include "mc/client/settings/game_settings.h"
#include "millis.h"

typedef struct mc_client_notice_draw_call mc_client_notice_draw_call;

typedef enum mc_client_notice_draw_status {
    MC_CLIENT_NOTICE_DRAW_OK,
    MC_CLIENT_NOTICE_DRAW_ARGUMENT,
    MC_CLIENT_NOTICE_DRAW_BUSY,
    MC_CLIENT_NOTICE_DRAW_ALLOCATION,
    MC_CLIENT_NOTICE_DRAW_DEPENDENCY,
    MC_CLIENT_NOTICE_DRAW_SOURCE_NULL,
    MC_CLIENT_NOTICE_DRAW_CLOCK,
    MC_CLIENT_NOTICE_DRAW_UNSUPPORTED_FP,
    MC_CLIENT_NOTICE_DRAW_CHILD_FAILED,
    MC_CLIENT_NOTICE_DRAW_CLEANUP_FAILED
} mc_client_notice_draw_status;

/* Heap-retained operation detail, deliberately separate from the compact
 * mc_client_notice_error embedded by value in Player/Startup diagnostics. */
typedef struct mc_client_notice_draw_error {
    mc_client_notice_draw_status status;
    const char *operation, *exception_class;
    mc_client_graphics_site site, child;
    bool native_policy, source_exception;
    mc_item_render_throwable throwable;
    mc_client_display_error display;
    mc_client_graphics_error graphics;
    mc_client_lighting_error lighting;
    mc_client_bitmap_font_wrap_error font;
    mc_client_texture_lookup_error texture;
    mc_client_application_owner_error application;
    mc_game_settings_error settings;
    mc_math_class_error math;
    mc_platform_millis_status clock;
    mc_client_render_engine_status graphics_owner, engine_owner;
    mc_client_bitmap_font_status font_owner;
    const mc_client_item_gui_error *item; /* Same retained child, not ambient state. */
} mc_client_notice_draw_error;

typedef struct mc_client_notice_draw_state {
    const mc_client_achievement_notice *owner;
    const client_application *application;
    const mc_client_render_engine_classes *graphics;
    mc_client_display *display;
    bool active, retiring, returned, inactive, expired, clock_read, watermark;
    bool projection_returned, background_drawn, tutorial, item_returned;
    int32_t source_pc, projection_pc, background_pc, physical_width, physical_height;
    int32_t x, y;
    int64_t watermark_value, now, g;
    double elapsed, easing;
    mc_scaled_resolution resolution;
    size_t gl_calls, text_calls, background_vertices, projection_returns;
    double vertices[4][5];
    mc_client_texture texture_id;
    bool texture_lookup_returned, texture_cache_hit, pending_lookup_owned;
    mc_client_item_gui_state item;
    const mc_client_notice_draw_error *failure, *cleanup;
} mc_client_notice_draw_state;

/* Actual VU.a plus private VU.b projection, using the real App namespace H,
 * real wall clock, canonical Font/Engine/NW and BB overload. No clock, caption,
 * icon or gameplay state is injected. NULL source operands fail only at their
 * reached dereference. Explicit graphics/Math/Display are native services from
 * the same bootstrap; they and their providers remain live/exclusive through
 * checked retirement. No generic renderer callback is admitted.
 *
 * A successfully allocated call is published on success AND failure. It pins
 * the Notice against mutation/free, its actual registered App, and each reached
 * Font/Engine/graphics/Math/Settings occurrence. Caller error storage is large:
 * keep it heap-owned and disjoint from owners/out. *out must initially be NULL.
 * Source failure never restores matrices/GL, expires a notice, commits a draw,
 * consumes another owner's pending lookup, or replays the source operation. */
mc_client_notice_draw_status mc_client_notice_draw_begin(mc_client_achievement_notice *,
                                                         mc_client_render_engine_classes *,
                                                         mc_math_classes *, mc_client_display *,
                                                         mc_client_notice_draw_call **out,
                                                         mc_client_notice_draw_error *);
bool mc_client_notice_draw_inspect(const mc_client_notice_draw_call *,
                                   mc_client_notice_draw_state *);
mc_client_notice_draw_status mc_client_notice_draw_retire(mc_client_notice_draw_call **,
                                                          mc_client_notice_draw_error *);
#endif
