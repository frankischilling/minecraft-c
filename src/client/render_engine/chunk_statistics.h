#ifndef MC_CLIENT_RENDER_ENGINE_CHUNK_STATISTICS_H
#define MC_CLIENT_RENDER_ENGINE_CHUNK_STATISTICS_H
#include "nw.h"

/* Actual dk static namespace, stored once in the same Engineclasses that own
 * nw. First active use executes nw.a load, dk.D store3 and dk.b store7.
 * Caller sites identify the actual getstatic/putstatic, without advancing any
 * unrelated renderer body. Errors and outputs remain disjoint from owners. */
bool mc_client_chunk_renderer_active_use(mc_client_render_engine_classes *, mc_client_graphics_site,
                                         mc_client_graphics_error *);
bool mc_client_chunk_statistics_get(mc_client_render_engine_classes *, mc_client_graphics_site,
                                    int32_t *updates, mc_client_graphics_error *);
bool mc_client_chunk_statistics_reset(mc_client_render_engine_classes *, mc_client_graphics_site,
                                      mc_client_graphics_error *);
/* The dk.a() getstatic8/iadd12/putstatic13 field leaf. Its containing renderer
 * must call only after its actual u branch reaches8; this is not dk.a()V. */
bool mc_client_chunk_statistics_increment(mc_client_render_engine_classes *,
                                          mc_client_graphics_site, mc_client_graphics_error *);
bool mc_client_chunk_statistics_inspect(const mc_client_render_engine_classes *,
                                        mc_client_render_engine_phase *,
                                        const mc_client_tessellator **D, int32_t *updates);
#endif
