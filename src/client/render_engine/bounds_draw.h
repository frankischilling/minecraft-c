#ifndef MC_CLIENT_RENDER_ENGINE_BOUNDS_DRAW_H
#define MC_CLIENT_RENDER_ENGINE_BOUNDS_DRAW_H

#include "mc/entity/bounds/bounds.h"
#include "nw.h"

/* Actual static bw.a(Leq;)V, reached through inherited bb.a at dk.a279.
 * Use the SAME loaded ge/nw namespace and Display. No BB, Random, renderer,
 * manager, bounds factory or World is constructed here. Caller keeps all owners
 * live/exclusive and retains the actual box through the call. Null bounds is
 * reached at getfield10 AFTER nw.begin5, leaving nw already tessellating.
 * Failure retains the exact NW/list prefix. No source draw/end/reset/abort or
 * GL state repair is added. Child error sites remain intact; parent identifies
 * the reached BW invocation and entry identifies this actual source body. */
bool mc_client_render_bounds_draw(mc_client_render_engine_classes *classes,
                                  mc_client_display *display, const mc_entity_bounds *bounds,
                                  mc_client_graphics_error *error);

#endif
