#include "mc/client/world/frustum.h"

#include "../display/world_frame_surface.h"
#include "../render/model_graph_internal.h"
#include "../render_engine/ge.h"
#include "../render_engine/internal.h"

#include <emmintrin.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_M_X64) && !defined(_M_AMD64) && !defined(__x86_64__)
#error "Source frustum arithmetic requires x86-64/SSE2"
#endif
#if defined(__FAST_MATH__) || (defined(__FINITE_MATH_ONLY__) && __FINITE_MATH_ONLY__ > 0) ||       \
    (defined(_M_FP_FAST) && _M_FP_FAST)
#error "Source frustum arithmetic requires strict floating transformations"
#endif

struct mc_client_source_dr {
    float **a;
    float *b, *c, *d;
};

typedef struct source_w {
    mc_client_source_dr base;
    mc_client_direct_float_buffer *f, *g, *h;
} source_w;

typedef struct mc_client_frustum_namespace {
    mc_client_render_arena arena;
    mc_client_render_engine_classes *graphics;
    source_w *e, *candidate;
    mc_client_frustum_error failure, first_failure;
    const char *last_store_class, *last_store_method;
    unsigned int last_store_pc, matrix_stores, plane_stores, normalized_planes;
    bool initialized, failed, active, returned;
} mc_client_frustum_namespace;

static mc_client_graphics_site site(const char *type, const char *method, const char *descriptor,
                                    unsigned int pc) {
    return (mc_client_graphics_site){type, method, descriptor, (int32_t)pc};
}

static bool fail(mc_client_frustum_namespace *n, mc_client_frustum_status status,
                 const char *operation, const char *type, const char *method,
                 const char *descriptor, unsigned int pc, bool native) {
    n->failure.status = status;
    n->failure.operation = operation;
    n->failure.site = site(type, method, descriptor, pc);
    n->failure.native_policy = native;
    return false;
}

static mc_client_frustum_status result(mc_client_frustum_error *error,
                                       mc_client_frustum_status status, const char *operation) {
    if (error != NULL)
        *error = (mc_client_frustum_error){.status = status,
                                           .operation = operation,
                                           .native_policy = status != MC_CLIENT_FRUSTUM_OK};
    return status;
}

static bool fp(mc_client_frustum_namespace *n, const char *method, const char *descriptor,
               unsigned int pc) {
    return (_mm_getcsr() & 0xffc0u) == 0x1f80u ||
           fail(n, MC_CLIENT_FRUSTUM_UNSUPPORTED_FP, "frustum native floating environment", "w",
                method, descriptor, pc, true);
}

static float add(float a, float b) {
    return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float sub(float a, float b) {
    return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float mul(float a, float b) {
    return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static float divide(float a, float b) {
    return _mm_cvtss_f32(_mm_div_ss(_mm_set_ss(a), _mm_set_ss(b)));
}

static bool graphics_result(mc_client_frustum_namespace *n, bool ok,
                            mc_client_graphics_site parent) {
    if (ok)
        return true;
    const mc_client_graphics_error *g = &n->failure.graphics;
    n->failure.status =
        g->status == MC_CLIENT_GRAPHICS_LIMIT               ? MC_CLIENT_FRUSTUM_LIMIT
        : g->status == MC_CLIENT_GRAPHICS_ALLOCATION_FAILED ? MC_CLIENT_FRUSTUM_ALLOCATION
        : g->status == MC_CLIENT_GRAPHICS_NULL_REFERENCE    ? MC_CLIENT_FRUSTUM_SOURCE_NULL
                                                            : MC_CLIENT_FRUSTUM_CHILD_FAILED;
    n->failure.operation = g->operation;
    n->failure.site = g->site;
    n->failure.parent = parent;
    n->failure.native_policy = g->native_policy;
    return false;
}

static void stored(mc_client_frustum_namespace *n, const char *type, const char *method,
                   unsigned int pc) {
    n->last_store_class = type;
    n->last_store_method = method;
    n->last_store_pc = pc;
}

static void *allocate(mc_client_frustum_namespace *n, size_t bytes, const char *type,
                      unsigned int pc) {
    void *value = mc_client_render_arena_allocate(&n->arena, bytes, site(type, "<init>", "()V", pc),
                                                  &n->failure.graphics);
    if (value == NULL)
        (void)graphics_result(n, false, site("w", "<clinit>", "()V", 4));
    return value;
}

static bool construct(mc_client_frustum_namespace *n) {
    n->candidate = mc_client_render_arena_allocate(
        &n->arena, sizeof(*n->candidate), site("w", "<clinit>", "()V", 0), &n->failure.graphics);
    if (n->candidate == NULL)
        return graphics_result(n, false, site("w", "<clinit>", "()V", 0));
    mc_client_source_dr *d = &n->candidate->base;
    float **rows = allocate(n, 16 * sizeof(*rows), "dr", 9);
    if (rows == NULL)
        return false;
    for (size_t i = 0; i < 16; ++i) {
        rows[i] = allocate(n, 16 * sizeof(float), "dr", 9);
        if (rows[i] == NULL)
            return false;
    }
    d->a = rows;
    stored(n, "dr", "<init>", 13);
    float **arrays[] = {&d->b, &d->c, &d->d};
    static const unsigned int allocation_pcs[] = {19, 27, 35}, store_pcs[] = {21, 29, 37};
    for (size_t i = 0; i < 3; ++i) {
        float *array = allocate(n, 16 * sizeof(float), "dr", allocation_pcs[i]);
        if (array == NULL)
            return false;
        *arrays[i] = array;
        stored(n, "dr", "<init>", store_pcs[i]);
    }
    mc_client_direct_float_buffer **buffers[] = {&n->candidate->f, &n->candidate->g,
                                                 &n->candidate->h};
    static const unsigned int ge_pcs[] = {7, 16, 25}, field_pcs[] = {10, 19, 28};
    for (size_t i = 0; i < 3; ++i) {
        const mc_client_graphics_site caller = site("w", "<init>", "()V", ge_pcs[i]);
        if (!graphics_result(n,
                             mc_client_ge_e(n->graphics, &n->arena, 16, caller, buffers[i],
                                            &n->failure.graphics),
                             caller))
            return false;
        stored(n, "w", "<init>", field_pcs[i]);
    }
    n->e = n->candidate; /* Actual static e7, after constructor return31. */
    stored(n, "w", "<clinit>", 7);
    n->initialized = true;
    return true;
}

static bool query(mc_client_frustum_namespace *n, mc_client_display *display,
                  mc_client_direct_float_buffer *buffer, bool projection, unsigned int pc) {
    const mc_client_graphics_site caller = site("w", "b", "()V", pc);
    void *storage;
    size_t remaining;
    if (!graphics_result(n,
                         mc_client_ge_float_writable_remaining(buffer, &storage, &remaining, caller,
                                                               &n->failure.graphics),
                         caller))
        return false;
    if (remaining < 16)
        return fail(n, MC_CLIENT_FRUSTUM_CHILD_FAILED, "actual LWJGL FloatBuffer minimum16", "w",
                    "b", "()V", pc, false);
    float matrix[16];
    if (!mc_client_display_source_frustum_matrix(display, projection, matrix,
                                                 &n->failure.graphics.display))
        return fail(n, MC_CLIENT_FRUSTUM_NATIVE_FAILED, n->failure.graphics.display.operation, "w",
                    "b", "()V", pc, true);
    memcpy(storage, matrix, sizeof(matrix)); /* GL write preserves Buffer position. */
    return true;
}

static bool normalize(mc_client_frustum_namespace *n, mc_math_classes *math, float *plane,
                      unsigned int parent_pc) {
    if (!fp(n, "a", "([[FI)V", 10))
        return false;
    const float sum =
        add(add(mul(plane[0], plane[0]), mul(plane[1], plane[1])), mul(plane[2], plane[2]));
    if (mc_math_classes_ensure(math, &n->failure.math) != MC_MATH_CLASS_OK) {
        n->failure.parent = site("w", "b", "()V", parent_pc);
        return fail(n, MC_CLIENT_FRUSTUM_CHILD_FAILED, "actual frustum MathHelper active use", "w",
                    "a", "([[FI)V", 35, true);
    }
    if (!fp(n, "a", "([[FI)V", 35))
        return false;
    const __m128d root = _mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd((double)sum));
    const float norm = _mm_cvtss_f32(_mm_cvtsd_ss(_mm_setzero_ps(), root));
    static const unsigned int pcs[] = {47, 56, 65, 74};
    for (size_t i = 0; i < 4; ++i) {
        plane[i] = divide(plane[i], norm);
        stored(n, "w", "a", pcs[i]);
    }
    ++n->normalized_planes;
    return true;
}

static bool update(mc_client_frustum_namespace *n, mc_client_display *display,
                   mc_math_classes *math) {
    source_w *w = n->e;
    mc_client_direct_float_buffer *buffers[] = {w->f, w->g, w->h};
    static const unsigned int clear_pcs[] = {4, 12, 20};
    for (size_t i = 0; i < 3; ++i) {
        const mc_client_graphics_site caller = site("w", "b", "()V", clear_pcs[i]);
        if (!graphics_result(n, mc_client_ge_float_clear(buffers[i], caller, &n->failure.graphics),
                             caller))
            return false;
    }
    if (!query(n, display, w->f, true, 31) || !query(n, display, w->g, false, 41))
        return false;
    float *arrays[] = {w->base.b, w->base.c};
    static const unsigned int flip_pcs[] = {48, 73}, limit_pcs[] = {53, 78}, get_pcs[] = {65, 90};
    for (size_t i = 0; i < 2; ++i) {
        mc_client_graphics_site caller = site("w", "b", "()V", flip_pcs[i]);
        if (!graphics_result(n, mc_client_ge_float_flip(buffers[i], caller, &n->failure.graphics),
                             caller))
            return false;
        caller.pc = (int32_t)limit_pcs[i];
        if (!graphics_result(
                n, mc_client_ge_float_limit(buffers[i], 16, caller, &n->failure.graphics), caller))
            return false;
        caller.pc = (int32_t)get_pcs[i];
        if (!graphics_result(n,
                             mc_client_ge_float_get_array(buffers[i], arrays[i], 16, caller,
                                                          &n->failure.graphics),
                             caller))
            return false;
    }
    if (!fp(n, "b", "()V", 111))
        return false;
    static const unsigned int matrix_pcs[] = {156, 219, 283, 347, 412, 477,  544,  611,
                                              679, 747, 816, 885, 953, 1021, 1090, 1159};
    mc_client_source_dr *d = &w->base;
    for (size_t i = 0; i < 16; ++i) {
        const size_t base = (i / 4) * 4, row = i % 4;
        float value = mul(d->c[base], d->b[row]);
        for (size_t k = 1; k < 4; ++k)
            value = add(value, mul(d->c[base + k], d->b[row + k * 4]));
        d->d[i] = value;
        stored(n, "w", "b", matrix_pcs[i]);
        ++n->matrix_stores;
    }
    static const unsigned int component[] = {0, 0, 1, 1, 2, 2};
    static const unsigned int plane_pcs[6][4] = {
        {1180, 1202, 1225, 1248}, {1278, 1300, 1323, 1346}, {1376, 1398, 1421, 1444},
        {1474, 1496, 1519, 1542}, {1572, 1595, 1618, 1641}, {1671, 1694, 1717, 1740}};
    static const unsigned int normal_pcs[] = {1255, 1353, 1451, 1549, 1648, 1747};
    for (size_t plane = 0; plane < 6; ++plane) {
        for (size_t k = 0; k < 4; ++k) {
            const float left = d->d[k * 4 + 3], right = d->d[k * 4 + component[plane]];
            const bool plus = plane == 1 || plane == 2 || plane == 5;
            d->a[plane][k] = plus ? add(left, right) : sub(left, right);
            stored(n, "w", "b", plane_pcs[plane][k]);
            ++n->plane_stores;
        }
        if (!normalize(n, math, d->a[plane], normal_pcs[plane]))
            return false;
    }
    n->returned = true;
    return true;
}

mc_client_frustum_status mc_client_frustum_update(mc_client_render_classes *models,
                                                  mc_client_render_engine_classes *graphics,
                                                  mc_client_display *display, mc_math_classes *math,
                                                  const mc_client_source_dr **out,
                                                  mc_client_frustum_error *error) {
    if (models == NULL || graphics == NULL || out == NULL || error == NULL)
        return result(error, MC_CLIENT_FRUSTUM_ARGUMENT, "frustum native namespace/output");
    if (models->frustum_retired)
        return result(error, MC_CLIENT_FRUSTUM_BUSY, "frustum terminal class namespace");
    mc_client_frustum_namespace *n = models->frustum;
    if (n == NULL) {
        n = calloc(1, sizeof(*n));
        if (n == NULL)
            return result(error, MC_CLIENT_FRUSTUM_ALLOCATION, "frustum class metadata allocation");
        if (mc_client_render_engine_classes_retain(graphics) != MC_CLIENT_RENDER_ENGINE_OK) {
            free(n);
            return result(error, MC_CLIENT_FRUSTUM_BUSY, "frustum actual ge namespace occurrence");
        }
        n->graphics = graphics;
        n->arena.options = graphics->arena.options;
        models->frustum = n;
    }
    if (n->active || n->graphics != graphics)
        return result(error, MC_CLIENT_FRUSTUM_BUSY, "frustum active/foreign graphics namespace");
    if (n->failed) {
        *error = n->first_failure;
        error->status = MC_CLIENT_FRUSTUM_CLASS_FAILED;
        error->parent = site("w", "a", "()Ldr;", 0);
        return error->status;
    }
    n->active = true;
    n->failure = (mc_client_frustum_error){0};
    n->returned = false;
    n->matrix_stores = n->plane_stores = n->normalized_planes = 0;
    if (!n->initialized && !construct(n)) {
        n->failed = true;
        n->first_failure = n->failure;
    } else if (!update(n, display, math)) {
        /* Method failure preserves the initialized class and source writes. */
    } else
        *out = &n->e->base;
    n->active = false;
    *error = n->failure;
    return error->status;
}

bool mc_client_frustum_inspect(const mc_client_render_classes *models,
                               mc_client_frustum_state *out) {
    if (models == NULL || out == NULL)
        return false;
    mc_client_frustum_namespace *n = models->frustum;
    if (n != NULL && n->active)
        return false;
    *out = (mc_client_frustum_state){0};
    if (n == NULL)
        return true;
    out->initialized = n->initialized;
    out->failed = n->failed;
    out->active = n->active;
    out->static_published = n->e != NULL;
    out->returned = n->returned;
    out->last_store_class = n->last_store_class;
    out->last_store_method = n->last_store_method;
    out->last_store_pc = n->last_store_pc;
    out->matrix_stores = n->matrix_stores;
    out->plane_stores = n->plane_stores;
    out->normalized_planes = n->normalized_planes;
    out->bytes = n->arena.bytes;
    out->allocations = n->arena.allocations;
    out->singleton = n->e == NULL ? NULL : &n->e->base;
    out->candidate = n->candidate == NULL ? NULL : &n->candidate->base;
    out->failure = &n->failure;
    if (n->candidate != NULL) {
        const mc_client_source_dr *d = &n->candidate->base;
        for (size_t i = 0; d->a != NULL && i < 16; ++i)
            out->planes[i] = d->a[i];
        out->projection = d->b;
        out->modelview = d->c;
        out->combined = d->d;
        const mc_client_direct_float_buffer *buffers[] = {n->candidate->f, n->candidate->g,
                                                          n->candidate->h};
        for (size_t i = 0; i < 3; ++i)
            if (buffers[i] != NULL)
                (void)mc_client_ge_float_inspect(buffers[i], &out->buffers[i]);
    }
    return true;
}

mc_client_frustum_status mc_client_frustum_box(const mc_client_source_dr *d, double min_x,
                                               double min_y, double min_z, double max_x,
                                               double max_y, double max_z, bool *out,
                                               mc_client_frustum_error *error) {
    if (out == NULL)
        return result(error, MC_CLIENT_FRUSTUM_ARGUMENT, "frustum box output");
    if (d == NULL || d->a == NULL) {
        result(error, MC_CLIENT_FRUSTUM_SOURCE_NULL, "actual dr plane array receiver");
        if (error != NULL) {
            error->native_policy = false;
            error->site = site("dr", "a", "(DDDDDD)Z", d == NULL ? 11 : 16);
        }
        return MC_CLIENT_FRUSTUM_SOURCE_NULL;
    }
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return result(error, MC_CLIENT_FRUSTUM_UNSUPPORTED_FP, "frustum box FP admission");
    for (size_t plane = 0; plane < 6; ++plane) {
        bool visible = false;
        for (unsigned int corner = 0; corner < 8; ++corner) {
            const double x = (corner & 1u) != 0 ? max_x : min_x;
            const double y = (corner & 2u) != 0 ? max_y : min_y;
            const double z = (corner & 4u) != 0 ? max_z : min_z;
            __m128d value = _mm_mul_sd(_mm_set_sd((double)d->a[plane][0]), _mm_set_sd(x));
            value =
                _mm_add_sd(value, _mm_mul_sd(_mm_set_sd((double)d->a[plane][1]), _mm_set_sd(y)));
            value =
                _mm_add_sd(value, _mm_mul_sd(_mm_set_sd((double)d->a[plane][2]), _mm_set_sd(z)));
            value = _mm_add_sd(value, _mm_set_sd((double)d->a[plane][3]));
            if (_mm_cvtsd_f64(value) > 0.0) {
                visible = true;
                break;
            }
        }
        if (!visible) {
            *out = false;
            return result(error, MC_CLIENT_FRUSTUM_OK, NULL);
        }
    }
    *out = true;
    return result(error, MC_CLIENT_FRUSTUM_OK, NULL);
}

mc_client_frustum_status mc_client_frustum_namespace_retire(mc_client_render_classes *models,
                                                            mc_client_frustum_error *error) {
    if (models == NULL)
        return result(error, MC_CLIENT_FRUSTUM_ARGUMENT, "frustum namespace retirement");
    mc_client_frustum_namespace *n = models->frustum;
    if (models->owners != 0 || (n != NULL && n->active))
        return result(error, MC_CLIENT_FRUSTUM_BUSY, "frustum live Models owner occurrences");
    if (n == NULL) {
        models->frustum_retired = true;
        return result(error, MC_CLIENT_FRUSTUM_OK, NULL);
    }
    if (mc_client_render_engine_classes_release(n->graphics) != MC_CLIENT_RENDER_ENGINE_OK)
        return result(error, MC_CLIENT_FRUSTUM_BUSY, "frustum ge namespace occurrence retirement");
    mc_client_render_arena_free(&n->arena);
    free(n);
    models->frustum = NULL;
    models->frustum_retired = true;
    return result(error, MC_CLIENT_FRUSTUM_OK, NULL);
}
