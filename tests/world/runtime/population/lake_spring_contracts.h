/* Private include fragment for tests/world/runtime/test_queries.c.
 * Install below its fixture/helpers, before main; call
 * population_lake_spring_contracts(f, side) before tree/plant contracts.
 * These are source-derived native contracts, never original/parity evidence.
 * Prepared terrain, nullable references, named statics and incoming flags are
 * labeled inputs. Actual owners, queues, counters, bindings and diagnostics
 * are never fabricated, rewound, drained or replaced by successful callbacks. */
#include "../../../../src/core/java/random_profile_internal.h"
#include "../../../support/java_random.h"
#include "mc/world/runtime/population/lake_spring.h"
#include "mc/world/runtime/scheduling.h"
#include <emmintrin.h>
#include <math.h>

typedef struct ls_image {
    mc_chunk_data data;
    uint8_t blocks[MC_CHUNK_BLOCKS];
} ls_image;
typedef struct ls_inputs {
    ls_image image;
    mc_world_chunk_nibble *e, *f, *g;
    uint8_t *h;
    mc_block *named[96];
    mc_world_reference_list *u;
    mc_world_provider *v;
    mc_java_random *r;
    bool a, o, B;
} ls_inputs;
typedef struct ls_profile {
    uint64_t unique, state, cache;
    unsigned int phase;
    mc_java_math_random_owner *math;
    size_t allocated;
    mc_platform_nanotime clock;
    bool cached;
} ls_profile;
typedef struct ls_queues {
    mc_world_light_list_view lights;
    const mc_world_light_region **light;
    mc_world_light_region_fields *light_fields;
    const mc_scheduled_tick **ordered, **membership;
    mc_scheduled_tick_fields *ordered_fields, *membership_fields;
    size_t e, f;
    size_t e_bytes, f_bytes;
    mc_world_schedule_class_state sequence;
} ls_queues;
typedef struct ls_counts {
    size_t air, liquid, grass, stone, changed;
} ls_counts;

static uint64_t ls_bits(double d)
{
    uint64_t u;
    memcpy(&u, &d, sizeof(u));
    return u;
}
static ls_profile ls_profile_read(const mc_java_random_profile *p)
{
    ls_profile s = {.unique = atomic_load_explicit(&p->uniquifier, memory_order_relaxed),
                    .phase = atomic_load_explicit(&p->holder_phase, memory_order_acquire),
                    .math = p->math,
                    .allocated = p->allocated_bytes,
                    .clock = p->clock};
    if (s.math != NULL) {
        s.state = atomic_load_explicit(&s.math->state, memory_order_relaxed);
        s.cache = ls_bits(s.math->next_next_gaussian);
        s.cached = s.math->have_next_next_gaussian;
    }
    return s;
}
static bool ls_profile_same(const mc_java_random_profile *p, const ls_profile *s)
{
    const ls_profile a = ls_profile_read(p);
    return a.unique == s->unique && a.phase == s->phase && a.math == s->math &&
           a.allocated == s->allocated && a.state == s->state && a.cache == s->cache &&
           a.cached == s->cached && a.clock.frequency == s->clock.frequency &&
           a.clock.initial_sample == s->clock.initial_sample &&
           a.clock.initialized == s->clock.initialized;
}
static size_t ls_index(int32_t x, int32_t y, int32_t z)
{
    return ((size_t)x * 16u + (size_t)z) * 128u + (size_t)y;
}
static unsigned int ls_nibble(const uint8_t *a, size_t i)
{
    return ((unsigned int)a[i >> 1u] >> ((i & 1u) * 4u)) & 15u;
}
static void ls_image_read(ls_image *s, const mc_world_chunk *c)
{
    s->data = *c->data;
    memcpy(s->blocks, c->data->blocks, sizeof(s->blocks));
}
static bool ls_image_same(const ls_image *s, const mc_world_chunk *c)
{
    const mc_chunk_data *d = c->data;
    return d->blocks == s->data.blocks && d->blocks_capacity == s->data.blocks_capacity &&
           d->minimum_height == s->data.minimum_height &&
           memcmp(s->blocks, d->blocks, sizeof(s->blocks)) == 0 &&
           memcmp(s->data.metadata, d->metadata, sizeof(d->metadata)) == 0 &&
           memcmp(s->data.skylight, d->skylight, sizeof(d->skylight)) == 0 &&
           memcmp(s->data.block_light, d->block_light, sizeof(d->block_light)) == 0 &&
           memcmp(s->data.heightmap, d->heightmap, sizeof(d->heightmap)) == 0;
}
static bool ls_support_same(const ls_image *s, const mc_world_chunk *c)
{
    return c->data->minimum_height == s->data.minimum_height &&
           memcmp(c->data->skylight, s->data.skylight, MC_CHUNK_NIBBLE_BYTES) == 0 &&
           memcmp(c->data->block_light, s->data.block_light, MC_CHUNK_NIBBLE_BYTES) == 0 &&
           memcmp(c->data->heightmap, s->data.heightmap, MC_CHUNK_HEIGHTMAP_BYTES) == 0;
}
static mc_block **ls_named(mc_registry_classes *r, const char *name)
{
    for (size_t i = 0; i < COUNT(r->blocks.named); ++i) {
        const char *n = mc_registry_block_static_name(i);
        if (n != NULL && strcmp(n, name) == 0)
            return &r->blocks.named[i];
    }
    return NULL;
}
static bool ls_id(mc_registry_classes *r, const char *name, int32_t *id)
{
    const mc_block *b = NULL;
    if (mc_registry_block_static_get(r, name, &b, NULL) != MC_REGISTRY_OK || b == NULL)
        return false;
    *id = b->fields.bn;
    return *id > 0 && *id <= UINT8_MAX;
}
static bool ls_inputs_read(ls_inputs *s, mc_world_chunk *c, mc_world_runtime *w,
                           mc_registry_classes *r)
{
    if (c == NULL || c->data == NULL || c->data->blocks == NULL ||
        c->data->blocks_capacity < MC_CHUNK_BLOCKS || c->e == NULL || c->f == NULL ||
        c->g == NULL || c->h != c->data->heightmap || c->e->a != c->data->metadata ||
        c->f->a != c->data->skylight || c->g->a != c->data->block_light ||
        COUNT(s->named) != COUNT(r->blocks.named) || w->fields.r == NULL)
        return false;
    ls_image_read(&s->image, c);
    s->e = c->e;
    s->f = c->f;
    s->g = c->g;
    s->h = c->h;
    memcpy(s->named, r->blocks.named, sizeof(s->named));
    s->u = w->fields.u;
    s->v = w->fields.v;
    s->r = w->fields.r;
    s->a = w->fields.a;
    s->o = w->fields.o;
    s->B = w->fields.B;
    return true;
}
static void ls_inputs_restore(const ls_inputs *s, mc_world_chunk *c, mc_world_runtime *w,
                              mc_registry_classes *r)
{
    *c->data = s->image.data;
    memcpy(c->data->blocks, s->image.blocks, sizeof(s->image.blocks));
    c->e = s->e;
    c->f = s->f;
    c->g = s->g;
    c->h = s->h;
    memcpy(r->blocks.named, s->named, sizeof(s->named));
    w->fields.u = s->u;
    w->fields.v = s->v;
    w->fields.r = s->r;
    w->fields.a = s->a;
    w->fields.o = s->o;
    w->fields.B = s->B;
    /* Only declared trial inputs above are restored. Dirty state, every actual
     * C/E/F record, scheduler sequence, Random object state, diagnostics and
     * publication/lease/binding/class state retain all reached effects. */
}
static void ls_terrain(mc_world_chunk *c, mc_world_runtime *w, int32_t fill)
{
    /* Explicit payload/flag inputs. Height128 makes every trial write an
     * interior edit, so no height/skylight rebuild is expected. Saved sky15
     * is intentional input even inside this prepared solid volume. */
    memset(c->data->blocks, fill, MC_CHUNK_BLOCKS);
    memset(c->data->metadata, 0xaa, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->skylight, 0xff, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->heightmap, 128, MC_CHUNK_HEIGHTMAP_BYTES);
    c->data->minimum_height = 128;
    w->fields.a = false;
    w->fields.o = false;
    w->fields.B = false;
}
static void ls_cavity(mc_world_chunk *c, mc_world_runtime *w, int32_t stone, int32_t x)
{
    ls_terrain(c, w, stone);
    c->data->blocks[ls_index(x + 1, 64, 8)] = 0;
}
static bool ls_bounds_same(const mc_light_bounds *a, const mc_light_bounds *b)
{
    return a->min_x == b->min_x && a->min_y == b->min_y && a->min_z == b->min_z &&
           a->max_x == b->max_x && a->max_y == b->max_y && a->max_z == b->max_z;
}
static bool ls_tick_same(const mc_scheduled_tick_fields *a, const mc_scheduled_tick_fields *b)
{
    return a->x == b->x && a->y == b->y && a->z == b->z && a->block == b->block &&
           a->due_time == b->due_time && a->sequence == b->sequence;
}
static void ls_queues_free(ls_queues *s)
{
    free(s->light);
    free(s->light_fields);
    free(s->ordered);
    free(s->membership);
    free(s->ordered_fields);
    free(s->membership_fields);
    *s = (ls_queues){0};
}
static bool ls_queues_read(mc_world_runtime *w, ls_queues *s)
{
    int32_t e = 0, f = 0;
    size_t written = SIZE_MAX;
    *s = (ls_queues){0};
    if (mc_world_runtime_light_list_get(w, &s->lights, NULL) != MC_WORLD_STORAGE_OK ||
        mc_scheduled_ordered_size(w->fields.E, &e) != MC_SCHEDULED_ORDERED_OK ||
        mc_scheduled_membership_size(w->fields.F, &f) != MC_SCHEDULED_MEMBERSHIP_OK || e < 0 ||
        f < 0 ||
        mc_scheduled_ordered_native_bytes(w->fields.E, &s->e_bytes) != MC_SCHEDULED_ORDERED_OK ||
        mc_scheduled_membership_native_bytes(w->fields.F, &s->f_bytes) !=
            MC_SCHEDULED_MEMBERSHIP_OK ||
        mc_world_runtime_schedule_class_get(w->classes, &s->sequence) != MC_WORLD_STORAGE_OK)
        goto fail;
    s->e = (size_t)e;
    s->f = (size_t)f;
    if (s->lights.count > SIZE_MAX / sizeof(*s->light) ||
        s->lights.count > SIZE_MAX / sizeof(*s->light_fields) ||
        s->e > SIZE_MAX / sizeof(*s->ordered) || s->e > SIZE_MAX / sizeof(*s->ordered_fields) ||
        s->f > SIZE_MAX / sizeof(*s->membership) || s->f > SIZE_MAX / sizeof(*s->membership_fields))
        goto fail;
    if (s->lights.count != 0) {
        s->light = malloc(s->lights.count * sizeof(*s->light));
        s->light_fields = malloc(s->lights.count * sizeof(*s->light_fields));
        if (s->light == NULL || s->light_fields == NULL)
            goto fail;
    }
    if (s->e != 0) {
        s->ordered = malloc(s->e * sizeof(*s->ordered));
        s->ordered_fields = malloc(s->e * sizeof(*s->ordered_fields));
        if (s->ordered == NULL || s->ordered_fields == NULL)
            goto fail;
    }
    if (s->f != 0) {
        s->membership = malloc(s->f * sizeof(*s->membership));
        s->membership_fields = malloc(s->f * sizeof(*s->membership_fields));
        if (s->membership == NULL || s->membership_fields == NULL)
            goto fail;
    }
    if (mc_scheduled_ordered_snapshot(w->fields.E, s->ordered, s->e, &written) !=
            MC_SCHEDULED_ORDERED_OK ||
        written != s->e)
        goto fail;
    written = SIZE_MAX;
    if (mc_scheduled_membership_snapshot(w->fields.F, s->membership, s->f, &written) !=
            MC_SCHEDULED_MEMBERSHIP_OK ||
        written != s->f)
        goto fail;
    for (size_t i = 0; i < s->e; ++i)
        if (mc_scheduled_tick_fields_get(s->ordered[i], &s->ordered_fields[i]) !=
            MC_SCHEDULED_TICK_OK)
            goto fail;
    for (size_t i = 0; i < s->f; ++i)
        if (mc_scheduled_tick_fields_get(s->membership[i], &s->membership_fields[i]) !=
            MC_SCHEDULED_TICK_OK)
            goto fail;
    for (size_t i = 0; i < s->lights.count; ++i) {
        if (mc_world_runtime_light_at(w, i, &s->light[i], NULL) != MC_WORLD_STORAGE_OK)
            goto fail;
        const mc_world_light_region_fields *v =
            mc_world_runtime_light_region_fields_get(s->light[i]);
        if (v == NULL || mc_world_runtime_light_type_fields_get(v->a) == NULL)
            goto fail;
        s->light_fields[i] = *v;
    }
    return true;
fail:
    ls_queues_free(s);
    return false;
}
static bool ls_scheduled_same(const ls_queues *a, const ls_queues *b)
{
    if (a->e != b->e || a->f != b->f || a->e_bytes != b->e_bytes || a->f_bytes != b->f_bytes)
        return false;
    for (size_t i = 0; i < a->e; ++i)
        if (a->ordered[i] != b->ordered[i] ||
            !ls_tick_same(&a->ordered_fields[i], &b->ordered_fields[i]))
            return false;
    for (size_t i = 0; i < a->f; ++i)
        if (a->membership[i] != b->membership[i] ||
            !ls_tick_same(&a->membership_fields[i], &b->membership_fields[i]))
            return false;
    return true;
}
static bool ls_lights_same(const ls_queues *a, const ls_queues *b)
{
    if (a->lights.identity != b->lights.identity || a->lights.count != b->lights.count ||
        a->lights.capacity != b->lights.capacity)
        return false;
    for (size_t i = 0; i < a->lights.count; ++i)
        if (a->light[i] != b->light[i] || a->light_fields[i].a != b->light_fields[i].a ||
            !ls_bounds_same(&a->light_fields[i].bounds, &b->light_fields[i].bounds))
            return false;
    return true;
}
static bool ls_sequence_delta(const ls_queues *a, const ls_queues *b, int64_t requests)
{
    const int64_t start = a->sequence.initialized ? a->sequence.next : 0;
    return requests == 0
               ? a->sequence.initialized == b->sequence.initialized &&
                     a->sequence.next == b->sequence.next
               : b->sequence.initialized && b->sequence.next == mc_java_i64_add(start, requests);
}
static bool ls_queues_after(mc_world_runtime *w, const ls_queues *before, bool lights,
                            int64_t requests)
{
    ls_queues after = {0};
    const bool read = ls_queues_read(w, &after);
    const bool result = read && ls_scheduled_same(before, &after) &&
                        (!lights || ls_lights_same(before, &after)) &&
                        ls_sequence_delta(before, &after, requests) && w->active_calls == 0 &&
                        w->local_leases == 0 && w->classes->enqueue_depth == 0;
    ls_queues_free(&after);
    return result;
}
static bool ls_cleared(const mc_population_lake_spring_error *e)
{
    const mc_registry_error empty = {.initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    return e->status == MC_WORLD_STORAGE_OK && e->operation == NULL && e->feature_class == NULL &&
           e->feature_method == NULL && e->bytecode == 0 && e->air_bytecode == 0 &&
           e->cause.status == MC_WORLD_STORAGE_OK && e->cause.operation == NULL &&
           e->cause.system_error == 0 && same_registry(&e->registry, &empty) &&
           same_registry(&e->material.registry, &empty) &&
           same_registry(&e->query.registry, &empty) &&
           same_registry(&e->mutation.registry, &empty) &&
           e->material.status == MC_WORLD_STORAGE_OK && e->query.status == MC_WORLD_STORAGE_OK &&
           e->mutation.status == MC_WORLD_STORAGE_OK && e->mutation.operation == NULL &&
           e->mutation.wrapper_bytecode == 0 && e->mutation.bytecode == 0 &&
           e->mutation.chunk_bytecode == 0 && e->mutation.hook_class == NULL &&
           e->mutation.cause_hook_class == NULL && e->mutation.schedule_bytecode == 0;
}
static bool ls_failure(const mc_population_lake_spring_error *e, mc_world_runtime *w, size_t side,
                       bool lake, unsigned int pc)
{
    return e->status != MC_WORLD_STORAGE_OK && e->operation != NULL && e->feature_class != NULL &&
           strcmp(e->feature_class, lake ? (side == 0 ? "dj" : "cd") : (side == 0 ? "xo" : "pc")) ==
               0 &&
           e->feature_method != NULL && strcmp(e->feature_method, "a") == 0 && e->bytecode == pc &&
           same_cause(&e->cause, &w->last_error) && e->cause.status == e->status &&
           e->cause.operation == e->operation && w->active_calls == 0 && w->local_leases == 0;
}
static void ls_random(mc_java_random *r, int64_t seed)
{
    mc_java_random_seed(r, seed);
    (void)mc_java_random_next_gaussian(r); /* Real API retains a cached Gaussian. */
}
static bool ls_lake_budget(mc_java_random *r, bool complete)
{
    int32_t n;
    if (!mc_java_random_next_bounded(r, 4, &n))
        return false;
    if (complete)
        for (int32_t i = 0; i < n + 4; ++i)
            for (unsigned int j = 0; j < 6; ++j)
                (void)mc_java_random_next_double(r);
    return true;
}
static bool ls_lava_budget(mc_java_random *r, int32_t *count)
{
    int32_t ignored;
    if (!mc_java_random_next_bounded(r, 3, count))
        return false;
    return *count == 0 || (mc_java_random_next_bounded(r, 3, &ignored) &&
                           mc_java_random_next_bounded(r, 3, &ignored));
}
/* Output properties, NOT a copied ellipsoid/mask/voxel oracle. Every actual
 * cell and packed nibble is inspected. No expected carved shape is generated. */
static bool ls_lake_image(const ls_image *before, const mc_world_chunk *c, int32_t target,
                          int32_t grass, int32_t stone, bool rim, ls_counts *n)
{
    *n = (ls_counts){0};
    if (!ls_support_same(before, c))
        return false;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            for (int32_t y = 0; y < 128; ++y) {
                const size_t i = ls_index(x, y, z);
                const int32_t old = before->blocks[i], id = c->data->blocks[i];
                const bool changed = id != old;
                if (changed) {
                    ++n->changed;
                    if (id == 0) {
                        if (x < 1 || x > 14 || z < 1 || z > 14 || y < 64 || y > 66)
                            return false;
                        ++n->air;
                    } else if (id == target) {
                        if (x < 1 || x > 14 || z < 1 || z > 14 || y < 61 || y > 63)
                            return false;
                        ++n->liquid;
                    } else if (id == grass && grass != 0) {
                        if (x < 1 || x > 14 || z < 1 || z > 14 || y < 63 || y > 65)
                            return false;
                        ++n->grass;
                    } else if (rim && id == stone) {
                        if (y < 60 || y > 67)
                            return false;
                        ++n->stone;
                    } else {
                        return false;
                    }
                }
                const unsigned int expected = changed ? 0u : ls_nibble(before->data.metadata, i);
                if (ls_nibble(c->data->metadata, i) != expected)
                    return false;
            }
    return true;
}
static bool ls_spring_image_extra(const ls_image *before, const mc_world_chunk *c, int32_t x,
                                  int32_t center, int32_t side, unsigned int side_metadata,
                                  int32_t east, unsigned int east_metadata)
{
    if (!ls_support_same(before, c))
        return false;
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i) {
        int32_t id = before->blocks[i];
        unsigned int metadata = ls_nibble(before->data.metadata, i);
        if (i == ls_index(x, 64, 8)) {
            id = center;
            metadata = 0;
        }
        if (side >= 0 && i == ls_index(x + 1, 64, 8)) {
            id = side;
            metadata = side_metadata;
        }
        if (east >= 0 && i == ls_index(x + 2, 64, 8)) {
            id = east;
            metadata = east_metadata;
        }
        if (c->data->blocks[i] != id || ls_nibble(c->data->metadata, i) != metadata)
            return false;
    }
    return true;
}
static bool ls_spring_image(const ls_image *before, const mc_world_chunk *c, int32_t x,
                            int32_t center, int32_t side, unsigned int side_metadata)
{
    return ls_spring_image_extra(before, c, x, center, side, side_metadata, -1, 0);
}
static bool ls_air_lake_image(const ls_image *before, const mc_world_chunk *c)
{
    size_t changed = 0;
    if (!ls_support_same(before, c))
        return false;
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z)
            for (int32_t y = 0; y < 128; ++y) {
                const size_t i = ls_index(x, y, z);
                const bool different = before->blocks[i] != c->data->blocks[i];
                if (different) {
                    if (c->data->blocks[i] != 0 || x < 1 || x > 14 || z < 1 || z > 14 || y < 61 ||
                        y > 66)
                        return false;
                    ++changed;
                }
                if (ls_nibble(c->data->metadata, i) !=
                    (different ? 0u : ls_nibble(before->data.metadata, i)))
                    return false;
            }
    return changed != 0;
}

static bool ls_lakes(fixture *f, size_t side, mc_world_chunk *c, mc_world_runtime *w, int32_t stone,
                     int32_t dirt, int32_t grass, int32_t water, int32_t lava)
{
    bool ok = false, saved = false;
    ls_inputs *input = calloc(1, sizeof(*input));
    ls_image *before = malloc(sizeof(*before));
    ls_queues q = {0}, after = {0};
    mc_registry_classes *r = f->registry[side];
    mc_population_lake_feature lake;
    mc_population_lake_spring_error e;
    mc_java_random random, expected, world_random;
    ls_profile profile;
    bool out = true;
    CHECK(input != NULL && before != NULL && ls_inputs_read(input, c, w, r));
    saved = true;
    const mc_java_random initial_world_random = *w->fields.r;
    CHECK(mc_population_lake_feature_init(&lake, INT32_MIN) == MC_WORLD_STORAGE_OK &&
          lake.a == INT32_MIN);
    CHECK(mc_population_lake_feature_init(&lake, water) == MC_WORLD_STORAGE_OK);
    CHECK(lake.a == water &&
          mc_population_lake_feature_scale(&lake, NAN, INFINITY, -INFINITY) ==
              MC_WORLD_STORAGE_OK &&
          lake.a == water);
    CHECK(mc_population_lake_feature_init(NULL, INT32_MIN) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          mc_population_lake_feature_scale(NULL, 0, 0, 0) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    const unsigned int scale_csr = _mm_getcsr();
    _mm_setcsr(scale_csr ^ 0x2000u);
    const mc_world_storage_status scaled =
        mc_population_lake_feature_scale(&lake, NAN, INFINITY, -INFINITY);
    _mm_setcsr(scale_csr);
    CHECK(scaled == MC_WORLD_STORAGE_OK && lake.a == water);

    /* Query17/inner air4 precede allocation35 and every supplied draw. */
    ls_terrain(c, w, stone);
    ls_image_read(before, c);
    ls_random(&random, 17);
    expected = random;
    CHECK(ls_queues_read(w, &q));
    w->fields.v = NULL;
    mc_world_storage_status status =
        mc_population_lake_place(&lake, w, &random, 8, 64, 8, &out, &e);
    w->fields.v = input->v;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          ls_failure(&e, w, side, true, 17) && e.air_bytecode == 4 &&
          mc_test_java_random_equal(&random, &expected) && ls_image_same(before, c));
    CHECK(ls_queues_read(w, &after) && ls_lights_same(&q, &after) &&
          ls_scheduled_same(&q, &after) && ls_sequence_delta(&q, &after, 0));
    ls_queues_free(&q);
    ls_queues_free(&after);

    /* NULL Random is source41 after the complete descent and native mask request. */
    out = true;
    CHECK(ls_queues_read(w, &q));
    status = mc_population_lake_place(&lake, w, NULL, 8, 64, 8, &out, &e);
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          ls_failure(&e, w, side, true, 41) && e.air_bytecode == 0 && ls_image_same(before, c) &&
          ls_queues_after(w, &q, true, 0));
    ls_queues_free(&q);
    /* A real host FP refusal occurs AFTER nextInt4 and BEFORE nextDouble59. */
    ls_random(&random, 18);
    expected = random;
    CHECK(ls_lake_budget(&expected, false));
    CHECK(ls_queues_read(w, &q));
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr ^ 0x2000u);
    status = mc_population_lake_place(&lake, w, &random, 8, 64, 8, &out, &e);
    _mm_setcsr(csr);
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          ls_failure(&e, w, side, true, 59) && mc_test_java_random_equal(&random, &expected) &&
          ls_image_same(before, c) && ls_queues_after(w, &q, true, 0));
    ls_queues_free(&q);

    /* Whole validation rejections: upper liquid; lower air mismatch; complete
     * descent through air to0; initially nonpositive Y. No writes may precede
     * the completed validation. Their successful diagnostics must be fresh. */
    for (unsigned int trial = 0; trial < 5; ++trial) {
        ls_terrain(c, w, trial == 0 ? stone : 0);
        if (trial == 0)
            for (int32_t x = 0; x < 16; ++x)
                for (int32_t z = 0; z < 16; ++z)
                    for (int32_t y = 64; y < 68; ++y)
                        c->data->blocks[ls_index(x, y, z)] = (uint8_t)water;
        if (trial == 1)
            c->data->blocks[ls_index(0, 64, 0)] = (uint8_t)stone;
        ls_image_read(before, c);
        ls_random(&random, (int64_t)trial + 31);
        expected = random;
        CHECK(ls_lake_budget(&expected, true) && ls_queues_read(w, &q));
        out = true;
        const int32_t y = trial < 3 ? 64 : trial == 3 ? 0 : -19;
        CHECK(mc_population_lake_place(&lake, w, &random, 8, y, 8, &out, &e) ==
                  MC_WORLD_STORAGE_OK &&
              !out && ls_cleared(&e) && mc_test_java_random_equal(&random, &expected) &&
              ls_image_same(before, c) && !w->fields.a && !w->fields.o);
        CHECK(ls_queues_read(w, &after) && ls_lights_same(&q, &after) &&
              ls_scheduled_same(&q, &after) && ls_sequence_delta(&q, &after, 0));
        ls_queues_free(&q);
        ls_queues_free(&after);
    }

    /* Genuine successful whole water bodies. The last case starts with target
     * liquid in the lower region: non-solid matching target is admitted, and
     * equal quiet setters retain its old metadata rather than being errors. */
    const int64_t seeds[] = {0, 1, 17, 99, 23};
    bool grass_seen = false;
    int64_t grass_seed = 0;
    for (size_t trial = 0; trial < COUNT(seeds); ++trial) {
        ls_terrain(c, w, trial == 4 ? stone : dirt);
        if (trial == 4)
            for (int32_t x = 0; x < 16; ++x)
                for (int32_t z = 0; z < 16; ++z)
                    for (int32_t y = 60; y < 64; ++y)
                        c->data->blocks[ls_index(x, y, z)] = (uint8_t)water;
        ls_image_read(before, c);
        ls_random(&random, seeds[trial]);
        expected = random;
        world_random = *w->fields.r;
        profile = ls_profile_read(f->profile);
        CHECK(ls_lake_budget(&expected, true) && ls_queues_read(w, &q));
        out = false;
        CHECK(mc_population_lake_place(&lake, w, &random, 8, 64, 8, &out, &e) ==
                  MC_WORLD_STORAGE_OK &&
              out && ls_cleared(&e) && mc_test_java_random_equal(&random, &expected) &&
              mc_test_java_random_equal(w->fields.r, &world_random) &&
              ls_profile_same(f->profile, &profile));
        ls_counts n;
        CHECK(ls_lake_image(before, c, water, grass, stone, false, &n) && n.air != 0 &&
              (trial == 4 || n.liquid != 0) && mc_world_chunk_state(c)->dirty);
        if (n.grass != 0 && !grass_seen) {
            grass_seen = true;
            grass_seed = seeds[trial];
        }
        CHECK(ls_queues_read(w, &after) && ls_scheduled_same(&q, &after) &&
              ls_sequence_delta(&q, &after, 0) && !w->fields.a && !w->fields.o &&
              w->active_calls == 0 && w->local_leases == 0 && w->classes->enqueue_depth == 0);
        ls_queues_free(&q);
        ls_queues_free(&after);
    }
    CHECK(grass_seen); /* Fixed ordinary input set must actually reach saved-sky/grass. */

    /* Quiet placement must actually complete with nullable observers when no
     * column height changes. A notifying substitute would fail real size9.
     * Saved-sky0 independently skips the late v load, even when v is NULL. */
    for (unsigned int dark = 0; dark < 2; ++dark) {
        ls_terrain(c, w, dark == 0 ? stone : dirt);
        if (dark != 0)
            memset(c->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
        ls_image_read(before, c);
        ls_random(&random, 17);
        expected = random;
        CHECK(ls_lake_budget(&expected, true) && ls_queues_read(w, &q));
        mc_block **cell = ls_named(r, "v");
        CHECK(cell != NULL);
        mc_block *block = *cell;
        if (dark == 0)
            w->fields.u = NULL;
        else
            *cell = NULL;
        out = false;
        status = mc_population_lake_place(&lake, w, &random, 8, 64, 8, &out, &e);
        w->fields.u = input->u;
        *cell = block;
        ls_counts n;
        CHECK(status == MC_WORLD_STORAGE_OK && out && ls_cleared(&e) &&
              ls_lake_image(before, c, water, grass, stone, false, &n) && n.air != 0 &&
              n.liquid != 0 && n.grass == 0 && mc_test_java_random_equal(&random, &expected) &&
              ls_queues_after(w, &q, false, 0));
        ls_queues_free(&q);
    }

    /* Discover only an input seed that ACTUALLY reached normal grass above;
     * repeat that full input with the actual named v cell NULL. No mask is
     * copied, no feature draw is skipped and no queue effect is rewound. */
    ls_terrain(c, w, dirt);
    ls_image_read(before, c);
    ls_random(&random, grass_seed);
    expected = random;
    CHECK(ls_lake_budget(&expected, true));
    mc_block **grass_cell = ls_named(r, "v");
    CHECK(grass_cell != NULL && ls_queues_read(w, &q));
    mc_block *grass_block = *grass_cell;
    *grass_cell = NULL;
    out = true;
    status = mc_population_lake_place(&lake, w, &random, 8, 64, 8, &out, &e);
    *grass_cell = grass_block;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          ls_failure(&e, w, side, true, 872) && e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          mc_test_java_random_equal(&random, &expected));
    ls_counts partial;
    CHECK(ls_lake_image(before, c, water, grass, stone, false, &partial) && partial.air != 0 &&
          partial.liquid != 0 && partial.grass == 0 && ls_queues_after(w, &q, false, 0));
    ls_queues_free(&q);

    /* NULL dirt comparison is unconditionally reached for the first upper
     * carved cell, even when the actual below block would not match dirt. */
    ls_terrain(c, w, stone);
    ls_image_read(before, c);
    ls_random(&random, 17);
    expected = random;
    CHECK(ls_lake_budget(&expected, true) && ls_queues_read(w, &q));
    mc_block **dirt_cell = ls_named(r, "w");
    CHECK(dirt_cell != NULL);
    mc_block *dirt_block = *dirt_cell;
    *dirt_cell = NULL;
    out = true;
    status = mc_population_lake_place(&lake, w, &random, 8, 64, 8, &out, &e);
    *dirt_cell = dirt_block;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          ls_failure(&e, w, side, true, 822) && e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          mc_test_java_random_equal(&random, &expected));
    CHECK(ls_lake_image(before, c, water, grass, stone, false, &partial) && partial.air != 0 &&
          partial.liquid != 0 && partial.grass == 0 && ls_queues_after(w, &q, false, 0));
    ls_queues_free(&q);

    /* First real quiet mutation stores its block BEFORE NULL metadata fails.
     * Exactly one changed voxel; all three packed arrays and heights retain
     * the prepared input. No observer/lighting/scheduler substitute is used. */
    ls_terrain(c, w, stone);
    ls_image_read(before, c);
    ls_random(&random, 19);
    expected = random;
    CHECK(ls_lake_budget(&expected, true) && ls_queues_read(w, &q));
    c->e = NULL;
    out = true;
    status = mc_population_lake_place(&lake, w, &random, 8, 64, 8, &out, &e);
    c->e = input->e;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          ls_failure(&e, w, side, true, 727) && e.mutation.bytecode == 66 &&
          e.mutation.chunk_bytecode == 128 && same_cause(&e.cause, &e.mutation.cause) &&
          mc_test_java_random_equal(&random, &expected));
    size_t changed = 0;
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        changed += before->blocks[i] != c->data->blocks[i];
    CHECK(changed == 1 && ls_support_same(before, c) &&
          memcmp(before->data.metadata, c->data->metadata, MC_CHUNK_NIBBLE_BYTES) == 0);
    CHECK(ls_queues_read(w, &after) && ls_lights_same(&q, &after) && ls_scheduled_same(&q, &after));
    ls_queues_free(&q);
    ls_queues_free(&after);

    /* Air is a legal constructor int; after full filling/grass work its real
     * m[0] NULL target receiver fails at905. This is a late failure test. */
    ls_terrain(c, w, stone);
    ls_image_read(before, c);
    ls_random(&random, 20);
    expected = random;
    CHECK(ls_lake_budget(&expected, true) &&
          mc_population_lake_feature_init(&lake, 0) == MC_WORLD_STORAGE_OK &&
          ls_queues_read(w, &q));
    out = true;
    CHECK(mc_population_lake_place(&lake, w, &random, 8, 64, 8, &out, &e) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          out && ls_failure(&e, w, side, true, 905) &&
          e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          mc_test_java_random_equal(&random, &expected) && ls_air_lake_image(before, c) &&
          ls_queues_after(w, &q, false, 0));
    ls_queues_free(&q);

    /* Full ordinary still-lava lake, actual grass and actual stone rim. Its
     * high-rim draw count is geometry-dependent: observe continuation, but do
     * not manufacture a second mask algorithm to claim an exact draw oracle. */
    CHECK(mc_population_lake_feature_init(&lake, lava) == MC_WORLD_STORAGE_OK);
    ls_terrain(c, w, dirt);
    ls_image_read(before, c);
    ls_random(&random, 17);
    expected = random;
    CHECK(ls_lake_budget(&expected, true) && ls_queues_read(w, &q));
    world_random = *w->fields.r;
    profile = ls_profile_read(f->profile);
    out = false;
    CHECK(mc_population_lake_place(&lake, w, &random, 8, 64, 8, &out, &e) == MC_WORLD_STORAGE_OK &&
          out && ls_cleared(&e) && mc_test_java_random_equal(w->fields.r, &world_random) &&
          ls_profile_same(f->profile, &profile));
    ls_counts lava_counts;
    CHECK(ls_lake_image(before, c, lava, grass, stone, true, &lava_counts) &&
          lava_counts.air != 0 && lava_counts.liquid != 0 && lava_counts.stone != 0 &&
          !mc_test_java_random_equal(&random, &expected) &&
          random.have_next_next_gaussian == expected.have_next_next_gaussian &&
          ls_bits(random.next_next_gaussian) == ls_bits(expected.next_next_gaussian));
    CHECK(ls_queues_read(w, &after) && ls_scheduled_same(&q, &after) &&
          ls_sequence_delta(&q, &after, 0));
    ls_queues_free(&q);
    ls_queues_free(&after);
    /* Late live u load belongs only to lava rim, after fill/grass. */
    ls_terrain(c, w, dirt);
    ls_image_read(before, c);
    ls_random(&random, 17);
    CHECK(ls_queues_read(w, &q));
    mc_block **stone_cell = ls_named(r, "u");
    CHECK(stone_cell != NULL);
    mc_block *stone_block = *stone_cell;
    *stone_cell = NULL;
    out = true;
    status = mc_population_lake_place(&lake, w, &random, 8, 64, 8, &out, &e);
    *stone_cell = stone_block;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          ls_failure(&e, w, side, true, 1200) && e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          !w->fields.a && !w->fields.o);
    CHECK(ls_lake_image(before, c, lava, grass, stone, true, &partial) && partial.air != 0 &&
          partial.liquid != 0 && ls_queues_after(w, &q, false, 0) &&
          mc_test_java_random_equal(w->fields.r, &initial_world_random));
    ok = true;
cleanup:
    ls_queues_free(&q);
    ls_queues_free(&after);
    if (saved)
        ls_inputs_restore(input, c, w, r);
    free(before);
    free(input);
    return ok;
}

static bool ls_new_record(const ls_queues *before, const ls_queues *after, int32_t x, int32_t block,
                          int64_t due)
{
    if (after->e != before->e + 1u || after->f != before->f + 1u)
        return false;
    size_t added_e = 0, added_f = 0, old_e = 0;
    const mc_scheduled_tick *added = NULL;
    for (size_t i = 0; i < after->e; ++i) {
        bool existed = false;
        for (size_t j = 0; j < before->e; ++j)
            if (after->ordered[i] == before->ordered[j]) {
                if (!ls_tick_same(&after->ordered_fields[i], &before->ordered_fields[j]))
                    return false;
                existed = true;
            }
        if (!existed) {
            const mc_scheduled_tick_fields *v = &after->ordered_fields[i];
            if (v->x != x || v->y != 64 || v->z != 8 || v->block != block || v->due_time != due ||
                v->sequence != (before->sequence.initialized ? before->sequence.next : 0))
                return false;
            added = after->ordered[i];
            ++added_e;
        } else {
            /* In-order inventory of every old occurrence remains a
             * subsequence, regardless of the tree's legitimate rebalancing. */
            if (old_e >= before->e || after->ordered[i] != before->ordered[old_e] ||
                !ls_tick_same(&after->ordered_fields[i], &before->ordered_fields[old_e]))
                return false;
            ++old_e;
        }
    }
    for (size_t i = 0; i < after->f; ++i) {
        bool existed = false;
        for (size_t j = 0; j < before->f; ++j)
            if (after->membership[i] == before->membership[j]) {
                if (!ls_tick_same(&after->membership_fields[i], &before->membership_fields[j]))
                    return false;
                existed = true;
            }
        if (!existed) {
            if (after->membership[i] != added)
                return false;
            ++added_f;
        }
    }
    for (size_t i = 0; i < before->f; ++i) {
        size_t occurrences = 0;
        for (size_t j = 0; j < after->f; ++j)
            occurrences += before->membership[i] == after->membership[j];
        if (occurrences != 1)
            return false;
    }
    return added_e == 1 && added_f == 1 && old_e == before->e;
}
static bool ls_springs(fixture *f, size_t side, mc_world_chunk *c, mc_world_runtime *w,
                       int32_t stone, int32_t dirt, int32_t grass, int32_t water, int32_t lava,
                       int32_t flowing_water, int32_t flowing_lava)
{
    bool ok = false, saved = false;
    ls_inputs *input = calloc(1, sizeof(*input));
    ls_image *before = malloc(sizeof(*before));
    ls_queues q = {0}, after = {0};
    mc_registry_classes *r = f->registry[side];
    mc_population_spring_feature spring;
    mc_population_lake_spring_error e;
    mc_java_random random, expected, world_random;
    ls_profile profile;
    bool out = true;
    CHECK(input != NULL && before != NULL && ls_inputs_read(input, c, w, r));
    saved = true;
    CHECK(mc_population_spring_feature_init(&spring, INT32_MAX) == MC_WORLD_STORAGE_OK &&
          spring.a == INT32_MAX);
    CHECK(mc_population_spring_feature_init(&spring, water) == MC_WORLD_STORAGE_OK &&
          spring.a == water &&
          mc_population_spring_feature_scale(&spring, NAN, -INFINITY, INFINITY) ==
              MC_WORLD_STORAGE_OK &&
          spring.a == water &&
          mc_population_spring_feature_init(NULL, INT32_MAX) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          mc_population_spring_feature_scale(NULL, 0, 0, 0) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    /* Above, below and center ordinary rejection; condition miss returns TRUE
     * with NULL supplied Random and without touching incoming a=true. */
    for (unsigned int trial = 0; trial < 4; ++trial) {
        ls_cavity(c, w, stone, 8);
        if (trial == 0)
            c->data->blocks[ls_index(8, 65, 8)] = 0;
        if (trial == 1)
            c->data->blocks[ls_index(8, 63, 8)] = 0;
        if (trial == 2)
            c->data->blocks[ls_index(8, 64, 8)] = (uint8_t)dirt;
        if (trial == 3) {
            c->data->blocks[ls_index(9, 64, 8)] = (uint8_t)stone;
            w->fields.a = true;
        }
        ls_image_read(before, c);
        CHECK(ls_queues_read(w, &q));
        out = trial != 3;
        CHECK(mc_population_spring_place(&spring, w, NULL, 8, 64, 8, &out, &e) ==
                  MC_WORLD_STORAGE_OK &&
              out == (trial == 3) && ls_cleared(&e) && ls_image_same(before, c) &&
              w->fields.a == (trial == 3));
        CHECK(ls_queues_read(w, &after) && ls_lights_same(&q, &after) &&
              ls_scheduled_same(&q, &after) && ls_sequence_delta(&q, &after, 0));
        ls_queues_free(&q);
        ls_queues_free(&after);
    }
    /* Wrapped Y+1 reaches the ordinary out-of-height air result before any
     * provider access; use a real nullable v input, restored before CHECK. */
    ls_terrain(c, w, stone);
    ls_image_read(before, c);
    CHECK(ls_queues_read(w, &q));
    w->fields.v = NULL;
    out = true;
    mc_world_storage_status status =
        mc_population_spring_place(&spring, w, NULL, 8, INT32_MAX, 8, &out, &e);
    w->fields.v = input->v;
    CHECK(status == MC_WORLD_STORAGE_OK && !out && ls_cleared(&e) && ls_image_same(before, c) &&
          ls_queues_after(w, &q, true, 0));
    ls_queues_free(&q);
    ls_cavity(c, w, stone, 8);
    ls_image_read(before, c);
    CHECK(ls_queues_read(w, &q));
    mc_block **stone_cell = ls_named(r, "u");
    CHECK(stone_cell != NULL);
    mc_block *stone_block = *stone_cell;
    *stone_cell = NULL;
    out = true;
    status = mc_population_spring_place(&spring, w, NULL, 8, 64, 8, &out, &e);
    *stone_cell = stone_block;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          ls_failure(&e, w, side, false, 14) && e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          ls_image_same(before, c) && ls_queues_after(w, &q, true, 0));
    ls_queues_free(&q);

    /* Actual still-water tick is concrete and consumes no Random. Successful
     * spring clears even an incoming true a at293. */
    for (unsigned int incoming = 0; incoming < 2; ++incoming) {
        ls_cavity(c, w, stone, 8);
        w->fields.a = incoming != 0;
        if (incoming == 0)
            c->data->blocks[ls_index(8, 64, 8)] = 0;
        ls_image_read(before, c);
        ls_random(&random, 70);
        expected = random;
        world_random = *w->fields.r;
        profile = ls_profile_read(f->profile);
        CHECK(ls_queues_read(w, &q));
        out = false;
        CHECK(mc_population_spring_place(&spring, w, &random, 8, 64, 8, &out, &e) ==
                  MC_WORLD_STORAGE_OK &&
              out && ls_cleared(&e) && ls_spring_image(before, c, 8, water, -1, 0) &&
              !w->fields.a && !w->fields.o && mc_test_java_random_equal(&random, &expected) &&
              mc_test_java_random_equal(w->fields.r, &world_random) &&
              ls_profile_same(f->profile, &profile));
        CHECK(ls_queues_read(w, &after) && ls_scheduled_same(&q, &after) &&
              ls_sequence_delta(&q, &after, 0));
        ls_queues_free(&q);
        ls_queues_free(&after);
    }
    /* Exact ignored ordinary false264: selected stone already occupies the
     * center. NULL observer input proves the setter did not notify; the real
     * inherited stone tick is empty, with no substitute dispatcher. */
    CHECK(mc_population_spring_feature_init(&spring, stone) == MC_WORLD_STORAGE_OK);
    ls_cavity(c, w, stone, 8);
    ls_image_read(before, c);
    ls_random(&random, 71);
    expected = random;
    CHECK(ls_queues_read(w, &q));
    w->fields.u = NULL;
    out = false;
    status = mc_population_spring_place(&spring, w, &random, 8, 64, 8, &out, &e);
    w->fields.u = input->u;
    CHECK(status == MC_WORLD_STORAGE_OK && out && ls_cleared(&e) && ls_image_same(before, c) &&
          !w->fields.a && !w->fields.o && mc_test_java_random_equal(&random, &expected) &&
          ls_queues_after(w, &q, true, 0));
    ls_queues_free(&q);
    /* Independently choose ordinary seeded streams for BOTH actual lava tick
     * count branches. A nonzero count performs its first two walk draws then
     * returns at the genuine solid ceiling, without ignition substitution. */
    CHECK(mc_population_spring_feature_init(&spring, lava) == MC_WORLD_STORAGE_OK);
    for (unsigned int branch = 0; branch < 2; ++branch) {
        bool found = false;
        int64_t seed = 0;
        int32_t count = -1;
        for (; seed < 64; ++seed) {
            ls_random(&expected, seed);
            CHECK(ls_lava_budget(&expected, &count));
            if ((count != 0) == (branch != 0)) {
                found = true;
                break;
            }
        }
        CHECK(found);
        ls_cavity(c, w, stone, 8);
        ls_image_read(before, c);
        ls_random(&random, seed);
        world_random = *w->fields.r;
        profile = ls_profile_read(f->profile);
        CHECK(ls_queues_read(w, &q));
        out = false;
        CHECK(mc_population_spring_place(&spring, w, &random, 8, 64, 8, &out, &e) ==
                  MC_WORLD_STORAGE_OK &&
              out && ls_cleared(&e) && ls_spring_image(before, c, 8, lava, -1, 0) &&
              mc_test_java_random_equal(&random, &expected) &&
              mc_test_java_random_equal(w->fields.r, &world_random) &&
              ls_profile_same(f->profile, &profile) && !w->fields.a && !w->fields.o);
        CHECK(ls_queues_read(w, &after) && ls_scheduled_same(&q, &after) &&
              ls_sequence_delta(&q, &after, 0));
        ls_queues_free(&q);
        ls_queues_free(&after);
    }
    /* Genuine post270 failures: concrete still-lava NULL Random13; genuinely
     * unsupported grass tick entry; natural m[0] NULL tick receiver. */
    for (unsigned int trial = 0; trial < 3; ++trial) {
        const int32_t target = trial == 0 ? lava : trial == 1 ? grass : 0;
        CHECK(mc_population_spring_feature_init(&spring, target) == MC_WORLD_STORAGE_OK);
        ls_cavity(c, w, stone, 8);
        ls_image_read(before, c);
        ls_random(&random, 75);
        expected = random;
        world_random = *w->fields.r;
        profile = ls_profile_read(f->profile);
        CHECK(ls_queues_read(w, &q));
        out = true;
        status =
            mc_population_spring_place(&spring, w, trial == 0 ? NULL : &random, 8, 64, 8, &out, &e);
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
              ls_failure(&e, w, side, false, 288) && w->fields.a && !w->fields.o &&
              ls_spring_image(before, c, 8, target, -1, 0) &&
              mc_test_java_random_equal(&random, &expected) &&
              mc_test_java_random_equal(w->fields.r, &world_random) &&
              ls_profile_same(f->profile, &profile) && same_cause(&e.cause, &e.mutation.cause));
        if (trial == 0)
            CHECK(e.mutation.hook_class != NULL &&
                  strcmp(e.mutation.hook_class, side == 0 ? "nx" : "is") == 0 &&
                  e.mutation.hook_bytecode == 13);
        if (trial == 1)
            CHECK(e.mutation.hook_class != NULL &&
                  strcmp(e.mutation.hook_class, side == 0 ? "wp" : "oj") == 0 &&
                  e.mutation.hook_bytecode == 0);
        if (trial == 2)
            CHECK(e.mutation.hook_class == NULL && e.mutation.hook_bytecode == 0);
        CHECK(ls_queues_read(w, &after) && ls_scheduled_same(&q, &after) &&
              ls_sequence_delta(&q, &after, 0));
        ls_queues_free(&q);
        ls_queues_free(&after);
    }

    /* Notify264 fails only after the actual quiet store/metadata/lighting.
     * NULL observer-list input exercises real size9; it is not a callback. */
    CHECK(mc_population_spring_feature_init(&spring, water) == MC_WORLD_STORAGE_OK);
    ls_cavity(c, w, stone, 8);
    ls_image_read(before, c);
    ls_random(&random, 76);
    expected = random;
    CHECK(ls_queues_read(w, &q));
    w->fields.u = NULL;
    out = true;
    status = mc_population_spring_place(&spring, w, &random, 8, 64, 8, &out, &e);
    w->fields.u = input->u;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          ls_failure(&e, w, side, false, 264) && e.mutation.wrapper_bytecode == 18 &&
          e.mutation.observer_bytecode == 9 && !w->fields.a &&
          ls_spring_image(before, c, 8, water, -1, 0) &&
          mc_test_java_random_equal(&random, &expected) && ls_queues_after(w, &q, false, 0));
    ls_queues_free(&q);

    /* Actual absent radius keeps initial deferred and later immediate records
     * out of E/F, but every constructor still consumes sequence. */
    mc_chunk_state *absent = NULL;
    CHECK(!mc_chunk_cache_lookup(f->providers[side].cache, 0, 1, &absent));
    for (unsigned int liquid = 0; liquid < 2; ++liquid) {
        const int32_t flowing = liquid == 0 ? flowing_water : flowing_lava;
        ls_cavity(c, w, stone, 8);
        ls_image_read(before, c);
        ls_random(&random, 80);
        expected = random;
        world_random = *w->fields.r;
        profile = ls_profile_read(f->profile);
        CHECK(mc_population_spring_feature_init(&spring, flowing) == MC_WORLD_STORAGE_OK &&
              ls_queues_read(w, &q));
        const mc_block *block = r->blocks.m[flowing];
        CHECK(block != NULL && block->kind == MC_BLOCK_OM_JC && block->subtype.om.b != NULL &&
              block->subtype.om.c != NULL);
        const int32_t old_sources = block->subtype.om.a;
        out = false;
        CHECK(mc_population_spring_place(&spring, w, &random, 8, 64, 8, &out, &e) ==
                  MC_WORLD_STORAGE_OK &&
              out && ls_cleared(&e) &&
              ls_spring_image(before, c, 8, flowing, flowing, liquid == 0 ? 1u : 2u) &&
              mc_test_java_random_equal(&random, &expected) &&
              mc_test_java_random_equal(w->fields.r, &world_random) &&
              ls_profile_same(f->profile, &profile) && !w->fields.a && !w->fields.o &&
              block->subtype.om.a == old_sources);
        for (size_t i = 0; i < 4; ++i)
            CHECK(block->subtype.om.b[i] && block->subtype.om.c[i] == 1000);
        CHECK(ls_queues_read(w, &after) && ls_scheduled_same(&q, &after) &&
              ls_sequence_delta(&q, &after, 4));
        ls_queues_free(&q);
        ls_queues_free(&after);
    }

    /* Real cache publications, no counter edits. Existing normal(1,0) and
     * normal(0,0), together with these actual Blank owners, satisfy radius8.
     * Publications remain in provider history and later capacity tests adapt. */
    CHECK(mc_chunk_cache_publish(f->providers[side].cache, 0, 1, f->providers[side].fallback) &&
          mc_chunk_cache_publish(f->providers[side].cache, 1, 1, f->providers[side].fallback) &&
          mc_chunk_cache_validate(f->providers[side].cache));
    for (unsigned int liquid = 0; liquid < 2; ++liquid) {
        const int32_t x = liquid == 0 ? 8 : 12;
        const int32_t flowing = liquid == 0 ? flowing_water : flowing_lava;
        const int32_t still = liquid == 0 ? water : lava;
        ls_cavity(c, w, stone, x);
        ls_image_read(before, c);
        ls_random(&random, 81);
        expected = random;
        world_random = *w->fields.r;
        profile = ls_profile_read(f->profile);
        CHECK(mc_population_spring_feature_init(&spring, flowing) == MC_WORLD_STORAGE_OK &&
              ls_queues_read(w, &q));
        int64_t time;
        CHECK(mc_world_info_time_get(w->fields.x, &time) == MC_WORLD_INFO_OK);
        out = false;
        CHECK(mc_population_spring_place(&spring, w, &random, x, 64, 8, &out, &e) ==
                  MC_WORLD_STORAGE_OK &&
              out && ls_cleared(&e) &&
              ls_spring_image(before, c, x, still, still, liquid == 0 ? 1u : 2u) &&
              mc_test_java_random_equal(&random, &expected) &&
              mc_test_java_random_equal(w->fields.r, &world_random) &&
              ls_profile_same(f->profile, &profile) && !w->fields.a && !w->fields.o);
        const mc_block *block = r->blocks.m[flowing];
        CHECK(block->subtype.om.a == 1);
        for (size_t i = 0; i < 4; ++i)
            CHECK(block->subtype.om.b[i] && block->subtype.om.c[i] == 1000);
        CHECK(ls_queues_read(w, &after) && ls_sequence_delta(&q, &after, 4) &&
              ls_new_record(&q, &after, x, flowing, mc_java_i64_add(time, liquid == 0 ? 5 : 30)));
        ls_queues_free(&q);
        ls_queues_free(&after);
    }

    /* The repeated D key is already in F, so initial deferred containment
     * keeps that SAME queued owner. The nested added tick settles the center
     * to E before explicit still→flow scheduling requests D again. Scheduler
     * comparison108 rejects that mismatch before its World.r read139. A real
     * NULL World.r therefore succeeds here, and both a/o flags clear. */
    ls_cavity(c, w, stone, 12);
    ls_image_read(before, c);
    ls_random(&random, 82);
    expected = random;
    world_random = *w->fields.r;
    CHECK(mc_population_spring_feature_init(&spring, flowing_lava) == MC_WORLD_STORAGE_OK &&
          ls_queues_read(w, &q));
    w->fields.r = NULL;
    out = true;
    status = mc_population_spring_place(&spring, w, &random, 12, 64, 8, &out, &e);
    w->fields.r = input->r;
    CHECK(status == MC_WORLD_STORAGE_OK && out && ls_cleared(&e) && !w->fields.a && !w->fields.o &&
          ls_spring_image(before, c, 12, lava, lava, 2) &&
          mc_test_java_random_equal(&random, &expected) &&
          mc_test_java_random_equal(w->fields.r, &world_random));
    CHECK(ls_queues_read(w, &after) && ls_scheduled_same(&q, &after) &&
          ls_sequence_delta(&q, &after, 4));
    ls_queues_free(&q);
    ls_queues_free(&after);

    /* A third prepared still-lava cell reaches a genuine late Random failure.
     * Side notification converts center first: its added tick settles it and
     * its explicit request skips the changed ID. The east still cell then
     * converts to flowing level2; its added immediate tick derives candidate4
     * from side level2 and reaches nextInt4 at289 on the actual NULL World.r.
     * Every earlier write/queue effect remains, including both a/o=true. */
    ls_cavity(c, w, stone, 12);
    c->data->blocks[ls_index(14, 64, 8)] = (uint8_t)lava;
    CHECK(mc_chunk_nibbles_set(c->data->metadata, MC_CHUNK_NIBBLE_BYTES, 14, 64, 8, 2));
    ls_image_read(before, c);
    ls_random(&random, 84);
    expected = random;
    world_random = *w->fields.r;
    profile = ls_profile_read(f->profile);
    CHECK(mc_population_spring_feature_init(&spring, flowing_lava) == MC_WORLD_STORAGE_OK &&
          ls_queues_read(w, &q));
    w->fields.r = NULL;
    out = true;
    status = mc_population_spring_place(&spring, w, &random, 12, 64, 8, &out, &e);
    w->fields.r = input->r;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          ls_failure(&e, w, side, false, 288) && w->fields.a && w->fields.o &&
          ls_spring_image_extra(before, c, 12, lava, lava, 2, flowing_lava, 2) &&
          e.mutation.hook_class != NULL &&
          strcmp(e.mutation.hook_class, side == 0 ? "om" : "jc") == 0 &&
          e.mutation.hook_bytecode == 559 && e.mutation.schedule_bytecode == 142 &&
          e.mutation.cause_hook_class != NULL &&
          strcmp(e.mutation.cause_hook_class, side == 0 ? "om" : "jc") == 0 &&
          e.mutation.cause_hook_bytecode == 289 && same_cause(&e.cause, &e.mutation.cause) &&
          mc_test_java_random_equal(&random, &expected) &&
          mc_test_java_random_equal(w->fields.r, &world_random) &&
          ls_profile_same(f->profile, &profile));
    CHECK(ls_queues_read(w, &after) && ls_scheduled_same(&q, &after) &&
          ls_sequence_delta(&q, &after, 5));
    ls_queues_free(&q);
    ls_queues_free(&after);

    /* Concrete scheduler receiver failure before spring270. Use an actual
     * nullable F field, restore it before capture; never forge membership. */
    ls_cavity(c, w, stone, 10);
    ls_image_read(before, c);
    ls_random(&random, 83);
    expected = random;
    CHECK(mc_population_spring_feature_init(&spring, flowing_water) == MC_WORLD_STORAGE_OK &&
          ls_queues_read(w, &q));
    mc_scheduled_membership *membership = w->fields.F;
    w->fields.F = NULL;
    out = true;
    status = mc_population_spring_place(&spring, w, &random, 10, 64, 8, &out, &e);
    w->fields.F = membership;
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out &&
          ls_failure(&e, w, side, false, 264) && !w->fields.a && !w->fields.o &&
          ls_spring_image(before, c, 10, flowing_water, -1, 0) &&
          e.mutation.schedule_bytecode == 205 &&
          e.mutation.membership == MC_SCHEDULED_MEMBERSHIP_INVALID_ARGUMENT &&
          mc_test_java_random_equal(&random, &expected));
    CHECK(ls_queues_read(w, &after) && ls_scheduled_same(&q, &after) &&
          ls_sequence_delta(&q, &after, 1));
    ok = true;
cleanup:
    ls_queues_free(&q);
    ls_queues_free(&after);
    if (saved)
        ls_inputs_restore(input, c, w, r);
    free(before);
    free(input);
    return ok;
}

static bool population_lake_spring_contracts(fixture *f, size_t side)
{
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_world_chunk *c = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    int32_t stone, dirt, grass, water, lava, flowing_water, flowing_lava;
    CHECK(w != NULL && r != NULL && c != NULL && c->d == w && w->active_calls == 0 &&
          w->local_leases == 0 && w->fields.u != NULL && w->fields.u->count == 0 &&
          f->resources[side].opens == 3 && !f->resources[side].bad);
    CHECK(ls_id(r, "u", &stone) && ls_id(r, "w", &dirt) && ls_id(r, "v", &grass) &&
          ls_id(r, "C", &water) && ls_id(r, "E", &lava) && ls_id(r, "B", &flowing_water) &&
          ls_id(r, "D", &flowing_lava));
    const ls_profile profile = ls_profile_read(f->profile);
    const size_t bindings = w->persistent_bindings;
    CHECK(ls_lakes(f, side, c, w, stone, dirt, grass, water, lava));
    CHECK(ls_springs(f, side, c, w, stone, dirt, grass, water, lava, flowing_water, flowing_lava));
    CHECK(w->active_calls == 0 && w->local_leases == 0 && w->persistent_bindings == bindings &&
          w->classes->enqueue_depth == 0 && ls_profile_same(f->profile, &profile) &&
          remember(f, side));
    ok = true;
cleanup:
    return ok;
}
