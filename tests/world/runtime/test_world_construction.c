#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/platform/time/millis.h"
#include "../../../src/world/runtime/runtime_internal.h"

#include <emmintrin.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* Native constructor-prefix contracts. No provider/World completion, recorded
 * original output, supplied clock or seed-derived default-Random expectation. */
static unsigned int checks;
static mc_math_trig trig;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #condition);                \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)

static uint32_t float_bits(float value) {
    uint32_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary32 required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t double_bits(double value) {
    uint64_t bits;
    _Static_assert(sizeof(bits) == sizeof(value), "binary64 required");
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t uniquifier(const mc_java_random_profile *profile) {
    return atomic_load_explicit(&profile->uniquifier, memory_order_relaxed);
}

static bool cold(const mc_java_random_profile *profile) {
    return profile->math == NULL &&
           atomic_load_explicit(&profile->holder_phase, memory_order_acquire) ==
               (unsigned int)MC_JAVA_RANDOM_HOLDER_COLD;
}

static bool same_fields(const mc_world_runtime_fields *left, const mc_world_runtime_fields *right) {
#define SAME(field) (left->field == right->field)
    return SAME(a) && SAME(C) && SAME(b) && SAME(D) && SAME(E) && SAME(F) && SAME(c) && SAME(G) &&
           SAME(d) && SAME(e) && SAME(H) && SAME(f) && SAME(g) && SAME(h) &&
           float_bits(left->i) == float_bits(right->i) &&
           float_bits(left->j) == float_bits(right->j) &&
           float_bits(left->k) == float_bits(right->k) &&
           float_bits(left->l) == float_bits(right->l) && SAME(m) && SAME(n) && SAME(o) &&
           SAME(I) && SAME(p) && SAME(q) && SAME(r) && SAME(s) && SAME(t) && SAME(u) && SAME(v) &&
           SAME(w) && SAME(x) && SAME(y) && SAME(J) && SAME(z) && SAME(K) && SAME(L) && SAME(M) &&
           SAME(N) && SAME(O) && SAME(P) && SAME(Q) && SAME(R) && SAME(B);
#undef SAME
}

static bool same_construction(const mc_world_runtime_construction *left,
                              const mc_world_runtime_construction *right) {
    return left->mode == right->mode && left->phase == right->phase &&
           left->entry == right->entry && left->last_store_pc == right->last_store_pc &&
           left->failed_call_pc == right->failed_call_pc &&
           left->contexts_bound == right->contexts_bound &&
           left->cache_registered == right->cache_registered &&
           left->direct_allocation_bytes == right->direct_allocation_bytes &&
           left->random_status == right->random_status && left->clock_status == right->clock_status;
}

/* Literal field/store facts from the paired original 43-field inventory.
 * All selected tails keep Dimension/provider/store null. g and I are genuine
 * unrepeatable values after their stores, so no guessed numeric value is used. */
static int check_fields(const mc_world_runtime_fields *f, unsigned int pc, bool has_info,
                        bool has_saved) {
    int result = 1;
    int32_t count = -1;
    mc_scheduled_tick *first = NULL;
    const mc_world_reference_list *lists[] = {f->C, f->b, f->D, f->c, f->G,
                                              f->d, f->e, f->u, f->K, f->R};
    const unsigned int stores[] = {17, 28, 39, 72, 83, 94, 105, 192, 203, 254};
    CHECK(!f->a);
    for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); ++i) {
        CHECK((lists[i] != NULL) == (pc >= stores[i]));
        if (lists[i] == NULL)
            continue;
        for (size_t j = 0; j < i; ++j)
            CHECK(lists[i] != lists[j]);
        CHECK((const void *)lists[i] != (const void *)f->E &&
              (const void *)lists[i] != (const void *)f->F &&
              (const void *)lists[i] != (const void *)f->P);
    }
    CHECK((f->E != NULL) == (pc >= 50) && (f->F != NULL) == (pc >= 61));
    if (f->E != NULL) {
        CHECK(mc_scheduled_ordered_size(f->E, &count) == MC_SCHEDULED_ORDERED_OK && count == 0);
        CHECK(mc_scheduled_ordered_first(f->E, &first) == MC_SCHEDULED_ORDERED_EMPTY &&
              first == NULL);
    }
    if (f->F != NULL)
        CHECK(mc_scheduled_membership_size(f->F, &count) == MC_SCHEDULED_MEMBERSHIP_OK &&
              count == 0);
    if (f->E != NULL && f->F != NULL)
        CHECK((const void *)f->E != (const void *)f->F);
    CHECK(f->H == (pc >= 112 ? INT64_C(16777215) : 0) && f->f == 0);
    if (pc < 131)
        CHECK(f->g == 0);
    CHECK(f->h == (pc >= 137 ? INT32_C(1013904223) : 0));
    CHECK(float_bits(f->i) == 0 && float_bits(f->j) == 0 && float_bits(f->k) == 0 &&
          float_bits(f->l) == 0);
    CHECK(f->m == 0 && f->n == 0 && !f->o);
    if (pc < 159)
        CHECK(f->I == 0);
    CHECK(f->p == (pc >= 165 ? 40 : 0) && f->q == 0);
    CHECK((f->r != NULL) == (pc >= 176));
    if (f->r != NULL)
        CHECK(f->r->state <= UINT64_C(0xffffffffffff) &&
              double_bits(f->r->next_next_gaussian) == 0 && !f->r->have_next_next_gaussian);
    CHECK(!f->s && f->t == NULL && f->v == NULL && f->w == NULL);
    CHECK((f->x != NULL) == has_info && (f->z != NULL) == has_saved);
    CHECK(!f->y && !f->J && !f->L && f->M == 0);
    CHECK(f->N == (pc >= 213) && f->O == (pc >= 218));
    CHECK((f->P != NULL) == (pc >= 229));
    if (f->P != NULL)
        CHECK((const void *)f->P != (const void *)f->E && (const void *)f->P != (const void *)f->F);
    CHECK(pc >= 243 ? (f->Q >= 0 && f->Q < 12000) : f->Q == 0);
    CHECK(!f->B);
    if (f->z != NULL)
        CHECK(mc_world_saved_data_count(f->z) == 0);
    result = 0;
cleanup:
    mc_scheduled_tick_release(first);
    return result;
}

static void drop_world(mc_world_runtime **world) {
    if (*world != NULL) {
        (void)mc_world_runtime_retire_world(*world, NULL);
        (void)mc_world_runtime_free(*world, NULL);
        *world = NULL;
    }
}

static int test_raw(mc_world_info_side side) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_world_runtime *world = NULL, *other = NULL, *candidate = NULL;
    mc_world_runtime_class_state class_state;
    mc_world_runtime_construction state, before;
    mc_world_runtime_fields fields, unchanged;
    mc_world_storage_error error;
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    unsigned int bindings = 0;
    uint64_t initial = 0;
    const mc_world_runtime_entry entry = side == MC_WORLD_INFO_CLIENT
                                             ? MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE
                                             : MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    initial = uniquifier(profile);
    CHECK(mc_world_runtime_classes_create(side, profile, &trig, &classes, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_get(classes, &class_state) == MC_WORLD_STORAGE_OK);
    CHECK(class_state.side == side && !class_state.initialized && class_state.enqueue_depth == 0 &&
          class_state.live_worlds == 0);
    limits.max_direct_allocation_bytes = 0;
    CHECK(mc_world_runtime_allocate_world(entry, classes, &limits, &candidate, &error) ==
          MC_WORLD_STORAGE_LIMIT);
    CHECK(candidate == NULL &&
          mc_world_runtime_classes_get(classes, &class_state) == MC_WORLD_STORAGE_OK);
    CHECK(!class_state.initialized && class_state.live_worlds == 0 &&
          uniquifier(profile) == initial);
    limits = mc_world_runtime_default_construction_limits();
    limits.max_scheduled_entries = (size_t)INT32_MAX + 1u;
    CHECK(mc_world_runtime_allocate_world(entry, classes, &limits, &candidate, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(candidate == NULL);
    CHECK(mc_world_runtime_classes_get(classes, &class_state) == MC_WORLD_STORAGE_OK &&
          !class_state.initialized && class_state.live_worlds == 0);
    CHECK(mc_world_runtime_allocate_world(entry, classes, NULL, &world, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(world, &state) == MC_WORLD_STORAGE_OK);
    CHECK(state.mode == MC_WORLD_RUNTIME_GAME && state.phase == MC_WORLD_RUNTIME_RAW &&
          state.entry == entry && state.last_store_pc == 0 && state.failed_call_pc == 0 &&
          !state.contexts_bound && !state.cache_registered &&
          state.direct_allocation_bytes == sizeof(*world));
    CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK &&
          check_fields(&fields, 0, false, false) == 0);
    before = state;
    CHECK(mc_world_runtime_construct_world(world, NULL, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_world_runtime_construction_get(world, &state) == MC_WORLD_STORAGE_OK &&
          same_construction(&before, &state));
    CHECK(mc_world_runtime_fields_get(world, &unchanged) == MC_WORLD_STORAGE_OK &&
          same_fields(&fields, &unchanged));
    unchanged.h = 91;
    fields = unchanged;
    CHECK(mc_world_runtime_fields_get(NULL, &unchanged) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          same_fields(&fields, &unchanged));
    CHECK(mc_world_runtime_fields_get(world, NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_world_runtime_construction_get(NULL, &state) == MC_WORLD_STORAGE_INVALID_ARGUMENT &&
          same_construction(&before, &state));
    CHECK(mc_world_runtime_construction_get(world, NULL) == MC_WORLD_STORAGE_INVALID_ARGUMENT);
    limits = mc_world_runtime_default_construction_limits();
    limits.max_direct_allocation_bytes = 0;
    candidate = world; /* A failure guard, not another owned World hold. */
    CHECK(mc_world_runtime_allocate_world(entry, classes, &limits, &candidate, &error) ==
          MC_WORLD_STORAGE_LIMIT);
    CHECK(candidate == world);
    candidate = NULL;
    CHECK(mc_world_runtime_random_profile(world) == profile &&
          mc_world_runtime_live(world) == NULL);
    CHECK(mc_world_runtime_acquisition(world) == NULL &&
          mc_world_runtime_lifecycle(world) == NULL && mc_world_runtime_saving(world) == NULL);
    CHECK(mc_world_runtime_allocate_world(entry, classes, NULL, &other, &error) ==
              MC_WORLD_STORAGE_OK &&
          other != world);
    CHECK(mc_world_runtime_classes_get(classes, &class_state) == MC_WORLD_STORAGE_OK &&
          class_state.initialized && class_state.live_worlds == 2);
    CHECK(mc_world_runtime_classes_free(classes, &error) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_runtime_binding_retain(world));
    ++bindings;
    CHECK(mc_world_runtime_binding_retain(world));
    ++bindings;
    CHECK(mc_world_runtime_free(world, &error) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_runtime_retire_world(world, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_retire_world(world, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_construction_get(world, &state) == MC_WORLD_STORAGE_OK &&
          state.phase == MC_WORLD_RUNTIME_RETIRED);
    CHECK(!mc_world_runtime_binding_retain(world));
    CHECK(mc_world_runtime_free(world, &error) == MC_WORLD_STORAGE_BUSY);
    mc_world_runtime_binding_release(world);
    --bindings;
    CHECK(mc_world_runtime_free(world, &error) == MC_WORLD_STORAGE_BUSY);
    mc_world_runtime_binding_release(world);
    --bindings;
    CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK &&
          check_fields(&fields, 0, false, false) == 0);
    CHECK(mc_world_runtime_free(world, &error) == MC_WORLD_STORAGE_OK);
    world = NULL;
    CHECK(mc_world_runtime_classes_get(classes, &class_state) == MC_WORLD_STORAGE_OK &&
          class_state.initialized && class_state.live_worlds == 1);
    CHECK(mc_world_runtime_retire_world(other, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_free(other, &error) == MC_WORLD_STORAGE_OK);
    other = NULL;
    CHECK(mc_world_runtime_classes_get(classes, &class_state) == MC_WORLD_STORAGE_OK &&
          class_state.initialized && class_state.enqueue_depth == 0 &&
          class_state.live_worlds == 0);
    CHECK(uniquifier(profile) == initial && cold(profile));
    result = 0;
cleanup:
    while (bindings != 0) {
        mc_world_runtime_binding_release(world);
        --bindings;
    }
    if (candidate == world || candidate == other)
        candidate = NULL;
    drop_world(&candidate);
    drop_world(&world);
    drop_world(&other);
    (void)mc_world_runtime_classes_free(classes, NULL);
    mc_java_random_profile_free(profile);
    return result;
}

static int test_tail(mc_world_runtime_entry entry, mc_world_info_side side) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_world_runtime *world = NULL;
    mc_world_info_name *name = NULL;
    mc_world_info *held_info = NULL;
    mc_world_runtime_fields fields, unchanged;
    mc_world_runtime_construction state, before;
    mc_world_runtime_class_state class_state;
    mc_world_info_fields info_fields;
    mc_world_storage_error error;
    mc_level_info read_info;
    const mc_world_runtime_state *live = NULL;
    const mc_world_info_live *cells = NULL;
    mc_world_runtime_world_arguments arguments = {0};
    uint64_t initial = 0, expected = 0;
    size_t allocated = 0;
    int64_t lower = 0, upper = 0;
    unsigned int bindings = 0;
    const uint16_t title[] = {'W', 0, 0xd800, 'x', 0xdc00};
    const bool direct = entry == MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA;
    const bool copying = entry == MC_WORLD_RUNTIME_CLIENT_COPY;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    initial = uniquifier(profile);
    allocated = mc_java_random_profile_allocation_bytes(profile);
    CHECK(mc_world_runtime_classes_create(side, profile, &trig, &classes, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, sizeof(title) / sizeof(title[0])},
                                  SIZE_MAX, &name) == MC_WORLD_INFO_OK);
    arguments.name = name;
    arguments.seed = INT64_MIN + 17;
    CHECK(mc_world_runtime_allocate_world(entry, classes, NULL, &world, &error) ==
          MC_WORLD_STORAGE_OK);
    CHECK(mc_platform_current_time_millis(&lower) == MC_PLATFORM_MILLIS_OK);
    CHECK(mc_world_runtime_construct_world(world, &arguments, &error) ==
          MC_WORLD_STORAGE_INVALID_ARGUMENT);
    CHECK(mc_platform_current_time_millis(&upper) == MC_PLATFORM_MILLIS_OK);
    CHECK(mc_world_runtime_construction_get(world, &state) == MC_WORLD_STORAGE_OK);
    CHECK(state.mode == MC_WORLD_RUNTIME_GAME &&
          state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && state.entry == entry);
    CHECK(state.last_store_pc == (copying  ? 259u
                                  : direct ? 295u
                                           : 276u) &&
          state.failed_call_pc == (copying  ? 264u
                                   : direct ? 300u
                                            : 281u));
    CHECK(!state.contexts_bound && !state.cache_registered &&
          state.random_status == MC_JAVA_RANDOM_PROFILE_OK &&
          state.clock_status == MC_PLATFORM_MILLIS_OK);
    CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK &&
          check_fields(&fields, 259, direct, !copying) == 0);
    if (lower <= upper)
        CHECK(fields.I >= lower && fields.I <= upper);
    /* Full-width class-state transitions, never a synthesized clock/LCG seed. */
    expected = initial * UINT64_C(1181783497276652981);
    expected *= UINT64_C(1181783497276652981);
    CHECK(uniquifier(profile) == expected && cold(profile));
    CHECK(mc_java_random_profile_allocation_bytes(profile) == allocated);
    CHECK(mc_world_runtime_random_profile(world) == profile &&
          mc_world_runtime_info(world) == fields.x);
    CHECK(mc_world_runtime_classes_free(classes, &error) == MC_WORLD_STORAGE_BUSY);
    if (direct) {
        CHECK(mc_world_info_fields_get(fields.x, &info_fields) == MC_WORLD_INFO_OK &&
              info_fields.value.seed == arguments.seed && info_fields.name == name);
        live = mc_world_runtime_live(world);
        cells = mc_world_info_live_get(fields.x);
        CHECK(live != NULL && cells != NULL && mc_world_runtime_live(world) == live);
        CHECK(live->time == cells->time && live->spawn_x == cells->spawn_x &&
              live->spawn_y == cells->spawn_y && live->spawn_z == cells->spawn_z);
        CHECK(live->world_force_load == &world->fields.y &&
              live->provider_force_load == &world->provider_force_load &&
              live->saving_disabled == &world->saving_disabled);
        *live->time = INT64_MIN + 7;
        CHECK(mc_world_info_spawn_set(fields.x, INT32_MIN, -7, INT32_MAX) == MC_WORLD_INFO_OK);
        CHECK(*live->spawn_x == INT32_MIN && *live->spawn_y == -7 && *live->spawn_z == INT32_MAX);
        CHECK(mc_world_info_fields_get(fields.x, &info_fields) == MC_WORLD_INFO_OK &&
              info_fields.value.time == INT64_MIN + 7);
        CHECK(mc_world_runtime_read_info(world, &read_info, &error) == MC_WORLD_STORAGE_OK &&
              read_info.time == INT64_MIN + 7 && read_info.spawn_x == INT32_MIN);
        *live->world_force_load = true;
        *live->provider_force_load = true;
        *live->saving_disabled = true;
        CHECK(world->fields.y && world->provider_force_load && world->saving_disabled);
        CHECK(mc_world_info_retain(fields.x) == MC_WORLD_INFO_OK);
        held_info = fields.x;
    } else {
        CHECK(mc_world_runtime_live(world) == NULL);
    }
    CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK);
    before = state;
    CHECK(mc_world_runtime_construct_world(world, &arguments, &error) ==
          MC_WORLD_STORAGE_UNSUPPORTED_STATE);
    CHECK(mc_world_runtime_fields_get(world, &unchanged) == MC_WORLD_STORAGE_OK &&
          same_fields(&fields, &unchanged));
    CHECK(mc_world_runtime_construction_get(world, &state) == MC_WORLD_STORAGE_OK &&
          same_construction(&before, &state));
    CHECK(uniquifier(profile) == expected && cold(profile));
    CHECK(mc_world_runtime_binding_retain(world));
    ++bindings;
    CHECK(mc_world_runtime_retire_world(world, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_retire_world(world, &error) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_live(world) == NULL && !mc_world_runtime_binding_retain(world));
    CHECK(mc_world_runtime_fields_get(world, &unchanged) == MC_WORLD_STORAGE_OK &&
          same_fields(&fields, &unchanged));
    CHECK(mc_world_runtime_free(world, &error) == MC_WORLD_STORAGE_BUSY);
    CHECK(mc_world_runtime_classes_get(classes, &class_state) == MC_WORLD_STORAGE_OK &&
          class_state.live_worlds == 1);
    mc_world_runtime_binding_release(world);
    --bindings;
    CHECK(mc_world_runtime_free(world, &error) == MC_WORLD_STORAGE_OK);
    world = NULL;
    if (held_info != NULL) {
        CHECK(mc_world_info_fields_get(held_info, &info_fields) == MC_WORLD_INFO_OK &&
              info_fields.name == name && info_fields.value.time == INT64_MIN + 7);
        CHECK(mc_world_info_live_get(held_info) == cells && *cells->spawn_z == INT32_MAX);
    }
    CHECK(mc_world_runtime_classes_get(classes, &class_state) == MC_WORLD_STORAGE_OK &&
          class_state.live_worlds == 0 && class_state.initialized);
    CHECK(uniquifier(profile) == expected && cold(profile));
    result = 0;
cleanup:
    while (bindings != 0) {
        mc_world_runtime_binding_release(world);
        --bindings;
    }
    drop_world(&world);
    mc_world_info_release(held_info);
    mc_world_info_name_release(name);
    (void)mc_world_runtime_classes_free(classes, NULL);
    mc_java_random_profile_free(profile);
    return result;
}

static int test_budget_prefixes(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_world_runtime *world = NULL;
    mc_world_runtime_fields fields;
    mc_world_runtime_construction state;
    mc_world_runtime_class_state class_state;
    mc_world_storage_error error;
    mc_world_runtime_world_arguments arguments = {0};
    mc_world_runtime_construction_limits limits = mc_world_runtime_default_construction_limits();
    uint64_t initial = 0;
    unsigned int seen = 0;
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    initial = uniquifier(profile);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, &trig, &classes, &error) ==
          MC_WORLD_STORAGE_OK);
    limits.max_scheduled_entries = 0;
    /* A finite native admission search discovers opaque owner sizes instead of
     * hardcoding ArrayList/TreeSet/HashSet layouts. Stop at F's first refusal,
     * before either Random constructor or any clock/file operation is reached. */
    for (size_t extra = 0; extra < 4096 && seen != 31u; ++extra) {
        limits.max_direct_allocation_bytes = sizeof(*world) + extra;
        CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_COPY, classes, &limits,
                                              &world, &error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_construct_world(world, &arguments, &error) ==
              MC_WORLD_STORAGE_LIMIT);
        CHECK(mc_world_runtime_construction_get(world, &state) == MC_WORLD_STORAGE_OK);
        CHECK(state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION && !state.contexts_bound &&
              !state.cache_registered);
        switch (state.failed_call_pc) {
            case 14:
                CHECK(state.last_store_pc == 6);
                seen |= 1u;
                break;
            case 25:
                CHECK(state.last_store_pc == 17);
                seen |= 2u;
                break;
            case 36:
                CHECK(state.last_store_pc == 28);
                seen |= 4u;
                break;
            case 47:
                CHECK(state.last_store_pc == 39);
                seen |= 8u;
                break;
            case 58:
                CHECK(state.last_store_pc == 50);
                seen |= 16u;
                break;
            default:
                CHECK(false);
        }
        CHECK(state.direct_allocation_bytes >= sizeof(*world) &&
              state.direct_allocation_bytes <= limits.max_direct_allocation_bytes);
        CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK &&
              check_fields(&fields, state.last_store_pc, false, false) == 0);
        CHECK(uniquifier(profile) == initial && cold(profile));
        CHECK(mc_world_runtime_retire_world(world, &error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_free(world, &error) == MC_WORLD_STORAGE_OK);
        world = NULL;
        CHECK(mc_world_runtime_classes_get(classes, &class_state) == MC_WORLD_STORAGE_OK &&
              class_state.live_worlds == 0);
    }
    CHECK(seen == 31u);
    result = 0;
cleanup:
    drop_world(&world);
    (void)mc_world_runtime_classes_free(classes, NULL);
    mc_java_random_profile_free(profile);
    return result;
}

static mc_world_storage_status construct_fp(mc_world_runtime *world,
                                            const mc_world_runtime_world_arguments *arguments,
                                            unsigned int control, mc_world_storage_error *error) {
    const unsigned int saved = _mm_getcsr();
    _mm_setcsr(control);
    const mc_world_storage_status status =
        mc_world_runtime_construct_world(world, arguments, error);
    _mm_setcsr(saved); /* Restore before CHECK, formatting or any cleanup. */
    return status;
}

static int test_random_failure(void) {
    int result = 1;
    mc_java_random_profile *profile = NULL;
    mc_world_runtime_classes *classes = NULL;
    mc_world_runtime *world = NULL;
    mc_world_runtime_fields fields, unchanged;
    mc_world_runtime_construction state, before;
    mc_world_storage_error error;
    mc_world_runtime_world_arguments arguments = {0};
    uint64_t initial = 0;
    const unsigned int saved = _mm_getcsr();
    const unsigned int controls[] = {saved | 0x8000u, saved | 0x0040u,
                                     (saved & ~0x6000u) | 0x2000u};
    CHECK(mc_java_random_profile_create(SIZE_MAX, &profile) == MC_JAVA_RANDOM_PROFILE_OK);
    initial = uniquifier(profile);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, profile, &trig, &classes, &error) ==
          MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < sizeof(controls) / sizeof(controls[0]); ++i) {
        CHECK(mc_world_runtime_allocate_world(MC_WORLD_RUNTIME_CLIENT_COPY, classes, NULL, &world,
                                              &error) == MC_WORLD_STORAGE_OK);
        CHECK(construct_fp(world, &arguments, controls[i], &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        CHECK(_mm_getcsr() == saved);
        CHECK(mc_world_runtime_construction_get(world, &state) == MC_WORLD_STORAGE_OK &&
              state.phase == MC_WORLD_RUNTIME_FAILED_CONSTRUCTION);
        CHECK(state.last_store_pc == 117 && state.failed_call_pc == 125 &&
              state.random_status == MC_JAVA_RANDOM_PROFILE_UNSUPPORTED_FP);
        CHECK(!state.contexts_bound && !state.cache_registered &&
              state.clock_status == MC_PLATFORM_MILLIS_OK);
        CHECK(mc_world_runtime_fields_get(world, &fields) == MC_WORLD_STORAGE_OK &&
              check_fields(&fields, 117, false, false) == 0);
        CHECK(uniquifier(profile) == initial && cold(profile));
        before = state;
        CHECK(mc_world_runtime_construct_world(world, &arguments, &error) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE);
        CHECK(mc_world_runtime_construction_get(world, &state) == MC_WORLD_STORAGE_OK &&
              same_construction(&before, &state));
        CHECK(mc_world_runtime_fields_get(world, &unchanged) == MC_WORLD_STORAGE_OK &&
              same_fields(&fields, &unchanged));
        CHECK(uniquifier(profile) == initial && cold(profile));
        CHECK(mc_world_runtime_retire_world(world, &error) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_free(world, &error) == MC_WORLD_STORAGE_OK);
        world = NULL;
    }
    result = 0;
cleanup:
    _mm_setcsr(saved);
    drop_world(&world);
    (void)mc_world_runtime_classes_free(classes, NULL);
    mc_java_random_profile_free(profile);
    return result;
}

int main(void) {
    if (!mc_math_trig_init(&trig))
        return 1;
    if (test_raw(MC_WORLD_INFO_CLIENT) != 0 || test_raw(MC_WORLD_INFO_SERVER) != 0 ||
        test_tail(MC_WORLD_RUNTIME_CLIENT_COPY, MC_WORLD_INFO_CLIENT) != 0 ||
        test_tail(MC_WORLD_RUNTIME_CLIENT_LOAD_OR_CREATE, MC_WORLD_INFO_CLIENT) != 0 ||
        test_tail(MC_WORLD_RUNTIME_CLIENT_DIRECT_METADATA, MC_WORLD_INFO_CLIENT) != 0 ||
        test_tail(MC_WORLD_RUNTIME_SERVER_LOAD_OR_CREATE, MC_WORLD_INFO_SERVER) != 0 ||
        test_budget_prefixes() != 0 || test_random_failure() != 0)
        return 1;
    printf("PASS: %u constructed World-prefix checks\n", checks);
    return 0;
}
