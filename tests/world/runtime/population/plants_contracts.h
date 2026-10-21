#include "mc/world/runtime/population/plants.h"

/* Source-derived native trials over actual World/Chunk fixtures.
 * These expectations do not establish original/native feature parity. Independent Random prefix
 * controls prepare ONE source candidate; no complete feature implementation serves as a test
 * oracle. */

typedef enum pop_family {
    POP_FLOWER,
    POP_TALL,
    POP_DEAD,
    POP_REED,
    POP_PUMPKIN,
    POP_CACTUS
} pop_family;

typedef enum pop_control {
    POP_NORMAL,
    POP_REJECT,
    POP_BLOCKED,
    POP_NULL_M,
    POP_BAD_INDEX,
    POP_NULL_ELEMENT,
    POP_WRONG_CAST,
    POP_NULL_SOIL,
    POP_NULL_LIGHT,
    POP_NULL_O,
    POP_NULL_STATIC,
    POP_NULL_NIBBLE,
    POP_SECOND_OLD_NULL,
    POP_SOIL_REJECT,
    POP_EQUAL_AIR,
    POP_LAYER_CONTINUE
} pop_control;

typedef struct pop_case {
    pop_family family;
    const char *receiver, *soil;
    pop_control control;
    int32_t inner, outer, orientation;
    unsigned int water_direction;
    bool negative;
    int32_t metadata;
} pop_case;

typedef struct pop_probe {
    int64_t seed;
    int32_t x, y, z, inner, outer, height, orientation;
    mc_java_random coordinates, conditional;
} pop_probe;

typedef struct pop_snapshot {
    mc_chunk_data data;
    uint8_t blocks[MC_CHUNK_BLOCKS];
    mc_world_chunk_nibble *e, *f, *g;
    uint8_t *h;
    mc_block **m;
    mc_block *elements[MC_REGISTRY_BLOCK_SLOTS], *named[96];
    bool *o;
} pop_snapshot;

static const unsigned int pop_coordinate_draws[] = {384, 768, 24, 80, 384, 60};
static const unsigned int pop_invoke_pc[] = {92, 133, 131, 176, 105, 112};
static const unsigned int pop_write_pc[] = {109, 154, 148, 198, 129, 134};
static const unsigned int pop_element_pc[] = {81, 122, 120};
static const unsigned int pop_cast_pc[] = {82, 123, 121};
static const char *const pop_client_class[] = {"be", "ap", "kt", "ir", "wx", "fx"};
static const char *const pop_server_class[] = {"am", "ad", "gw", "fs", "op", "ea"};

static size_t pop_index(int32_t x, int32_t y, int32_t z) {
    return (((size_t)(uint32_t)x & 15u) << 11u) | (((size_t)(uint32_t)z & 15u) << 7u) | (size_t)y;
}

static void pop_seed(mc_java_random *r, int64_t seed) {
    mc_java_random_seed(r, seed);
    /* Prepare an actual seeded continuation through its owner API. Do not
     * forge the opaque LCG or Gaussian-cache protocol to select a test path. */
}

static bool pop_same_random(const mc_java_random *a, const mc_java_random *b) {
    return a->state == b->state && a->have_next_next_gaussian == b->have_next_next_gaussian &&
           memcmp(&a->next_next_gaussian, &b->next_next_gaussian, sizeof(a->next_next_gaussian)) ==
               0;
}

static bool pop_steps(mc_java_random *r, unsigned int count) {
    int32_t ignored;
    /* All coordinate bounds are powers of two: one next(31) per invocation.
     * Bound2 is an independent continuation budget, not a coordinate replay. */
    for (unsigned int i = 0; i < count; ++i)
        if (!mc_java_random_next_bounded(r, 2, &ignored))
            return false;
    return true;
}

static bool pop_axis(mc_java_random *r, int32_t origin, int32_t bound, int32_t *out) {
    int32_t plus, minus;
    if (!mc_java_random_next_bounded(r, bound, &plus) ||
        !mc_java_random_next_bounded(r, bound, &minus))
        return false;
    *out = mc_java_i32_sub(mc_java_i32_add(origin, plus), minus);
    return true;
}

static bool pop_layer_probe_no_revisit(const pop_case *t, const pop_probe *p) {
    if (t->family != POP_REED && t->family != POP_CACTUS)
        return false;
    mc_java_random later = p->conditional;
    const int32_t origin = t->negative ? -8 : 8;
    const unsigned int attempts = t->family == POP_REED ? 20u : 10u;
    for (unsigned int attempt = 1; attempt < attempts; ++attempt) {
        int32_t x, y = 64, z;
        if (!pop_axis(&later, origin, t->family == POP_REED ? 4 : 8, &x) ||
            (t->family != POP_REED && !pop_axis(&later, 64, 4, &y)) ||
            !pop_axis(&later, 8, t->family == POP_REED ? 4 : 8, &z))
            return false;
        if (x == p->x && y == p->y && z == p->z)
            return false;
    }
    return true;
}

static bool pop_find_probe(const pop_case *t, pop_probe *p) {
    const int32_t origin = t->negative ? -8 : 8;
    for (int64_t seed = 0; seed < 65536; ++seed) {
        mc_java_random r;
        pop_seed(&r, seed);
        *p = (pop_probe){.seed = seed, .height = 1, .y = 64};
        if (!pop_axis(&r, origin, t->family == POP_REED ? 4 : 8, &p->x) ||
            (t->family != POP_REED && !pop_axis(&r, 64, 4, &p->y)) ||
            !pop_axis(&r, 8, t->family == POP_REED ? 4 : 8, &p->z))
            return false;
        p->coordinates = r;
        if (t->family == POP_REED || t->family == POP_CACTUS) {
            if (!mc_java_random_next_bounded(&r, 3, &p->inner) ||
                !mc_java_random_next_bounded(&r, p->inner + 1, &p->outer))
                return false;
            p->height = p->outer + (t->family == POP_REED ? 2 : 1);
        } else if (t->family == POP_PUMPKIN &&
                   !mc_java_random_next_bounded(&r, 4, &p->orientation)) {
            return false;
        }
        p->conditional = r;
        const unsigned int local_x = (uint32_t)p->x & 15u;
        if (local_x <= 1u || local_x >= 14u || p->z <= 1 || p->z >= 14 ||
            (p->x == origin && p->y == 64 && p->z == 8))
            continue;
        if ((t->inner >= 0 && p->inner != t->inner) || (t->outer >= 0 && p->outer != t->outer) ||
            (t->orientation >= 0 && p->orientation != t->orientation))
            continue;
        if (t->control == POP_LAYER_CONTINUE && !pop_layer_probe_no_revisit(t, p))
            continue;
        return true;
    }
    return false; /* Test input search failure is never a feature success. */
}

static mc_block **pop_named_cell(mc_registry_classes *r, const char *name) {
    for (size_t i = 0; i < COUNT(r->blocks.named); ++i) {
        const char *actual = mc_registry_block_static_name(i);
        if (actual != NULL && strcmp(actual, name) == 0)
            return &r->blocks.named[i];
    }
    return NULL;
}

static bool pop_block(fixture *f, size_t side, const char *name, const mc_block **out) {
    return mc_registry_block_static_get(f->registry[side], name, out, NULL) == MC_REGISTRY_OK &&
           *out != NULL && (*out)->fields.bn > 0 && (*out)->fields.bn <= UINT8_MAX;
}

static void pop_save(pop_snapshot *s, mc_world_chunk *c, mc_registry_classes *r) {
    s->data = *c->data;
    memcpy(s->blocks, c->data->blocks, sizeof(s->blocks));
    s->e = c->e;
    s->f = c->f;
    s->g = c->g;
    s->h = c->h;
    s->m = r->blocks.m;
    s->o = r->blocks.o;
    memcpy(s->elements, r->blocks.m, sizeof(s->elements));
    memcpy(s->named, r->blocks.named, sizeof(s->named));
}

static void pop_restore(const pop_snapshot *s, mc_world_chunk *c, mc_registry_classes *r) {
    *c->data = s->data;
    memcpy(c->data->blocks, s->blocks, sizeof(s->blocks));
    c->e = s->e;
    c->f = s->f;
    c->g = s->g;
    c->h = s->h;
    r->blocks.m = s->m;
    r->blocks.o = s->o;
    memcpy(r->blocks.m, s->elements, sizeof(s->elements));
    memcpy(r->blocks.named, s->named, sizeof(s->named));
    /* Do not restore dirty, real leases, cache/pending entries, queue contents,
     * source shared Chunk.a or class phases. Genuine reached effects survive. */
}

static mc_world_storage_status pop_call(const pop_case *t, mc_world_runtime *w, mc_java_random *r,
                                        int32_t id, int32_t x, int32_t y, int32_t z, bool *out,
                                        mc_population_plant_feature_error *e) {
    mc_population_flower_feature flower;
    mc_population_tall_grass_feature tall;
    mc_population_dead_bush_feature dead;
    switch (t->family) {
        case POP_FLOWER:
            if (mc_population_flower_feature_init(&flower, id) != MC_WORLD_STORAGE_OK)
                return MC_WORLD_STORAGE_INVALID_ARGUMENT;
            return mc_population_flowers_place(&flower, w, r, x, y, z, out, e);
        case POP_TALL:
            if (mc_population_tall_grass_feature_init(&tall, id, t->metadata) !=
                MC_WORLD_STORAGE_OK)
                return MC_WORLD_STORAGE_INVALID_ARGUMENT;
            return mc_population_tall_grass_place(&tall, w, r, x, y, z, out, e);
        case POP_DEAD:
            if (mc_population_dead_bush_feature_init(&dead, id) != MC_WORLD_STORAGE_OK)
                return MC_WORLD_STORAGE_INVALID_ARGUMENT;
            return mc_population_dead_bush_place(&dead, w, r, x, y, z, out, e);
        case POP_REED:
            return mc_population_reeds_place(w, r, x, y, z, out, e);
        case POP_PUMPKIN:
            return mc_population_pumpkins_place(w, r, x, y, z, out, e);
        case POP_CACTUS:
            return mc_population_cacti_place(w, r, x, y, z, out, e);
    }
    return MC_WORLD_STORAGE_INVALID_ARGUMENT;
}

static bool pop_trial(fixture *f, size_t side, mc_world_chunk *negative, const pop_case *t) {
    bool ok = false, prepared = false, out = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_world_chunk *c =
        t->negative ? negative : mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    pop_snapshot *saved = NULL;
    const mc_block *receiver, *soil, *stone, *water;
    pop_probe p;
    CHECK(pop_block(f, side, t->receiver, &receiver) && pop_block(f, side, t->soil, &soil) &&
          pop_block(f, side, "u", &stone) && pop_block(f, side, "C", &water));
    CHECK(c != NULL && c->data != NULL && c->data->blocks != NULL && c->e != NULL && c->f != NULL &&
          c->g != NULL && c->h != NULL && r->blocks.m != NULL && COUNT(r->blocks.named) == 96 &&
          pop_find_probe(t, &p));
    saved = malloc(sizeof(*saved));
    CHECK(saved != NULL);
    pop_save(saved, c, r);
    prepared = true;
    const uint8_t fill = (uint8_t)(t->family == POP_CACTUS ? water->fields.bn : stone->fields.bn);
    memset(c->data->blocks, fill, MC_CHUNK_BLOCKS);
    memset(c->data->metadata, 0xaa, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->block_light, strcmp(t->receiver, "ag") == 0 ? 0xcc : 0x88,
           MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->heightmap, 127, MC_CHUNK_HEIGHTMAP_BYTES);
    const size_t cell = pop_index(p.x, p.y, p.z);
    c->data->blocks[cell] = 0;
    c->data->blocks[pop_index(p.x, p.y - 1, p.z)] = (uint8_t)soil->fields.bn;
    if (t->family == POP_REED && t->control != POP_REJECT) {
        const int32_t dx[] = {-1, 1, 0, 0}, dz[] = {0, 0, -1, 1};
        const unsigned int direction = t->water_direction;
        if (direction >= COUNT(dx))
            goto cleanup;
        c->data->blocks[pop_index(p.x + dx[direction], p.y - 1, p.z + dz[direction])] =
            (uint8_t)water->fields.bn;
    }
    if (t->control == POP_BLOCKED) {
        c->data->blocks[cell] = fill;
        r->blocks.m = NULL;
        const char *const skipped[] = {"v", "w", "aB", "aY", "aW", "bb"};
        for (size_t i = 0; i < COUNT(skipped); ++i)
            *pop_named_cell(r, skipped[i]) = NULL;
    }
    int32_t id = receiver->fields.bn;
    if (t->control == POP_EQUAL_AIR) {
        /* Real mutable m[0] selects this actual receiver, while the actual
         * feature constructor stores0. Successful survival reaches a quiet
         * id0 write to existing air: ordinary false must be discarded. */
        r->blocks.m[0] = (mc_block *)receiver;
        id = 0;
        if (t->family == POP_TALL) {
            const unsigned int shift = ((uint32_t)p.y & 1u) * 4u;
            c->data->metadata[cell / 2u] =
                (uint8_t)((c->data->metadata[cell / 2u] & ~(15u << shift)) |
                          (((uint32_t)t->metadata & 15u) << shift));
        }
    }
    if (t->control == POP_NULL_M) {
        r->blocks.m = NULL;
        if (t->family <= POP_DEAD)
            id = -1; /* Captured NULL must fail before this signed index. */
    }
    if (t->control == POP_BAD_INDEX)
        id = -1;
    if (t->control == POP_NULL_ELEMENT)
        r->blocks.m[id] = NULL;
    if (t->control == POP_WRONG_CAST)
        r->blocks.m[id] = (mc_block *)stone;
    if (t->control == POP_NULL_SOIL)
        *pop_named_cell(r, t->soil) = NULL;
    if (t->control == POP_NULL_LIGHT)
        c->g = NULL;
    if (t->control == POP_NULL_O)
        r->blocks.o = NULL;
    if (t->control == POP_NULL_STATIC)
        *pop_named_cell(r, t->family == POP_REED     ? "aY"
                           : t->family == POP_CACTUS ? "aW"
                                                     : "bb") = NULL;
    if (t->control == POP_NULL_NIBBLE)
        c->e = NULL;
    if (t->control == POP_SECOND_OLD_NULL) {
        c->data->blocks[pop_index(p.x, p.y + 1, p.z)] = (uint8_t)stone->fields.bn;
        r->blocks.m[stone->fields.bn] = NULL;
    }
    const size_t count = f->providers[side].cache->count;
    const size_t insertions = f->providers[side].cache->insertion_count;
    mc_java_random random, expected;
    pop_seed(&random, p.seed);
    const unsigned int prefix_draws = t->family == POP_REED ? 4u : 6u;
    expected = p.conditional;
    if (t->control == POP_BLOCKED || t->control == POP_REJECT ||
        (t->control == POP_NULL_M && t->family != POP_CACTUS) ||
        (t->control == POP_NULL_STATIC && t->family == POP_PUMPKIN))
        expected = p.coordinates;
    const bool completes = t->control == POP_NORMAL || t->control == POP_REJECT ||
                           t->control == POP_BLOCKED || t->control == POP_SOIL_REJECT ||
                           t->control == POP_EQUAL_AIR;
    bool budget_ok = true;
    if (completes && t->control != POP_SOIL_REJECT)
        budget_ok = pop_steps(&expected, pop_coordinate_draws[t->family] - prefix_draws);
    const bool initial_output = t->control == POP_NULL_NIBBLE || t->control == POP_SECOND_OLD_NULL;
    out = initial_output;
    mc_population_plant_feature_error e = {0};
    const mc_world_storage_status status =
        pop_call(t, w, &random, id, t->negative ? -8 : 8, 64, 8, &out, &e);
    const uint8_t placed = c->data->blocks[cell];
    const uint8_t metadata =
        (uint8_t)(((uint32_t)c->data->metadata[cell / 2u] >> ((p.y & 1) * 4)) & 15u);
    const size_t next = pop_index(p.x, p.y + 1, p.z);
    const uint8_t placed_next = c->data->blocks[next];
    const uint8_t metadata_next =
        (uint8_t)(((uint32_t)c->data->metadata[next / 2u] >> (((p.y + 1) & 1) * 4)) & 15u);
    size_t matches = 0;
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i)
        if (c->data->blocks[i] == (uint8_t)receiver->fields.bn)
            ++matches;
    bool column = true;
    const int32_t wanted_height = t->family == POP_REED || t->family == POP_CACTUS ? p.height : 1;
    for (int32_t layer = 0; layer < wanted_height; ++layer)
        column = column &&
                 c->data->blocks[pop_index(p.x, p.y + layer, p.z)] == (uint8_t)receiver->fields.bn;
    const bool unchanged_random_cache =
        random.have_next_next_gaussian == p.coordinates.have_next_next_gaussian &&
        memcmp(&random.next_next_gaussian, &p.coordinates.next_next_gaussian,
               sizeof(random.next_next_gaussian)) == 0;
    const bool continuation = pop_same_random(&random, &expected);
    const bool retained_cause = same_cause(&e.cause, mc_world_runtime_last_error(w));
    pop_restore(saved, c, r);
    prepared = false; /* No assertion below can strand a nullable control. */
    CHECK(budget_ok && w->active_calls == 0 && unchanged_random_cache &&
          f->providers[side].cache->count == count &&
          f->providers[side].cache->insertion_count == insertions);
    /* POP_REJECT is used only where the initial gate omits conditional draws:
     * soil-mismatch pumpkins/flower descendants and no-water reeds. */
    /* Rejected soil can revisit the still-air cell and draw another height.
     * Its exact budget is intentionally not inferred from the implementation;
     * blocked/wrapped and every one-column success separately check full RNG. */
    if (t->control != POP_SOIL_REJECT)
        CHECK(continuation);
    if (completes) {
        CHECK(status == MC_WORLD_STORAGE_OK && out && e.status == MC_WORLD_STORAGE_OK &&
              e.feature_class == NULL && e.operation == NULL && e.bytecode == 0);
        if (t->control == POP_NORMAL) {
            CHECK(placed == receiver->fields.bn && column && matches == (size_t)wanted_height);
            CHECK(metadata == (t->family == POP_TALL      ? (t->metadata & 15)
                               : t->family == POP_PUMPKIN ? p.orientation
                                                          : 0));
        } else {
            CHECK(matches == 0 && placed == (t->control == POP_BLOCKED ? fill : 0));
        }
    } else {
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out == initial_output &&
              retained_cause && e.feature_class != NULL &&
              strcmp(e.feature_class,
                     side == 0 ? pop_client_class[t->family] : pop_server_class[t->family]) == 0 &&
              e.feature_method != NULL && strcmp(e.feature_method, "a") == 0);
        if (t->control == POP_NULL_NIBBLE || t->control == POP_SECOND_OLD_NULL) {
            CHECK(e.bytecode == pop_write_pc[t->family] && placed == receiver->fields.bn &&
                  e.mutation.status == status && same_cause(&e.cause, &e.mutation.cause));
            if (t->control == POP_NULL_NIBBLE)
                CHECK(matches == 1 && metadata == 10 &&
                      e.mutation.chunk_bytecode ==
                          (t->family == POP_TALL || t->family == POP_PUMPKIN ? 154u : 128u));
            else
                CHECK(p.height >= 2 && matches == 2 && placed_next == receiver->fields.bn &&
                      metadata == 0 && metadata_next == 10 && e.mutation.chunk_bytecode == 117);
        } else {
            CHECK(matches == 0 && placed == 0);
            if (t->control == POP_NULL_M && t->family <= POP_DEAD)
                CHECK(e.bytecode == pop_element_pc[t->family] &&
                      e.registry.status == MC_REGISTRY_NULL_REFERENCE);
            else if (t->control == POP_BAD_INDEX)
                CHECK(e.bytecode == pop_element_pc[t->family] &&
                      e.registry.status == MC_REGISTRY_INDEX_OUT_OF_RANGE);
            else if (t->control == POP_WRONG_CAST)
                CHECK(e.bytecode == pop_cast_pc[t->family] && e.registry.status == MC_REGISTRY_OK &&
                      e.plant.declaring_class == NULL);
            else if (t->control == POP_NULL_M && t->family == POP_REED)
                CHECK(e.bytecode == 69 && e.material.bytecode == 23 &&
                      e.registry.status == MC_REGISTRY_NULL_REFERENCE);
            else {
                CHECK(e.bytecode == pop_invoke_pc[t->family] && e.plant.status == status);
                if (t->control == POP_NULL_ELEMENT || t->control == POP_NULL_STATIC)
                    CHECK(e.plant.declaring_class == NULL);
                if (t->control == POP_NULL_SOIL)
                    CHECK(e.plant.bytecode == 4 && e.plant.caller_bytecode == 35 &&
                          e.registry.status == MC_REGISTRY_NULL_REFERENCE &&
                          e.plant.declaring_class != NULL &&
                          strcmp(e.plant.declaring_class,
                                 strcmp(t->receiver, "aA") == 0 ? (side == 0 ? "ni" : "ij")
                                 : t->family == POP_DEAD        ? (side == 0 ? "jb" : "fz")
                                                                : (side == 0 ? "wb" : "nw")) == 0 &&
                          e.plant.caller_class != NULL &&
                          strcmp(e.plant.caller_class, side == 0 ? "wb" : "nw") == 0);
                if (t->control == POP_NULL_O)
                    CHECK(e.plant.bytecode == 4 && e.plant.caller_bytecode == 37 &&
                          e.registry.status == MC_REGISTRY_NULL_REFERENCE);
                if (t->control == POP_NULL_LIGHT)
                    CHECK(e.plant.bytecode == 5 && e.plant.query.status == status &&
                          e.registry.status == MC_REGISTRY_OK);
            }
        }
    }
    ok = true;
cleanup:
    if (prepared)
        pop_restore(saved, c, r);
    free(saved);
    return ok;
}

static bool pop_wrapped_and_descent(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    /* Reach the installed provider's actual capacity through real acquisitions;
     * no cache counter or native allocation ledger is forged. */
    bool limited = false;
    for (int32_t attempt = 0; attempt < 16; ++attempt) {
        mc_chunk_state *state = NULL;
        const mc_chunk_acquire_status status =
            mc_world_runtime_get(w, 1024 + 4 * attempt, 2048 + 4 * attempt, &state);
        CHECK(remember(f, side));
        if (status == MC_CHUNK_ACQUIRE_LIMIT || status == MC_CHUNK_ACQUIRE_PARTIAL_LIMIT) {
            limited = true;
            break;
        }
        CHECK(status == MC_CHUNK_ACQUIRE_OK && state != NULL);
    }
    CHECK(limited && f->providers[side].cache->insertion_count ==
                         f->providers[side].cache->insertion_capacity);
    /* A canonical flower reaches unbounded sky acquisition after bounded air.
     * Select a first-candidate miss at the now-real publication limit. */
    const mc_block *flower, *mushroom;
    CHECK(pop_block(f, side, "ae", &flower) && pop_block(f, side, "ag", &mushroom));
    mc_java_random border_random, border_expected, border_probe;
    int64_t border_seed;
    for (border_seed = 0; border_seed < 256; ++border_seed) {
        int32_t px, py, pz;
        pop_seed(&border_probe, border_seed);
        CHECK(pop_axis(&border_probe, INT32_MAX, 8, &px) &&
              pop_axis(&border_probe, INT32_MIN, 4, &py) &&
              pop_axis(&border_probe, INT32_MIN, 8, &pz));
        if (py < 0 && !mc_chunk_cache_contains(f->providers[side].cache, mc_java_i32_shr(px, 4),
                                               mc_java_i32_shr(pz, 4)))
            break;
    }
    CHECK(border_seed < 256);
    pop_seed(&border_random, border_seed);
    pop_seed(&border_expected, border_seed);
    CHECK(pop_steps(&border_expected, 6));
    const pop_case border_case = {.family = POP_FLOWER};
    bool border_out = true;
    mc_population_plant_feature_error border_error = {0};
    const mc_world_storage_status border_status =
        pop_call(&border_case, w, &border_random, flower->fields.bn, INT32_MAX, INT32_MIN,
                 INT32_MIN, &border_out, &border_error);
    CHECK(border_status == MC_WORLD_STORAGE_LIMIT && border_out &&
          pop_same_random(&border_random, &border_expected) && border_error.bytecode == 92 &&
          border_error.plant.bytecode == 18 && border_error.plant.query.bytecode == 7 &&
          w->active_calls == 0);
    const char *const receiver[] = {"ae", "Y", "Z", "aY", "bb", "aW"};
    for (unsigned int kind = 0; kind < COUNT(receiver); ++kind) {
        const mc_block *b;
        CHECK(pop_block(f, side, receiver[kind], &b));
        const pop_case t = {.family = (pop_family)kind};
        mc_java_random random, expected;
        pop_seed(&random, 19);
        (void)mc_java_random_next_gaussian(&random);
        CHECK(random.have_next_next_gaussian);
        expected = random;
        if (kind == POP_CACTUS) {
            for (unsigned int attempt = 0; attempt < 10; ++attempt) {
                int32_t inner, ignored;
                CHECK(pop_steps(&expected, 6) &&
                      mc_java_random_next_bounded(&expected, 3, &inner) &&
                      mc_java_random_next_bounded(&expected, inner + 1, &ignored));
            }
        } else {
            CHECK(pop_steps(&expected, pop_coordinate_draws[kind]));
        }
        const size_t count = f->providers[side].cache->count;
        const size_t inserted = f->providers[side].cache->insertion_count;
        bool out = false;
        mc_population_plant_feature_error e = {0};
        /* For these success/continuation controls, the first three features
         * use a real mushroom receiver: its Y gate rejects every wrapped Y
         * before light/sky acquisition. The feature's captured ID stays live. */
        mc_block *old_element = f->registry[side]->blocks.m[b->fields.bn];
        if (kind <= POP_DEAD)
            f->registry[side]->blocks.m[b->fields.bn] = (mc_block *)mushroom;
        const mc_world_storage_status status =
            pop_call(&t, w, &random, b->fields.bn, INT32_MAX, INT32_MIN, INT32_MIN, &out, &e);
        f->registry[side]->blocks.m[b->fields.bn] = old_element;
        CHECK(status == MC_WORLD_STORAGE_OK && out && pop_same_random(&random, &expected) &&
              e.feature_class == NULL && f->providers[side].cache->count == count &&
              f->providers[side].cache->insertion_count == inserted && w->active_calls == 0);
    }
    mc_world_chunk *c = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    const mc_block *stone;
    CHECK(c != NULL && c->data->blocks != NULL && pop_block(f, side, "u", &stone));
    mc_block **leaves = pop_named_cell(f->registry[side], "L");
    CHECK(leaves != NULL && *leaves != NULL);
    const size_t origin = pop_index(8, 64, 8), lower = pop_index(8, 63, 8);
    for (unsigned int kind = POP_TALL; kind <= POP_DEAD; ++kind) {
        const uint8_t a = c->data->blocks[origin], b = c->data->blocks[lower];
        mc_block *old_leaves = *leaves;
        c->data->blocks[origin] = 0;
        c->data->blocks[lower] = (uint8_t)stone->fields.bn;
        *leaves = NULL;
        const pop_case t = {.family = (pop_family)kind};
        bool out = false;
        mc_population_plant_feature_error e = {0};
        const mc_world_storage_status status = pop_call(&t, w, NULL, 0, 8, 64, 8, &out, &e);
        *leaves = old_leaves;
        c->data->blocks[origin] = a;
        c->data->blocks[lower] = b;
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && !out && e.bytecode == 23 &&
              e.registry.status == MC_REGISTRY_NULL_REFERENCE && w->active_calls == 0);
        /* Actual RAW World has a missing reached provider, never forged phase.
         * It fails at the descent World call before the NULL Random invocation. */
        CHECK(pop_call(&t, f->worlds[4u + side], NULL, 0, 8, 64, 8, &out, &e) ==
                  MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
              !out && e.bytecode == 9 && f->worlds[4u + side]->active_calls == 0);
    }
    ok = true;
cleanup:
    return ok;
}

static bool pop_inherited_predicates(fixture *f, size_t side) {
    bool ok = false, prepared = false;
    pop_snapshot *saved = NULL;
    mc_world_chunk *c = NULL;
    const mc_block *sapling, *crops, *grass, *dirt, *farm, *stone, *water;
    CHECK(pop_block(f, side, "z", &sapling) && pop_block(f, side, "aA", &crops) &&
          pop_block(f, side, "v", &grass) && pop_block(f, side, "w", &dirt) &&
          pop_block(f, side, "aB", &farm) && pop_block(f, side, "u", &stone) &&
          pop_block(f, side, "C", &water));
    CHECK(sapling->kind == MC_BLOCK_HE_EU && crops->kind == MC_BLOCK_NI_IJ);
    const mc_block *receivers[] = {sapling, crops}, *soils[] = {grass, dirt, farm, stone};
    for (size_t receiver = 0; receiver < COUNT(receivers); ++receiver)
        for (size_t soil = 0; soil < COUNT(soils); ++soil)
            for (unsigned int placement = 0; placement < 2; ++placement) {
                const plant_case t = {.receiver = receivers[receiver],
                                      .below = (uint8_t)soils[soil]->fields.bn,
                                      .light = 8,
                                      .placement = placement != 0,
                                      .want = receiver == 0 ? soil < 3 : soil == 2};
                CHECK(
                    plant_trial(f, side, &t, (uint8_t)water->fields.bn, (uint8_t)stone->fields.bn));
            }
    c = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    mc_registry_classes *r = f->registry[side];
    CHECK(c != NULL && c->data->blocks != NULL && r->blocks.m != NULL &&
          pop_named_cell(r, "aB") != NULL);
    saved = malloc(sizeof(*saved));
    CHECK(saved != NULL);
    pop_save(saved, c, r);
    prepared = true;
    c->data->blocks[pop_index(8, 64, 8)] = 0;
    c->data->blocks[pop_index(8, 63, 8)] = (uint8_t)farm->fields.bn;
    memset(c->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->block_light, 0x88, MC_CHUNK_NIBBLE_BYTES);
    *pop_named_cell(r, "aB") = NULL;
    bool survival = true, placement = false;
    mc_world_plant_error se, pe;
    const mc_world_storage_status ss =
        mc_world_runtime_plant_survives(f->worlds[side], crops, 8, 64, 8, &survival, &se);
    const mc_world_storage_status ps =
        mc_world_runtime_plant_can_place(f->worlds[side], crops, 8, 64, 8, &placement, &pe);
    pop_restore(saved, c, r);
    prepared = false;
    CHECK(ss == MC_WORLD_STORAGE_UNSUPPORTED_STATE && ps == ss && survival && !placement &&
          se.bytecode == 4 && pe.bytecode == 4 && se.caller_bytecode == 35 &&
          pe.caller_bytecode == 23 && se.registry.status == MC_REGISTRY_NULL_REFERENCE &&
          pe.registry.status == MC_REGISTRY_NULL_REFERENCE && se.declaring_class != NULL &&
          pe.declaring_class != NULL && se.caller_class != NULL && pe.caller_class != NULL &&
          strcmp(se.declaring_class, side == 0 ? "ni" : "ij") == 0 &&
          strcmp(pe.declaring_class, side == 0 ? "ni" : "ij") == 0 &&
          strcmp(se.caller_class, side == 0 ? "wb" : "nw") == 0 &&
          strcmp(pe.caller_class, side == 0 ? "wb" : "nw") == 0);
    ok = true;
cleanup:
    if (prepared)
        pop_restore(saved, c, f->registry[side]);
    free(saved);
    return ok;
}

/* Targets root's narrow B successor: frozen feature A wrongly attributed a
 * NULL Random invocation to Registry and overwrote the prior World diagnostic.
 * Keep A immutable; these cases must fail against that diagnostic defect. */
static bool pop_null_random_diagnostics(fixture *f, size_t side) {
    bool ok = false;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_world_chunk *c = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    const mc_block *stone, *leaves;
    CHECK(c != NULL && c->data->blocks != NULL && r->blocks.m != NULL && w->fields.r != NULL &&
          pop_block(f, side, "u", &stone) && pop_block(f, side, "L", &leaves));
    const unsigned int first_draw[] = {14, 55, 53, 13, 14, 14};
    const mc_registry_error empty = {.initializer_pc = -1,
                                     .block_id = -1,
                                     .constructor_pc = -1,
                                     .active_initializer_pc = -1,
                                     .item_argument = -1};
    const size_t origin = pop_index(8, 64, 8);
    for (unsigned int kind = 0; kind < COUNT(first_draw); ++kind) {
        /* Obtain a REAL prior World Registry diagnostic through a reached
         * detailed material array failure; never assign native diagnostics. */
        const uint8_t old = c->data->blocks[origin];
        mc_block **old_m = r->blocks.m;
        c->data->blocks[origin] = (uint8_t)stone->fields.bn;
        r->blocks.m = NULL;
        mc_material *material = NULL;
        mc_world_material_error prior_error;
        const mc_world_storage_status prior_status =
            mc_world_runtime_material_get_detailed(w, 8, 64, 8, &material, &prior_error);
        r->blocks.m = old_m;
        c->data->blocks[origin] = old;
        CHECK(prior_status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && material == NULL &&
              prior_error.bytecode == 23 &&
              prior_error.registry.status == MC_REGISTRY_NULL_REFERENCE);
        const mc_registry_error prior = *mc_world_runtime_last_registry_error(w);
        const mc_java_random world_random = *w->fields.r;
        const size_t cells[] = {origin, pop_index(8, 63, 8), pop_index(8, 62, 8)};
        uint8_t saved[COUNT(cells)];
        for (size_t i = 0; i < COUNT(cells); ++i)
            saved[i] = c->data->blocks[cells[i]];
        if (kind == POP_TALL || kind == POP_DEAD) {
            c->data->blocks[cells[0]] = 0;
            c->data->blocks[cells[1]] = (uint8_t)leaves->fields.bn;
            c->data->blocks[cells[2]] = (uint8_t)stone->fields.bn;
        }
        const pop_case t = {.family = (pop_family)kind};
        const bool initial_output = (kind & 1u) != 0;
        bool out = initial_output;
        mc_population_plant_feature_error e = {0};
        const mc_world_storage_status status = pop_call(&t, w, NULL, 0, 8, 64, 8, &out, &e);
        const bool registry_retained =
            same_registry(&prior, mc_world_runtime_last_registry_error(w));
        const bool no_fallback_random = pop_same_random(&world_random, w->fields.r);
        const bool reached_cause = same_cause(&e.cause, mc_world_runtime_last_error(w));
        for (size_t i = 0; i < COUNT(cells); ++i)
            c->data->blocks[cells[i]] = saved[i];
        CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE && out == initial_output &&
              registry_retained && no_fallback_random && reached_cause && w->active_calls == 0 &&
              e.bytecode == first_draw[kind] && e.feature_class != NULL &&
              strcmp(e.feature_class,
                     side == 0 ? pop_client_class[kind] : pop_server_class[kind]) == 0 &&
              e.feature_method != NULL && strcmp(e.feature_method, "a") == 0 &&
              same_registry(&e.registry, &empty) && same_registry(&e.material.registry, &empty) &&
              same_registry(&e.plant.registry, &empty) &&
              same_registry(&e.mutation.registry, &empty));
    }
    ok = true;
cleanup:
    return ok;
}

static uint8_t pop_layer_metadata(const mc_world_chunk *c, size_t cell) {
    const unsigned int shift = (unsigned int)(cell & 1u) * 4u;
    return (uint8_t)(((uint32_t)c->data->metadata[cell / 2u] >> shift) & 15u);
}

static bool pop_layer_continuation_one(fixture *f, size_t side, const pop_case *t) {
    bool ok = false, prepared = false;
    pop_snapshot *saved = NULL;
    mc_world_runtime *w = f->worlds[side];
    mc_registry_classes *r = f->registry[side];
    mc_world_chunk *c = mc_world_store_chunk_object(f->stores[side], f->normal[side][0]);
    const mc_block *plant, *soil, *stone, *water;
    pop_probe p;
    CHECK((t->family == POP_REED || t->family == POP_CACTUS) && !t->negative &&
          t->control == POP_LAYER_CONTINUE);
    CHECK(pop_block(f, side, t->receiver, &plant) && pop_block(f, side, t->soil, &soil) &&
          pop_block(f, side, "u", &stone) && pop_block(f, side, "C", &water));
    CHECK(c != NULL && c->data != NULL && c->data->blocks != NULL &&
          c->data->blocks_capacity >= MC_CHUNK_BLOCKS && c->e != NULL && c->f != NULL &&
          c->g != NULL && c->h != NULL && c->e->a == c->data->metadata &&
          c->f->a == c->data->skylight && c->g->a == c->data->block_light &&
          c->h == c->data->heightmap && r->blocks.m != NULL &&
          r->blocks.m[plant->fields.bn] == plant && r->blocks.m[stone->fields.bn] == stone &&
          plant->fields.bn != water->fields.bn && plant->fields.bn != stone->fields.bn &&
          plant->fields.bn != soil->fields.bn && water->fields.bA != NULL &&
          stone->fields.bA != NULL && !mc_material_a(water->fields.bA) &&
          mc_material_a(stone->fields.bA) &&
          plant->kind == (t->family == POP_REED ? MC_BLOCK_RI_KO : MC_BLOCK_OR_JE));
    CHECK(pop_find_probe(t, &p) && p.height == 3 && pop_layer_probe_no_revisit(t, &p));
    mc_java_random random, expected = p.conditional;
    const unsigned int first_coordinates = t->family == POP_REED ? 4u : 6u;
    CHECK(pop_steps(&expected, pop_coordinate_draws[(size_t)t->family] - first_coordinates));
    pop_seed(&random, p.seed);
    saved = malloc(sizeof(*saved));
    CHECK(saved != NULL);
    pop_save(saved, c, r);
    prepared = true;

    const uint8_t plant_id = (uint8_t)plant->fields.bn;
    const uint8_t soil_id = (uint8_t)soil->fields.bn;
    const uint8_t stone_id = (uint8_t)stone->fields.bn;
    const uint8_t water_id = (uint8_t)water->fields.bn;
    const size_t base = pop_index(p.x, p.y, p.z);
    const size_t below = pop_index(p.x, mc_java_i32_sub(p.y, 1), p.z);
    const size_t layer1 = pop_index(p.x, mc_java_i32_add(p.y, 1), p.z);
    const size_t layer2 = pop_index(p.x, mc_java_i32_add(p.y, 2), p.z);
    const size_t side_cell = pop_index(mc_java_i32_sub(p.x, 1), p.y, p.z);
    const size_t gate = pop_index(mc_java_i32_sub(p.x, 1), mc_java_i32_sub(p.y, 1), p.z);
    memset(c->data->blocks, water_id, MC_CHUNK_BLOCKS);
    memset(c->data->metadata, 0xaa, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->block_light, 0x88, MC_CHUNK_NIBBLE_BYTES);
    memset(c->data->heightmap, 127, MC_CHUNK_HEIGHTMAP_BYTES);
    c->data->blocks[base] = 0;
    c->data->blocks[below] = soil_id;
    c->data->blocks[layer1] = plant_id;
    c->data->blocks[layer2] = stone_id;
    if (t->family == POP_CACTUS)
        c->data->blocks[side_cell] = stone_id;
    /* Reed: the real -X material/water gate is true, but stone below rejects
     * layer0. Cactus: real sand below could support layer0, but its -X solid
     * material rejects first. In both cases layer1 sees the unchanged air base
     * below and rejects; layer2 sees the PREPARED actual same plant at layer1,
     * accepts, and must write over existing nonair stone without an air gate. */
    const size_t count = f->providers[side].cache->count;
    const size_t insertions = f->providers[side].cache->insertion_count;
    bool out = false;
    mc_population_plant_feature_error e = {0};
    const mc_world_storage_status status =
        pop_call(t, w, &random, plant->fields.bn, 8, 64, 8, &out, &e);
    const uint8_t got_base = c->data->blocks[base];
    const uint8_t got_below = c->data->blocks[below];
    const uint8_t got_layer1 = c->data->blocks[layer1];
    const uint8_t got_layer2 = c->data->blocks[layer2];
    const uint8_t got_gate = c->data->blocks[gate];
    const uint8_t got_side = c->data->blocks[side_cell];
    const uint8_t metadata_base = pop_layer_metadata(c, base);
    const uint8_t metadata_layer1 = pop_layer_metadata(c, layer1);
    const uint8_t metadata_layer2 = pop_layer_metadata(c, layer2);
    bool complete_blocks = true;
    size_t matches = 0;
    for (size_t i = 0; i < MC_CHUNK_BLOCKS; ++i) {
        uint8_t wanted = water_id;
        if (i == base)
            wanted = 0;
        else if (i == below)
            wanted = soil_id;
        else if (i == layer1 || i == layer2)
            wanted = plant_id;
        else if (t->family == POP_CACTUS && i == side_cell)
            wanted = stone_id;
        if (c->data->blocks[i] != wanted)
            complete_blocks = false;
        if (c->data->blocks[i] == plant_id)
            ++matches;
    }
    const bool continuation = pop_same_random(&random, &expected);
    /* Restore all prepared data/reference/Registry inputs BEFORE a CHECK can
     * leave. Keep genuine dirty/class/queue/cache/pending effects and owners. */
    pop_restore(saved, c, r);
    prepared = false;
    CHECK(status == MC_WORLD_STORAGE_OK && out && complete_blocks && matches == 2 &&
          got_base == 0 && got_below == soil_id && got_layer1 == plant_id &&
          got_layer2 == plant_id && got_gate == water_id &&
          got_side == (t->family == POP_CACTUS ? stone_id : water_id) && metadata_base == 10 &&
          metadata_layer1 == 10 && metadata_layer2 == 0);
    CHECK(continuation && w->active_calls == 0 && f->providers[side].cache->count == count &&
          f->providers[side].cache->insertion_count == insertions &&
          mc_chunk_pending_contains(f->providers[side].pending, mc_chunk_cache_key(0, 0)));
    CHECK(e.status == MC_WORLD_STORAGE_OK && e.operation == NULL && e.feature_class == NULL &&
          e.feature_method == NULL && e.bytecode == 0 && e.air_bytecode == 0 &&
          e.cause.status == MC_WORLD_STORAGE_OK && e.cause.operation == NULL &&
          e.cause.system_error == 0 && plant_cleared(&e.plant) &&
          e.material.status == MC_WORLD_STORAGE_OK && e.material.operation == NULL &&
          e.material.bytecode == 0 && e.mutation.status == MC_WORLD_STORAGE_OK &&
          e.mutation.operation == NULL && e.mutation.bytecode == 0);
    ok = true;
cleanup:
    if (prepared)
        pop_restore(saved, c, r);
    free(saved);
    return ok;
}

static bool pop_layer_continuation_contracts(fixture *f, size_t side) {
    bool ok = false;
    const pop_case trials[] = {{.family = POP_REED,
                                .receiver = "aY",
                                .soil = "u",
                                .control = POP_LAYER_CONTINUE,
                                .inner = 2,
                                .outer = 1,
                                .orientation = -1},
                               {.family = POP_CACTUS,
                                .receiver = "aW",
                                .soil = "F",
                                .control = POP_LAYER_CONTINUE,
                                .inner = 2,
                                .outer = 2,
                                .orientation = -1}};
    for (size_t i = 0; i < COUNT(trials); ++i)
        CHECK(pop_layer_continuation_one(f, side, &trials[i]));
    ok = true;
cleanup:
    return ok;
}

static bool population_plant_contracts(fixture *f, size_t side) {
    bool ok = false;
    mc_chunk_state *negative_state = NULL;
    /* A genuine negative-coordinate Chunk is encoded, loaded and owned by the
     * existing Store. No forged cache key, j/k header or lease/count is used.
     * r.-1.0.mcr is distinct from the baseline r.0.0.mcr image check. */
    CHECK(mc_world_runtime_get(f->worlds[side], -1, 0, &negative_state) == MC_CHUNK_ACQUIRE_OK);
    CHECK(remember(f, side) && negative_state != NULL && !negative_state->blank);
    mc_world_chunk *negative = mc_world_store_chunk_object(f->stores[side], negative_state);
    CHECK(negative != NULL && pop_inherited_predicates(f, side) &&
          pop_wrapped_and_descent(f, side) && pop_null_random_diagnostics(f, side));
    CHECK(pop_layer_continuation_contracts(f, side));
    const pop_case defaults[] = {{POP_FLOWER, "ae", "v", POP_NORMAL, -1, -1, -1, 0, false, 0},
                                 {POP_TALL, "Y", "aB", POP_NORMAL, -1, -1, -1, 0, false, 2},
                                 {POP_DEAD, "Z", "F", POP_NORMAL, -1, -1, -1, 0, false, 0},
                                 {POP_REED, "aY", "v", POP_NORMAL, -1, -1, -1, 0, false, 0},
                                 {POP_PUMPKIN, "bb", "v", POP_NORMAL, -1, -1, -1, 0, false, 0},
                                 {POP_CACTUS, "aW", "F", POP_NORMAL, -1, -1, -1, 0, false, 0}};
    for (size_t i = 0; i < COUNT(defaults); ++i) {
        pop_case t = defaults[i];
        CHECK(pop_trial(f, side, negative, &t));
        t.control = POP_BLOCKED;
        CHECK(pop_trial(f, side, negative, &t));
        t.control = POP_NULL_NIBBLE;
        CHECK(pop_trial(f, side, negative, &t));
        t.control = POP_NULL_M;
        CHECK(pop_trial(f, side, negative, &t));
        if (i <= POP_DEAD) {
            t.control = POP_EQUAL_AIR;
            CHECK(pop_trial(f, side, negative, &t));
            const pop_control controls[] = {POP_BAD_INDEX, POP_NULL_ELEMENT, POP_WRONG_CAST,
                                            POP_NULL_SOIL, POP_NULL_LIGHT};
            for (size_t j = 0; j < COUNT(controls); ++j) {
                t.control = controls[j];
                if (controls[j] == POP_NULL_SOIL && i == POP_TALL)
                    t.soil = "v"; /* inherited substrate's first static */
                CHECK(pop_trial(f, side, negative, &t));
                t.soil = defaults[i].soil;
            }
        } else {
            t.control = POP_NULL_STATIC;
            CHECK(pop_trial(f, side, negative, &t));
        }
        if (i != POP_CACTUS) {
            t = defaults[i];
            t.control = POP_REJECT;
            if (i != POP_REED)
                t.soil = "u";
            CHECK(pop_trial(f, side, negative, &t));
        }
        if (i == POP_REED || i == POP_CACTUS) {
            t = defaults[i];
            t.control = POP_SOIL_REJECT;
            t.soil = i == POP_REED ? "F" : "w";
            CHECK(pop_trial(f, side, negative, &t));
        }
    }
    /* Actual mushroom, sapling and crops receivers pass the feature cast and
     * reach their real declarations; tall-grass metadata2 covers fern. */
    const pop_case inherited[] = {{POP_FLOWER, "ag", "u", POP_NORMAL, -1, -1, -1, 0, false, 0},
                                  {POP_FLOWER, "ag", "u", POP_NULL_O, -1, -1, -1, 0, false, 0},
                                  {POP_FLOWER, "z", "v", POP_NORMAL, -1, -1, -1, 0, false, 0},
                                  {POP_FLOWER, "aA", "aB", POP_NORMAL, -1, -1, -1, 0, false, 0},
                                  {POP_FLOWER, "aA", "aB", POP_NULL_SOIL, -1, -1, -1, 0, false, 0},
                                  {POP_FLOWER, "aA", "v", POP_REJECT, -1, -1, -1, 0, false, 0},
                                  {POP_FLOWER, "ae", "v", POP_NORMAL, -1, -1, -1, 0, true, 0},
                                  {POP_CACTUS, "aW", "F", POP_NORMAL, 2, 1, -1, 0, true, 0}};
    for (size_t i = 0; i < COUNT(inherited); ++i)
        CHECK(pop_trial(f, side, negative, &inherited[i]));
    pop_case grass = defaults[POP_TALL];
    grass.metadata = 1;
    CHECK(pop_trial(f, side, negative, &grass));
    for (int32_t inner = 0; inner < 3; ++inner)
        for (int32_t outer = 0; outer <= inner; ++outer)
            for (unsigned int kind = 0; kind < 2; ++kind) {
                pop_case t = defaults[kind == 0 ? POP_REED : POP_CACTUS];
                t.inner = inner;
                t.outer = outer;
                CHECK(pop_trial(f, side, negative, &t));
            }
    for (unsigned int direction = 0; direction < 4; ++direction) {
        pop_case t = defaults[POP_REED];
        t.water_direction = direction;
        CHECK(pop_trial(f, side, negative, &t));
    }
    for (int32_t orientation = 0; orientation < 4; ++orientation) {
        pop_case t = defaults[POP_PUMPKIN];
        t.orientation = orientation;
        CHECK(pop_trial(f, side, negative, &t));
    }
    for (unsigned int kind = 0; kind < 2; ++kind) {
        pop_case t = defaults[kind == 0 ? POP_REED : POP_CACTUS];
        t.inner = 2;
        t.outer = 1;
        t.control = POP_SECOND_OLD_NULL;
        CHECK(pop_trial(f, side, negative, &t));
    }
    ok = true;
cleanup:
    /* Even a failed child-loading/setup path is handed to real cleanup. */
    if (!remember(f, side))
        ok = false;
    return ok;
}
