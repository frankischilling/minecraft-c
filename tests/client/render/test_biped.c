#include "client/render/biped_internal.h"
#include "mc/block/registry/registry.h"
#include "mc/client/gui/achievement_notice.h"
#include "random_profile_internal.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xmmintrin.h>

static unsigned checks;
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(x)) {                                                                                \
            fprintf(stderr, "Biped check%u line%d: %s\n", checks, __LINE__, #x);                   \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

static uint32_t fb(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t db(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void parts(const mc_client_biped_model *m, const mc_client_model_part *p[9]) {
    const mc_client_model_part *all[9] = {m->a, m->b, m->c, m->d, m->e, m->f, m->g, m->h, m->i};
    memcpy(p, all, sizeof(all));
}

static bool aliases(const mc_client_biped_model *m) {
    const mc_client_model_part *p[9];
    parts(m, p);
    const mc_entity_vector *positions[72];
    const mc_client_model_vertex *vertices[288];
    size_t position_count = 0, vertex_count = 0;
    for (size_t part = 0; part < 9; ++part) {
        if (p[part] == NULL || p[part]->j == NULL || p[part]->k == NULL)
            return false;
        for (size_t i = 0; i < 8; ++i) {
            const mc_client_model_vertex *v = p[part]->j[i];
            if (v == NULL || v->a == NULL)
                return false;
            for (size_t j = 0; j < position_count; ++j)
                if (positions[j] == v->a)
                    return false;
            positions[position_count++] = v->a;
            vertices[vertex_count++] = v;
        }
        for (size_t face = 0; face < 6; ++face) {
            const mc_client_model_quad *q = p[part]->k[face];
            if (q == NULL || q->a == NULL || q->b != 4 || q->c)
                return false;
            for (size_t i = 0; i < 4; ++i) {
                const mc_client_model_vertex *v = q->a[i];
                if (v == NULL)
                    return false;
                for (size_t j = 0; j < vertex_count; ++j)
                    if (vertices[j] == v)
                        return false;
                bool found = false;
                for (size_t j = 0; j < 8; ++j)
                    found = found || v->a == p[part]->j[j]->a;
                if (!found)
                    return false;
                vertices[vertex_count++] = v;
            }
        }
    }
    return position_count == 72 && vertex_count == 288;
}

static bool distinct(const mc_client_biped_model *a, const mc_client_biped_model *b) {
    if (a == b)
        return false;
    const mc_client_model_part *p[9], *q[9];
    parts(a, p);
    parts(b, q);
    for (size_t i = 0; i < 9; ++i)
        for (size_t j = 0; j < 9; ++j) {
            if (p[i] == q[j] || p[i]->j == q[j]->j || p[i]->k == q[j]->k)
                return false;
            for (size_t x = 0; x < 8; ++x)
                for (size_t y = 0; y < 8; ++y)
                    if (p[i]->j[x] == q[j]->j[y] || p[i]->j[x]->a == q[j]->j[y]->a)
                        return false;
            for (size_t x = 0; x < 6; ++x)
                for (size_t y = 0; y < 6; ++y) {
                    if (p[i]->k[x] == q[j]->k[y] || p[i]->k[x]->a == q[j]->k[y]->a)
                        return false;
                    for (size_t u = 0; u < 4; ++u)
                        for (size_t v = 0; v < 4; ++v)
                            if (p[i]->k[x]->a[u] == q[j]->k[y]->a[v])
                                return false;
                }
        }
    return true;
}

static void geometry(const mc_client_biped_model *m) {
    printf("BIPED %08" PRIx32 " %d %d %d %d\n", fb(m->m), m->n, m->j, m->k, m->l);
    const mc_client_model_part *p[9];
    parts(m, p);
    for (size_t part = 0; part < 9; ++part) {
        const mc_client_model_part *s = p[part];
        printf("PART %zu %d %d %d %d %d %d %d %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
               " %08" PRIx32 " %08" PRIx32 "\n",
               part, s->l, s->m, s->g, s->h, s->i, s->n, s->o, fb(s->a), fb(s->b), fb(s->c),
               fb(s->d), fb(s->e), fb(s->f));
        for (size_t i = 0; i < 8; ++i) {
            const mc_client_model_vertex *v = s->j[i];
            printf("VERTEX %zu %zu %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %08" PRIx32
                   " %08" PRIx32 "\n",
                   part, i, db(v->a->x), db(v->a->y), db(v->a->z), fb(v->b), fb(v->c));
        }
        for (size_t face = 0; face < 6; ++face) {
            const mc_client_model_quad *q = s->k[face];
            printf("QUAD %zu %zu %d %d", part, face, q->b, q->c);
            for (size_t i = 0; i < 4; ++i) {
                const mc_client_model_vertex *v = q->a[i];
                int index = -1;
                bool copy = true;
                for (size_t j = 0; j < 8; ++j) {
                    if (v->a == s->j[j]->a)
                        index = (int)j;
                    if (v == s->j[j])
                        copy = false;
                }
                printf(" %d %d %08" PRIx32 " %08" PRIx32, index, copy, fb(v->b), fb(v->c));
            }
            putchar('\n');
        }
    }
}

static int contracts(mc_java_random_profile *profile, mc_entity_classes *entities,
                     mc_client_render_classes *classes) {
    CHECK(mc_java_random_profile_bind_constructor_inputs(profile, NULL, 0) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    mc_client_biped_owner *o = NULL;
    mc_client_biped_error error;
    CHECK(mc_client_biped_allocate(classes, 0, &o, &error) == MC_CLIENT_MODEL_OK);
    mc_client_biped_state state;
    CHECK(mc_client_biped_inspect(o, &state) && state.phase == MC_CLIENT_BIPED_RAW &&
          state.holds == 1);
    CHECK(state.allocations == 0 && state.fields->a == NULL && state.fields->i == NULL &&
          fb(state.fields->m) == 0);
    CHECK(mc_client_render_classes_free(classes) == MC_CLIENT_MODEL_BUSY);
    CHECK(mc_client_biped_retain(o) == MC_CLIENT_MODEL_OK);
    mc_client_biped_owner *second = o;
    CHECK(mc_client_biped_release(&second, &error) == MC_CLIENT_MODEL_OK && second == NULL);
    CHECK(mc_client_biped_inspect(o, &state) && state.holds == 1);
    o->active = true;
    second = o;
    CHECK(mc_client_biped_release(&second, &error) == MC_CLIENT_MODEL_BUSY && second == o);
    o->active = false;
    CHECK(mc_client_biped_construct_zero(o, &error) == MC_CLIENT_MODEL_OK);
    CHECK(mc_client_biped_inspect(o, &state) && state.parts_completed == 9 &&
          state.vertices_completed == 72 && state.faces_completed == 54 &&
          state.uv_copies_completed == 216 && state.allocations == 435);
    CHECK(aliases(state.fields));
    CHECK(mc_client_biped_construct_zero(o, &error) == MC_CLIENT_MODEL_INVALID_PHASE);
    CHECK(mc_client_biped_release(&o, &error) == MC_CLIENT_MODEL_OK && o == NULL);
    size_t limit = sizeof(mc_client_biped_owner), failures = 0;
    for (;;) {
        CHECK(mc_client_biped_allocate(classes, limit, &o, &error) == MC_CLIENT_MODEL_OK);
        const mc_client_model_status status = mc_client_biped_construct_zero(o, &error);
        CHECK(mc_client_biped_inspect(o, &state));
        if (status == MC_CLIENT_MODEL_OK) {
            CHECK(failures == 435 && state.allocations == 435);
            CHECK(mc_client_biped_release(&o, NULL) == MC_CLIENT_MODEL_OK);
            break;
        }
        CHECK(status == MC_CLIENT_MODEL_LIMIT && state.phase == MC_CLIENT_BIPED_FAILED &&
              state.allocations == failures);
        CHECK(error.model.status == MC_CLIENT_MODEL_LIMIT && error.frame_count >= 2 &&
              error.frame_count <= 5);
        CHECK(error.model.source_pc == error.frames[0].pc &&
              strcmp(error.model.source_class, error.frames[0].source_class) == 0);
        CHECK(state.next_allocation_bytes > 0 &&
              state.allocation_bytes <= SIZE_MAX - state.next_allocation_bytes);
        limit = state.allocation_bytes + state.next_allocation_bytes;
        CHECK(mc_client_biped_construct_zero(o, NULL) == MC_CLIENT_MODEL_INVALID_PHASE);
        CHECK(mc_client_biped_release(&o, NULL) == MC_CLIENT_MODEL_OK && o == NULL);
        mc_entity_vector_snapshot vectors;
        CHECK(mc_entity_vector_get_snapshot(entities, &vectors) == MC_ENTITY_OK &&
              vectors.holds == 0);
        CHECK(++failures <= 435);
    }
    /* Retrying native cleanup: an actual foreign-namespace Vec3 is rejected
     * before any graph position retires. Restore the real ledger occurrence. */
    CHECK(mc_client_biped_allocate(classes, 0, &o, NULL) == MC_CLIENT_MODEL_OK &&
          mc_client_biped_construct_zero(o, NULL) == MC_CLIENT_MODEL_OK);
    mc_entity_classes *foreign = NULL;
    mc_entity_vector *foreign_vector = NULL;
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &foreign) == MC_ENTITY_OK);
    CHECK(mc_entity_vector_create(foreign, 1, 2, 3, &foreign_vector, NULL) == MC_ENTITY_OK);
    model_allocation *record = o->allocations;
    while (record->vector == NULL)
        record = record->next;
    mc_entity_vector *saved = record->vector;
    record->vector = foreign_vector;
    CHECK(mc_client_biped_release(&o, &error) == MC_CLIENT_MODEL_BUSY && o != NULL &&
          error.retirement.vector.status == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_client_biped_inspect(o, &state) && state.phase == MC_CLIENT_BIPED_RETIRING &&
          state.fields == NULL);
    record->vector = saved;
    CHECK(mc_client_biped_release(&o, &error) == MC_CLIENT_MODEL_OK && o == NULL);
    CHECK(mc_entity_vector_release(foreign, foreign_vector) == MC_ENTITY_OK &&
          mc_entity_classes_free(foreign) == MC_ENTITY_OK);
    mc_java_random_constructor_inputs_view tape;
    CHECK(mc_java_random_profile_constructor_inputs_get(profile, &tape) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          tape.consumed == 0 && tape.failure == MC_JAVA_RANDOM_INPUT_NONE);
    printf("Biped contracts: %u checks;435 allocation prefixes\n", checks);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *entities = NULL;
    mc_client_render_classes *classes = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &entities) == MC_ENTITY_OK);
    CHECK(mc_client_render_classes_create(entities, profile, &classes) == MC_CLIENT_MODEL_OK);
    if (strcmp(argv[1], "contracts") == 0) {
        CHECK(contracts(profile, entities, classes) == 0);
    } else {
        const bool pair = strcmp(argv[1], "pair") == 0, pool = strcmp(argv[1], "pool") == 0,
                   notice_case = strcmp(argv[1], "notice") == 0;
        CHECK(pair || pool || notice_case || strcmp(argv[1], "cold") == 0);
        mc_entity_vector *pooled[2] = {0};
        mc_entity_vector_snapshot pool_before = {0};
        if (pool) {
            CHECK(mc_entity_vector_pool_get(entities, 1, 2, 3, &pooled[0], NULL) == MC_ENTITY_OK);
            CHECK(mc_entity_vector_pool_get(entities, 4, 5, 6, &pooled[1], NULL) == MC_ENTITY_OK);
            CHECK(mc_entity_vector_get_snapshot(entities, &pool_before) == MC_ENTITY_OK);
        }
        mc_client_achievement_notice *notice = NULL;
        mc_registry_classes *registry = NULL;
        mc_materials *materials = NULL;
        const mc_client_biped_model *notice_model = NULL;
        if (notice_case) {
            CHECK(mc_materials_create(&materials));
            const mc_registry_options opts = {
                .side = MC_REGISTRY_CLIENT, .random_profile = profile, .materials = materials};
            CHECK(mc_registry_classes_allocate(&opts, &registry, NULL) == MC_REGISTRY_OK);
            CHECK(mc_client_achievement_notice_allocate(classes, registry, profile, &notice,
                                                        NULL) == MC_CLIENT_NOTICE_OK);
            CHECK(mc_client_achievement_notice_construct(notice, NULL, NULL) ==
                  MC_CLIENT_NOTICE_OK);
            const mc_client_item_renderer_fields *item = mc_client_item_renderer_fields_get(
                mc_client_achievement_notice_fields_get(notice)->h);
            notice_model = item->base_model;
            CHECK(item->base_blocks != item->e && item->f != NULL);
            puts("CASE notice-base");
            geometry(notice_model);
            CHECK(aliases(notice_model));
        }
        const uint64_t before = atomic_load(&profile->uniquifier);
        mc_client_biped_owner *first = NULL, *second = NULL;
        mc_client_biped_state a, b;
        CHECK(mc_client_biped_allocate(classes, 0, &first, NULL) == MC_CLIENT_MODEL_OK &&
              mc_client_biped_construct_zero(first, NULL) == MC_CLIENT_MODEL_OK);
        CHECK(mc_client_biped_inspect(first, &a) && aliases(a.fields));
        printf("CASE %s\n", argv[1]);
        geometry(a.fields);
        puts("ALIASES 72 216 1");
        if (pair) {
            CHECK(mc_client_biped_allocate(classes, 0, &second, NULL) == MC_CLIENT_MODEL_OK &&
                  mc_client_biped_construct_zero(second, NULL) == MC_CLIENT_MODEL_OK);
            CHECK(mc_client_biped_inspect(second, &b) && aliases(b.fields) &&
                  distinct(a.fields, b.fields));
            puts("CASE pair-second");
            geometry(b.fields);
            puts("DISTINCT 1");
        }
        CHECK(atomic_load(&profile->uniquifier) == before);
        puts("RANDOM unchanged 1");
        if (pool) {
            mc_entity_vector_snapshot after;
            CHECK(mc_entity_vector_get_snapshot(entities, &after) == MC_ENTITY_OK &&
                  after.pool_identity == pool_before.pool_identity &&
                  after.size == pool_before.size && after.cursor == pool_before.cursor);
            CHECK(pooled[0]->x == 1 && pooled[0]->y == 2 && pooled[0]->z == 3 &&
                  pooled[1]->x == 4 && pooled[1]->y == 5 && pooled[1]->z == 6);
            puts("POOL 2 2 unchanged 1");
        }
        if (notice_case) {
            CHECK(distinct(a.fields, notice_model));
            puts("NOTICE distinct 1");
            CHECK(mc_client_achievement_notice_free(&notice, NULL) == MC_CLIENT_NOTICE_OK);
            CHECK(aliases(a.fields));
            mc_entity_vector_snapshot vectors;
            CHECK(mc_entity_vector_get_snapshot(entities, &vectors) == MC_ENTITY_OK &&
                  vectors.holds == 72);
            puts("LIFETIME notice-first 1");
            CHECK(mc_registry_classes_free(registry, NULL) == MC_REGISTRY_OK);
            mc_materials_destroy(materials);
        }
        CHECK(mc_client_biped_release(&first, NULL) == MC_CLIENT_MODEL_OK &&
              mc_client_biped_release(&second, NULL) == MC_CLIENT_MODEL_OK);
        for (size_t i = 0; i < 2; ++i)
            if (pooled[i] != NULL)
                CHECK(mc_entity_vector_release(entities, pooled[i]) == MC_ENTITY_OK);
        puts("END clean");
    }
    CHECK(mc_client_render_classes_free(classes) == MC_CLIENT_MODEL_OK &&
          mc_entity_classes_free(entities) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
    return 0;
}
