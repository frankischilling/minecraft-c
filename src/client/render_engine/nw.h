#ifndef MC_CLIENT_RENDER_ENGINE_NW_H
#define MC_CLIENT_RENDER_ENGINE_NW_H
#include "ge.h"

typedef struct mc_client_tessellator mc_client_tessellator;

typedef struct mc_client_nw_fields {
    const mc_client_direct_byte_buffer *d;
    const mc_client_direct_int_buffer *e;
    const mc_client_direct_float_buffer *f;
    const int32_t *g;
    int32_t h;
    double i, j;
    int32_t k;
    bool l, m, n;
    int32_t o, p;
    bool q;
    int32_t r;
    double s, t, u;
    int32_t v;
    bool w, x;
    const mc_client_direct_int_buffer *y;
    int32_t z, A, B;
} mc_client_nw_fields;

typedef struct mc_client_nw_state {
    mc_client_render_engine_phase phase;
    bool static_b, static_c;
    const mc_client_tessellator *a, *failed_candidate;
    mc_client_nw_fields fields;
    size_t constructor_bytes, constructor_allocations;
    int32_t last_store_pc;
    mc_client_graphics_error failure;
} mc_client_nw_state;

/* Cold creation is native-only and has no ge/nw active use or Display.
 * Old public classes_create delegates here with matching original defaults. */
mc_client_render_engine_status mc_client_render_engine_classes_allocate_context(
    const mc_client_render_engine_options *ge, const mc_client_render_engine_options *nw,
    const mc_client_graphics_limits *, mc_client_render_engine_classes **out);

bool mc_client_nw_active_use(mc_client_render_engine_classes *, mc_client_graphics_site caller,
                             mc_client_tessellator **out, mc_client_graphics_error *);
bool mc_client_nw_inspect(const mc_client_render_engine_classes *, mc_client_nw_state *);

/* Actual methods on the genuine singleton. These preserve source stores and
 * per-call class holds. Caller keeps that real class context live/exclusive.
 * Default c=false CPU construction and vertex work require no Display. */
bool mc_client_nw_begin(mc_client_tessellator *, int32_t mode, mc_client_graphics_error *);
bool mc_client_nw_begin_quads(mc_client_tessellator *, mc_client_graphics_error *);
bool mc_client_nw_uv(mc_client_tessellator *, double u, double v, mc_client_graphics_error *);
/* Actual nw.b(DDD)V: raw source offset stores, including outside a begin/draw. */
bool mc_client_nw_translation(mc_client_tessellator *, double x, double y, double z,
                              mc_client_graphics_error *);
bool mc_client_nw_color(mc_client_tessellator *, int32_t red, int32_t green, int32_t blue,
                        int32_t alpha, mc_client_graphics_error *);
/* Actual nw.a(FFF)V -> a(III)V -> a(IIII)V, with binary32 *255 and Java f2i
 * before the integer color guard/stores. It does not require an open batch. */
bool mc_client_nw_color_float(mc_client_tessellator *, float red, float green, float blue,
                              mc_client_graphics_error *);
bool mc_client_nw_rgb(mc_client_tessellator *, int32_t packed_rgb, mc_client_graphics_error *);
bool mc_client_nw_vertex(mc_client_tessellator *, double x, double y, double z,
                         mc_client_graphics_error *);
bool mc_client_nw_vertex_uv(mc_client_tessellator *, double x, double y, double z, double u,
                            double v, mc_client_graphics_error *);
bool mc_client_nw_draw(mc_client_tessellator *, mc_client_display *, mc_client_graphics_error *);

/* Native retirement of the actual namespace candidate/singleton, no source
 * draw/delete/dispose. Called only from last inactive class release. */
void mc_client_nw_free_namespace(mc_client_render_engine_classes *);
#endif
