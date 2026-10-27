#include "model_graph_internal.h"
#include <stdlib.h>
#include <string.h>

#include "model_graph_trace.inc"

static mc_client_model_status fail(mc_client_model_graph *o, mc_client_model_status status,
                                   const char *source, int32_t pc) {
    (*o->failure) =
        (mc_client_model_error){.status = status, .source_class = source, .source_pc = pc};
    trace_failure(o, source, pc);
    return status;
}

void *mc_client_model_graph_allocate(mc_client_model_graph *o, size_t bytes, const char *source,
                                     int32_t pc) {
    if (o->progress != NULL)
        o->progress->next_allocation_bytes = bytes > SIZE_MAX - sizeof(model_allocation)
                                                 ? SIZE_MAX
                                                 : sizeof(model_allocation) + bytes;
    if (bytes > SIZE_MAX - sizeof(model_allocation) ||
        sizeof(model_allocation) + bytes > o->max_bytes - (*o->bytes)) {
        (void)fail(o, MC_CLIENT_MODEL_LIMIT, source, pc);
        return NULL;
    }
    model_allocation *a = calloc(1, sizeof(*a) + bytes);
    if (a == NULL) {
        (void)fail(o, MC_CLIENT_MODEL_ALLOCATION_FAILED, source, pc);
        return NULL;
    }
    (*o->bytes) += sizeof(*a) + bytes;
    a->next = (*o->allocations);
    (*o->allocations) = a;
    return a + 1;
}

static mc_client_model_vertex *vertex(mc_client_model_graph *o, float x, float y, float z, float u,
                                      float v, int32_t pc) {
    mc_client_model_vertex *p = mc_client_model_graph_allocate(o, sizeof(*p), "ps", pc);
    if (p == NULL)
        return NULL;
    model_allocation *record = (*o->allocations);
    mc_entity_vector_error error = {0};
    if (mc_entity_vector_create(o->classes->entities, (double)x, (double)y, (double)z,
                                &record->vector, &error) != MC_ENTITY_OK) {
        (void)fail(o, MC_CLIENT_MODEL_VECTOR_FAILED, "ib", 7);
        (*o->failure).vector = error;
        return NULL;
    }
    p->a = record->vector;
    p->b = u;
    p->c = v;
    return p;
}

static mc_client_model_vertex *uv(mc_client_model_graph *o, const mc_client_model_vertex *p,
                                  float u, float v) {
    mc_client_model_vertex *copy = mc_client_model_graph_allocate(o, sizeof(*copy), "ib", 0);
    if (copy != NULL) {
        copy->a = p->a;
        copy->b = u;
        copy->c = v;
    }
    return copy;
}

static mc_client_model_quad *quad(mc_client_model_graph *o, mc_client_model_vertex *const p[4],
                                  int32_t u0, int32_t v0, int32_t u1, int32_t v1, int32_t pc) {
    mc_client_model_quad *q = mc_client_model_graph_allocate(o, sizeof(*q), "ps", pc);
    if (q == NULL)
        return NULL;
    mc_client_model_vertex **array =
        mc_client_model_graph_allocate(o, 4 * sizeof(*array), "ps", pc + 5);
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
        if (o->detail != NULL)
            o->detail->uv_copy = (int32_t)i;
        mc_client_model_vertex *copy = uv(o, array[i], u[i], v[i]);
        if (copy == NULL)
            return NULL;
        array[i] = copy;
        if (o->progress != NULL)
            ++o->progress->uv_copies_completed;
    }
    return q;
}

static bool box(mc_client_model_graph *o, mc_client_model_part *p, float x, float y, float z,
                int32_t w, int32_t h, int32_t d, float inflate) {
    p->j = mc_client_model_graph_allocate(o, 8 * sizeof(*p->j), "ps", 3);
    if (p->j == NULL)
        return false;
    p->k = mc_client_model_graph_allocate(o, 6 * sizeof(*p->k), "ps", 12);
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
        if (o->detail != NULL)
            o->detail->vertex = (int32_t)i;
        positions[i] = vertex(o, values[i][0], values[i][1], values[i][2], values[i][3],
                              values[i][4], vertex_pc[i]);
        if (positions[i] == NULL)
            return false;
        if (o->progress != NULL)
            ++o->progress->vertices_completed;
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
        if (o->detail != NULL) {
            o->detail->face = (int32_t)face;
            o->detail->vertex = -1;
            o->detail->uv_copy = -1;
        }
        mc_client_model_vertex *array[4];
        for (size_t i = 0; i < 4; ++i)
            array[i] = positions[order[face][i]];
        mc_client_model_quad *q = quad(o, array, texture[face][0], texture[face][1],
                                       texture[face][2], texture[face][3], quad_pc[face]);
        if (q == NULL)
            return false;
        p->k[face] = q;
        if (o->progress != NULL)
            ++o->progress->faces_completed;
    }
    if (p->g)
        for (size_t face = 0; face < 6; ++face) {
            if (o->detail != NULL) {
                o->detail->face = (int32_t)face;
                o->detail->uv_copy = -1;
            }
            mc_client_model_quad *q = p->k[face];
            mc_client_model_vertex **reversed =
                mc_client_model_graph_allocate(o, 4 * sizeof(*reversed), "tz", 5);
            if (reversed == NULL)
                return false;
            for (size_t i = 0; i < 4; ++i)
                reversed[i] = q->a[3 - i];
            q->a = reversed;
        }
    if (o->progress != NULL)
        ++o->progress->parts_completed;
    return true;
}

static mc_client_model_part *part(mc_client_model_graph *o, int32_t u, int32_t v, int32_t pc) {
    mc_client_model_part *p = mc_client_model_graph_allocate(o, sizeof(*p), "fh", pc);
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

bool mc_client_model_graph_fill_zero(mc_client_model_graph *o, mc_client_biped_model *m) {
    m->n = false;
    root_store(o, "ko", 6);
    m->j = false;
    root_store(o, "fh", 6);
    m->k = false;
    root_store(o, "fh", 11);
    m->l = false;
    root_store(o, "fh", 16);
    current_part(o, 'i');
    m->i = part(o, 0, 0, 20);
    if (m->i != NULL)
        root_store(o, "fh", 29);
    if (m->i == NULL || !box(o, m->i, -5, 0, -1, 10, 16, 1, 0))
        return false;
    current_part(o, 'h');
    m->h = part(o, 24, 0, 51);
    if (m->h != NULL)
        root_store(o, "fh", 61);
    if (m->h == NULL || !box(o, m->h, -3, -6, -1, 6, 6, 1, 0))
        return false;
    current_part(o, 'a');
    m->a = part(o, 0, 0, 84);
    if (m->a != NULL)
        root_store(o, "fh", 93);
    if (m->a == NULL || !box(o, m->a, -4, -8, -4, 8, 8, 8, 0))
        return false;
    pivot(m->a, 0, 0.0f + 0.0f, 0);
    current_part(o, 'b');
    m->b = part(o, 32, 0, 129);
    if (m->b != NULL)
        root_store(o, "fh", 139);
    if (m->b == NULL || !box(o, m->b, -4, -8, -4, 8, 8, 8, 0.0f + 0.5f))
        return false;
    pivot(m->b, 0, 0.0f + 0.0f, 0);
    current_part(o, 'c');
    m->c = part(o, 16, 16, 178);
    if (m->c != NULL)
        root_store(o, "fh", 189);
    if (m->c == NULL || !box(o, m->c, -4, 0, -2, 8, 12, 4, 0))
        return false;
    pivot(m->c, 0, 0.0f + 0.0f, 0);
    current_part(o, 'd');
    m->d = part(o, 40, 16, 223);
    if (m->d != NULL)
        root_store(o, "fh", 234);
    if (m->d == NULL || !box(o, m->d, -3, -2, -2, 4, 12, 4, 0))
        return false;
    pivot(m->d, -5, 2.0f + 0.0f, 0);
    current_part(o, 'e');
    m->e = part(o, 40, 16, 269);
    if (m->e != NULL)
        root_store(o, "fh", 280);
    if (m->e == NULL)
        return false;
    m->e->g = true;
    if (!box(o, m->e, -1, -2, -2, 4, 12, 4, 0))
        return false;
    pivot(m->e, 5, 2.0f + 0.0f, 0);
    current_part(o, 'f');
    m->f = part(o, 0, 16, 323);
    if (m->f != NULL)
        root_store(o, "fh", 333);
    if (m->f == NULL || !box(o, m->f, -2, 0, -2, 4, 12, 4, 0))
        return false;
    pivot(m->f, -2, 12.0f + 0.0f, 0);
    current_part(o, 'g');
    m->g = part(o, 0, 16, 368);
    if (m->g != NULL)
        root_store(o, "fh", 378);
    if (m->g == NULL)
        return false;
    m->g->g = true;
    if (!box(o, m->g, -2, 0, -2, 4, 12, 4, 0))
        return false;
    pivot(m->g, 2, 12.0f + 0.0f, 0);
    return true;
}

mc_client_model_status mc_client_model_graph_retire(mc_client_model_graph *o,
                                                    mc_client_model_error *error) {
    for (model_allocation *a = *o->allocations; a != NULL; a = a->next)
        if (a->vector != NULL) {
            const mc_entity_status status =
                mc_entity_vector_release(o->classes->entities, a->vector);
            if (status != MC_ENTITY_OK) {
                if (error != NULL)
                    *error = (mc_client_model_error){
                        .status = MC_CLIENT_MODEL_BUSY,
                        .vector = {.status = status,
                                   .operation = "native model vector retirement"}};
                return MC_CLIENT_MODEL_BUSY;
            }
            a->vector = NULL;
        }
    model_allocation *a = *o->allocations;
    while (a != NULL) {
        model_allocation *next = a->next;
        free(a);
        a = next;
    }
    *o->allocations = NULL;
    if (error != NULL)
        *error = (mc_client_model_error){0};
    return MC_CLIENT_MODEL_OK;
}
