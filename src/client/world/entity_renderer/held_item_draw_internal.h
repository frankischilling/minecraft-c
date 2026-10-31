#ifndef MC_CLIENT_HELD_ITEM_DRAW_INTERNAL_H
#define MC_CLIENT_HELD_ITEM_DRAW_INTERNAL_H
#include "mc/client/render/model_draw.h"
#include "mc/client/world/held_item_draw.h"

/* Actual DS.b on its genuine manager-owned renderer. Caller keeps renderer,
 * namespaces and Display alive/exclusive through pending model-list cleanup.
 * No current color, matrix, NW state or pool cursor is installed here. */
bool entity_renderer_ds_arm_draw(mc_client_managed_renderer *, mc_math_classes *,
                                 mc_client_render_engine_classes *, mc_entity_classes *,
                                 mc_client_display *, mc_client_model_draw_error *,
                                 mc_math_class_error *);

/* Borrow the SAME already active RA occurrence. The parent owns its guard and
 * occurrence through child retirement. Ordinary PX callers use public begin
 * because PX's active guard is distinct from RA's guard. */
mc_client_held_item_draw_status entity_renderer_held_draw_entered(mc_client_held_item *,
                                                                  mc_math_classes *, float partial,
                                                                  bool overlays,
                                                                  mc_client_held_item_draw_call **,
                                                                  mc_client_held_item_draw_error *);
#endif
