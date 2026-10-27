#ifndef MC_CLIENT_RENDER_ENGINE_FONT_LEASE_H
#define MC_CLIENT_RENDER_ENGINE_FONT_LEASE_H
#include "mc/client/render_engine/render_engine.h"
#include "source.h"

typedef struct mc_client_font_lease mc_client_font_lease;

/* Borrowed actual owner identity, for native same-namespace admission at a
 * reached source use. This neither initializes ge/nw nor derives it from p. */
const mc_client_render_engine_classes *
mc_client_render_engine_class_identity(const mc_client_render_engine *);

/* Native capture after the real sj ID store245. Retains actual Engine/classes
 * and one immutable Display generation; a numeric ID owns no occurrence.
 * A failed capture preserves every earlier registration/store effect. */
bool mc_client_render_engine_font_lease(mc_client_render_engine *, mc_client_display *,
                                        mc_client_texture id, mc_client_graphics_site,
                                        mc_client_font_lease **out, mc_client_graphics_error *);
bool mc_client_font_lease_begin(mc_client_font_lease *, mc_client_graphics_error *);
bool mc_client_font_lease_bind(mc_client_font_lease *, mc_client_graphics_error *);
bool mc_client_font_lease_end(mc_client_font_lease *, mc_client_graphics_error *);
/* No source deletion/ledger mutation. A terminal cleanup can retain only its
 * last class hold if the caller already dropped the primary context while
 * real resources remain: draw is then revoked, namespace retirement is allowed
 * and release is retryable. All remaining ownership stays in this same object. */
bool mc_client_font_lease_release(mc_client_font_lease **, mc_client_graphics_error *);
#endif
