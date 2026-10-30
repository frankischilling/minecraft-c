/* Independent real TH owner fixture. No JVM, device or supplied constructor result. */
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
int mc_manager_frame_core_fixture_main(int argc, char **argv);
#define main mc_manager_frame_core_fixture_main
#include "test_manager.c"
#undef main

#include "entity/entity_internal.h"
#include "mc/entity/player/player.h"
#include <inttypes.h>
#include <xmmintrin.h>

typedef struct frame_input {
    unsigned int id;
    char name[64];
    uint32_t partial, angles[4];
    uint64_t positions[6];
} frame_input;

typedef struct saved_object {
    const void *object;
    size_t size;
    unsigned char *bytes;
} saved_object;

typedef struct graph_snapshot {
    saved_object *objects;
    size_t count, bytes;
} graph_snapshot;

/* These are native observation bounds, never Java reflective allocation counts. */
#define FRAME_OBJECT_LIMIT 32768u
#define FRAME_BYTE_LIMIT (16u * 1024u * 1024u)

static int save_object(graph_snapshot *s, const void *object, size_t bytes) {
    if (object == NULL)
        return 0;
    for (size_t i = 0; i < s->count; ++i)
        if (s->objects[i].object == object)
            return s->objects[i].size == bytes ? 0 : -1;
    if (bytes == 0 || s->count == FRAME_OBJECT_LIMIT || bytes > FRAME_BYTE_LIMIT - s->bytes)
        return -1;
    unsigned char *copy = malloc(bytes);
    if (copy == NULL)
        return -1;
    memcpy(copy, object, bytes);
    s->objects[s->count++] = (saved_object){object, bytes, copy};
    s->bytes += bytes;
    return 1;
}

static int save_vertex(graph_snapshot *s, const mc_client_model_vertex *v) {
    const int added = save_object(s, v, sizeof(*v));
    CHECK(added >= 0);
    if (added != 0)
        CHECK(v->a != NULL && save_object(s, v->a, sizeof(*v->a)) >= 0);
    return 0;
}

static int save_part(graph_snapshot *s, const mc_client_model_part *p) {
    const int added = save_object(s, p, sizeof(*p));
    CHECK(added >= 0);
    if (added == 0)
        return 0;
    /* Actual supported model_graph_box allocates eight vertices and six quads. */
    CHECK((p->j == NULL) == (p->k == NULL));
    if (p->j == NULL)
        return 0;
    CHECK(save_object(s, p->j, 8 * sizeof(*p->j)) >= 0);
    CHECK(save_object(s, p->k, 6 * sizeof(*p->k)) >= 0);
    for (size_t i = 0; i < 8; ++i)
        CHECK(p->j[i] != NULL && save_vertex(s, p->j[i]) == 0);
    for (size_t i = 0; i < 6; ++i) {
        const mc_client_model_quad *q = p->k[i];
        CHECK(q != NULL && q->b == 4 && q->a != NULL);
        CHECK(save_object(s, q, sizeof(*q)) >= 0);
        CHECK(save_object(s, q->a, 4 * sizeof(*q->a)) >= 0);
        for (size_t j = 0; j < 4; ++j)
            CHECK(q->a[j] != NULL && save_vertex(s, q->a[j]) == 0);
    }
    return 0;
}

static int save_biped(graph_snapshot *s, const mc_client_biped_model *m) {
    if (m == NULL)
        return 0;
    CHECK(save_object(s, m, sizeof(*m)) >= 0);
    const mc_client_model_part *parts[] = {m->a, m->b, m->c, m->d, m->e, m->f, m->g, m->h, m->i};
    for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); ++i)
        CHECK(save_part(s, parts[i]) == 0);
    return 0;
}

static int save_model(graph_snapshot *s, const manager_model *m) {
    const int added = save_object(s, m, sizeof(*m));
    CHECK(added >= 0);
    if (added == 0)
        return 0;
    CHECK(save_biped(s, &m->fields) == 0 && save_part(s, m->extra_j) == 0 &&
          save_part(s, m->extra_k) == 0);
    const manager_part_array *arrays[] = {m->array_a, m->array_b};
    for (size_t i = 0; i < 2; ++i) {
        const manager_part_array *a = arrays[i];
        if (a == NULL)
            continue;
        CHECK(a->length >= 0 && a->length <= 1024);
        CHECK(save_object(s, a, sizeof(*a) + (size_t)a->length * sizeof(*a->elements)) >= 0);
        for (int32_t j = 0; j < a->length; ++j)
            CHECK(save_part(s, a->elements[j]) == 0);
    }
    return 0;
}

static int save_graph(graph_snapshot *s, mc_client_render_manager *m) {
    s->objects = calloc(FRAME_OBJECT_LIMIT, sizeof(*s->objects));
    CHECK(s->objects != NULL && m->map != NULL && m->map == m->fields.o);
    CHECK(save_object(s, m->map, sizeof(*m->map)) >= 0 && m->map->capacity <= 1024);
    CHECK(save_object(s, m->map->buckets, m->map->capacity * sizeof(*m->map->buckets)) >= 0);
    for (size_t i = 0; i < m->map->capacity; ++i)
        for (const manager_map_entry *e = m->map->buckets[i]; e != NULL; e = e->next)
            CHECK(save_object(s, e, sizeof(*e)) >= 0);
    for (size_t i = 0; i < m->allocated_renderers; ++i) {
        const mc_client_managed_renderer *r = m->renderers[i];
        mc_client_managed_renderer_state state;
        CHECK(mc_client_managed_renderer_inspect(r, &state));
        CHECK(save_object(s, r, sizeof(*r)) >= 0);
        CHECK(save_biped(s, state.bw_model) == 0);
        CHECK(save_object(s, state.bw_blocks, sizeof(*state.bw_blocks)) >= 0);
        CHECK(save_object(s, state.own_blocks, sizeof(*state.own_blocks)) >= 0);
        if (r->bb != NULL) {
            const mc_client_item_renderer_fields *f = mc_client_item_renderer_fields_get(r->bb);
            CHECK(f != NULL && save_object(s, f, sizeof(*f)) >= 0);
        }
        for (const manager_model *model = r->models; model != NULL; model = model->next)
            CHECK(save_model(s, model) == 0);
    }
    return 0;
}

static bool graph_same(const graph_snapshot *s) {
    for (size_t i = 0; i < s->count; ++i)
        if (memcmp(s->objects[i].object, s->objects[i].bytes, s->objects[i].size) != 0)
            return false;
    return true;
}

static void graph_free(graph_snapshot *s) {
    for (size_t i = 0; i < s->count; ++i)
        free(s->objects[i].bytes);
    free(s->objects);
    memset(s, 0, sizeof(*s));
}

typedef struct identity_book {
    const void *objects[512];
    size_t count;
} identity_book;

static size_t object_id(identity_book *book, const void *p) {
    if (p == NULL)
        return 0;
    for (size_t i = 0; i < book->count; ++i)
        if (book->objects[i] == p)
            return i + 1;
    if (book->count == sizeof(book->objects) / sizeof(book->objects[0]))
        return SIZE_MAX;
    book->objects[book->count++] = p;
    return book->count;
}

typedef struct reference_row {
    const char *field, *type;
    const void *object;
} reference_row;

typedef struct roster_row {
    const char *key;
    mc_client_managed_renderer *renderer;
} roster_row;

static int reference_order(const void *a, const void *b) {
    return strcmp(((const reference_row *)a)->field, ((const reference_row *)b)->field);
}

static int roster_order(const void *a, const void *b) {
    return strcmp(((const roster_row *)a)->key, ((const roster_row *)b)->key);
}

static const char *model_type(const manager_model *m) {
    return m == NULL ? "null" : m->source_class;
}

static int emit_roster(mc_client_render_manager *m) {
    roster_row rows[28];
    size_t count = 0, reference_count = 0;
    identity_book book = {0};
    for (size_t i = 0; i < m->map->capacity; ++i)
        for (manager_map_entry *e = m->map->buckets[i]; e != NULL; e = e->next) {
            mc_entity_class_literal_view key;
            CHECK(count < 28 && mc_entity_class_literal_inspect(e->key, &key) == MC_ENTITY_OK);
            CHECK(key.owner == m->classes->models->entities && e->owner != NULL);
            rows[count++] = (roster_row){key.client_name, e->owner};
        }
    CHECK(count == 28);
    qsort(rows, count, sizeof(*rows), roster_order);
    for (size_t i = 0; i < count; ++i) {
        const mc_client_managed_renderer *r = rows[i].renderer;
        mc_client_managed_renderer_state s;
        CHECK(mc_client_managed_renderer_inspect(r, &s));
        CHECK(s.bw_model != NULL && s.bw_blocks != NULL && s.manager == m);
        const size_t model = object_id(&book, s.bw_model), cv = object_id(&book, s.bw_blocks);
        CHECK(model != SIZE_MAX && cv != SIZE_MAX);
        printf("RM_ROSTER %s %s %d fh cv %zu %zu\n", rows[i].key, s.source_class, s.manager == m,
               model, cv);
        reference_row refs[16];
        size_t n = 0;
        refs[n++] = (reference_row){"bw.a", "fh", s.bw_model};
        refs[n++] = (reference_row){"bw.b", "th", s.manager};
        refs[n++] = (reference_row){"bw.e", "cv", s.bw_blocks};
        if (r->primary != NULL) {
            refs[n++] = (reference_row){"gv.e", model_type(r->primary), r->primary};
            refs[n++] = (reference_row){"gv.f", model_type(r->secondary), r->secondary};
        }
        char own[16];
        if (r->bb != NULL) {
            refs[n++] = (reference_row){"bb.e", "cv", s.own_blocks};
            refs[n++] = (reference_row){"bb.f", "java.util.Random", s.random};
        } else if (r->own_model != NULL) {
            CHECK(snprintf(own, sizeof(own), "%s.a", s.source_class) > 0);
            refs[n++] = (reference_row){own, model_type(r->own_model), r->own_model};
        } else if (r->own_blocks != NULL) {
            CHECK(snprintf(own, sizeof(own), "%s.a", s.source_class) > 0);
            refs[n++] = (reference_row){own, "cv", r->own_blocks};
        } else if (r->random != NULL) {
            CHECK(snprintf(own, sizeof(own), "%s.a", s.source_class) > 0);
            refs[n++] = (reference_row){own, "java.util.Random", r->random};
        }
        if (r->g != NULL)
            refs[n++] = (reference_row){"ds.g", model_type(r->g), r->g};
        if (r->h != NULL)
            refs[n++] = (reference_row){"ds.h", model_type(r->h), r->h};
        CHECK(n <= sizeof(refs) / sizeof(refs[0]));
        qsort(refs, n, sizeof(*refs), reference_order);
        for (size_t j = 0; j < n; ++j) {
            const size_t id = object_id(&book, refs[j].object);
            CHECK(id != SIZE_MAX);
            printf("RM_REF %s %s %s %zu\n", rows[i].key, refs[j].field, refs[j].type, id);
        }
        reference_count += n;
    }
    CHECK(reference_count == 128);
    return 0;
}

static int read_inputs(const char *path, frame_input inputs[24]) {
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    for (unsigned int i = 0; i < 24; ++i) {
        frame_input *p = &inputs[i];
        const int fields = fscanf(
            file,
            "%u %63s %8" SCNx32 " %8" SCNx32 " %8" SCNx32 " %8" SCNx32 " %8" SCNx32 " %16" SCNx64
            " %16" SCNx64 " %16" SCNx64 " %16" SCNx64 " %16" SCNx64 " %16" SCNx64,
            &p->id, p->name, &p->partial, &p->angles[0], &p->angles[1], &p->angles[2],
            &p->angles[3], &p->positions[0], &p->positions[1], &p->positions[2], &p->positions[3],
            &p->positions[4], &p->positions[5]);
        CHECK(fields == 13 && p->id == i);
    }
    char extra;
    CHECK(fscanf(file, " %c", &extra) == EOF && !ferror(file));
    CHECK(fclose(file) == 0);
    return 0;
}

static void pose_words(const mc_entity_base_fields *p, uint32_t a[4], uint64_t v[6]) {
    a[0] = bits32(p->aU);
    a[1] = bits32(p->yaw);
    a[2] = bits32(p->aV);
    a[3] = bits32(p->aT);
    v[0] = bits64(p->bl);
    v[1] = bits64(p->bm);
    v[2] = bits64(p->bn);
    v[3] = bits64(p->x);
    v[4] = bits64(p->y);
    v[5] = bits64(p->z);
}

static void seed_scalars(mc_client_render_manager *m) {
    const uint32_t a[] = {UINT32_C(0x3f012345), UINT32_C(0xbf054321)};
    const uint64_t p[] = {UINT64_C(0x3ff123456789abcd), UINT64_C(0x3ff123456789abce),
                          UINT64_C(0x3ff123456789abcf)};
    memcpy(&m->fields.i, &a[0], sizeof(a[0]));
    memcpy(&m->fields.j, &a[1], sizeof(a[1]));
    memcpy(&m->fields.l, &p[0], sizeof(p[0]));
    memcpy(&m->fields.m, &p[1], sizeof(p[1]));
    memcpy(&m->fields.n, &p[2], sizeof(p[2]));
}

static void seed_pose(mc_entity *actor, const frame_input *input) {
    mc_entity_base_fields *p = mc_entity_runtime_fields(actor);
    float *a[] = {&p->aU, &p->yaw, &p->aV, &p->aT};
    double *v[] = {&p->bl, &p->bm, &p->bn, &p->x, &p->y, &p->z};
    for (size_t i = 0; i < 4; ++i)
        memcpy(a[i], &input->angles[i], sizeof(input->angles[i]));
    for (size_t i = 0; i < 6; ++i)
        memcpy(v[i], &input->positions[i], sizeof(input->positions[i]));
}

static void emit_input(const frame_input *input, const mc_entity *actor) {
    uint32_t a[4];
    uint64_t p[6];
    pose_words(&actor->base, a, p);
    printf("RM_INPUT %u %s %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
           " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
           "\n",
           input->id, input->name, input->partial, a[0], a[1], a[2], a[3], p[0], p[1], p[2], p[3],
           p[4], p[5]);
}

typedef struct frame_side {
    uint64_t uniquifier, origins[3];
    int32_t next_id;
    uint32_t angles[4];
    uint64_t positions[6];
    mc_java_random randoms[28];
    const mc_java_random *random_ids[28];
    size_t random_count;
    const void *map, *ra;
} frame_side;

static int get_side(const fixture *f, const mc_client_render_manager *m, const mc_entity *actor,
                    frame_side *s) {
    memset(s, 0, sizeof(*s));
    s->uniquifier = atomic_load_explicit(&f->profile->uniquifier, memory_order_relaxed);
    s->next_id = actor->classes->next_id;
    pose_words(&actor->base, s->angles, s->positions);
    s->origins[0] = bits64(f->classes->b);
    s->origins[1] = bits64(f->classes->c);
    s->origins[2] = bits64(f->classes->d);
    s->map = m->fields.o;
    s->ra = m->fields.f;
    for (size_t i = 0; i < m->allocated_renderers; ++i) {
        mc_client_managed_renderer_state state;
        CHECK(mc_client_managed_renderer_inspect(m->renderers[i], &state));
        if (state.random != NULL) {
            CHECK(s->random_count < 28);
            s->random_ids[s->random_count] = state.random;
            s->randoms[s->random_count++] = *state.random;
        }
    }
    return 0;
}

static int emit_side(const char *tag, unsigned int id, const fixture *f,
                     const mc_client_render_manager *m, const mc_entity *actor,
                     const frame_side *before, const graph_snapshot *graph) {
    frame_side after;
    CHECK(get_side(f, m, actor, &after) == 0 && before->random_count == after.random_count);
    bool random_same = true;
    for (size_t i = 0; i < before->random_count; ++i)
        random_same =
            random_same && before->random_ids[i] == after.random_ids[i] &&
            before->randoms[i].state == after.randoms[i].state &&
            bits64(before->randoms[i].next_next_gaussian) ==
                bits64(after.randoms[i].next_next_gaussian) &&
            before->randoms[i].have_next_next_gaussian == after.randoms[i].have_next_next_gaussian;
    const bool shared = before->uniquifier == after.uniquifier;
    const bool ids = before->next_id == after.next_id;
    const bool pose = memcmp(before->angles, after.angles, sizeof(before->angles)) == 0 &&
                      memcmp(before->positions, after.positions, sizeof(before->positions)) == 0;
    const bool aliases = m == f->classes->a && before->map == after.map && before->ra == after.ra &&
                         m->map->size == 28 && graph_same(graph);
    const bool origins = memcmp(before->origins, after.origins, sizeof(before->origins)) == 0;
    CHECK(shared && random_same && ids && pose && aliases && origins);
    /* Absent RAW children cannot qualify the source's instantiated children. */
    CHECK(actor->random == NULL && actor->watcher == NULL && f->profile->math == NULL);
    if (strcmp(tag, "RM_SIDE") == 0)
        printf("%s %u", tag, id);
    else
        printf("%s", tag);
    printf(" %d absent absent %d %d %d absent %d %d\n", shared, random_same, ids, pose, aliases,
           origins);
    return 0;
}

static int awake(fixture *f, mc_client_render_manager *m, mc_entity *actor,
                 const frame_input *input, const graph_snapshot *graph) {
    seed_pose(actor, input);
    seed_scalars(m);
    frame_side before;
    CHECK(get_side(f, m, actor, &before) == 0);
    bool sleeping = true;
    mc_player_error player;
    CHECK(mc_player_is_sleeping(actor, &sleeping, &player) == MC_PLAYER_OK && !sleeping);
    emit_input(input, actor);
    printf("RM_N %u gs %d\n", input->id, sleeping);
    float partial;
    memcpy(&partial, &input->partial, sizeof(partial));
    CHECK(mc_client_render_manager_bind_frame(m, NULL, NULL, NULL, NULL, actor, NULL, partial,
                                              f->error) == MC_CLIENT_MANAGER_OK);
    mc_client_manager_state s;
    CHECK(mc_client_render_manager_inspect(f->classes, &s) && s.last_frame_pc == 257);
    printf("RM_FRAME %u %d %d %d %d %d %08" PRIx32 " %08" PRIx32 " %016" PRIx64 " %016" PRIx64
           " %016" PRIx64 "\n",
           input->id, s.fields.g == NULL, s.fields.e == NULL, s.fields.k == NULL,
           s.fields.h == actor, s.fields.p == NULL, bits32(s.fields.i), bits32(s.fields.j),
           bits64(s.fields.l), bits64(s.fields.m), bits64(s.fields.n));
    CHECK(emit_side("RM_SIDE", input->id, f, m, actor, &before, graph) == 0);
    return 0;
}

static void scalar_row(const char *tag, const mc_client_manager_fields *p) {
    printf("%s %08" PRIx32 " %08" PRIx32 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n", tag,
           bits32(p->i), bits32(p->j), bits64(p->l), bits64(p->m), bits64(p->n));
}

static int null_living(fixture *f, mc_client_render_manager *m, mc_entity *actor,
                       const graph_snapshot *graph) {
    seed_scalars(m);
    const mc_client_manager_fields before = m->fields;
    frame_side side;
    CHECK(get_side(f, m, actor, &side) == 0);
    scalar_row("RM_NULL_BEFORE", &before);
    CHECK(mc_client_render_manager_bind_frame(m, NULL, NULL, NULL, NULL, NULL, NULL, 0.5f,
                                              f->error) == MC_CLIENT_MANAGER_NULL_REFERENCE);
    CHECK(!f->error->native_policy && f->error->pc == 29 && f->error->frame_pc == 29 &&
          f->error->th_pc == 29 && strcmp(f->error->source_class, "th") == 0 &&
          strcmp(f->error->source_method, "a") == 0 &&
          strcmp(f->error->source_descriptor, "(Lfd;Lji;Lsj;Lls;Lkv;F)V") == 0);
    printf("RM_NULL_THROW java.lang.NullPointerException %s %s pc%d\n", f->error->source_class,
           f->error->source_method, f->error->pc);
    CHECK(m->last_frame_pc == 24 && m->fields.g == NULL && m->fields.e == NULL &&
          m->fields.k == NULL && m->fields.h == NULL && m->fields.p == NULL &&
          m->living_occurrence == NULL);
    printf("RM_NULL_AFTER %d %d %d %d %d %08" PRIx32 " %08" PRIx32 " %016" PRIx64 " %016" PRIx64
           " %016" PRIx64 "\n",
           m->fields.g == NULL, m->fields.e == NULL, m->fields.k == NULL, m->fields.h == NULL,
           m->fields.p == NULL, bits32(m->fields.i), bits32(m->fields.j), bits64(m->fields.l),
           bits64(m->fields.m), bits64(m->fields.n));
    const bool same =
        bits32(before.i) == bits32(m->fields.i) && bits32(before.j) == bits32(m->fields.j) &&
        bits64(before.l) == bits64(m->fields.l) && bits64(before.m) == bits64(m->fields.m) &&
        bits64(before.n) == bits64(m->fields.n);
    CHECK(same && graph_same(graph));
    printf("RM_NULL_STABLE %d %d\n", same, graph_same(graph));
    CHECK(emit_side("RM_NULL_SIDE", 0, f, m, actor, &side, graph) == 0);
    return 0;
}

static int controls(fixture *f, mc_client_render_manager *m, mc_entity *actor) {
    frame_input input = {.partial = UINT32_C(0x3f000000),
                         .angles = {0, UINT32_C(0x40000000), 0, UINT32_C(0x40800000)},
                         .positions = {0, 0, 0, UINT64_C(0x4000000000000000),
                                       UINT64_C(0x4010000000000000), UINT64_C(0x4018000000000000)}};
    seed_pose(actor, &input);
    seed_scalars(m);
    CHECK(mc_client_render_manager_bind_frame(m, NULL, NULL, NULL, NULL, actor, NULL, 0.5f,
                                              f->error) == MC_CLIENT_MANAGER_OK);
    const mc_client_manager_fields normal = m->fields;
    input.positions[0] = UINT64_C(0x7ff0000000001234);
    input.positions[3] = UINT64_C(0xfff8000000005678);
    seed_pose(actor, &input);
    seed_scalars(m);
    const mc_client_manager_fields sentinel = m->fields;
    CHECK(mc_client_render_manager_bind_frame(m, NULL, NULL, NULL, NULL, actor, NULL, 0.5f,
                                              f->error) == MC_CLIENT_MANAGER_UNSUPPORTED_NUMERIC);
    CHECK(f->error->native_policy && f->error->pc == 198 && f->error->frame_pc == 198 &&
          m->last_frame_pc == 179 && m->fields.h == actor && m->fields.g == NULL &&
          m->fields.e == NULL && m->fields.k == NULL && m->fields.p == NULL &&
          bits32(m->fields.i) == bits32(normal.i) && bits32(m->fields.j) == bits32(normal.j) &&
          bits64(m->fields.l) == bits64(sentinel.l) && bits64(m->fields.m) == bits64(sentinel.m) &&
          bits64(m->fields.n) == bits64(sentinel.n));
    printf("RM_CONTROL numeric_late UNSUPPORTED_NUMERIC 198 179 refs=1 completed_ij=1 "
           "preserved_lmn=1 native_policy=1\n");
    input.positions[0] = 0;
    input.positions[3] = UINT64_C(0x4000000000000000);
    seed_pose(actor, &input);
    seed_scalars(m);
    const unsigned int saved = _mm_getcsr(), unsupported = (saved & ~0x6000u) | 0x6000u;
    _mm_setcsr(unsupported);
    const mc_client_manager_status result =
        mc_client_render_manager_bind_frame(m, NULL, NULL, NULL, NULL, actor, NULL, 0.5f, f->error);
    const bool unchanged = _mm_getcsr() == unsupported;
    _mm_setcsr(saved);
    CHECK(result == MC_CLIENT_MANAGER_UNSUPPORTED_FP && f->error->native_policy &&
          f->error->pc == 150 && f->error->frame_pc == 150 && m->last_frame_pc == 29 &&
          bits32(m->fields.i) == bits32(sentinel.i) && bits32(m->fields.j) == bits32(sentinel.j) &&
          bits64(m->fields.l) == bits64(sentinel.l) && bits64(m->fields.m) == bits64(sentinel.m) &&
          bits64(m->fields.n) == bits64(sentinel.n) && m->fields.h == actor &&
          m->fields.g == NULL && m->fields.e == NULL && m->fields.k == NULL &&
          m->fields.p == NULL && unchanged && _mm_getcsr() == saved);
    printf("RM_CONTROL unsupported_fp UNSUPPORTED_FP 150 29 refs=1 preserved_scalars=1 "
           "control_untouched=1 caller_restored=1\n");
    return 0;
}

static int cold(const char *root) {
    fixture f;
    CHECK(open_fixture(&f, root, NULL) == 0);
    mc_client_render_manager *m = NULL;
    CHECK(mc_client_render_manager_static_get(f.classes, &m, f.error) ==
          MC_CLIENT_MANAGER_REGISTRY_FAILED);
    CHECK(m == NULL && f.error->registry_delegated && !f.error->native_policy &&
          strcmp(f.error->source_class, "th") == 0 && f.error->pc == 494 && f.error->th_pc == 494 &&
          f.error->registry.status == MC_REGISTRY_NULL_REFERENCE &&
          strcmp(f.error->registry.constructor_class, "iz") == 0 &&
          f.error->registry.constructor_pc == 2 && f.error->registry.initializer_pc == 3674 &&
          strcmp(f.error->registry.active_initializer_class, "ep") == 0 &&
          f.error->registry.active_initializer_pc == 23);
    printf("RM_COLD_NATIVE NULL_REFERENCE %s <init> pc%d registry_pc%d active_%s_pc%d\n",
           f.error->registry.constructor_class, f.error->registry.constructor_pc,
           f.error->registry.initializer_pc, f.error->registry.active_initializer_class,
           f.error->registry.active_initializer_pc);
    mc_client_manager_state s;
    CHECK(mc_client_render_manager_inspect(f.classes, &s) && s.phase == MC_CLIENT_MANAGER_FAILED &&
          s.a == NULL && s.candidate != NULL && s.entries == 18 && s.completed_bw_stores == 0 &&
          s.last_put_pc == 478);
    CHECK(mc_client_render_manager_static_get(f.classes, &m, f.error) ==
              MC_CLIENT_MANAGER_CLASS_ERRONEOUS &&
          m == NULL && f.error->registry.status == MC_REGISTRY_NULL_REFERENCE &&
          strcmp(f.error->registry.constructor_class, "iz") == 0 &&
          f.error->registry.constructor_pc == 2);
    printf("RM_CONTROL cold_erroneous candidate=1 entries=%zu bw_stores=%zu last_put=%d "
           "repeat=CLASS_ERRONEOUS\n",
           s.entries, s.completed_bw_stores, s.last_put_pc);
    CHECK(close_fixture(&f) == 0);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 4);
    const char *profile = argv[2];
    CHECK(strcmp(profile, "cold_th") == 0 || strcmp(profile, "prepared_awake") == 0 ||
          strcmp(profile, "prepared_null") == 0 || strcmp(profile, "controls") == 0);
    frame_input inputs[24];
    CHECK(read_inputs(argv[3], inputs) == 0);
    printf("RM_CASE %s\n", profile);
    CHECK(fflush(stdout) == 0); /* Keep observer order before direct source stdout writes. */
    if (strcmp(profile, "cold_th") == 0) {
        CHECK(cold(argv[1]) == 0);
    } else {
        fixture f;
        CHECK(open_fixture(&f, argv[1], NULL) == 0);
        CHECK(mc_registry_classes_initialize_statistics(f.registry, &f.error->registry) ==
              MC_REGISTRY_OK);
        mc_registry_statistic_view stats;
        CHECK(mc_registry_classes_inspect_statistics(f.registry, &stats, &f.error->registry) ==
              MC_REGISTRY_OK);
        CHECK(stats.phase == MC_REGISTRY_READY && stats.C != NULL && stats.D != NULL &&
              stats.E != NULL && stats.F != NULL);
        printf("RM_PREP jl a normal %zu %zu %zu %zu %zu uncredited uncredited uncredited "
               "uncredited %d %d\n",
               stats.map_size, mc_statistic_list_size(stats.b), mc_statistic_list_size(stats.c),
               mc_statistic_list_size(stats.d), mc_statistic_list_size(stats.e), stats.G, stats.H);
        mc_client_render_manager *m = NULL;
        CHECK(mc_client_render_manager_static_get(f.classes, &m, f.error) == MC_CLIENT_MANAGER_OK &&
              m != NULL);
        CHECK(inspect_complete(&f, m, false) == 0);
        printf("RM_TH_NORMAL %d\n", f.classes->a == m);
        CHECK(emit_roster(m) == 0);
        printf("RM_CTOR th native_bucket_map %zu %zu %zu %s uncredited uncredited\n", m->map->size,
               m->put_count, m->bw_count, m->fields.f == NULL ? "null" : "nonnull");
        printf("RM_ORIGINS %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n", bits64(f.classes->b),
               bits64(f.classes->c), bits64(f.classes->d));
        mc_entity *actor = NULL;
        mc_entity_error entity_error;
        CHECK(mc_local_player_allocate(f.entities, &actor, &entity_error) == MC_ENTITY_OK &&
              actor != NULL);
        mc_entity_snapshot actor_state;
        mc_entity_class_literal_view actor_type;
        CHECK(mc_entity_get_snapshot(actor, &actor_state) == MC_ENTITY_OK &&
              actor_state.phase == MC_ENTITY_RAW);
        CHECK(mc_entity_class_literal_inspect(actor_state.class_literal, &actor_type) ==
              MC_ENTITY_OK);
        CHECK(strcmp(actor_type.client_name, "dc") == 0 && actor->random == NULL &&
              actor->watcher == NULL && f.profile->math == NULL);
        printf("RM_NATIVE_ACTOR %s RAW constructor=uncredited N=gs random=absent watcher=absent "
               "math=absent\n",
               actor_type.client_name);
        graph_snapshot graph = {0};
        CHECK(save_graph(&graph, m) == 0);
        printf("RM_NATIVE_GRAPH %zu %zu\n", graph.count, graph.bytes);
        if (strcmp(profile, "controls") == 0) {
            CHECK(controls(&f, m, actor) == 0);
        } else {
            const size_t count = strcmp(profile, "prepared_awake") == 0 ? 24 : 1;
            for (size_t i = 0; i < count; ++i)
                CHECK(awake(&f, m, actor, &inputs[i], &graph) == 0);
            if (strcmp(profile, "prepared_null") == 0)
                CHECK(null_living(&f, m, actor, &graph) == 0);
        }
        CHECK(graph_same(&graph));
        graph_free(&graph);
        mc_entity_release(actor); /* Drop caller hold before common namespace retirement. */
        CHECK(close_fixture(&f) == 0);
    }
    CHECK(!ferror(stdout));
    printf("Source RenderManager frame: %s; %u checks\n", profile, checks);
    return 0;
}
