/* Genuine GAME World/Store/normal Chunk and published LocalPlayer components.
 * Imported fixture mains are never run. Expected results live only in the
 * independently captured original packet, not in this native transport. */
#define MC_WORLD_ENTITY_TICK_MAIN mc_sleep_wake_unused_tick_main
#include "../../world/runtime/test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN

#include "../../../src/entity/player/nbt_internal.h"
#include "mc/entity/player/coordinates.h"
#include "mc/entity/player/sleep.h"
#include "mc/world/chunk/nibbles.h"
#include <ctype.h>
#include <errno.h>

static const int32_t sw_facing[4][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}};

typedef struct sw_fixture {
    fixture *world;
    mc_entity *actor, *other;
    mc_player_session_classes *sessions;
    mc_player_sleep_call *call;
    mc_entity_bounds *original_bounds;
    const mc_player_coordinates *before_v, *before_q, *before_bn;
    struct mc_player_nbt_coordinate *pins[3];
} sw_fixture;

typedef struct sw_snapshot {
    mc_entity_base_fields base;
    mc_local_player_snapshot player;
    mc_world_runtime_fields world;
    mc_world_provider_fields provider;
    const mc_entity_bounds *bounds_identity;
    const mc_java_random *random_identity;
    const mc_entity_watcher *watcher_identity;
    mc_entity_base_fields other_base;
    mc_local_player_snapshot other_player;
    mc_java_random actor_random, world_random;
} sw_snapshot;

static bool sw_cell(mc_chunk_data *data, int32_t x, int32_t y, int32_t z, int32_t id,
                    int32_t metadata) {
    data->blocks[boxes_index(x, y, z)] = (uint8_t)id;
    return mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z, (uint8_t)metadata);
}

static bool sw_player(sw_fixture *s, mc_entity **out) {
    bool ok = false;
    fixture *f = s->world;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL, *token = NULL;
    const uint16_t user[] = {'B', 'e', 'd', 'N', 'i', 'g', 'h', 't', 'C',
                             'o', 'm', 'p', 'o', 'n', 'e', 'n', 't'},
                   sid[] = {'-'};
    if (s->sessions == NULL)
        CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &s->sessions,
                                               NULL) == MC_PLAYER_OK);
    CHECK(mc_player_string_create(user, COUNT(user), &username) == MC_PLAYER_OK &&
          mc_player_string_create(sid, COUNT(sid), &token) == MC_PLAYER_OK &&
          mc_player_session_create(s->sessions, username, token, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], out, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(*out, f->worlds[0], NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK &&
          mc_entity_set_position(*out, 8.5, 64.0 + (double)1.62f, 8.5) == MC_ENTITY_OK &&
          (*out)->local_player->fields.living.Y == 20 && !(*out)->local_player->fields.player.u);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(f->worlds[0], *out, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    ok = true;
cleanup:
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    mc_player_string_release(token);
    return ok;
}

static bool sw_coordinate(mc_entity *p, const mc_player_coordinates **field, int32_t x, int32_t y,
                          int32_t z) {
    bool ok = false;
    struct mc_player_nbt_coordinate *node = NULL;
    CHECK(mc_player_coordinate_new_held(p, (mc_player_site){0}, "authored sleep component BR input",
                                        &node, NULL) == MC_PLAYER_OK);
    node->value = (mc_player_coordinates){x, y, z};
    mc_player_nbt_coordinate_store_held(p, node, field);
    ok = true;
cleanup:
    return ok;
}

static bool sw_retire(sw_fixture *s) {
    bool ok = true;
    if (s->call != NULL && mc_player_sleep_retire(&s->call, NULL) != MC_PLAYER_SLEEP_OK)
        return false;
    for (size_t i = 0; i < COUNT(s->pins); ++i) {
        if (s->pins[i] != NULL &&
            mc_player_coordinate_unpin_admit_held(s->actor, s->pins[i], 1) != MC_PLAYER_OK)
            return false;
        mc_player_coordinate_unpin_commit_held(s->actor, &s->pins[i]);
    }
    /* Restore only the deliberately nullable source projections, after every
     * observed snapshot and the actual invocation's checked retirement. */
    if (s->actor != NULL) {
        s->actor->base.world = s->world->worlds[0];
        if (s->original_bounds != NULL)
            s->actor->bounds = s->original_bounds;
    }
    mc_entity_release(s->actor);
    mc_entity_release(s->other);
    if (mc_player_session_classes_free(&s->sessions, NULL) != MC_PLAYER_OK)
        ok = false;
    if (!tick_cleanup(s->world))
        ok = false;
    s->world = NULL;
    return ok;
}

static bool sw_prepare(sw_fixture *s, const char *root, const int32_t input[5]) {
    bool ok = false;
    s->world = calloc(1, sizeof(*s->world));
    CHECK(s->world != NULL && tick_setup(s->world, root));
    fixture *f = s->world;
    mc_world_runtime *world = f->worlds[0];
    /* The actual Bed helper captures this loaded normal-Chunk footprint.
     * Source exit probes for these profiles stay in the central Chunk. */
    for (int32_t x = -2; x <= 1; ++x)
        for (int32_t z = -2; z <= 1; ++z) {
            if (x == 0 && z == 0)
                continue;
            mc_chunk_state *chunk = NULL;
            CHECK(tick_seed(f, 0, x, z) &&
                  mc_world_runtime_get(world, x, z, &chunk) == MC_CHUNK_ACQUIRE_OK &&
                  chunk != NULL && !chunk->blank && chunk->data != NULL);
        }
    CHECK(mc_world_info_spawn_set(world->fields.x, 8, 64, 8) == MC_WORLD_INFO_OK);
    mc_chunk_data *data = f->states[0]->data;
    memset(data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(data->skylight, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(data->block_light, 0, MC_CHUNK_NIBBLE_BYTES);
    memset(data->heightmap, 64, MC_CHUNK_HEIGHTMAP_BYTES);
    for (int32_t x = 0; x < 16; ++x)
        for (int32_t z = 0; z < 16; ++z) {
            CHECK(sw_cell(data, x, 63, z, 2, 0));
            for (int32_t y = 64; y < 128; ++y)
                CHECK(mc_chunk_nibbles_set(data->skylight, MC_CHUNK_NIBBLE_BYTES, x, y, z, 15));
        }
    CHECK(sw_player(s, &s->actor));
    mc_player_fields *p = &s->actor->local_player->fields.player;
    const mc_player_coordinates spawn = {42, 66, 44};
    CHECK(mc_player_saved_spawn_set(s->actor, &spawn, NULL) == MC_PLAYER_OK &&
          sw_coordinate(s->actor, &p->private_bN, -11, 65, 13));
    p->private_a = 17;
    p->w = .375f;
    p->y = -.625f;
    s->actor->base.velocity_x = .25;
    s->actor->base.velocity_y = -.375;
    s->actor->base.velocity_z = .125;
    mc_java_random_reseed(s->actor->random, 38);
    mc_java_random_reseed(world->fields.r, 2468);
    world->fields.f = 5;
    const int32_t direction = input[2], dx = sw_facing[direction][0], dz = sw_facing[direction][1];
    CHECK(sw_cell(data, 8, 64, 8, 26, direction | 8) &&
          sw_cell(data, 8 - dx, 64, 8 - dz, 26, direction));
    if (input[1] == 0) {
        CHECK(sw_coordinate(s->actor, &p->v, 21, 67, -23));
        if (input[4] == 1)
            CHECK(sw_player(s, &s->other));
        if (input[4] == 2)
            world->fields.B = true;
        if (input[4] == 3)
            CHECK(sw_cell(data, 8, 64, 8, 0, direction | 8));
    } else {
        const mc_player_sleep_context context = {.code_registry = f->registry[0]};
        mc_player_sleep_error error = {0};
        mc_player_sleep_state state = {0};
        const mc_player_sleep_status status =
            mc_player_try_sleep_begin(s->actor, 8, 64, 8, &context, &s->call, &error);
        if (status != MC_PLAYER_SLEEP_OK)
            fprintf(stderr, "sleep preparation case%" PRId32 " status%d pc%u %s\n", input[0],
                    (int)status, error.site.pc, error.operation == NULL ? "-" : error.operation);
        CHECK(status == MC_PLAYER_SLEEP_OK && mc_player_sleep_inspect(s->call, &state) &&
              state.returned && state.result != NULL && p->u && world->fields.J);
        mc_player_sleep_result_view view = {0};
        CHECK(mc_player_sleep_result_inspect(state.result, &view) && view.ordinal == 0 &&
              mc_player_sleep_retire(&s->call, NULL) == MC_PLAYER_SLEEP_OK && s->call == NULL);
        p->private_a = 37;
        CHECK(sw_cell(data, 8, 64, 8, 26, direction | 12));
        if (input[4] == 1)
            CHECK(sw_cell(data, 8, 64, 8, 0, 0) && sw_cell(data, 8 - dx, 64, 8 - dz, 0, 0));
        if (input[4] == 2 || input[4] == 6) {
            p->v = NULL;
            mc_player_nbt_coordinates_collect_held(s->actor);
        }
        if (input[4] == 3)
            for (int32_t x = 6; x <= 10; ++x)
                for (int32_t z = 6; z <= 10; ++z)
                    CHECK(sw_cell(data, x, 65, z, 1, 0));
        if (input[4] == 4)
            world->fields.B = true;
        if (input[4] == 5 || input[4] == 6)
            s->actor->base.world = NULL;
    }
    s->original_bounds = s->actor->bounds;
    if ((input[1] == 0 && input[4] == 4) || (input[1] == 1 && input[4] == 7))
        s->actor->bounds = NULL;
    s->before_v = p->v;
    s->before_q = p->private_b;
    s->before_bn = p->private_bN;
    /* The original observer also roots these entry identities. These pins
     * prevent native allocator reuse from falsifying an identity comparison.
     * Separate controls below exercise call ownership without observer pins. */
    const mc_player_coordinates *values[] = {s->before_v, s->before_q, s->before_bn};
    for (size_t i = 0; i < COUNT(values); ++i)
        CHECK(mc_player_coordinate_pin_held(s->actor, values[i], &s->pins[i], NULL) ==
              MC_PLAYER_OK);
    CHECK(world->fields.d->count == (s->other == NULL ? 1u : 2u) &&
          world->fields.b->count == world->fields.d->count &&
          world->fields.d->items[0] == s->actor && world->fields.b->items[0] == s->actor);
    ok = true;
cleanup:
    return ok;
}

/* Lossless nonzero maximal byte runs. The independent reader reconstructs
 * each complete plane, then hashes it; this file contains no expected hash. */
static bool sw_runs(const uint8_t *bytes, size_t length) {
    if (putchar('[') == EOF)
        return false;
    bool first = true;
    for (size_t at = 0; at < length;) {
        const uint8_t value = bytes[at];
        size_t end = at + 1;
        while (end < length && bytes[end] == value)
            ++end;
        if (value != 0) {
            if (printf("%s[%zu,%zu,%u]", first ? "" : ",", at, end - at, (unsigned int)value) < 0)
                return false;
            first = false;
        }
        at = end;
    }
    return putchar(']') != EOF;
}

static bool sw_planes(const mc_chunk_data *data) {
    return printf("{\"blocks\":") >= 0 && sw_runs(data->blocks, MC_CHUNK_BLOCKS) &&
           printf(",\"metadata\":") >= 0 && sw_runs(data->metadata, MC_CHUNK_NIBBLE_BYTES) &&
           printf(",\"sky\":") >= 0 && sw_runs(data->skylight, MC_CHUNK_NIBBLE_BYTES) &&
           printf(",\"block_light\":") >= 0 && sw_runs(data->block_light, MC_CHUNK_NIBBLE_BYTES) &&
           printf(",\"height\":") >= 0 && sw_runs(data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES) &&
           putchar('}') != EOF;
}

static bool sw_xyz(const mc_player_coordinates *p) {
    return p == NULL ? printf("null") >= 0
                     : printf("[%" PRId32 ",%" PRId32 ",%" PRId32 "]", p->x, p->y, p->z) >= 0;
}

static bool sw_bounds(const mc_entity_bounds *p) {
    return p == NULL ? printf("null") >= 0
                     : printf("[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                              "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]",
                              movement_double_bits(p->min_x), movement_double_bits(p->min_y),
                              movement_double_bits(p->min_z), movement_double_bits(p->max_x),
                              movement_double_bits(p->max_y), movement_double_bits(p->max_z)) >= 0;
}

static const char *sw_bool(bool value) {
    return value ? "true" : "false";
}

static bool sw_state(const sw_fixture *s) {
    const mc_entity *a = s->actor;
    const mc_player_fields *p = &a->local_player->fields.player;
    const mc_world_runtime_fields *w = &s->world->worlds[0]->fields;
    const mc_chunk_data *data = s->world->states[0]->data;
    uint8_t metadata = 0;
    if (!mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, &metadata))
        return false;
    if (printf("{\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
               "\"],\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
               "\"],\"dimensions\":[\"%08" PRIx32 "\",\"%08" PRIx32 "\",\"%08" PRIx32
               "\"],\"sleep_offsets\":[\"%08" PRIx32 "\",\"%08" PRIx32
               "\"],\"sleeping\":%s,\"timer\":%" PRId32 ",\"v\":",
               movement_double_bits(a->base.x), movement_double_bits(a->base.y),
               movement_double_bits(a->base.z), movement_double_bits(a->base.velocity_x),
               movement_double_bits(a->base.velocity_y), movement_double_bits(a->base.velocity_z),
               movement_float_bits(a->base.width), movement_float_bits(a->base.height),
               movement_float_bits(a->base.offset), movement_float_bits(p->w),
               movement_float_bits(p->y), sw_bool(p->u), p->private_a) < 0 ||
        !sw_xyz(p->v) || printf(",\"Q\":") < 0 || !sw_xyz(p->private_b) ||
        printf(",\"private_bN\":") < 0 || !sw_xyz(p->private_bN) || printf(",\"bounds\":") < 0 ||
        !sw_bounds(a->bounds) || printf(",\"old_bounds\":") < 0 || !sw_bounds(s->original_bounds))
        return false;
    if (printf(",\"world_J\":%s,\"remote\":%s,\"world_players\":%zu,\"world_entities\":%zu,"
               "\"other_sleeping\":%s,\"world_same\":%s,\"bounds_same\":%s,\"v_same\":%s,"
               "\"Q_same\":%s,\"Q_v_same\":%s,\"Q_before_v_same\":%s,\"private_bN_same\":%s,"
               "\"head\":[%u,%u],\"planes\":",
               sw_bool(w->J), sw_bool(w->B), w->d->count, w->b->count,
               s->other == NULL ? "null" : sw_bool(s->other->local_player->fields.player.u),
               sw_bool(a->base.world == s->world->worlds[0]),
               sw_bool(a->bounds == s->original_bounds), sw_bool(p->v == s->before_v),
               sw_bool(p->private_b == s->before_q), sw_bool(p->private_b == p->v),
               sw_bool(p->private_b == s->before_v), sw_bool(p->private_bN == s->before_bn),
               (unsigned int)data->blocks[boxes_index(8, 64, 8)], (unsigned int)metadata) < 0)
        return false;
    return sw_planes(data) && putchar('}') != EOF;
}

static bool sw_random(const mc_java_random *r) {
    return printf("[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]", r->state,
                  sw_bool(r->have_next_next_gaussian),
                  movement_double_bits(r->next_next_gaussian)) >= 0;
}

static bool sw_snapshot_take(const sw_fixture *s, sw_snapshot *out) {
    *out = (sw_snapshot){.base = s->actor->base,
                         .player = s->actor->local_player->fields,
                         .world = s->world->worlds[0]->fields,
                         .bounds_identity = s->actor->bounds,
                         .random_identity = s->actor->random,
                         .watcher_identity = s->actor->watcher,
                         .actor_random = *s->actor->random,
                         .world_random = *s->world->worlds[0]->fields.r};
    if (s->other != NULL) {
        out->other_base = s->other->base;
        out->other_player = s->other->local_player->fields;
    }
    return mc_world_provider_fields_get(out->world.t, &out->provider) == MC_WORLD_PROVIDER_OK;
}

/* Compare actual fields, never C aggregate padding. Floats retain exact raw
 * words, including signed zero/NaNs. A real changed field remains a failure. */
static bool sw_field_same(const char *owner, const char *field, const void *a, const void *b,
                          size_t bytes) {
    if (memcmp(a, b, bytes) == 0)
        return true;
    fprintf(stderr, "sleep/wake unselected field changed: %s.%s\n", owner, field);
    return false;
}

#define SW_FIELD(member)                                                                           \
    do {                                                                                           \
        if (!sw_field_same(__func__, #member, &a->member, &b->member, sizeof(a->member)))          \
            return false;                                                                          \
    } while (0)

static bool sw_base_same(const mc_entity_base_fields *a, const mc_entity_base_fields *b) {
    SW_FIELD(id);
    SW_FIELD(aE);
    SW_FIELD(aF);
    SW_FIELD(aG);
    SW_FIELD(aH);
    SW_FIELD(world);
    SW_FIELD(aJ);
    SW_FIELD(aK);
    SW_FIELD(aL);
    SW_FIELD(x);
    SW_FIELD(y);
    SW_FIELD(z);
    SW_FIELD(velocity_x);
    SW_FIELD(velocity_y);
    SW_FIELD(velocity_z);
    SW_FIELD(yaw);
    SW_FIELD(aT);
    SW_FIELD(aU);
    SW_FIELD(aV);
    SW_FIELD(aX);
    SW_FIELD(aY);
    SW_FIELD(aZ);
    SW_FIELD(ba);
    SW_FIELD(bb);
    SW_FIELD(bc);
    SW_FIELD(bd);
    SW_FIELD(be);
    SW_FIELD(offset);
    SW_FIELD(width);
    SW_FIELD(height);
    SW_FIELD(bi);
    SW_FIELD(bj);
    SW_FIELD(bk);
    SW_FIELD(private_b);
    SW_FIELD(bl);
    SW_FIELD(bm);
    SW_FIELD(bn);
    SW_FIELD(y_size);
    SW_FIELD(bp);
    SW_FIELD(bq);
    SW_FIELD(br);
    SW_FIELD(bt);
    SW_FIELD(bu);
    SW_FIELD(bv);
    SW_FIELD(bw);
    SW_FIELD(bx);
    SW_FIELD(by);
    SW_FIELD(bz);
    SW_FIELD(private_c);
    SW_FIELD(bC);
    SW_FIELD(bE);
    SW_FIELD(private_d);
    SW_FIELD(private_e);
    SW_FIELD(bF);
    SW_FIELD(bG);
    SW_FIELD(bH);
    SW_FIELD(bI);
    SW_FIELD(bM);
    SW_FIELD(client_only.bA);
    SW_FIELD(client_only.bB);
    SW_FIELD(client_only.bJ);
    SW_FIELD(client_only.bK);
    SW_FIELD(client_only.bL);
    return true;
}

static bool sw_living_same(const mc_living_entity_fields *a, const mc_living_entity_fields *b) {
    SW_FIELD(E);
    SW_FIELD(F);
    SW_FIELD(G);
    SW_FIELD(H);
    SW_FIELD(I);
    SW_FIELD(J);
    SW_FIELD(K);
    SW_FIELD(L);
    SW_FIELD(M);
    SW_FIELD(N);
    SW_FIELD(O);
    SW_FIELD(P);
    SW_FIELD(Q);
    SW_FIELD(R);
    SW_FIELD(S);
    SW_FIELD(T);
    SW_FIELD(U);
    SW_FIELD(V);
    SW_FIELD(W);
    SW_FIELD(X);
    SW_FIELD(Y);
    SW_FIELD(Z);
    SW_FIELD(private_a);
    SW_FIELD(aa);
    SW_FIELD(ab);
    SW_FIELD(ac);
    SW_FIELD(ad);
    SW_FIELD(ae);
    SW_FIELD(af);
    SW_FIELD(ag);
    SW_FIELD(ah);
    SW_FIELD(ai);
    SW_FIELD(aj);
    SW_FIELD(ak);
    SW_FIELD(al);
    SW_FIELD(am);
    SW_FIELD(an);
    SW_FIELD(ao);
    SW_FIELD(ap);
    SW_FIELD(aq);
    SW_FIELD(ar);
    SW_FIELD(as);
    SW_FIELD(at);
    SW_FIELD(au);
    SW_FIELD(av);
    SW_FIELD(aw);
    SW_FIELD(ax);
    SW_FIELD(ay);
    SW_FIELD(az);
    SW_FIELD(aA);
    SW_FIELD(aB);
    SW_FIELD(private_b);
    SW_FIELD(aC);
    return true;
}

static bool sw_player_same(const mc_player_fields *a, const mc_player_fields *b) {
    SW_FIELD(c);
    SW_FIELD(d);
    SW_FIELD(e);
    SW_FIELD(f);
    SW_FIELD(g);
    SW_FIELD(h);
    SW_FIELD(i);
    SW_FIELD(j);
    SW_FIELD(k);
    SW_FIELD(l);
    SW_FIELD(m);
    SW_FIELD(n);
    SW_FIELD(o);
    SW_FIELD(p);
    SW_FIELD(q);
    SW_FIELD(r);
    SW_FIELD(s);
    SW_FIELD(t);
    SW_FIELD(u);
    SW_FIELD(v);
    SW_FIELD(private_a);
    SW_FIELD(w);
    SW_FIELD(x);
    SW_FIELD(y);
    SW_FIELD(private_b);
    SW_FIELD(private_bN);
    SW_FIELD(z);
    SW_FIELD(A);
    SW_FIELD(B);
    SW_FIELD(C);
    SW_FIELD(private_bO);
    SW_FIELD(D);
    return true;
}

static bool sw_local_same(const mc_local_player_snapshot *a, const mc_local_player_snapshot *b) {
    if (!sw_living_same(&a->living, &b->living) || !sw_player_same(&a->player, &b->player))
        return false;
    SW_FIELD(input);
    SW_FIELD(minecraft);
    SW_FIELD(filters);
    SW_FIELD(skin_url);
    SW_FIELD(living_pc);
    SW_FIELD(player_pc);
    SW_FIELD(local_pc);
    SW_FIELD(watcher_hook_pc);
    return true;
}

static bool sw_world_same(const mc_world_runtime_fields *a, const mc_world_runtime_fields *b) {
    SW_FIELD(a);
    SW_FIELD(C);
    SW_FIELD(b);
    SW_FIELD(D);
    SW_FIELD(E);
    SW_FIELD(F);
    SW_FIELD(c);
    SW_FIELD(G);
    SW_FIELD(d);
    SW_FIELD(e);
    SW_FIELD(H);
    SW_FIELD(f);
    SW_FIELD(g);
    SW_FIELD(h);
    SW_FIELD(i);
    SW_FIELD(j);
    SW_FIELD(k);
    SW_FIELD(l);
    SW_FIELD(m);
    SW_FIELD(n);
    SW_FIELD(o);
    SW_FIELD(I);
    SW_FIELD(p);
    SW_FIELD(q);
    SW_FIELD(r);
    SW_FIELD(s);
    SW_FIELD(t);
    SW_FIELD(u);
    SW_FIELD(v);
    SW_FIELD(w);
    SW_FIELD(x);
    SW_FIELD(y);
    SW_FIELD(J);
    SW_FIELD(z);
    SW_FIELD(K);
    SW_FIELD(L);
    SW_FIELD(M);
    SW_FIELD(N);
    SW_FIELD(O);
    SW_FIELD(P);
    SW_FIELD(Q);
    SW_FIELD(R);
    SW_FIELD(B);
    return true;
}

static bool sw_provider_same(const mc_world_provider_fields *a, const mc_world_provider_fields *b) {
    SW_FIELD(a);
    SW_FIELD(b);
    SW_FIELD(c);
    SW_FIELD(d);
    SW_FIELD(e);
    SW_FIELD(f);
    SW_FIELD(g);
    SW_FIELD(h);
    return true;
}

#undef SW_FIELD

static bool sw_unselected_actor(const sw_snapshot *before, const sw_snapshot *after) {
    mc_entity_base_fields base = after->base;
    base.x = before->base.x;
    base.y = before->base.y;
    base.z = before->base.z;
    base.velocity_x = before->base.velocity_x;
    base.velocity_y = before->base.velocity_y;
    base.velocity_z = before->base.velocity_z;
    base.width = before->base.width;
    base.height = before->base.height;
    base.offset = before->base.offset;
    mc_local_player_snapshot player = after->player;
    player.player.w = before->player.player.w;
    player.player.y = before->player.player.y;
    player.player.u = before->player.player.u;
    player.player.private_a = before->player.player.private_a;
    player.player.v = before->player.player.v;
    player.player.private_b = before->player.player.private_b;
    /* These are native diagnostics, not original instance fields. */
    player.living_pc = before->player.living_pc;
    player.player_pc = before->player.player_pc;
    player.local_pc = before->player.local_pc;
    player.watcher_hook_pc = before->player.watcher_hook_pc;
    return sw_base_same(&base, &before->base) && sw_local_same(&player, &before->player) &&
           after->bounds_identity == before->bounds_identity &&
           after->random_identity == before->random_identity &&
           after->watcher_identity == before->watcher_identity &&
           sw_base_same(&after->other_base, &before->other_base) &&
           sw_local_same(&after->other_player, &before->other_player);
}

static bool sw_unselected_world(const sw_snapshot *before, const sw_snapshot *after) {
    mc_world_runtime_fields world = after->world;
    world.J = before->world.J;
    return sw_world_same(&world, &before->world) &&
           sw_provider_same(&after->provider, &before->provider);
}

static bool sw_profile(const char *root, const int32_t input[5]) {
    bool ok = false;
    sw_fixture s = {0};
    sw_snapshot before = {0}, after = {0};
    char path[8192];
    const int written = snprintf(path, sizeof(path), "%s/case-%" PRId32, root, input[0]);
    CHECK(written > 0 && (size_t)written < sizeof(path) && sw_prepare(&s, path, input));
    CHECK(sw_snapshot_take(&s, &before));
    /* Flush initializer output before the single source-call row. */
    CHECK(fflush(stdout) == 0);
    CHECK(printf("SLEEP_WAKE {\"case\":%" PRId32 ",\"input\":[%" PRId32 ",%" PRId32 ",%" PRId32
                 ",%" PRId32 ",%" PRId32 "],\"before\":",
                 input[0], input[0], input[1], input[2], input[3], input[4]) >= 0 &&
          sw_state(&s));
    const mc_player_sleep_context context = {.code_registry = s.world->registry[0]};
    mc_player_sleep_error error = {0};
    const mc_player_sleep_status status =
        input[1] == 0 ? mc_player_try_sleep_begin(s.actor, 8, 64, 8, &context, &s.call, &error)
                      : mc_player_wake_begin(s.actor, (input[3] & 4) != 0, (input[3] & 2) != 0,
                                             (input[3] & 1) != 0, &context, &s.call, &error);
    mc_player_sleep_state state = {0};
    CHECK(s.call != NULL && mc_player_sleep_inspect(s.call, &state) && !state.active &&
          state.wake == (input[1] != 0) && state.receiver_identity == (uintptr_t)s.actor &&
          state.failure != NULL && state.failure->status == status && error.status == status);
    const bool fault = status == MC_PLAYER_SLEEP_SOURCE_NULL;
    if (status != MC_PLAYER_SLEEP_OK && !fault)
        fprintf(stderr, "sleep/wake case%" PRId32 " status%d pc%u %s\n", input[0], (int)status,
                error.site.pc, error.operation == NULL ? "-" : error.operation);
    CHECK(status == MC_PLAYER_SLEEP_OK ||
          (fault && !error.native_policy && error.source_exception &&
           error.exception_class != NULL &&
           strcmp(error.exception_class, "java/lang/NullPointerException") == 0));
    CHECK(state.returned == !fault && error.returned == !fault);
    CHECK(sw_snapshot_take(&s, &after));
    const bool actor_preserved = sw_unselected_actor(&before, &after);
    const bool world_preserved = sw_unselected_world(&before, &after);
    CHECK(actor_preserved && world_preserved);
    CHECK(printf(",\"after\":") >= 0 && sw_state(&s) && printf(",\"result\":") >= 0);
    if (state.result == NULL)
        CHECK(printf("null") >= 0);
    else {
        mc_player_sleep_result_view result = {0};
        const mc_player_sleep_result *current_a = NULL;
        CHECK(mc_player_sleep_result_inspect(state.result, &result) && result.ordinal == 0 &&
              result.name_length == 2 && result.name_units[0] == 'O' &&
              result.name_units[1] == 'K' &&
              mc_player_sleep_result_static_get(s.actor->classes, "a", &current_a, NULL) ==
                  MC_PLAYER_SLEEP_RESULT_OK);
        CHECK(printf("{\"name\":\"OK\",\"ordinal\":%" PRId32 ",\"current_a\":%s}", result.ordinal,
                     sw_bool(state.result == current_a)) >= 0);
    }
    CHECK(printf(",\"source_failure_site\":") >= 0);
    if (fault) {
        const mc_player_site site = error.child.class_name != NULL ? error.child : error.site;
        CHECK(site.class_name != NULL && site.method != NULL && site.descriptor != NULL &&
              printf("{\"class\":\"%s\",\"method\":\"%s\",\"descriptor\":\"%s\",\"pc\":%u}",
                     site.class_name, site.method, site.descriptor, site.pc) >= 0);
    } else
        CHECK(printf("null") >= 0);
    CHECK(printf(",\"exception\":\"%s\",\"unselected_actor_preserved\":%s,"
                 "\"unselected_world_preserved\":%s,\"random_before\":[",
                 fault ? error.exception_class : "NONE", sw_bool(actor_preserved),
                 sw_bool(world_preserved)) >= 0 &&
          sw_random(&before.actor_random) && putchar(',') != EOF &&
          sw_random(&before.world_random) && printf("],\"random_after\":[") >= 0 &&
          sw_random(&after.actor_random) && putchar(',') != EOF && sw_random(&after.world_random) &&
          puts("]}") >= 0 && fflush(stdout) == 0);
    CHECK(mc_player_sleep_retire(&s.call, NULL) == MC_PLAYER_SLEEP_OK && s.call == NULL &&
          mc_player_sleep_retire(&s.call, NULL) == MC_PLAYER_SLEEP_OK);
    CHECK(s.world->worlds[0]->active_calls == 0 && s.world->worlds[0]->local_leases == 0);
    ok = true;
cleanup:
    if (!sw_retire(&s))
        ok = false;
    return ok;
}

static bool sw_profiles(const char *root, const char *path) {
    bool ok = false;
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    char line[256];
    int32_t number = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        int32_t input[5] = {0};
        char *cursor = line;
        for (size_t i = 0; i < COUNT(input); ++i) {
            while (isspace((unsigned char)*cursor))
                ++cursor;
            errno = 0;
            char *end = NULL;
            const intmax_t value = strtoimax(cursor, &end, 10);
            CHECK(errno == 0 && end != cursor && value >= INT32_MIN && value <= INT32_MAX &&
                  (*end == '\0' || isspace((unsigned char)*end)));
            input[i] = (int32_t)value;
            cursor = end;
        }
        while (isspace((unsigned char)*cursor))
            ++cursor;
        CHECK(*cursor == '\0' && number < 76 && input[0] == number &&
              (input[1] == 0 || input[1] == 1) && input[2] >= 0 && input[2] < 4 && input[3] >= 0 &&
              input[3] < 8 && input[4] >= 0 && input[4] < 8 && sw_profile(root, input));
        ++number;
    }
    CHECK(!ferror(file) && number == 76);
    ok = true;
cleanup:
    if (file != NULL && fclose(file) != 0)
        ok = false;
    return ok;
}

static bool sw_native_controls(const char *root) {
    bool ok = false;
    sw_fixture s = {0};
    const int32_t profile[] = {0, 1, 0, 3, 0};
    mc_player_sleep_error error = {0};
    mc_player_sleep_cleanup cleanup = {0};
    mc_player_sleep_state state = {0};
    char path[8192];
    const int written = snprintf(path, sizeof(path), "%s/native-controls", root);
    CHECK(written > 0 && (size_t)written < sizeof(path) && sw_prepare(&s, path, profile));
    /* These controls deliberately remove all observer roots. Only the actual
     * retained GS source locals may keep replaced coordinate nodes alive. */
    for (size_t i = 0; i < COUNT(s.pins); ++i)
        mc_player_coordinate_unpin_commit_held(s.actor, &s.pins[i]);
    const mc_player_sleep_context context = {.code_registry = s.world->registry[0]};
    CHECK(mc_player_wake_begin(s.actor, false, false, false, &context, NULL, &error) ==
              MC_PLAYER_SLEEP_ARGUMENT &&
          error.native_policy && s.call == NULL);
    CHECK(mc_player_sleep_retire(NULL, &cleanup) == MC_PLAYER_SLEEP_ARGUMENT &&
          !mc_player_sleep_inspect(NULL, &state));
    const size_t player_holds = s.actor->holds;
    const size_t registry_holds = s.world->registry[0]->runtime_borrows;
    CHECK(mc_player_wake_begin(s.actor, false, true, true, &context, &s.call, &error) ==
              MC_PLAYER_SLEEP_OK &&
          mc_player_sleep_inspect(s.call, &state) && state.returned && state.source_pc == 234 &&
          state.result == NULL && s.actor->holds > player_holds &&
          s.world->registry[0]->runtime_borrows > registry_holds);
    mc_player_sleep_call *const actual_call = s.call;
    const mc_entity_base_fields actual_base = s.actor->base;
    const mc_local_player_snapshot actual_player = s.actor->local_player->fields;
    CHECK(mc_player_wake_begin(s.actor, true, true, true, &context, &s.call, &error) ==
              MC_PLAYER_SLEEP_ARGUMENT &&
          s.call == actual_call && sw_base_same(&actual_base, &s.actor->base) &&
          sw_local_same(&actual_player, &s.actor->local_player->fields));
    mc_player_fields *p = &s.actor->local_player->fields.player;
    CHECK(p->private_b != NULL && p->v != NULL && p->private_b != p->v &&
          p->private_b->x == p->v->x && p->private_b->y == p->v->y && p->private_b->z == p->v->z);
    const mc_player_coordinates *const old_v = p->v;
    struct mc_player_nbt_coordinate *node = s.actor->local_player->nbt_coordinates;
    while (node != NULL && &node->value != old_v)
        node = node->next;
    CHECK(node != NULL && node->pins >= 2);
    const size_t pins = node->pins;
    const mc_player_sleep_error original_failure = *state.failure;
    p->v = NULL;
    mc_player_nbt_coordinates_collect_held(s.actor);
    CHECK(old_v->x == 8 && old_v->y == 64 && old_v->z == 8);
    node->pins = 0;
    const mc_player_sleep_status refused = mc_player_sleep_retire(&s.call, &cleanup);
    node->pins = pins;
    CHECK(refused == MC_PLAYER_SLEEP_CLEANUP_FAILED && s.call == actual_call &&
          cleanup.child == MC_PLAYER_SLEEP_CLEANUP_COORDINATE &&
          mc_player_sleep_inspect(s.call, &state) &&
          memcmp(state.failure, &original_failure, sizeof(original_failure)) == 0 &&
          !state.active && state.returned && state.source_pc == 234 &&
          sw_base_same(&s.actor->base, &actual_base));
    CHECK(mc_player_sleep_retire(&s.call, &cleanup) == MC_PLAYER_SLEEP_OK && s.call == NULL &&
          cleanup.status == MC_PLAYER_SLEEP_OK && s.actor->holds == player_holds &&
          s.world->registry[0]->runtime_borrows == registry_holds);
    /* Real aliases remain aliases in the input; GS.a(BR) must publish a fresh
     * copy even when CURRENT v is also Q and private_bN. */
    CHECK(sw_coordinate(s.actor, &p->v, -3, 68, 19));
    p->private_b = p->v;
    p->private_bN = p->v;
    mc_player_nbt_coordinates_collect_held(s.actor);
    const mc_player_coordinates *const shared = p->v;
    CHECK(mc_player_wake_begin(s.actor, true, false, true, &context, &s.call, &error) ==
              MC_PLAYER_SLEEP_OK &&
          p->private_b != shared && p->v == shared && p->private_bN == shared &&
          p->private_b->x == -3 && p->private_b->y == 68 && p->private_b->z == 19 &&
          mc_player_sleep_retire(&s.call, NULL) == MC_PLAYER_SLEEP_OK);
    /* A genuine null-World prefix has already resized before failure. Its
     * retained first cause survives a checked cleanup refusal unchanged. */
    s.actor->base.world = NULL;
    CHECK(mc_player_wake_begin(s.actor, false, true, true, &context, &s.call, NULL) ==
              MC_PLAYER_SLEEP_SOURCE_NULL &&
          mc_player_sleep_inspect(s.call, &state) && !state.returned && state.source_pc == 48 &&
          !state.failure->native_policy && state.failure->source_exception &&
          s.actor->base.width == .6f && s.actor->base.height == 1.8f &&
          s.actor->base.offset == 1.62f);
    const mc_player_sleep_error failed = *state.failure;
    node = s.actor->local_player->nbt_coordinates;
    while (node != NULL && &node->value != shared)
        node = node->next;
    CHECK(node != NULL && node->pins == 2);
    const size_t failed_pins = node->pins;
    node->pins = 0;
    const mc_player_sleep_status failed_cleanup = mc_player_sleep_retire(&s.call, &cleanup);
    node->pins = failed_pins;
    CHECK(failed_cleanup == MC_PLAYER_SLEEP_CLEANUP_FAILED && s.call != NULL &&
          cleanup.child == MC_PLAYER_SLEEP_CLEANUP_COORDINATE &&
          mc_player_sleep_inspect(s.call, &state) &&
          memcmp(state.failure, &failed, sizeof(failed)) == 0 && !state.returned &&
          state.source_pc == 48 && mc_player_sleep_retire(&s.call, NULL) == MC_PLAYER_SLEEP_OK &&
          s.call == NULL && s.actor->holds == player_holds);
    /* No v means no reached Block.T access: an absent defining Registry is
     * valid on this route, and GS.a(NULL) still clears the actual saved spawn. */
    s.actor->base.world = s.world->worlds[0];
    p->v = NULL;
    mc_player_nbt_coordinates_collect_held(s.actor);
    CHECK(mc_player_wake_begin(s.actor, true, false, true, NULL, &s.call, &error) ==
              MC_PLAYER_SLEEP_OK &&
          mc_player_sleep_inspect(s.call, &state) && state.returned && state.source_pc == 234 &&
          p->private_b == NULL && p->v == NULL &&
          s.world->registry[0]->runtime_borrows == registry_holds &&
          mc_player_sleep_retire(&s.call, NULL) == MC_PLAYER_SLEEP_OK && s.call == NULL);
    ok = true;
cleanup:
    if (!sw_retire(&s))
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    char root[8192];
    if (argc != 4 || strcmp(argv[1], "sleep-wake") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    CHECK(sw_profiles(root, argv[3]) && sw_native_controls(root));
    result = 0;
cleanup:
    if (result == 0 && printf("PASS: %u Player sleep/wake component checks\n", checks) < 0)
        return 1;
    return result;
}
