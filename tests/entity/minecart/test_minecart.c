/* Source-derived constructor/class contracts. No gameplay observations. */
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/minecart/classes_internal.h"
#include "../../../src/entity/minecart/entity_internal.h"
#include "../../../src/world/runtime/construction_internal.h"
#include "../../../src/world/runtime/runtime_internal.h"
#include "../../support/java_random.h"
#include "mc/entity/minecart/minecart.h"

#include <emmintrin.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static unsigned int checks, cases;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        const bool ok_ = !!(condition);                                                            \
        if (!ok_) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

/* These field/bit/profile helpers retain the existing Entity fixture's literal
 * base expectations. Only this file's main and its own groups are compiled. */
#define BASE_INTS(X)                                                                               \
    X(id);                                                                                         \
    X(private_b);                                                                                  \
    X(bt);                                                                                         \
    X(bu);                                                                                         \
    X(bv);                                                                                         \
    X(bw);                                                                                         \
    X(by);                                                                                         \
    X(bz);                                                                                         \
    X(bG);                                                                                         \
    X(bH);                                                                                         \
    X(bI);                                                                                         \
    X(client_only.bJ);                                                                             \
    X(client_only.bK);                                                                             \
    X(client_only.bL);
#define BASE_DOUBLES(X)                                                                            \
    X(aE);                                                                                         \
    X(aJ);                                                                                         \
    X(aK);                                                                                         \
    X(aL);                                                                                         \
    X(x);                                                                                          \
    X(y);                                                                                          \
    X(z);                                                                                          \
    X(velocity_x);                                                                                 \
    X(velocity_y);                                                                                 \
    X(velocity_z);                                                                                 \
    X(bl);                                                                                         \
    X(bm);                                                                                         \
    X(bn);                                                                                         \
    X(private_d);                                                                                  \
    X(private_e);
#define BASE_FLOATS(X)                                                                             \
    X(yaw);                                                                                        \
    X(aT);                                                                                         \
    X(aU);                                                                                         \
    X(aV);                                                                                         \
    X(offset);                                                                                     \
    X(width);                                                                                      \
    X(height);                                                                                     \
    X(bi);                                                                                         \
    X(bj);                                                                                         \
    X(bk);                                                                                         \
    X(y_size);                                                                                     \
    X(bp);                                                                                         \
    X(br);                                                                                         \
    X(bE);
#define BASE_BOOLS(X)                                                                              \
    X(aF);                                                                                         \
    X(aX);                                                                                         \
    X(aY);                                                                                         \
    X(aZ);                                                                                         \
    X(ba);                                                                                         \
    X(bb);                                                                                         \
    X(bc);                                                                                         \
    X(bd);                                                                                         \
    X(be);                                                                                         \
    X(bq);                                                                                         \
    X(bx);                                                                                         \
    X(private_c);                                                                                  \
    X(bC);                                                                                         \
    X(bF);                                                                                         \
    X(bM);
#define BASE_REFS(X)                                                                               \
    X(aG);                                                                                         \
    X(aH);                                                                                         \
    X(world);                                                                                      \
    X(client_only.bA);                                                                             \
    X(client_only.bB);

static uint64_t double_bits(double value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary32 required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool base_equal(const mc_entity_base_fields *left, const mc_entity_base_fields *right) {
#define EQUAL_SCALAR(field)                                                                        \
    if (left->field != right->field)                                                               \
    return false
#define EQUAL_DOUBLE(field)                                                                        \
    if (double_bits(left->field) != double_bits(right->field))                                     \
    return false
#define EQUAL_FLOAT(field)                                                                         \
    if (float_bits(left->field) != float_bits(right->field))                                       \
    return false
    BASE_INTS(EQUAL_SCALAR)
    BASE_DOUBLES(EQUAL_DOUBLE)
    BASE_FLOATS(EQUAL_FLOAT)
    BASE_BOOLS(EQUAL_SCALAR)
    BASE_REFS(EQUAL_SCALAR)
#undef EQUAL_SCALAR
#undef EQUAL_DOUBLE
#undef EQUAL_FLOAT
    return true;
}

static bool bounds_equal(const mc_entity_bounds *left, const mc_entity_bounds *right) {
    return double_bits(left->min_x) == double_bits(right->min_x) &&
           double_bits(left->min_y) == double_bits(right->min_y) &&
           double_bits(left->min_z) == double_bits(right->min_z) &&
           double_bits(left->max_x) == double_bits(right->max_x) &&
           double_bits(left->max_y) == double_bits(right->max_y) &&
           double_bits(left->max_z) == double_bits(right->max_z);
}

static mc_entity_base_fields base_literals(int32_t id) {
    /* Original complete base constructor scalar state; every omitted field is
     * an independently required raw zero/null, including all five client slots. */
    const mc_entity_base_fields value = {.id = id,
                                         .aE = 1.0,
                                         .bd = true,
                                         .width = 0.6f,
                                         .height = 1.8f,
                                         .private_b = 1,
                                         .bu = 1,
                                         .bw = 300,
                                         .bz = 300,
                                         .private_c = true};
    return value;
}

static bool fresh_random(const mc_java_random *value) {
    return value != NULL && value->state <= UINT64_C(0xffffffffffff) &&
           double_bits(value->next_next_gaussian) == 0 && !value->have_next_next_gaussian;
}

static size_t profile_budget(void) {
    return sizeof(mc_java_random_profile) + sizeof(mc_java_math_random_owner);
}

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static uint64_t advanced(uint64_t before, unsigned int count) {
    for (unsigned int i = 0; i < count; ++i)
        before *= UINT64_C(1181783497276652981);
    return before;
}

static bool holder_is(const mc_java_random_profile *profile, mc_java_random_holder_phase phase) {
    return atomic_load_explicit(&profile->holder_phase, memory_order_acquire) ==
           (unsigned int)phase;
}

static bool watcher_is(const mc_entity_snapshot *value, bool installed) {
    mc_entity_watcher_snapshot watch = {0};
    if (value->watcher == NULL ||
        mc_entity_watcher_get_snapshot(value->watcher, &watch) != MC_ENTITY_OK)
        return false;
    if (watch.dirty || watch.size != (installed ? 1u : 0u))
        return false;
    return installed ? watch.record != NULL && watch.index == 0 && watch.type == 0 &&
                           watch.byte_value == 0 && watch.record_dirty
                     : watch.record == NULL;
}

static int retire_classes(mc_entity_classes **classes) {
    const mc_entity_status status = mc_entity_classes_free(*classes);
    if (status != MC_ENTITY_OK) {
        fprintf(stderr, "class cleanup: %s\n", mc_entity_status_string(status));
        return 1;
    }
    *classes = NULL;
    return 0;
}

static const int32_t shapes[10][2][3] = {{{0, 0, -1}, {0, 0, 1}},  {{-1, 0, 0}, {1, 0, 0}},
                                         {{-1, -1, 0}, {1, 0, 0}}, {{-1, 0, 0}, {1, -1, 0}},
                                         {{0, 0, -1}, {0, -1, 1}}, {{0, -1, -1}, {0, 0, 1}},
                                         {{0, 0, 1}, {1, 0, 0}},   {{0, 0, 1}, {-1, 0, 0}},
                                         {{0, 0, -1}, {-1, 0, 0}}, {{0, 0, -1}, {1, 0, 0}}};
static const uint16_t allocation_pcs[31] = {2,   8,   14,  32,  51,  57,  75,  94,  100, 118, 137,
                                            143, 161, 180, 186, 204, 223, 229, 247, 267, 273, 291,
                                            311, 317, 335, 355, 361, 379, 399, 405, 423};
static const uint16_t previous_pcs[31] = {0,   2,   8,   28,  47,  51,  71,  90,  94,  114, 133,
                                          137, 157, 176, 180, 200, 219, 223, 243, 262, 267, 287,
                                          306, 311, 331, 350, 355, 375, 394, 399, 419};
_Static_assert(MC_ENTITY_CLASS_KIND_MINECART == 22 && MC_ENTITY_CLASS_KIND_BOAT == 23 &&
                   MC_ENTITY_CLASS_KIND_ENTITY == 24 && MC_ENTITY_CLASS_KIND_PLAYER == 25 &&
                   MC_ENTITY_CLASS_KIND_LOCAL_PLAYER == 26 && MC_ENTITY_CLASS_KIND_PARTICLE == 27 &&
                   MC_ENTITY_CLASS_KIND_PICKUP_PARTICLE == 28 && MC_ENTITY_CLASS_KIND_VV == 29 &&
                   MC_ENTITY_CLASS_KIND_CF == 30 && MC_ENTITY_CLASS_KIND_LX == 31 &&
                   MC_ENTITY_CLASS_KIND_C == 32 && MC_ENTITY_CLASS_KIND_COUNT == 33,
               "canonical selectors stay stable when appending client renderer classes");
_Static_assert(COUNT(shapes) == 10 && COUNT(allocation_pcs) == 31 && COUNT(previous_pcs) == 31,
               "complete literal Minecart initializer");

static size_t request_size(size_t index) {
    if (index == 0)
        return sizeof(mc_minecart_shape_array) + 10u * sizeof(mc_minecart_row_array *);
    if ((index - 1u) % 3u == 0)
        return sizeof(mc_minecart_row_array) + 2u * sizeof(mc_minecart_int_array *);
    return sizeof(mc_minecart_int_array) + 3u * sizeof(int32_t);
}

static size_t graph_bytes(void) {
    size_t result = 0;
    for (size_t i = 0; i < COUNT(allocation_pcs); ++i)
        result += request_size(i);
    return result;
}

static size_t h_bytes(void) {
    return sizeof(mc_minecart_stack_array) + 36u * sizeof(mc_item_stack *);
}

static size_t ready_direct(void) {
    return sizeof(mc_minecart_entity_object) + sizeof(mc_entity_bounds) + sizeof(mc_java_random) +
           sizeof(mc_entity_watcher) + sizeof(mc_entity_watch_map) +
           sizeof(mc_entity_watch_record) + sizeof(mc_entity_watch_node) + h_bytes();
}

static size_t ready_static(void) {
    return sizeof(mc_entity_classes) + graph_bytes() + sizeof(mc_entity_bounds_pool) +
           sizeof(mc_entity_type_map) + 7u * sizeof(mc_entity_type_entry);
}

static bool neutral_site(const mc_minecart_site *s) {
    return !s->selected && !s->entered && s->declaring_class == NULL && s->method == NULL &&
           s->bytecode == 0;
}

static bool neutral_error(const mc_entity_error *e) {
    return e->status == MC_ENTITY_OK && e->operation == NULL && e->bytecode == 0 &&
           e->profile == MC_JAVA_RANDOM_PROFILE_OK && e->item == MC_ITEM_STACK_OK &&
           e->world == MC_WORLD_STORAGE_OK && neutral_site(&e->minecart_world_constructor) &&
           neutral_site(&e->minecart_constructor) && neutral_site(&e->minecart_initializer) &&
           neutral_site(&e->minecart_initializer_cause);
}

static mc_entity_error poison(void) {
    const mc_minecart_site old = {true, true, "old", "old", 999};
    return (mc_entity_error){.status = MC_ENTITY_BUSY,
                             .operation = "old",
                             .bytecode = 998,
                             .profile = MC_JAVA_RANDOM_PROFILE_LIMIT,
                             .item = MC_ITEM_STACK_LIMIT,
                             .world = MC_WORLD_STORAGE_LIMIT,
                             .minecart_world_constructor = old,
                             .minecart_constructor = old,
                             .minecart_initializer = old,
                             .minecart_initializer_cause = old};
}

static bool site_is(const mc_minecart_site *s, mc_entity_source source, const char *method,
                    uint16_t pc, bool entered) {
    return s->selected && s->entered == entered && s->declaring_class != NULL &&
           strcmp(s->declaring_class, source == MC_ENTITY_CLIENT ? "yl" : "pr") == 0 &&
           s->method != NULL && strcmp(s->method, method) == 0 && s->bytecode == pc;
}

static const char *world_method(mc_entity_source source) {
    return source == MC_ENTITY_CLIENT ? "<init>(Lfd;)V" : "<init>(Ldj;)V";
}

static const char *full_method(mc_entity_source source) {
    return source == MC_ENTITY_CLIENT ? "<init>(Lfd;DDDI)V" : "<init>(Ldj;DDDI)V";
}

static bool same_site(mc_minecart_site a, mc_minecart_site b) {
    return a.selected == b.selected && a.entered == b.entered &&
           a.declaring_class == b.declaring_class && a.method == b.method &&
           a.bytecode == b.bytecode;
}

static bool same_error(const mc_entity_error *a, const mc_entity_error *b) {
    return a->status == b->status && a->operation == b->operation && a->bytecode == b->bytecode &&
           a->profile == b->profile && a->item == b->item && a->world == b->world &&
           same_site(a->minecart_world_constructor, b->minecart_world_constructor) &&
           same_site(a->minecart_constructor, b->minecart_constructor) &&
           same_site(a->minecart_initializer, b->minecart_initializer) &&
           same_site(a->minecart_initializer_cause, b->minecart_initializer_cause);
}

static size_t graph_identities(const mc_minecart_class_snapshot *s, const void **out) {
    size_t count = 0;
    out[count++] = s->j;
    for (size_t i = 0; i < 10; ++i) {
        out[count++] = s->rows[i];
        for (size_t j = 0; j < 2; ++j)
            out[count++] = s->endpoints[i][j];
    }
    return count;
}

static bool graph_is(const mc_minecart_class_snapshot *s, mc_entity_source source) {
    if (s->source != source || s->j == NULL || s->phase != MC_ENTITY_CLASS_READY ||
        s->bytecode != 442 || s->length != 10 || s->native_array_count != 31 ||
        s->native_allocation_bytes != graph_bytes() || !neutral_error(&s->failure))
        return false;
    const void *ids[31];
    if (graph_identities(s, ids) != 31)
        return false;
    for (size_t i = 0; i < 31; ++i) {
        if (ids[i] == NULL)
            return false;
        for (size_t j = 0; j < i; ++j)
            if (ids[i] == ids[j])
                return false;
    }
    for (size_t i = 0; i < 10; ++i) {
        if (s->row_lengths[i] != 2)
            return false;
        for (size_t j = 0; j < 2; ++j) {
            if (s->endpoint_lengths[i][j] != 3)
                return false;
            for (size_t k = 0; k < 3; ++k)
                if (s->values[i][j][k] != shapes[i][j][k])
                    return false;
        }
    }
    return true;
}

static bool no_graph(const mc_minecart_class_snapshot *s) {
    if (s->j != NULL || s->length != 0 || s->native_array_count != 0 ||
        s->native_allocation_bytes != 0)
        return false;
    for (size_t i = 0; i < 10; ++i) {
        if (s->rows[i] != NULL || s->row_lengths[i] != 0)
            return false;
        for (size_t j = 0; j < 2; ++j) {
            if (s->endpoints[i][j] != NULL || s->endpoint_lengths[i][j] != 0)
                return false;
            for (size_t k = 0; k < 3; ++k)
                if (s->values[i][j][k] != 0)
                    return false;
        }
    }
    return true;
}

static bool fields_are(const mc_minecart_entity_snapshot *s, bool constructed, int32_t type) {
    const mc_minecart_entity_fields *v = &s->fields;
    if (v->a != 0 || v->b != 0 || v->c != (constructed ? 1 : 0) || v->private_i || v->d != type ||
        v->e != 0 || v->private_k != 0 || double_bits(v->f) != 0 || double_bits(v->g) != 0 ||
        double_bits(v->private_l) != 0 || double_bits(v->private_m) != 0 ||
        double_bits(v->private_n) != 0 || double_bits(v->private_o) != 0 ||
        double_bits(v->private_p) != 0 || double_bits(v->client_only.private_q) != 0 ||
        double_bits(v->client_only.private_r) != 0 || double_bits(v->client_only.private_s) != 0 ||
        s->inventory_length != (constructed ? 36u : 0u) || (v->h != NULL) != constructed)
        return false;
    for (size_t i = 0; i < 36; ++i)
        if (s->inventory[i] != NULL)
            return false;
    return true;
}

static int class_owners(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *owners[3] = {NULL};
    mc_entity *raw[6] = {NULL};
    mc_minecart_class_snapshot views[3] = {{0}}, cold = {0}, warm = {0};
    mc_entity_class_snapshot generic = {0};
    const mc_entity_class_literal *literal = NULL, *base = NULL;
    mc_entity_class_literal_view inspected = {0};
    mc_entity_error error = poison();
    ++cases;
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    const uint64_t before = uniquifier(profile);
    const mc_platform_nanotime clock = profile->clock;
    for (size_t i = 0; i < 3; ++i) {
        const mc_entity_source side =
            i == 2 ? (source == MC_ENTITY_CLIENT ? MC_ENTITY_SERVER : MC_ENTITY_CLIENT) : source;
        CHECK(mc_entity_classes_create(side, profile, NULL, &owners[i]) == MC_ENTITY_OK);
        CHECK(mc_minecart_class_get_snapshot(owners[i], &cold) == MC_ENTITY_OK &&
              cold.phase == MC_ENTITY_CLASS_COLD && cold.bytecode == 0 && no_graph(&cold) &&
              neutral_error(&cold.failure));
    }
    CHECK(mc_entity_class_literal_get(owners[0], MC_ENTITY_CLASS_KIND_MINECART, &literal) ==
              MC_ENTITY_OK &&
          mc_entity_class_literal_get(owners[0], MC_ENTITY_CLASS_KIND_ENTITY, &base) ==
              MC_ENTITY_OK &&
          mc_entity_class_literal_inspect(literal, &inspected) == MC_ENTITY_OK &&
          inspected.kind == MC_ENTITY_CLASS_KIND_MINECART && inspected.owner == owners[0] &&
          strcmp(inspected.client_name, "yl") == 0 && strcmp(inspected.server_name, "pr") == 0);
    bool assignable = false;
    CHECK(mc_entity_class_literal_is_assignable_from(base, literal, &assignable) == MC_ENTITY_OK &&
          assignable);
    CHECK(mc_minecart_class_get_snapshot(owners[0], &cold) == MC_ENTITY_OK &&
          cold.phase == MC_ENTITY_CLASS_COLD && no_graph(&cold) &&
          owners[0]->entity_phase == MC_ENTITY_CLASS_COLD);
    CHECK(mc_item_entity_allocate(owners[0], &raw[0], NULL) == MC_ENTITY_OK &&
          mc_falling_entity_allocate(owners[0], &raw[1], NULL) == MC_ENTITY_OK &&
          raw[0]->minecart == NULL && raw[1]->minecart == NULL &&
          mc_entity_native_bytes(raw[0]) == sizeof(mc_entity) &&
          mc_entity_native_bytes(raw[1]) == sizeof(mc_entity));
    CHECK(mc_minecart_class_get_snapshot(owners[0], &cold) == MC_ENTITY_OK &&
          cold.phase == MC_ENTITY_CLASS_COLD);
    for (size_t i = 0; i < 3; ++i) {
        error = poison();
        CHECK(mc_minecart_entity_allocate(owners[i], &raw[i + 2], &error) == MC_ENTITY_OK &&
              neutral_error(&error));
        CHECK(mc_minecart_class_get_snapshot(owners[i], &views[i]) == MC_ENTITY_OK &&
              graph_is(&views[i], owners[i]->source));
        CHECK(mc_entity_classes_get_snapshot(owners[i], &generic) == MC_ENTITY_OK &&
              generic.next_id == 0 && generic.entity_class == MC_ENTITY_CLASS_READY &&
              generic.bounds_class == MC_ENTITY_CLASS_COLD &&
              generic.watcher_class == MC_ENTITY_CLASS_COLD &&
              owners[i]->registry_phase == MC_ENTITY_CLASS_COLD);
        CHECK(raw[i + 2]->minecart == &((mc_minecart_entity_object *)raw[i + 2])->state &&
              mc_entity_native_bytes(raw[i + 2]) == sizeof(mc_minecart_entity_object));
    }
    CHECK(mc_minecart_entity_allocate(owners[0], &raw[5], NULL) == MC_ENTITY_OK &&
          mc_minecart_class_get_snapshot(owners[0], &warm) == MC_ENTITY_OK && warm.j == views[0].j);
    const void *warm_ids[31], *first_ids[31];
    CHECK(graph_is(&warm, source) && graph_identities(&warm, warm_ids) == 31 &&
          graph_identities(&views[0], first_ids) == 31);
    for (size_t i = 0; i < 31; ++i)
        CHECK(warm_ids[i] == first_ids[i]);
    const void *ids[3][31];
    for (size_t i = 0; i < 3; ++i)
        CHECK(graph_identities(&views[i], ids[i]) == 31);
    for (size_t a = 0; a < 3; ++a)
        for (size_t b = 0; b < a; ++b)
            for (size_t i = 0; i < 31; ++i)
                for (size_t j = 0; j < 31; ++j)
                    CHECK(ids[a][i] != ids[b][j]);
    CHECK(uniquifier(profile) == before && holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) &&
          profile->math == NULL && profile->clock.initialized == clock.initialized &&
          profile->clock.frequency == clock.frequency &&
          profile->clock.initial_sample == clock.initial_sample);
    mc_entity_release(raw[3]);
    raw[3] = NULL;
    CHECK(mc_entity_classes_retain(owners[1], source, profile) == MC_ENTITY_OK);
    const mc_entity_status busy = mc_entity_classes_free(owners[1]);
    const mc_entity_status released = mc_entity_classes_release(owners[1]);
    CHECK(busy == MC_ENTITY_BUSY && released == MC_ENTITY_OK);
    result = 0;
cleanup:
    for (size_t i = 0; i < COUNT(raw); ++i)
        mc_entity_release(raw[i]);
    for (size_t i = 0; i < COUNT(owners); ++i) {
        if (owners[i] != NULL) {
            mc_entity_class_snapshot after = {0};
            if (mc_entity_classes_get_snapshot(owners[i], &after) != MC_ENTITY_OK ||
                after.live_entities != 0 ||
                (result == 0 &&
                 after.allocation_bytes != sizeof(mc_entity_classes) + graph_bytes()))
                result = 1;
        }
        if (retire_classes(&owners[i]) != 0)
            result = 1;
    }
    if (owners[0] == NULL && owners[1] == NULL && owners[2] == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int initializer_cut(mc_entity_source source, size_t cut) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL, *sentinel_owner = NULL;
    mc_entity *sentinel = NULL, *out = NULL;
    mc_entity_error error = poison(), first = {0};
    mc_minecart_class_snapshot view = {0};
    mc_entity_class_snapshot generic = {0};
    size_t prefix = 0;
    for (size_t i = 0; i < cut; ++i)
        prefix += request_size(i);
    const mc_entity_limits limits = {sizeof(mc_entity_classes) + prefix + request_size(cut) - 1u,
                                     SIZE_MAX};
    ++cases;
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, &limits, &classes) == MC_ENTITY_OK &&
          mc_entity_classes_create(source, profile, NULL, &sentinel_owner) == MC_ENTITY_OK &&
          mc_item_entity_allocate(sentinel_owner, &sentinel, NULL) == MC_ENTITY_OK);
    const uint64_t before = uniquifier(profile);
    out = sentinel;
    CHECK(mc_minecart_entity_allocate(classes, &out, &error) == MC_ENTITY_LIMIT && out == sentinel);
    first = error;
    CHECK(error.status == MC_ENTITY_LIMIT && error.operation != NULL &&
          strcmp(error.operation, "minecart class initialization") == 0 &&
          error.bytecode == allocation_pcs[cut] &&
          site_is(&error.minecart_initializer, source, "<clinit>()V", allocation_pcs[cut], true) &&
          same_site(error.minecart_initializer_cause, error.minecart_initializer) &&
          neutral_site(&error.minecart_world_constructor) &&
          neutral_site(&error.minecart_constructor) && error.profile == MC_JAVA_RANDOM_PROFILE_OK &&
          error.item == MC_ITEM_STACK_OK && error.world == MC_WORLD_STORAGE_OK);
    CHECK(mc_minecart_class_get_snapshot(classes, &view) == MC_ENTITY_OK &&
          view.phase == MC_ENTITY_CLASS_FAILED && view.bytecode == previous_pcs[cut] &&
          no_graph(&view) && same_error(&view.failure, &first));
    CHECK(mc_entity_classes_get_snapshot(classes, &generic) == MC_ENTITY_OK &&
          generic.next_id == 0 && generic.live_entities == 0 &&
          generic.allocation_bytes == sizeof(mc_entity_classes) &&
          generic.entity_class == MC_ENTITY_CLASS_READY &&
          generic.bounds_class == MC_ENTITY_CLASS_COLD &&
          generic.watcher_class == MC_ENTITY_CLASS_COLD);
    error = poison();
    CHECK(mc_minecart_entity_allocate(classes, &out, &error) == MC_ENTITY_CLASS_FAILED_STATUS &&
          out == sentinel && error.bytecode == 0 &&
          strcmp(error.operation, "minecart class initialization") == 0 &&
          site_is(&error.minecart_initializer, source, "<clinit>()V", 0, false) &&
          same_site(error.minecart_initializer_cause, first.minecart_initializer) &&
          same_site(error.minecart_world_constructor, first.minecart_world_constructor) &&
          same_site(error.minecart_constructor, first.minecart_constructor) &&
          error.profile == first.profile && error.item == first.item && error.world == first.world);
    CHECK(mc_minecart_class_get_snapshot(classes, &view) == MC_ENTITY_OK &&
          same_error(&view.failure, &first) && view.bytecode == previous_pcs[cut] &&
          no_graph(&view) && uniquifier(profile) == before &&
          holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL);
    result = 0;
cleanup:
    if (out != sentinel)
        mc_entity_release(out);
    mc_entity_release(sentinel);
    if (retire_classes(&sentinel_owner) != 0)
        result = 1;
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL && sentinel_owner == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static bool ready_is_minecart(const mc_entity_snapshot *base,
                              const mc_minecart_entity_snapshot *own, mc_entity_source source,
                              int32_t id, mc_world_runtime *world, bool full, double x, double y,
                              double z, int32_t type) {
    mc_entity_base_fields expected = base_literals(id);
    expected.aF = true;
    expected.world = world;
    expected.width = 0.98f;
    expected.height = 0.7f;
    expected.offset = 0.35f;
    mc_entity_bounds box = {-(double)0.3f, 0,           -(double)0.3f, (double)0.3f,
                            (double)1.8f,  (double)0.3f};
    if (full) {
        expected.x = expected.aJ = x;
        expected.y = y + (double)0.35f;
        expected.z = expected.aL = z;
        expected.aK = y;
        const double minimum = (y + (double)0.35f) - (double)0.35f;
        box = (mc_entity_bounds){x - (double)0.49f,      minimum,
                                 z - (double)0.49f,      x + (double)0.49f,
                                 minimum + (double)0.7f, z + (double)0.49f};
    }
    return base->source == source && base->phase == MC_ENTITY_READY && base->base_bytecode == 231 &&
           base_equal(&base->base, &expected) && base->item_bytecode == 0 &&
           base->falling_bytecode == 0 && base->item.b == 0 && base->item.e == 0 &&
           base->item.c == 0 && base->item.f == 0 && float_bits(base->item.d) == 0 &&
           base->falling.a == 0 && base->falling.b == 0 && base->stack == NULL &&
           base->bounds != NULL && bounds_equal(base->bounds, &box) && fresh_random(base->random) &&
           watcher_is(base, true) && float_bits(base->base.offset) == UINT32_C(0x3eb33333) &&
           own->source == source && own->class_literal == base->class_literal &&
           own->phase == MC_ENTITY_READY && own->world_bytecode == 57 &&
           own->bytecode == (full ? 58 : 0) && fields_are(own, true, type);
}

static int constructors(mc_entity_source source, bool exact_budget) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entities[5] = {NULL}, *alias = NULL;
    mc_entity_snapshot base = {0};
    mc_minecart_entity_snapshot own = {0};
    mc_entity_class_snapshot state = {0};
    mc_entity_error error = {0};
    const double xyz[5][3] = {{0, 0, 0},
                              {8.5, 64, 8.5},
                              {-1.25, -2.5, 4.75},
                              {-0.0, -0.0, -0.0},
                              {0x1.0000000000001p50, 0x1.0000000000001p50, -0x1p50}};
    const int32_t types[5] = {0, 0, INT32_MIN, INT32_MAX, -257};
    const size_t count = exact_budget ? 1u : COUNT(entities);
    const mc_entity_limits exact = {ready_static() + ready_direct(), 1};
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, exact_budget ? &exact : NULL, &classes) ==
              MC_ENTITY_OK);
    const uint64_t before = uniquifier(profile);
    for (size_t i = 0; i < count; ++i) {
        ++cases;
        const mc_entity_base_fields zero_base = {0};
        CHECK(mc_minecart_entity_allocate(classes, &entities[i], NULL) == MC_ENTITY_OK &&
              mc_entity_get_snapshot(entities[i], &base) == MC_ENTITY_OK &&
              mc_minecart_entity_get_snapshot(entities[i], &own) == MC_ENTITY_OK &&
              base.phase == MC_ENTITY_RAW && base_equal(&base.base, &zero_base) &&
              base.base_bytecode == 0 && own.world_bytecode == 0 && own.bytecode == 0 &&
              fields_are(&own, false, 0) && base.bounds == NULL && base.random == NULL &&
              base.watcher == NULL && base.base.world == NULL);
        error = poison();
        const bool full = exact_budget || i != 0;
        CHECK((full ? mc_minecart_entity_construct(entities[i], NULL, xyz[i][0], xyz[i][1],
                                                   xyz[i][2], types[i], &error)
                    : mc_minecart_entity_construct_world(entities[i], NULL, &error)) ==
                  MC_ENTITY_OK &&
              neutral_error(&error));
        CHECK(mc_entity_get_snapshot(entities[i], &base) == MC_ENTITY_OK &&
              mc_minecart_entity_get_snapshot(entities[i], &own) == MC_ENTITY_OK &&
              ready_is_minecart(&base, &own, source, (int32_t)i, NULL, full, xyz[i][0], xyz[i][1],
                                xyz[i][2], types[i]));
        CHECK(mc_entity_native_bytes(entities[i]) == ready_direct() &&
              uniquifier(profile) == advanced(before, (unsigned int)i + 1u) &&
              holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL);
        for (size_t j = 0; j < i; ++j) {
            mc_minecart_entity_snapshot other = {0};
            CHECK(mc_minecart_entity_get_snapshot(entities[j], &other) == MC_ENTITY_OK &&
                  other.fields.h != own.fields.h && entities[j]->bounds != base.bounds &&
                  entities[j]->random != base.random && entities[j]->watcher != base.watcher);
        }
        const mc_minecart_stack_array *h = own.fields.h;
        const mc_entity_bounds *bounds = base.bounds;
        error = poison();
        CHECK(mc_minecart_entity_construct_world(entities[i], NULL, &error) ==
                  MC_ENTITY_INVALID_PHASE &&
              neutral_site(&error.minecart_world_constructor) &&
              neutral_site(&error.minecart_constructor) &&
              neutral_site(&error.minecart_initializer) &&
              neutral_site(&error.minecart_initializer_cause));
        CHECK(mc_entity_get_snapshot(entities[i], &base) == MC_ENTITY_OK && base.bounds == bounds &&
              mc_minecart_entity_get_snapshot(entities[i], &own) == MC_ENTITY_OK &&
              own.fields.h == h &&
              ready_is_minecart(&base, &own, source, (int32_t)i, NULL, full, xyz[i][0], xyz[i][1],
                                xyz[i][2], types[i]));
    }
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK &&
          state.next_id == (int32_t)count && state.live_entities == count &&
          state.allocation_bytes == ready_static() + count * ready_direct() &&
          state.watcher_type_count == 7 && state.bounds_pool_cursor == 0 &&
          state.bounds_class == MC_ENTITY_CLASS_READY &&
          state.watcher_class == MC_ENTITY_CLASS_READY);
    CHECK(mc_entity_retain(entities[0]) == MC_ENTITY_OK);
    alias = entities[0];
    mc_entity_release(entities[0]);
    entities[0] = NULL;
    CHECK(mc_entity_classes_free(classes) == MC_ENTITY_BUSY &&
          mc_entity_native_bytes(alias) == ready_direct());
    mc_entity_release(alias);
    alias = NULL;
    for (size_t i = 1; i < count; ++i) {
        mc_entity_release(entities[i]);
        entities[i] = NULL;
    }
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK &&
          state.live_entities == 0 && state.allocation_bytes == ready_static() &&
          state.next_id == (int32_t)count &&
          uniquifier(profile) == advanced(before, (unsigned int)count));
    result = 0;
cleanup:
    mc_entity_release(alias);
    for (size_t i = 0; i < COUNT(entities); ++i)
        mc_entity_release(entities[i]);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int raw_gate(mc_entity_source source, unsigned int mode) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL, *sentinel_owner = NULL;
    mc_entity *entity = NULL, *sentinel = NULL, *out = NULL;
    mc_entity_error error = poison();
    mc_minecart_class_snapshot own = {0};
    mc_entity_class_snapshot state = {0};
    mc_entity_limits limits = {SIZE_MAX, mode == 0 ? 0u : 1u};
    if (mode != 0)
        limits.max_allocation_bytes = sizeof(mc_entity_classes) + graph_bytes() +
                                      sizeof(mc_minecart_entity_object) - (mode == 1 ? 1u : 0u);
    ++cases;
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, &limits, &classes) == MC_ENTITY_OK &&
          mc_entity_classes_create(source, profile, NULL, &sentinel_owner) == MC_ENTITY_OK &&
          mc_item_entity_allocate(sentinel_owner, &sentinel, NULL) == MC_ENTITY_OK);
    const uint64_t before = uniquifier(profile);
    entity = sentinel;
    const mc_entity_status status = mc_minecart_entity_allocate(classes, &entity, &error);
    if (mode < 2) {
        CHECK(status == MC_ENTITY_LIMIT && entity == sentinel && error.bytecode == 0 &&
              error.operation != NULL && strcmp(error.operation, "minecart allocation") == 0 &&
              neutral_site(&error.minecart_initializer) &&
              neutral_site(&error.minecart_initializer_cause));
        entity = NULL; /* The borrowed sentinel was never another owned hold. */
    } else {
        CHECK(status == MC_ENTITY_OK && entity != sentinel && neutral_error(&error) &&
              entity->phase == MC_ENTITY_RAW &&
              mc_entity_native_bytes(entity) == sizeof(mc_minecart_entity_object));
    }
    CHECK(mc_minecart_class_get_snapshot(classes, &own) == MC_ENTITY_OK && graph_is(&own, source) &&
          mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 0 &&
          state.allocation_bytes == sizeof(mc_entity_classes) + graph_bytes() +
                                        (mode < 2 ? 0u : sizeof(mc_minecart_entity_object)) &&
          uniquifier(profile) == before && profile->math == NULL);
    out = sentinel;
    CHECK(mc_minecart_entity_allocate(classes, &out, &error) == MC_ENTITY_LIMIT &&
          out == sentinel && neutral_site(&error.minecart_initializer_cause));
    result = 0;
cleanup:
    if (out != sentinel && out != entity)
        mc_entity_release(out);
    if (entity != sentinel)
        mc_entity_release(entity);
    mc_entity_release(sentinel);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (retire_classes(&sentinel_owner) != 0)
        result = 1;
    if (classes == NULL && sentinel_owner == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

typedef struct constructor_cut {
    size_t allowed;
    const char *operation;
    uint16_t pc, base_pc;
    int types;
    bool random, watcher, bounds_failed, watcher_failed;
} constructor_cut;

static int constructor_failure(mc_entity_source source, const constructor_cut *cut, bool full) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entity = NULL;
    mc_entity_snapshot base = {0};
    mc_minecart_entity_snapshot own = {0};
    mc_entity_class_snapshot state = {0};
    mc_entity_error error = poison();
    const mc_entity_limits limits = {sizeof(mc_entity_classes) + graph_bytes() +
                                         sizeof(mc_minecart_entity_object) + cut->allowed,
                                     1};
    ++cases;
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, &limits, &classes) == MC_ENTITY_OK &&
          mc_minecart_entity_allocate(classes, &entity, NULL) == MC_ENTITY_OK);
    const uint64_t before = uniquifier(profile);
    CHECK((full ? mc_minecart_entity_construct(entity, NULL, 8.5, 64, 8.5, INT32_MIN, &error)
                : mc_minecart_entity_construct_world(entity, NULL, &error)) == MC_ENTITY_LIMIT);
    CHECK(error.status == MC_ENTITY_LIMIT && error.operation != NULL &&
          strcmp(error.operation, cut->operation) == 0 && error.bytecode == cut->pc &&
          error.profile == MC_JAVA_RANDOM_PROFILE_OK && error.world == MC_WORLD_STORAGE_OK &&
          error.item == MC_ITEM_STACK_OK &&
          site_is(&error.minecart_world_constructor, source, world_method(source),
                  cut->base_pc == 231 ? 8 : 2, true) &&
          (full ? site_is(&error.minecart_constructor, source, full_method(source), 2, true)
                : neutral_site(&error.minecart_constructor)) &&
          neutral_site(&error.minecart_initializer) &&
          neutral_site(&error.minecart_initializer_cause));
    CHECK(mc_entity_get_snapshot(entity, &base) == MC_ENTITY_OK &&
          mc_minecart_entity_get_snapshot(entity, &own) == MC_ENTITY_OK &&
          base.phase == MC_ENTITY_FAILED_CONSTRUCTION && base.base_bytecode == cut->base_pc &&
          own.phase == MC_ENTITY_FAILED_CONSTRUCTION &&
          own.world_bytecode == (cut->base_pc == 231 ? 2 : 0) && own.bytecode == 0 &&
          fields_are(&own, false, 0) && base.base.id == 0 && base.base.world == NULL &&
          base.item_bytecode == 0 && base.falling_bytecode == 0);
    CHECK((base.random != NULL) == cut->random && (base.watcher != NULL) == cut->watcher &&
          (base.random == NULL || fresh_random(base.random)) &&
          uniquifier(profile) == advanced(before, cut->random ? 1u : 0u) &&
          holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL);
    mc_entity_base_fields expected = base_literals(0);
    if (cut->base_pc == 24)
        expected = (mc_entity_base_fields){.id = 0, .aE = 1.0};
    else if (cut->base_pc == 119)
        expected = (mc_entity_base_fields){
            .id = 0, .aE = 1.0, .bd = true, .width = 0.6f, .height = 1.8f, .private_b = 1};
    CHECK(base_equal(&base.base, &expected));
    const mc_entity_bounds zero = {0};
    const mc_entity_bounds positioned = {-(double)0.3f, 0,           -(double)0.3f, (double)0.3f,
                                         (double)1.8f,  (double)0.3f};
    CHECK(cut->base_pc == 24 ? base.bounds == NULL
                             : base.bounds != NULL &&
                                   bounds_equal(base.bounds, cut->watcher ? &positioned : &zero));
    if (cut->watcher)
        CHECK(watcher_is(&base, cut->base_pc == 231));
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK && state.next_id == 1 &&
          state.live_entities == 1 && state.bounds_pool_cursor == 0 &&
          state.bounds_class ==
              (cut->bounds_failed ? MC_ENTITY_CLASS_FAILED : MC_ENTITY_CLASS_READY) &&
          state.watcher_class == (cut->watcher_failed ? MC_ENTITY_CLASS_FAILED
                                  : cut->types >= 0   ? MC_ENTITY_CLASS_READY
                                                      : MC_ENTITY_CLASS_COLD) &&
          state.watcher_type_count == (cut->types < 0 ? 0u : (size_t)cut->types));
    const size_t static_bytes =
        sizeof(mc_entity_classes) + graph_bytes() +
        (state.bounds_pool_identity == NULL ? 0u : sizeof(mc_entity_bounds_pool)) +
        (state.watcher_types_identity == NULL
             ? 0u
             : sizeof(mc_entity_type_map) +
                   state.watcher_type_count * sizeof(mc_entity_type_entry));
    const size_t direct_bytes =
        sizeof(mc_minecart_entity_object) + (base.bounds == NULL ? 0u : sizeof(mc_entity_bounds)) +
        (base.random == NULL ? 0u : sizeof(mc_java_random)) +
        (base.watcher == NULL ? 0u : sizeof(mc_entity_watcher) + sizeof(mc_entity_watch_map)) +
        (cut->base_pc == 231 ? sizeof(mc_entity_watch_record) + sizeof(mc_entity_watch_node) : 0u);
    CHECK(state.allocation_bytes == static_bytes + direct_bytes &&
          mc_entity_native_bytes(entity) == direct_bytes &&
          mc_entity_classes_free(classes) == MC_ENTITY_BUSY);
    error = poison();
    CHECK(mc_minecart_entity_construct_world(entity, NULL, &error) == MC_ENTITY_INVALID_PHASE &&
          neutral_site(&error.minecart_world_constructor) &&
          neutral_site(&error.minecart_constructor));
    mc_entity_release(entity);
    entity = NULL;
    CHECK(mc_entity_classes_get_snapshot(classes, &state) == MC_ENTITY_OK &&
          state.live_entities == 0 && state.allocation_bytes == static_bytes &&
          state.next_id == 1 && uniquifier(profile) == advanced(before, cut->random ? 1u : 0u));
    result = 0;
cleanup:
    mc_entity_release(entity);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int constructor_cuts(mc_entity_source source) {
    const size_t pool = sizeof(mc_entity_bounds_pool), box = sizeof(mc_entity_bounds);
    const size_t random = sizeof(mc_java_random), map = sizeof(mc_entity_type_map);
    const size_t entry = sizeof(mc_entity_type_entry), watcher = sizeof(mc_entity_watcher);
    const size_t watchmap = sizeof(mc_entity_watch_map), record = sizeof(mc_entity_watch_record);
    const size_t node = sizeof(mc_entity_watch_node), prefix = pool + box + random;
    const size_t typed = prefix + map + 7u * entry;
    constructor_cut cuts[16] = {
        {0, "bounds class initialization", 0, 24, -1, false, false, true, false},
        {pool, "bounds factory", 0, 24, -1, false, false, false, false},
        {pool + box, "entity constructor", 123, 119, -1, false, false, false, false},
        {prefix, "watcher class initialization", 0, 179, -1, true, false, false, true}};
    const uint16_t puts[7] = {19, 32, 45, 58, 71, 84, 98};
    for (size_t i = 0; i < 7; ++i)
        cuts[4 + i] = (constructor_cut){prefix + map + i * entry,
                                        "watcher class initialization",
                                        puts[i],
                                        179,
                                        (int)i,
                                        true,
                                        false,
                                        false,
                                        true};
    cuts[11] =
        (constructor_cut){typed, "entity constructor", 183, 179, 7, true, false, false, false};
    cuts[12] = (constructor_cut){
        typed + watcher, "watcher constructor", 5, 179, 7, true, false, false, false};
    cuts[13] = (constructor_cut){
        typed + watcher + watchmap, "watcher registration", 144, 221, 7, true, true, false, false};
    cuts[14] = (constructor_cut){typed + watcher + watchmap + record,
                                 "watcher registration",
                                 169,
                                 221,
                                 7,
                                 true,
                                 true,
                                 false,
                                 false};
    cuts[15] = (constructor_cut){typed + watcher + watchmap + record + node,
                                 "minecart constructor",
                                 8,
                                 231,
                                 7,
                                 true,
                                 true,
                                 false,
                                 false};
    for (size_t i = 0; i < COUNT(cuts); ++i)
        for (unsigned int full = 0; full < 2; ++full)
            if (constructor_failure(source, &cuts[i], full != 0) != 0)
                return 1;
    return 0;
}

static int admission(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *item = NULL;
    mc_entity_error error = poison();
    mc_minecart_entity_snapshot own, saved;
    mc_minecart_class_snapshot class_out, class_saved, cold = {0};
    ++cases;
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK);
    CHECK(mc_minecart_entity_allocate(classes, NULL, &error) == MC_ENTITY_INVALID_ARGUMENT &&
          neutral_site(&error.minecart_initializer) &&
          neutral_site(&error.minecart_initializer_cause));
    CHECK(mc_minecart_class_get_snapshot(classes, &cold) == MC_ENTITY_OK &&
          cold.phase == MC_ENTITY_CLASS_COLD && no_graph(&cold) &&
          classes->entity_phase == MC_ENTITY_CLASS_COLD);
    CHECK(mc_item_entity_allocate(classes, &item, NULL) == MC_ENTITY_OK);
    mc_entity *out = item;
    error = poison();
    CHECK(mc_minecart_entity_allocate(NULL, &out, &error) == MC_ENTITY_INVALID_ARGUMENT &&
          out == item && neutral_site(&error.minecart_initializer) &&
          neutral_site(&error.minecart_world_constructor));
    memset(&own, 0xa5, sizeof(own));
    memcpy(&saved, &own, sizeof(saved));
    CHECK(mc_minecart_entity_get_snapshot(item, &own) == MC_ENTITY_INVALID_ARGUMENT &&
          memcmp(&own, &saved, sizeof(own)) == 0);
    CHECK(mc_minecart_entity_get_snapshot(NULL, &own) == MC_ENTITY_INVALID_ARGUMENT &&
          memcmp(&own, &saved, sizeof(own)) == 0);
    CHECK(mc_minecart_entity_get_snapshot(item, NULL) == MC_ENTITY_INVALID_ARGUMENT);
    memset(&class_out, 0xa5, sizeof(class_out));
    memcpy(&class_saved, &class_out, sizeof(class_saved));
    CHECK(mc_minecart_class_get_snapshot(NULL, &class_out) == MC_ENTITY_INVALID_ARGUMENT &&
          memcmp(&class_out, &class_saved, sizeof(class_out)) == 0);
    CHECK(mc_minecart_class_get_snapshot(classes, NULL) == MC_ENTITY_INVALID_ARGUMENT);
    error = poison();
    CHECK(mc_minecart_entity_construct_world(item, NULL, &error) == MC_ENTITY_INVALID_ARGUMENT &&
          item->phase == MC_ENTITY_RAW && item->minecart == NULL &&
          neutral_site(&error.minecart_world_constructor) &&
          neutral_site(&error.minecart_constructor));
    error = poison();
    CHECK(mc_minecart_entity_construct(NULL, NULL, 0, 0, 0, 0, &error) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          neutral_site(&error.minecart_world_constructor) &&
          neutral_site(&error.minecart_constructor));
    CHECK(mc_minecart_class_get_snapshot(classes, &cold) == MC_ENTITY_OK &&
          cold.phase == MC_ENTITY_CLASS_COLD && no_graph(&cold) && classes->next_id == 0 &&
          holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL);
    result = 0;
cleanup:
    mc_entity_release(item);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

static int world_bindings(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL, *foreign_profile = NULL;
    mc_entity_classes *classes = NULL, *limited = NULL;
    mc_world_runtime_classes *world_classes[2] = {NULL};
    mc_world_runtime *worlds[2] = {NULL};
    mc_entity *entity = NULL;
    mc_entity_error error = {0};
    mc_entity_snapshot base = {0};
    mc_minecart_entity_snapshot own = {0};
    mc_math_trig trig;
    /* Real RAW World owners deliberately prove profile-only constructor
     * admission. No fake World, GAME phase setter, Registry or store is used. */
    CHECK(mc_math_trig_init(&trig) &&
          mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_java_random_profile_create(profile_budget(), &foreign_profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK);
    const mc_world_info_side other_side =
        source == MC_ENTITY_CLIENT ? MC_WORLD_INFO_SERVER : MC_WORLD_INFO_CLIENT;
    const mc_world_runtime_entry entry = other_side == MC_WORLD_INFO_SERVER
                                             ? MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE
                                             : MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE;
    for (size_t i = 0; i < 2; ++i) {
        CHECK(mc_world_runtime_classes_create(other_side, i == 0 ? profile : foreign_profile, &trig,
                                              &world_classes[i], NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_allocate_world(entry, world_classes[i], NULL, &worlds[i], NULL) ==
                  MC_WORLD_STORAGE_OK);
        CHECK(worlds[i]->construction.phase == MC_WORLD_RUNTIME_RAW &&
              worlds[i]->persistent_bindings == 0 && worlds[i]->active_calls == 0 &&
              worlds[i]->classes->registry == NULL);
    }
    const uint64_t before = uniquifier(profile), foreign_before = uniquifier(foreign_profile);
    ++cases;
    CHECK(mc_minecart_entity_allocate(classes, &entity, NULL) == MC_ENTITY_OK);
    error = poison();
    CHECK(mc_minecart_entity_construct_world(entity, worlds[1], &error) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          error.operation != NULL && strcmp(error.operation, "minecart world profile") == 0 &&
          entity->phase == MC_ENTITY_RAW && entity->base.world == NULL &&
          neutral_site(&error.minecart_world_constructor) &&
          neutral_site(&error.minecart_constructor) && uniquifier(profile) == before &&
          uniquifier(foreign_profile) == foreign_before);
    for (unsigned int full = 0; full < 2; ++full) {
        ++cases;
        if (entity == NULL)
            CHECK(mc_minecart_entity_allocate(classes, &entity, NULL) == MC_ENTITY_OK);
        error = poison();
        CHECK(
            (full
                 ? mc_minecart_entity_construct(entity, worlds[0], 8.5, 64, 8.5, INT32_MAX, &error)
                 : mc_minecart_entity_construct_world(entity, worlds[0], &error)) == MC_ENTITY_OK &&
            neutral_error(&error));
        CHECK(mc_entity_get_snapshot(entity, &base) == MC_ENTITY_OK &&
              mc_minecart_entity_get_snapshot(entity, &own) == MC_ENTITY_OK &&
              ready_is_minecart(&base, &own, source, (int32_t)full, worlds[0], full != 0, 8.5, 64,
                                8.5, full ? INT32_MAX : 0));
        CHECK(worlds[0]->construction.phase == MC_WORLD_RUNTIME_RAW &&
              worlds[0]->persistent_bindings == 1 && worlds[0]->active_calls == 0 &&
              mc_world_runtime_free(worlds[0], NULL) == MC_WORLD_STORAGE_BUSY);
        mc_entity_release(entity);
        entity = NULL;
        CHECK(worlds[0]->persistent_bindings == 0 && worlds[0]->active_calls == 0 &&
              uniquifier(profile) == advanced(before, full + 1u));
    }
    ++cases;
    const mc_entity_limits h_limit = {ready_static() + ready_direct() - h_bytes(), 1};
    CHECK(mc_entity_classes_create(source, profile, &h_limit, &limited) == MC_ENTITY_OK &&
          mc_minecart_entity_allocate(limited, &entity, NULL) == MC_ENTITY_OK);
    error = poison();
    CHECK(mc_minecart_entity_construct(entity, worlds[0], 8.5, 64, 8.5, 1, &error) ==
              MC_ENTITY_LIMIT &&
          error.bytecode == 8 &&
          site_is(&error.minecart_world_constructor, source, world_method(source), 8, true) &&
          site_is(&error.minecart_constructor, source, full_method(source), 2, true));
    CHECK(mc_entity_get_snapshot(entity, &base) == MC_ENTITY_OK && base.base.world == worlds[0] &&
          base.base_bytecode == 231 && watcher_is(&base, true) &&
          mc_minecart_entity_get_snapshot(entity, &own) == MC_ENTITY_OK && own.fields.h == NULL &&
          own.world_bytecode == 2 && own.bytecode == 0 && fields_are(&own, false, 0) &&
          mc_entity_native_bytes(entity) == ready_direct() - h_bytes() &&
          worlds[0]->persistent_bindings == 1);
    mc_entity_release(entity);
    entity = NULL;
    CHECK(worlds[0]->persistent_bindings == 0 && limited->live_entities == 0 &&
          limited->allocated_bytes == ready_static());
    ++cases;
    CHECK(mc_minecart_entity_allocate(classes, &entity, NULL) == MC_ENTITY_OK);
    const size_t bindings = worlds[0]->persistent_bindings;
    worlds[0]->persistent_bindings = SIZE_MAX; /* Explicit native overflow control. */
    const mc_entity_status status =
        mc_minecart_entity_construct(entity, worlds[0], 1, 2, 3, 0, &error);
    worlds[0]->persistent_bindings = bindings; /* Restore before every CHECK. */
    CHECK(status == MC_ENTITY_WORLD_FAILED && error.status == MC_ENTITY_WORLD_FAILED &&
          error.bytecode == 205 && error.world == MC_WORLD_STORAGE_LIMIT &&
          error.profile == MC_JAVA_RANDOM_PROFILE_OK && error.item == MC_ITEM_STACK_OK &&
          site_is(&error.minecart_world_constructor, source, world_method(source), 2, true) &&
          site_is(&error.minecart_constructor, source, full_method(source), 2, true));
    CHECK(mc_entity_get_snapshot(entity, &base) == MC_ENTITY_OK && base.base_bytecode == 200 &&
          base.base.world == NULL && watcher_is(&base, false) &&
          mc_minecart_entity_get_snapshot(entity, &own) == MC_ENTITY_OK &&
          fields_are(&own, false, 0) && own.world_bytecode == 0 && own.bytecode == 0 &&
          worlds[0]->persistent_bindings == bindings &&
          mc_entity_native_bytes(entity) == sizeof(mc_minecart_entity_object) +
                                                sizeof(mc_entity_bounds) + sizeof(mc_java_random) +
                                                sizeof(mc_entity_watcher) +
                                                sizeof(mc_entity_watch_map) &&
          uniquifier(profile) == advanced(before, 4) &&
          uniquifier(foreign_profile) == foreign_before);
    result = 0;
cleanup:
    mc_entity_release(entity);
    if (retire_classes(&limited) != 0)
        result = 1;
    if (retire_classes(&classes) != 0)
        result = 1;
    for (size_t i = 0; i < 2; ++i) {
        if (mc_world_runtime_free(worlds[i], NULL) != MC_WORLD_STORAGE_OK) {
            result = 1;
        } else {
            worlds[i] = NULL;
            if (mc_world_runtime_classes_free(world_classes[i], NULL) != MC_WORLD_STORAGE_OK)
                result = 1;
            else
                world_classes[i] = NULL;
        }
    }
    if (classes == NULL && limited == NULL && world_classes[0] == NULL &&
        world_classes[1] == NULL) {
        mc_java_random_profile_free(profile);
        mc_java_random_profile_free(foreign_profile);
    }
    return result;
}

static int ids_and_fp(mc_entity_source source) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_entity_classes *classes = NULL;
    mc_entity *entities[4] = {NULL};
    mc_entity_snapshot views[4] = {{0}};
    mc_minecart_entity_snapshot own = {0};
    mc_entity_error error = {0};
    CHECK(mc_java_random_profile_create(profile_budget(), &profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, profile, NULL, &classes) == MC_ENTITY_OK);
    const uint64_t before = uniquifier(profile);
    for (size_t i = 0; i < COUNT(entities); ++i)
        CHECK(mc_minecart_entity_allocate(classes, &entities[i], NULL) == MC_ENTITY_OK);
    ++cases;
    const int32_t saved = classes->next_id;
    classes->next_id = INT32_MAX; /* Native counter-boundary input, not a fake Entity. */
    const mc_entity_status first = mc_minecart_entity_construct_world(entities[0], NULL, &error);
    const mc_entity_status second =
        mc_minecart_entity_construct(entities[1], NULL, 0, 0, 0, INT32_MIN, &error);
    const int32_t advanced_id = classes->next_id;
    classes->next_id = saved; /* Restore native control before every CHECK. */
    CHECK(first == MC_ENTITY_OK && second == MC_ENTITY_OK && advanced_id == INT32_MIN + 1 &&
          mc_entity_get_snapshot(entities[0], &views[0]) == MC_ENTITY_OK &&
          views[0].base.id == INT32_MAX &&
          mc_entity_get_snapshot(entities[1], &views[1]) == MC_ENTITY_OK &&
          views[1].base.id == INT32_MIN && mc_entity_hash_code(entities[0]) == INT32_MAX &&
          mc_entity_hash_code(entities[1]) == INT32_MIN &&
          !mc_entity_equals(entities[0], entities[1]) &&
          uniquifier(profile) == advanced(before, 2));
    for (size_t i = 2; i < COUNT(entities); ++i) {
        ++cases;
        error = poison();
        const unsigned int control = _mm_getcsr();
        _mm_setcsr(control | 0x8000u);
        const mc_entity_status status =
            i == 2 ? mc_minecart_entity_construct_world(entities[i], NULL, &error)
                   : mc_minecart_entity_construct(entities[i], NULL, 1, 2, 3, 1, &error);
        _mm_setcsr(control); /* Restore strict FP before every CHECK or cleanup. */
        CHECK(status == MC_ENTITY_PROFILE_FAILED && error.bytecode == 127 &&
              error.profile == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP &&
              site_is(&error.minecart_world_constructor, source, world_method(source), 2, true) &&
              (i == 2
                   ? neutral_site(&error.minecart_constructor)
                   : site_is(&error.minecart_constructor, source, full_method(source), 2, true)));
        CHECK(mc_entity_get_snapshot(entities[i], &views[i]) == MC_ENTITY_OK &&
              views[i].phase == MC_ENTITY_FAILED_CONSTRUCTION && views[i].base_bytecode == 119 &&
              views[i].bounds != NULL && views[i].random == NULL && views[i].watcher == NULL &&
              mc_minecart_entity_get_snapshot(entities[i], &own) == MC_ENTITY_OK &&
              fields_are(&own, false, 0) && own.world_bytecode == 0 && own.bytecode == 0 &&
              uniquifier(profile) == advanced(before, 2) &&
              holder_is(profile, MC_JAVA_RANDOM_HOLDER_COLD) && profile->math == NULL);
    }
    result = 0;
cleanup:
    for (size_t i = 0; i < COUNT(entities); ++i)
        mc_entity_release(entities[i]);
    if (retire_classes(&classes) != 0)
        result = 1;
    if (classes == NULL)
        mc_java_random_profile_free(profile);
    return result;
}

int main(void) {
    if ((_mm_getcsr() & 0xffc0u) != 0x1f80u)
        return 1;
    for (unsigned int i = 0; i < 2; ++i) {
        const mc_entity_source source = i == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER;
        if (class_owners(source) || admission(source) || constructors(source, false) ||
            constructors(source, true) || raw_gate(source, 0) || raw_gate(source, 1) ||
            raw_gate(source, 2) || constructor_cuts(source) || world_bindings(source) ||
            ids_and_fp(source))
            return 1;
        for (size_t cut = 0; cut < COUNT(allocation_pcs); ++cut)
            if (initializer_cut(source, cut) != 0)
                return 1;
    }
    printf("PASS entity.minecart cases=%u checks=%u\n", cases, checks);
    return 0;
}
