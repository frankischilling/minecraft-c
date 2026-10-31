#include "core/java/random_profile_internal.h"
#include "entity/entity_internal.h"
#include "entity/particles/particle_internal.h"
#include "mc/client/particles/effect_renderer.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c);                                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

static uint64_t db(double d) {
    uint64_t b;
    memcpy(&b, &d, sizeof b);
    return b;
}

static uint64_t fb(float f) {
    uint32_t b;
    memcpy(&b, &f, sizeof b);
    return b;
}

static double dv(uint64_t b) {
    double d;
    memcpy(&d, &b, sizeof d);
    return d;
}

static float fv(uint64_t v) {
    uint32_t b = (uint32_t)v;
    float f;
    memcpy(&f, &b, sizeof f);
    return f;
}

static unsigned long long ull(uint64_t b) {
    return (unsigned long long)b;
}

static void h(uint64_t b) {
    printf("\t%016llx", ull(b));
}

static uint64_t math_state(const mc_java_random_profile *p) {
    CHECK(p->math != NULL);
    return atomic_load_explicit(&p->math->state, memory_order_relaxed);
}

static uint64_t u(const mc_java_random_profile *p) {
    return atomic_load_explicit(&p->uniquifier, memory_order_relaxed);
}

static mc_entity *falling(mc_entity_classes *c) {
    mc_entity *p = NULL;
    CHECK(mc_falling_entity_allocate(c, &p, NULL) == MC_ENTITY_OK);
    CHECK(mc_falling_entity_construct_world(p, NULL, NULL) == MC_ENTITY_OK);
    return p;
}

static mc_entity *particle(mc_entity_classes *c, mc_entity *a, mc_entity *b, float f) {
    mc_entity *p = NULL;
    CHECK(mc_pickup_particle_allocate(c, &p, NULL) == MC_ENTITY_OK);
    CHECK(mc_pickup_particle_construct(p, NULL, a, b, f, NULL) == MC_ENTITY_OK);
    return p;
}

static void print_particle(const char *label, mc_entity *p, mc_entity *a, mc_entity *b,
                           mc_java_random_profile *profile) {
    mc_particle_snapshot s;
    CHECK(mc_particle_read(p, &s));
    CHECK(s.phase == MC_ENTITY_READY);
    CHECK(s.particle_pc == 306 && s.pickup_pc == 60);
    CHECK(p->base_bytecode == 231);
    printf("particle\t%s\t%d", label, p->base.id);
    h(db(p->base.x));
    h(db(p->base.y));
    h(db(p->base.z));
    h(db(p->base.velocity_x));
    h(db(p->base.velocity_y));
    h(db(p->base.velocity_z));
    h(fb(p->base.offset));
    h(fb(p->base.width));
    h(fb(p->base.height));
    printf("\t%d", p->base.be ? 1 : 0);
    h(db(p->bounds->min_x));
    h(db(p->bounds->min_y));
    h(db(p->bounds->min_z));
    h(db(p->bounds->max_x));
    h(db(p->bounds->max_y));
    h(db(p->bounds->max_z));
    printf("\t%d", s.particle.b);
    h(fb(s.particle.c));
    h(fb(s.particle.d));
    printf("\t%d\t%d", s.particle.e, s.particle.f);
    h(fb(s.particle.g));
    h(fb(s.particle.h));
    h(fb(s.particle.i));
    h(fb(s.particle.j));
    h(fb(s.particle.k));
    printf("\t%d\t%d", s.pickup.p, s.pickup.q);
    h(fb(s.pickup.r));
    printf("\t%d\t%d\t%d", s.pickup.a == a ? 1 : 0, s.pickup.o == b ? 1 : 0,
           p->base.world == NULL ? 1 : 0);
    h(p->random->state);
    h(math_state(profile));
    h(u(profile));
    putchar('\n');
    CHECK(p->random->next_next_gaussian == 0 && !p->random->have_next_next_gaussian);
    mc_entity_watcher_snapshot w;
    CHECK(mc_entity_watcher_get_snapshot(p->watcher, &w) == MC_ENTITY_OK);
    CHECK(w.size == 1 && w.index == 0 && w.byte_value == 0 && !w.dirty && w.record_dirty);
}

static const void *manager_array, *manager_lists[4];

static void print_manager(const char *label, mc_client_effect_renderer *m, mc_entity *a,
                          mc_entity *b) {
    mc_effect_renderer_snapshot s;
    CHECK(mc_client_effect_renderer_read(m, &s));
    CHECK(s.phase == MC_EFFECT_RENDERER_READY);
    bool distinct = true;
    for (size_t i = 0; i < 4; i++)
        for (size_t j = 0; j < i; j++)
            distinct = distinct && s.lists[i] != s.lists[j];
    if (manager_array == NULL) {
        manager_array = s.layers;
        for (size_t i = 0; i < 4; i++)
            manager_lists[i] = s.lists[i];
    }
    bool stable = manager_array == s.layers;
    for (size_t i = 0; i < 4; i++)
        stable = stable && manager_lists[i] == s.lists[i];
    CHECK(distinct && stable);
    printf("manager\t%s\t4\t%d\t%d", label, distinct ? 1 : 0, stable ? 1 : 0);
    for (size_t i = 0; i < 4; i++)
        printf("\t%zu\t%zu", s.sizes[i], s.capacities[i]);
    mc_entity *first = s.sizes[3] ? mc_client_effect_renderer_particle(m, 3, 0) : NULL;
    mc_entity *last = s.sizes[3] ? mc_client_effect_renderer_particle(m, 3, s.sizes[3] - 1) : NULL;
    printf("\t%d\t%d\t%d\t%d",
           first == NULL ? 0
           : first == a  ? 1
           : first == b  ? 2
                         : 3,
           last == NULL ? 0
           : last == a  ? 1
           : last == b  ? 2
                        : 3,
           s.world == NULL ? 1 : 0, s.render_engine == NULL ? 1 : 0);
    h(s.random->state);
    putchar('\n');
}

static void null_cold(mc_entity_classes *classes, mc_java_random_profile *profile,
                      mc_entity *target) {
    uint64_t before = u(profile);
    int32_t counter = classes->next_id;
    mc_entity *raw = NULL;
    mc_particle_error e;
    CHECK(profile->math == NULL);
    CHECK(mc_pickup_particle_allocate(classes, &raw, NULL) == MC_ENTITY_OK);
    CHECK(mc_pickup_particle_construct(raw, NULL, NULL, target, 1, &e) == MC_ENTITY_STATE_ERROR);
    CHECK(e.cause == MC_PARTICLE_NULL_REFERENCE && e.source_pc == 3 &&
          raw->phase == MC_ENTITY_FAILED_CONSTRUCTION && raw->base_bytecode == 0 &&
          raw->random == NULL && raw->bounds == NULL);
    printf("null-cold\t%d\t%d\n", u(profile) == before ? 1 : 0,
           classes->next_id == counter ? 1 : 0);
    CHECK(profile->math == NULL && u(profile) == before && classes->next_id == counter);
    mc_entity_release(raw);
}

static const uint64_t cases[][7] = {
    {0, 0, 0, 0, 0, 0, 0},
    {UINT64_C(0x3ff4000000000000), UINT64_C(0xc000000000000000), UINT64_C(0x400e000000000000),
     UINT64_C(0x3fd0000000000000), UINT64_C(0xbfe0000000000000), UINT64_C(0x3ff0000000000000),
     UINT64_C(0x3f800000)},
    {UINT64_C(0x7fefffffffffffff), UINT64_C(0xffefffffffffffff), 1, UINT64_C(0x7fefffffffffffff),
     UINT64_C(0xffefffffffffffff), 1, UINT64_C(0x80000000)},
    {UINT64_C(0x7ff0000000000000), UINT64_C(0xfff0000000000000), 0, UINT64_C(0x7ff0000000000000),
     UINT64_C(0xfff0000000000000), 0, UINT64_C(0x7fc12345)},
    {UINT64_C(0x7ff8123456789abc), UINT64_C(0xfff8fedcba987654), 0, UINT64_C(0x7ff8123456789abc),
     UINT64_C(0xfff8fedcba987654), 0, UINT64_C(0x7f800000)},
    {UINT64_C(0x8000000000000000), 1, UINT64_C(0x8000000000000001), 1, UINT64_C(0x8000000000000001),
     UINT64_C(0x0010000000000000), UINT64_C(0xff800000)},
    {0, 0, 0, UINT64_C(0x7ff0000000000001), UINT64_C(0x3ff0000000000000),
     UINT64_C(0xfff0000000000001), UINT64_C(0x7f812345)},
    {UINT64_C(0x3ff0000000000000), UINT64_C(0x4000000000000000), UINT64_C(0x4008000000000000), 0, 0,
     0, UINT64_C(0xffc12345)}};

static void reference(const char *path) {
    mc_java_random_constructor_input inputs[64];
    size_t count = 0;
    FILE *f = NULL;
    if (path != NULL) {
#ifdef _MSC_VER
        CHECK(fopen_s(&f, path, "r") == 0);
#else
        f = fopen(path, "r");
        CHECK(f != NULL);
#endif
        CHECK(f != NULL);
        char line[256];
        while (fgets(line, sizeof line, f) != NULL) {
            unsigned long long a, b;
#ifdef _MSC_VER
            int scanned = sscanf_s(line, "input\t%llx\t%llx", &a, &b);
#else
            int scanned = sscanf(line, "input\t%llx\t%llx", &a, &b);
#endif
            if (scanned == 2) {
                CHECK(count < 64);
                inputs[count].committed_uniquifier = (uint64_t)a;
                uint64_t bits = (uint64_t)b;
                memcpy(&inputs[count].nanotime, &bits, sizeof bits);
                count++;
            } else
                CHECK(strncmp(line, "Inputs: 14 constructors", 22) == 0 ||
                      strncmp(line, "Draws: 48 floats, 58 doubles, 1 longs", 35) == 0);
        }
        CHECK(!ferror(f) && fclose(f) == 0 && count == 14);
    }
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_client_effect_renderer *manager = NULL;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    if (path != NULL)
        CHECK(mc_java_random_profile_bind_constructor_inputs(profile, inputs, count) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &classes) == MC_ENTITY_OK);
    mc_entity *picked = falling(classes), *target = falling(classes);
    CHECK(mc_client_effect_renderer_allocate(classes, NULL, &manager, NULL) ==
          MC_EFFECT_RENDERER_OK);
    CHECK(mc_client_effect_renderer_construct(manager, NULL, NULL, NULL) == MC_EFFECT_RENDERER_OK);
    print_manager("constructed", manager, NULL, NULL);
    mc_effect_renderer_snapshot ms;
    CHECK(mc_client_effect_renderer_read(manager, &ms));
    printf("manager-next");
    h((uint64_t)mc_java_random_next_i64((mc_java_random *)ms.random));
    h(ms.random->state);
    putchar('\n');
    null_cold(classes, profile, target);
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const uint64_t *c = cases[i];
        picked->base.x = dv(c[0]);
        picked->base.y = dv(c[1]);
        picked->base.z = dv(c[2]);
        picked->base.velocity_x = dv(c[3]);
        picked->base.velocity_y = dv(c[4]);
        picked->base.velocity_z = dv(c[5]);
        mc_entity *t = i == 6 ? NULL : i == 7 ? picked : target;
        size_t holds = picked->holds, tholds = target->holds;
        mc_entity *p = particle(classes, picked, t, fv(c[6]));
        CHECK(picked->holds == holds + (t == picked ? 2u : 1u));
        CHECK(target->holds == tholds + (t == target ? 1u : 0u));
        char label[64];
        snprintf(label, sizeof label, "ctor%zu", i);
        print_particle(label, p, picked, t, profile);
        const float next = mc_java_random_next_float(p->random);
        double nextm;
        CHECK(mc_java_math_random(profile, &nextm) == MC_JAVA_RANDOM_PROFILE_OK);
        printf("next\t%zu", i);
        h(fb(next));
        h(db(nextm));
        h(p->random->state);
        h(math_state(profile));
        putchar('\n');
        for (int j = 1; j <= 4; j++) {
            CHECK(mc_pickup_particle_tick(p, NULL) == MC_ENTITY_OK);
            snprintf(label, sizeof label, "tick%zu-%d", i, j);
            print_particle(label, p, picked, t, profile);
            CHECK(p->base.be == (j >= 3));
        }
        int32_t layer = -1;
        CHECK(mc_pickup_particle_layer(p, &layer) == MC_ENTITY_OK && layer == 3);
        printf("layer\t%zu\t%d\n", i, layer);
        if (i == 0) {
            p->particle->pickup.p = INT32_MAX;
            p->particle->pickup.q = INT32_MIN;
            p->base.be = false;
            CHECK(mc_pickup_particle_tick(p, NULL) == MC_ENTITY_OK);
            print_particle("wrap", p, picked, t, profile);
            CHECK(p->particle->pickup.p == INT32_MIN && p->base.be);
        }
        mc_entity_release(p);
        CHECK(picked->holds == holds && target->holds == tholds);
    }
    uint64_t before = u(profile);
    int32_t counter = classes->next_id;
    mc_entity *raw = NULL;
    mc_particle_error e;
    CHECK(mc_pickup_particle_allocate(classes, &raw, NULL) == MC_ENTITY_OK);
    CHECK(mc_pickup_particle_construct(raw, NULL, NULL, target, 1, &e) == MC_ENTITY_STATE_ERROR);
    CHECK(e.cause == MC_PARTICLE_NULL_REFERENCE && e.source_pc == 3 &&
          raw->phase == MC_ENTITY_FAILED_CONSTRUCTION && raw->base_bytecode == 0 &&
          raw->random == NULL && raw->bounds == NULL);
    printf("null\t%d\t%d\n", u(profile) == before ? 1 : 0, classes->next_id == counter ? 1 : 0);
    CHECK(u(profile) == before && classes->next_id == counter);
    mc_entity_release(raw);
    mc_entity *first = particle(classes, picked, target, 0),
              *second = particle(classes, picked, target, 0);
    CHECK(mc_client_effect_renderer_add(manager, first, NULL) == MC_EFFECT_RENDERER_OK);
    print_manager("one", manager, first, second);
    for (int i = 0; i < 3; i++) {
        CHECK(mc_client_effect_renderer_tick(manager, NULL) == MC_EFFECT_RENDERER_OK);
        char label[32];
        snprintf(label, sizeof label, "tick%d", i);
        print_manager(label, manager, first, second);
    }
    first->particle->pickup.p = 0;
    first->base.be = false;
    CHECK(mc_client_effect_renderer_add(manager, first, NULL) == MC_EFFECT_RENDERER_OK);
    for (int i = 0; i < 4000; i++)
        CHECK(mc_client_effect_renderer_add(manager, second, NULL) == MC_EFFECT_RENDERER_OK);
    CHECK(first->holds == 1 && second->holds == 4001);
    print_manager("cap", manager, first, second);
    CHECK(mc_client_effect_renderer_tick(manager, NULL) == MC_EFFECT_RENDERER_OK);
    print_manager("duplicates1", manager, first, second);
    printf("duplicate-state\t%d\t%d\t%s\n", first->particle->pickup.p, second->particle->pickup.p,
           second->base.be ? "true" : "false");
    CHECK(second->particle->pickup.p == 4000 && second->holds == 3);
    CHECK(mc_client_effect_renderer_tick(manager, NULL) == MC_EFFECT_RENDERER_OK);
    print_manager("duplicates2", manager, first, second);
    CHECK(second->holds == 1);
    CHECK(mc_client_effect_renderer_add(manager, second, NULL) == MC_EFFECT_RENDERER_OK);
    CHECK(mc_client_effect_renderer_change_world(manager, NULL, NULL) == MC_EFFECT_RENDERER_OK);
    print_manager("clear", manager, first, second);
    printf("final");
    h(u(profile));
    h(math_state(profile));
    putchar('\n');
    if (path != NULL)
        CHECK(mc_java_random_profile_constructor_inputs_complete(profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    mc_entity_release(first);
    mc_entity_release(second);
    CHECK(mc_client_effect_renderer_free(&manager) == MC_EFFECT_RENDERER_OK);
    mc_entity_release(picked);
    mc_entity_release(target);
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
}

static void controls(void) {
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *c = NULL, *foreign = NULL;
    mc_particle_error e;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &c) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, profile, NULL, &foreign) == MC_ENTITY_OK);
    mc_entity *a = falling(c), *b = falling(c), *other = falling(foreign), *raw = NULL,
              *raw_peer = NULL;
    CHECK(mc_pickup_particle_allocate(c, &raw, NULL) == MC_ENTITY_OK);
    CHECK(mc_falling_entity_allocate(c, &raw_peer, NULL) == MC_ENTITY_OK);
    uint64_t before = u(profile);
    int32_t counter = c->next_id;
    CHECK(mc_pickup_particle_construct(raw, NULL, raw_peer, b, 0, &e) == MC_ENTITY_INVALID_PHASE);
    CHECK(raw->phase == MC_ENTITY_RAW && raw_peer->phase == MC_ENTITY_RAW && u(profile) == before &&
          c->next_id == counter);
    CHECK(mc_pickup_particle_construct(raw, NULL, a, raw_peer, 0, &e) == MC_ENTITY_INVALID_PHASE &&
          raw->phase == MC_ENTITY_RAW);
    CHECK(mc_pickup_particle_construct(raw, NULL, other, b, 0, &e) == MC_ENTITY_INVALID_ARGUMENT &&
          raw->phase == MC_ENTITY_RAW);
    mc_entity *p = particle(c, a, b, 0);
    before = u(profile);
    counter = c->next_id;
    CHECK(mc_pickup_particle_construct(raw, NULL, p, b, 0, &e) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_pickup_particle_construct(raw, NULL, a, p, 0, &e) == MC_ENTITY_INVALID_ARGUMENT);
    CHECK(raw->phase == MC_ENTITY_RAW && u(profile) == before && c->next_id == counter);
    int32_t layer = -1;
    CHECK(mc_pickup_particle_layer(raw, &layer) == MC_ENTITY_OK && layer == 3);
    CHECK(mc_pickup_particle_tick(raw, &e) == MC_ENTITY_INVALID_PHASE);
    mc_entity_release(raw);
    mc_entity_release(raw_peer);
    mc_entity_release(p);
    for (unsigned int i = 0; i < 3; i++) {
        CHECK(mc_pickup_particle_allocate(c, &raw, NULL) == MC_ENTITY_OK);
        size_t ah = a->holds, bh = b->holds;
        a->holds = i == 0 ? SIZE_MAX : i == 2 ? SIZE_MAX - 1 : ah;
        b->holds = i == 1 ? SIZE_MAX : bh;
        mc_entity *target = i == 2 ? a : b;
        CHECK(mc_pickup_particle_construct(raw, NULL, a, target, 0, &e) == MC_ENTITY_LIMIT);
        CHECK(raw->phase == MC_ENTITY_FAILED_CONSTRUCTION && raw->particle->particle_pc == 306 &&
              raw->particle->pickup.q == 0 && raw->particle->pickup.r == 0);
        CHECK(raw->particle->pickup_pc == (i == 0 ? 36u : 41u));
        CHECK(raw->particle->pickup.a == (i == 0 ? NULL : a) && raw->particle->pickup.o == NULL);
        CHECK(e.cause == MC_PARTICLE_OWNER && e.entity.status == MC_ENTITY_LIMIT);
        /* Restore the true finite owner plus every hold actually published. */
        a->holds = ah + (i == 0 ? 0u : 1u);
        b->holds = bh;
        mc_entity_release(raw);
        CHECK(a->holds == ah && b->holds == bh);
    }
    size_t ah = a->holds;
    p = particle(c, a, a, 0);
    CHECK(a->holds == ah + 2);
    mc_entity_release(p);
    CHECK(a->holds == ah);
    mc_entity_release(a);
    mc_entity_release(b);
    mc_entity_release(other);
    CHECK(mc_entity_classes_free(c) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_free(foreign) == MC_ENTITY_OK);
    mc_java_random_profile_free(profile);
}

int main(int argc, char **argv) {
    if (argc > 2)
        return 2;
    reference(argc == 2 ? argv[1] : NULL);
    controls();
    printf("Pickup particles: %u checks\n", checks);
    return ferror(stdout) ? 1 : 0;
}
