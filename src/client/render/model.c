#include "mc/client/render/model.h"
#include <stdlib.h>
#include <string.h>

struct mc_client_render_classes {
    mc_entity_classes *entities;
    mc_java_random_profile *profile;
    size_t owners;
    bool cv_ready, cv_a;
};

typedef struct model_allocation {
    struct model_allocation *next;
    mc_entity_vector *vector;
    long double alignment;
} model_allocation;

struct mc_client_item_renderer {
    mc_client_render_classes *classes;
    mc_client_model_phase phase;
    mc_client_item_renderer_fields fields;
    model_allocation *allocations;
    size_t bytes, max_bytes;
    mc_client_model_error failure;
};

static mc_client_model_status fail(mc_client_item_renderer *o, mc_client_model_status status,
                                   const char *source, int32_t pc) {
    o->failure = (mc_client_model_error){.status = status, .source_class = source, .source_pc = pc};
    return status;
}

static void *allocate(mc_client_item_renderer *o, size_t bytes, const char *source, int32_t pc) {
    if (bytes > SIZE_MAX - sizeof(model_allocation) ||
        sizeof(model_allocation) + bytes > o->max_bytes - o->bytes) {
        (void)fail(o, MC_CLIENT_MODEL_LIMIT, source, pc);
        return NULL;
    }
    model_allocation *a = calloc(1, sizeof(*a) + bytes);
    if (a == NULL) {
        (void)fail(o, MC_CLIENT_MODEL_ALLOCATION_FAILED, source, pc);
        return NULL;
    }
    o->bytes += sizeof(*a) + bytes;
    a->next = o->allocations;
    o->allocations = a;
    return a + 1;
}

static mc_client_model_vertex *vertex(mc_client_item_renderer *o, float x, float y, float z,
                                      float u, float v, int32_t pc) {
    mc_client_model_vertex *p = allocate(o, sizeof(*p), "ps", pc);
    if (p == NULL)
        return NULL;
    model_allocation *record = o->allocations;
    mc_entity_vector_error error = {0};
    if (mc_entity_vector_create(o->classes->entities, (double)x, (double)y, (double)z,
                                &record->vector, &error) != MC_ENTITY_OK) {
        (void)fail(o, MC_CLIENT_MODEL_VECTOR_FAILED, "ib", 7);
        o->failure.vector = error;
        return NULL;
    }
    p->a = record->vector;
    p->b = u;
    p->c = v;
    return p;
}

static mc_client_model_vertex *uv(mc_client_item_renderer *o, const mc_client_model_vertex *p,
                                  float u, float v) {
    mc_client_model_vertex *copy = allocate(o, sizeof(*copy), "ib", 0);
    if (copy != NULL) {
        copy->a = p->a;
        copy->b = u;
        copy->c = v;
    }
    return copy;
}

static mc_client_model_quad *quad(mc_client_item_renderer *o, mc_client_model_vertex *const p[4],
                                  int32_t u0, int32_t v0, int32_t u1, int32_t v1, int32_t pc) {
    mc_client_model_quad *q = allocate(o, sizeof(*q), "ps", pc);
    if (q == NULL)
        return NULL;
    mc_client_model_vertex **array = allocate(o, 4 * sizeof(*array), "ps", pc + 5);
    if (array == NULL)
        return NULL;
    memcpy(array, p, 4 * sizeof(*array));
    q->a = array;
    q->b = 4;
    const float u[4] = {(float)u1 / 64.0f - 0.0015625f, (float)u0 / 64.0f + 0.0015625f,
                        (float)u0 / 64.0f + 0.0015625f, (float)u1 / 64.0f - 0.0015625f};
    const float v[4] = {(float)v0 / 32.0f + 0.003125f, (float)v0 / 32.0f + 0.003125f,
                        (float)v1 / 32.0f - 0.003125f, (float)v1 / 32.0f - 0.003125f};
    for (size_t i = 0; i < 4; ++i) {
        mc_client_model_vertex *copy = uv(o, array[i], u[i], v[i]);
        if (copy == NULL)
            return NULL;
        array[i] = copy;
    }
    return q;
}

static bool box(mc_client_item_renderer *o, mc_client_model_part *p, float x, float y, float z,
                int32_t w, int32_t h, int32_t d, float inflate) {
    p->j = allocate(o, 8 * sizeof(*p->j), "ps", 3);
    if (p->j == NULL)
        return false;
    p->k = allocate(o, 6 * sizeof(*p->k), "ps", 12);
    if (p->k == NULL)
        return false;
    float x1 = x + (float)w, y1 = y + (float)h, z1 = z + (float)d;
    x -= inflate;
    y -= inflate;
    z -= inflate;
    x1 += inflate;
    y1 += inflate;
    z1 += inflate;
    if (p->g) {
        const float t = x1;
        x1 = x;
        x = t;
    }
    const float values[8][5] = {{x, y, z, 0, 0},    {x1, y, z, 0, 8}, {x1, y1, z, 8, 8},
                                {x, y1, z, 8, 0},   {x, y, z1, 0, 0}, {x1, y, z1, 0, 8},
                                {x1, y1, z1, 8, 8}, {x, y1, z1, 8, 0}};
    static const int32_t vertex_pc[8] = {92, 106, 122, 140, 156, 171, 188, 207};
    mc_client_model_vertex *positions[8];
    for (size_t i = 0; i < 8; ++i) {
        positions[i] = vertex(o, values[i][0], values[i][1], values[i][2], values[i][3],
                              values[i][4], vertex_pc[i]);
        if (positions[i] == NULL)
            return false;
    }
    memcpy(p->j, positions, sizeof(positions));
    static const unsigned order[6][4] = {{5, 1, 2, 6}, {0, 4, 7, 3}, {5, 4, 0, 1},
                                         {2, 3, 7, 6}, {1, 0, 3, 2}, {4, 5, 6, 7}};
    const int32_t l = p->l, m = p->m;
    const int32_t texture[6][4] = {{l + d + w, m + d, l + d + w + d, m + d + h},
                                   {l + 0, m + d, l + d, m + d + h},
                                   {l + d, m + 0, l + d + w, m + d},
                                   {l + d + w, m + 0, l + d + w + w, m + d},
                                   {l + d, m + d, l + d + w, m + d + h},
                                   {l + d + w + d, m + d, l + d + w + d + w, m + d + h}};
    static const int32_t quad_pc[6] = {295, 372, 439, 506, 579, 650};
    for (size_t face = 0; face < 6; ++face) {
        mc_client_model_vertex *array[4];
        for (size_t i = 0; i < 4; ++i)
            array[i] = positions[order[face][i]];
        mc_client_model_quad *q = quad(o, array, texture[face][0], texture[face][1],
                                       texture[face][2], texture[face][3], quad_pc[face]);
        if (q == NULL)
            return false;
        p->k[face] = q;
    }
    if (p->g)
        for (size_t face = 0; face < 6; ++face) {
            mc_client_model_quad *q = p->k[face];
            mc_client_model_vertex **reversed = allocate(o, 4 * sizeof(*reversed), "tz", 5);
            if (reversed == NULL)
                return false;
            for (size_t i = 0; i < 4; ++i)
                reversed[i] = q->a[3 - i];
            q->a = reversed;
        }
    return true;
}

static mc_client_model_part *part(mc_client_item_renderer *o, int32_t u, int32_t v, int32_t pc) {
    mc_client_model_part *p = allocate(o, sizeof(*p), "fh", pc);
    if (p != NULL) {
        p->n = false;
        p->o = 0;
        p->g = false;
        p->h = true;
        p->i = false;
        p->l = u;
        p->m = v;
    }
    return p;
}

static void pivot(mc_client_model_part *p, float x, float y, float z) {
    p->a = x;
    p->b = y;
    p->c = z;
}

static mc_client_biped_model *biped(mc_client_item_renderer *o) {
    mc_client_biped_model *m = allocate(o, sizeof(*m), "bw", 5);
    if (m == NULL)
        return NULL;
    m->n = false;
    m->j = false;
    m->k = false;
    m->l = false;
    m->i = part(o, 0, 0, 20);
    if (m->i == NULL || !box(o, m->i, -5, 0, -1, 10, 16, 1, 0))
        return NULL;
    m->h = part(o, 24, 0, 51);
    if (m->h == NULL || !box(o, m->h, -3, -6, -1, 6, 6, 1, 0))
        return NULL;
    m->a = part(o, 0, 0, 84);
    if (m->a == NULL || !box(o, m->a, -4, -8, -4, 8, 8, 8, 0))
        return NULL;
    pivot(m->a, 0, 0.0f + 0.0f, 0);
    m->b = part(o, 32, 0, 129);
    if (m->b == NULL || !box(o, m->b, -4, -8, -4, 8, 8, 8, 0.0f + 0.5f))
        return NULL;
    pivot(m->b, 0, 0.0f + 0.0f, 0);
    m->c = part(o, 16, 16, 178);
    if (m->c == NULL || !box(o, m->c, -4, 0, -2, 8, 12, 4, 0))
        return NULL;
    pivot(m->c, 0, 0.0f + 0.0f, 0);
    m->d = part(o, 40, 16, 223);
    if (m->d == NULL || !box(o, m->d, -3, -2, -2, 4, 12, 4, 0))
        return NULL;
    pivot(m->d, -5, 2.0f + 0.0f, 0);
    m->e = part(o, 40, 16, 269);
    if (m->e == NULL)
        return NULL;
    m->e->g = true;
    if (!box(o, m->e, -1, -2, -2, 4, 12, 4, 0))
        return NULL;
    pivot(m->e, 5, 2.0f + 0.0f, 0);
    m->f = part(o, 0, 16, 323);
    if (m->f == NULL || !box(o, m->f, -2, 0, -2, 4, 12, 4, 0))
        return NULL;
    pivot(m->f, -2, 12.0f + 0.0f, 0);
    m->g = part(o, 0, 16, 368);
    if (m->g == NULL)
        return NULL;
    m->g->g = true;
    if (!box(o, m->g, -2, 0, -2, 4, 12, 4, 0))
        return NULL;
    pivot(m->g, 2, 12.0f + 0.0f, 0);
    return m;
}

static mc_client_render_blocks *blocks(mc_client_item_renderer *o, const char *source, int32_t pc) {
    if (!o->classes->cv_ready) {
        o->classes->cv_a = true;
        o->classes->cv_ready = true;
    }
    mc_client_render_blocks *p = allocate(o, sizeof(*p), source, pc);
    if (p != NULL) {
        p->d = -1;
        p->e = false;
        p->f = false;
        p->b = true;
        p->g = p->h = p->i = p->j = p->k = p->l = 0;
        p->O = 1;
    }
    return p;
}

mc_client_model_status mc_client_render_classes_create(mc_entity_classes *entities,
                                                       mc_java_random_profile *profile,
                                                       mc_client_render_classes **out) {
    if (entities == NULL || profile == NULL || out == NULL)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    mc_client_render_classes *p = calloc(1, sizeof(*p));
    if (p == NULL)
        return MC_CLIENT_MODEL_ALLOCATION_FAILED;
    if (mc_entity_classes_retain(entities, MC_ENTITY_CLIENT, profile) != MC_ENTITY_OK) {
        free(p);
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    }
    p->entities = entities;
    p->profile = profile;
    *out = p;
    return MC_CLIENT_MODEL_OK;
}

mc_client_model_status mc_client_render_classes_free(mc_client_render_classes *p) {
    if (p == NULL)
        return MC_CLIENT_MODEL_OK;
    if (p->owners != 0)
        return MC_CLIENT_MODEL_BUSY;
    if (mc_entity_classes_release(p->entities) != MC_ENTITY_OK)
        return MC_CLIENT_MODEL_BUSY;
    free(p);
    return MC_CLIENT_MODEL_OK;
}

mc_client_model_status mc_client_render_classes_retain(mc_client_render_classes *p,
                                                       mc_java_random_profile *profile) {
    if (p == NULL || p->profile != profile)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    if (p->owners == SIZE_MAX)
        return MC_CLIENT_MODEL_LIMIT;
    ++p->owners;
    return MC_CLIENT_MODEL_OK;
}

mc_client_model_status mc_client_render_classes_release(mc_client_render_classes *p) {
    if (p == NULL || p->owners == 0)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    --p->owners;
    return MC_CLIENT_MODEL_OK;
}

bool mc_client_render_blocks_static_get(const mc_client_render_classes *p, bool *ready, bool *a) {
    if (p == NULL || ready == NULL || a == NULL)
        return false;
    *ready = p->cv_ready;
    *a = p->cv_a;
    return true;
}

mc_client_model_status mc_client_item_renderer_allocate(mc_client_render_classes *classes,
                                                        size_t max_bytes,
                                                        mc_client_item_renderer **out) {
    if (classes == NULL || out == NULL)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    if (max_bytes == 0)
        max_bytes = 1024u * 1024u;
    if (max_bytes < sizeof(mc_client_item_renderer) || classes->owners == SIZE_MAX)
        return MC_CLIENT_MODEL_LIMIT;
    mc_client_item_renderer *p = calloc(1, sizeof(*p));
    if (p == NULL)
        return MC_CLIENT_MODEL_ALLOCATION_FAILED;
    p->classes = classes;
    p->max_bytes = max_bytes;
    p->bytes = sizeof(*p);
    ++classes->owners;
    *out = p;
    return MC_CLIENT_MODEL_OK;
}

mc_client_model_status mc_client_item_renderer_construct(mc_client_item_renderer *o,
                                                         mc_client_model_error *error) {
    if (o == NULL)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    if (o->phase != MC_CLIENT_MODEL_RAW)
        return MC_CLIENT_MODEL_INVALID_PHASE;
    o->phase = MC_CLIENT_MODEL_CONSTRUCTING;
    mc_client_biped_model *model = biped(o);
    if (model == NULL)
        goto failure;
    o->fields.base_model = model;
    o->fields.base_blocks = blocks(o, "bw", 16);
    if (o->fields.base_blocks == NULL)
        goto failure;
    o->fields.shadow_size = 0.0f;
    o->fields.shadow_opacity = 1.0f;
    o->fields.e = blocks(o, "bb", 5);
    if (o->fields.e == NULL)
        goto failure;
    mc_java_random *random = allocate(o, sizeof(*random), "bb", 16);
    if (random == NULL)
        goto failure;
    const mc_java_random_profile_status status =
        mc_java_random_construct_default(o->classes->profile, random);
    if (status != MC_JAVA_RANDOM_PROFILE_OK) {
        (void)fail(o, MC_CLIENT_MODEL_RANDOM_FAILED, "bb", 20);
        o->failure.random = status;
        goto failure;
    }
    o->fields.f = random;
    o->fields.a = true;
    o->fields.shadow_size = 0.15f;
    o->fields.shadow_opacity = 0.75f;
    o->phase = MC_CLIENT_MODEL_READY;
    if (error != NULL)
        *error = (mc_client_model_error){0};
    return MC_CLIENT_MODEL_OK;
failure:
    o->phase = MC_CLIENT_MODEL_FAILED;
    if (error != NULL)
        *error = o->failure;
    return o->failure.status;
}

mc_client_model_phase mc_client_item_renderer_phase(const mc_client_item_renderer *p) {
    return p == NULL ? MC_CLIENT_MODEL_FAILED : p->phase;
}

const mc_client_item_renderer_fields *
mc_client_item_renderer_fields_get(const mc_client_item_renderer *p) {
    return p == NULL ? NULL : &p->fields;
}

size_t mc_client_item_renderer_allocation_bytes(const mc_client_item_renderer *p) {
    return p == NULL ? 0 : p->bytes;
}

mc_client_model_status mc_client_item_renderer_free(mc_client_item_renderer **out) {
    if (out == NULL)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    mc_client_item_renderer *o = *out;
    if (o == NULL)
        return MC_CLIENT_MODEL_OK;
    if (o->phase == MC_CLIENT_MODEL_CONSTRUCTING)
        return MC_CLIENT_MODEL_BUSY;
    for (model_allocation *a = o->allocations; a != NULL; a = a->next)
        if (a->vector != NULL) {
            if (mc_entity_vector_release(o->classes->entities, a->vector) != MC_ENTITY_OK)
                return MC_CLIENT_MODEL_BUSY;
            a->vector = NULL;
        }
    model_allocation *a = o->allocations;
    while (a != NULL) {
        model_allocation *next = a->next;
        free(a);
        a = next;
    }
    --o->classes->owners;
    free(o);
    *out = NULL;
    return MC_CLIENT_MODEL_OK;
}

mc_client_model_status mc_client_item_renderer_draw(mc_client_item_renderer *p,
                                                    mc_client_model_error *error) {
    if (p == NULL)
        return MC_CLIENT_MODEL_INVALID_ARGUMENT;
    if (error != NULL)
        *error = (mc_client_model_error){.status = MC_CLIENT_MODEL_UNIMPLEMENTED_DEPENDENCY,
                                         .source_class = "bb",
                                         .source_pc = 0};
    return MC_CLIENT_MODEL_UNIMPLEMENTED_DEPENDENCY;
}
