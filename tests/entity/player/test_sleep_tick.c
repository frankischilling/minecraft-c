/* Prepared original parent inputs; actual mc_player_tick continues beyond
 * the original111 boundary. Only eight audited sleep fields are compared on
 * those32 rows. This file contains fixture inputs/transport, no Source body. */
#define MC_WORLD_ENTITY_TICK_MAIN mc_sleep_parent_unused_world_main
#include "../../world/runtime/test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN
#include "mc/block/registry/registry.h"
#include "mc/block/registry/runtime.h"
#include "mc/entity/player/coordinates.h"
#include "mc/entity/player/sleep.h"
#include <ctype.h>
#include <errno.h>

typedef struct st_fixture {
    fixture *world;
    client_application *app;
    mc_entity *actor, *other;
    mc_movement_input *input;
    mc_player_tick_error *error;
    mc_player_sleep_call *preparation;
    mc_entity_bounds *original_bounds;
    uintptr_t before_v, before_q, before_bn;
    const mc_block *before_t;
    uintptr_t before_list;
} st_fixture;

static FILE *st_stream;

typedef struct st_snapshot {
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
} st_snapshot;

static bool st_cell(mc_chunk_data *data, int32_t x, int32_t y, int32_t z, int32_t id,
                    int32_t metadata) {
    data->blocks[boxes_index(x, y, z)] = (uint8_t)id;
    return mc_chunk_nibbles_set(data->metadata, MC_CHUNK_NIBBLE_BYTES, x, y, z, (uint8_t)metadata);
}

static bool st_coordinate(mc_entity *p, const mc_player_coordinates **field, int32_t x, int32_t y,
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

static bool st_runs(const uint8_t *bytes, size_t length) {
    if (fputc('[', st_stream) == EOF)
        return false;
    bool first = true;
    for (size_t at = 0; at < length;) {
        const uint8_t value = bytes[at];
        size_t end = at + 1;
        while (end < length && bytes[end] == value)
            ++end;
        if (value != 0) {
            if (fprintf(st_stream, "%s[%zu,%zu,%u]", first ? "" : ",", at, end - at,
                        (unsigned int)value) < 0)
                return false;
            first = false;
        }
        at = end;
    }
    return fputc(']', st_stream) != EOF;
}

static bool st_planes(const mc_chunk_data *data) {
    return fprintf(st_stream, "{\"blocks\":") >= 0 && st_runs(data->blocks, MC_CHUNK_BLOCKS) &&
           fprintf(st_stream, ",\"metadata\":") >= 0 &&
           st_runs(data->metadata, MC_CHUNK_NIBBLE_BYTES) && fprintf(st_stream, ",\"sky\":") >= 0 &&
           st_runs(data->skylight, MC_CHUNK_NIBBLE_BYTES) &&
           fprintf(st_stream, ",\"block_light\":") >= 0 &&
           st_runs(data->block_light, MC_CHUNK_NIBBLE_BYTES) &&
           fprintf(st_stream, ",\"height\":") >= 0 &&
           st_runs(data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES) && fputc('}', st_stream) != EOF;
}

static bool st_xyz(const mc_player_coordinates *p) {
    return p == NULL
               ? fprintf(st_stream, "null") >= 0
               : fprintf(st_stream, "[%" PRId32 ",%" PRId32 ",%" PRId32 "]", p->x, p->y, p->z) >= 0;
}

static bool st_bounds(const mc_entity_bounds *p) {
    return p == NULL ? fprintf(st_stream, "null") >= 0
                     : fprintf(st_stream,
                               "[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                               "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]",
                               movement_double_bits(p->min_x), movement_double_bits(p->min_y),
                               movement_double_bits(p->min_z), movement_double_bits(p->max_x),
                               movement_double_bits(p->max_y), movement_double_bits(p->max_z)) >= 0;
}

static const char *st_bool(bool value) {
    return value ? "true" : "false";
}

static bool st_state(const st_fixture *s) {
    const mc_entity *a = s->actor;
    const mc_player_fields *p = &a->local_player->fields.player;
    const mc_world_runtime_fields *w = &s->world->worlds[0]->fields;
    const mc_chunk_data *data = s->world->states[0]->data;
    uint8_t metadata = 0;
    if (!mc_chunk_nibbles_get(data->metadata, MC_CHUNK_NIBBLE_BYTES, 8, 64, 8, &metadata))
        return false;
    if (fprintf(st_stream,
                "{\"position\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                "\"],\"motion\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                "\"],\"dimensions\":[\"%08" PRIx32 "\",\"%08" PRIx32 "\",\"%08" PRIx32
                "\"],\"sleep_offsets\":[\"%08" PRIx32 "\",\"%08" PRIx32
                "\"],\"sleeping\":%s,\"timer\":%" PRId32 ",\"v\":",
                movement_double_bits(a->base.x), movement_double_bits(a->base.y),
                movement_double_bits(a->base.z), movement_double_bits(a->base.velocity_x),
                movement_double_bits(a->base.velocity_y), movement_double_bits(a->base.velocity_z),
                movement_float_bits(a->base.width), movement_float_bits(a->base.height),
                movement_float_bits(a->base.offset), movement_float_bits(p->w),
                movement_float_bits(p->y), st_bool(p->u), p->private_a) < 0 ||
        !st_xyz(p->v) || fprintf(st_stream, ",\"Q\":") < 0 || !st_xyz(p->private_b) ||
        fprintf(st_stream, ",\"private_bN\":") < 0 || !st_xyz(p->private_bN) ||
        fprintf(st_stream, ",\"bounds\":") < 0 || !st_bounds(a->bounds) ||
        fprintf(st_stream, ",\"old_bounds\":") < 0 || !st_bounds(s->original_bounds))
        return false;
    if (fprintf(st_stream,
                ",\"world_J\":%s,\"remote\":%s,\"world_players\":%zu,\"world_entities\":%zu,"
                "\"other_sleeping\":%s,\"world_same\":%s,\"bounds_same\":%s,\"v_same\":%s,"
                "\"Q_same\":%s,\"Q_v_same\":%s,\"Q_before_v_same\":%s,\"private_bN_same\":%s,"
                "\"head\":[%u,%u],\"planes\":",
                st_bool(w->J), st_bool(w->B), w->d->count, w->b->count,
                s->other == NULL ? "null" : st_bool(s->other->local_player->fields.player.u),
                st_bool(a->base.world == s->world->worlds[0]),
                st_bool(a->bounds == s->original_bounds), st_bool((uintptr_t)p->v == s->before_v),
                st_bool((uintptr_t)p->private_b == s->before_q), st_bool(p->private_b == p->v),
                st_bool((uintptr_t)p->private_b == s->before_v),
                st_bool((uintptr_t)p->private_bN == s->before_bn),
                (unsigned int)data->blocks[boxes_index(8, 64, 8)], (unsigned int)metadata) < 0)
        return false;
    const mc_block *bed = NULL;
    if (mc_registry_block_static_get(s->world->registry[0], "T", &bed, NULL) != MC_REGISTRY_OK)
        return false;
    const mc_block_fields *fields = mc_block_fields_get(bed);
    if (fields == NULL || !st_planes(data) ||
        fprintf(st_stream,
                ",\"sky_darkness\":%" PRId32 ",\"bed_static_id\":%" PRId32
                ",\"bed_static_same\":%s,\"client_null\":%s,\"player_list_same\":%s,"
                "\"player_list_members\":[",
                w->f, fields->bn, st_bool(bed == s->before_t),
                st_bool(a->local_player->fields.minecraft == NULL),
                st_bool((uintptr_t)w->d == s->before_list)) < 0)
        return false;
    for (size_t i = 0; i < w->d->count; ++i) {
        const mc_entity *member = w->d->items[i];
        if (member == NULL || !mc_entity_is_kind(member, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER) ||
            fprintf(st_stream, "%s{\"class\":\"dc\",\"actor\":%s}", i == 0 ? "" : ",",
                    st_bool(member == a)) < 0)
            return false;
    }
    return fprintf(st_stream, "]}") >= 0;
}

static bool st_random(const mc_java_random *r) {
    return fprintf(st_stream, "[\"%016" PRIx64 "\",%s,\"%016" PRIx64 "\"]", r->state,
                   st_bool(r->have_next_next_gaussian),
                   movement_double_bits(r->next_next_gaussian)) >= 0;
}

static bool st_snapshot_take(const st_fixture *s, st_snapshot *out) {
    *out = (st_snapshot){.base = s->actor->base,
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
static bool st_field_same(const char *owner, const char *field, const void *a, const void *b,
                          size_t bytes) {
    if (memcmp(a, b, bytes) == 0)
        return true;
    fprintf(stderr, "sleep/wake unselected field changed: %s.%s\n", owner, field);
    return false;
}

#define SW_FIELD(member)                                                                           \
    do {                                                                                           \
        if (!st_field_same(__func__, #member, &a->member, &b->member, sizeof(a->member)))          \
            return false;                                                                          \
    } while (0)

static bool st_base_same(const mc_entity_base_fields *a, const mc_entity_base_fields *b) {
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

static bool st_living_same(const mc_living_entity_fields *a, const mc_living_entity_fields *b) {
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

static bool st_player_same(const mc_player_fields *a, const mc_player_fields *b) {
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

static bool st_local_same(const mc_local_player_snapshot *a, const mc_local_player_snapshot *b) {
    if (!st_living_same(&a->living, &b->living) || !st_player_same(&a->player, &b->player))
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

static bool st_world_same(const mc_world_runtime_fields *a, const mc_world_runtime_fields *b) {
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

static bool st_provider_same(const mc_world_provider_fields *a, const mc_world_provider_fields *b) {
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

static bool st_unselected_actor(const st_snapshot *before, const st_snapshot *after) {
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
    return st_base_same(&base, &before->base) && st_local_same(&player, &before->player) &&
           after->bounds_identity == before->bounds_identity &&
           after->random_identity == before->random_identity &&
           after->watcher_identity == before->watcher_identity &&
           st_base_same(&after->other_base, &before->other_base) &&
           st_local_same(&after->other_player, &before->other_player);
}

static bool st_unselected_world(const st_snapshot *before, const st_snapshot *after) {
    mc_world_runtime_fields world = after->world;
    world.J = before->world.J;
    return st_world_same(&world, &before->world) &&
           st_provider_same(&after->provider, &before->provider);
}

static bool st_fault_input(const int32_t input[8]) {
    return input[5] != 0 &&
           ((input[7] & 2) != 0 ||
            (input[4] == 0 &&
             ((input[7] & 1) != 0 || (input[2] != 0 && input[3] == 0 && (input[7] & 4) != 0))));
}

static bool st_prepare(st_fixture *s, const char *root, const char *resource_path,
                       const int32_t input[8]) {
    bool ok = false;
    s->world = calloc(1, sizeof(*s->world));
    s->app = calloc(1, sizeof(*s->app));
    s->error = calloc(1, sizeof(*s->error));
    CHECK(s->world != NULL && s->app != NULL && s->error != NULL);
    mc_player_tick_error_init(s->error);
    CHECK(mc_resource_files_create(resource_path, &s->app->files) == MC_REGISTRY_OK);
    tick_resources = mc_resource_files_source(s->app->files);
    const bool world_ready = tick_setup(s->world, root);
    tick_resources = NULL;
    CHECK(world_ready && app_setup(s->world, s->app, root));
    fixture *f = s->world;
    mc_world_runtime *world = f->worlds[0];
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
            CHECK(st_cell(data, x, 63, z, 2, 0));
            for (int32_t y = 64; y < 128; ++y)
                CHECK(mc_chunk_nibbles_set(data->skylight, MC_CHUNK_NIBBLE_BYTES, x, y, z, 15));
        }
    /* Four actual pre111 failures retain the original NULL dc.b. The other32
     * use a genuinely constructed App for the later, unobserved continuation.
     * The difference is an explicit input join, never an equality assertion. */
    client_application *receiver_app = st_fault_input(input) ? NULL : s->app;
    CHECK(mc_local_player_allocate(f->entity_classes[0], &s->actor, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(s->actor, world, receiver_app, s->app->session, 0, NULL,
                                    NULL) == MC_PLAYER_OK &&
          mc_entity_set_position(s->actor, 8.5, 64.0 + (double)1.62f, 8.5) == MC_ENTITY_OK);
    bool inserted = false;
    CHECK(mc_world_runtime_insert_entity(world, s->actor, &inserted, NULL) == MC_WORLD_STORAGE_OK &&
          inserted && mc_entity_is_kind(s->actor, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER));
    CHECK(mc_movement_input_create_from_options(s->app->settings, &s->input, NULL) ==
              MC_MOVEMENT_INPUT_OK &&
          mc_local_player_set_movement_input(s->actor, s->input, NULL) == MC_PLAYER_OK);
    mc_player_fields *fields = &s->actor->local_player->fields.player;
    const mc_player_coordinates saved = {42, 66, 44};
    CHECK(mc_player_saved_spawn_set(s->actor, &saved, NULL) == MC_PLAYER_OK &&
          st_coordinate(s->actor, &fields->private_bN, -11, 65, 13));
    mc_java_random_reseed(s->actor->random, 38);
    mc_java_random_reseed(world->fields.r, 2468);
    world->fields.f = 5;
    static const int32_t facing[4][2] = {{0, 1}, {-1, 0}, {0, -1}, {1, 0}};
    const int32_t direction = input[1];
    const int32_t dx = facing[direction][0], dz = facing[direction][1];
    CHECK(st_cell(data, 8, 64, 8, 26, direction | 8) &&
          st_cell(data, 8 - dx, 64, 8 - dz, 26, direction));
    if (input[5] != 0) {
        const mc_player_sleep_context context = {.code_registry = f->registry[0]};
        mc_player_sleep_state state = {0};
        CHECK(mc_player_try_sleep_begin(s->actor, 8, 64, 8, &context, &s->preparation, NULL) ==
                  MC_PLAYER_SLEEP_OK &&
              mc_player_sleep_inspect(s->preparation, &state) && state.returned &&
              state.result != NULL && fields->u && world->fields.J);
        mc_player_sleep_result_view result = {0};
        CHECK(mc_player_sleep_result_inspect(state.result, &result) && result.ordinal == 0 &&
              mc_player_sleep_retire(&s->preparation, NULL) == MC_PLAYER_SLEEP_OK &&
              s->preparation == NULL);
    }
    fields->private_a = input[6];
    world->fields.f = input[2] != 0 ? 0 : 5;
    if (input[3] != 0)
        CHECK(st_cell(data, 8, 64, 8, 0, 0) && st_cell(data, 8 - dx, 64, 8 - dz, 0, 0));
    world->fields.B = input[4] != 0;
    s->original_bounds = s->actor->bounds;
    if ((input[7] & 1) != 0) {
        fields->v = NULL;
        mc_player_nbt_coordinates_collect_held(s->actor);
    }
    if ((input[7] & 2) != 0)
        s->actor->base.world = NULL;
    if ((input[7] & 4) != 0)
        s->actor->bounds = NULL;
    s->before_v = (uintptr_t)fields->v;
    s->before_q = (uintptr_t)fields->private_b;
    s->before_bn = (uintptr_t)fields->private_bN;
    s->before_list = (uintptr_t)world->fields.d;
    CHECK(mc_registry_block_static_get(f->registry[0], "T", &s->before_t, NULL) == MC_REGISTRY_OK &&
          s->before_t != NULL && world->fields.d->count == 1 &&
          world->fields.d->items[0] == s->actor && world->fields.b->count == 1);
    /* No observer coordinate pins: real invocation ownership must stand alone.
     * Q setter allocates the new copy before collecting the previous Q, so
     * its source identity comparison cannot be allocator-address reuse. */
    ok = true;
cleanup:
    tick_resources = NULL;
    return ok;
}

static bool st_retire(st_fixture *s) {
    /* A refusal preserves every owner and the first real diagnostic; no source
     * replay or dangling-graph cleanup is attempted after a failed admission. */
    if (s->error != NULL && !mc_player_tick_error_retire(s->error))
        return false;
    if (mc_player_sleep_retire(&s->preparation, NULL) != MC_PLAYER_SLEEP_OK)
        return false;
    if (s->actor != NULL) {
        if (s->world != NULL && s->world->worlds[0] != NULL)
            s->actor->base.world = s->world->worlds[0];
        if (s->original_bounds != NULL)
            s->actor->bounds = s->original_bounds;
    }
    if (mc_movement_input_retire(&s->input, NULL) != MC_MOVEMENT_INPUT_OK)
        return false;
    mc_entity_release(s->actor);
    s->actor = NULL;
    client_application *app = s->app;
    if (app != NULL) {
        if (mc_client_application_retire_statistics(app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
            mc_client_application_retire_water_effect(app, NULL) != MC_CLIENT_STATISTICS_OWNER_OK ||
            mc_client_achievement_notice_free(&app->achievement_notice, NULL) !=
                MC_CLIENT_NOTICE_OK ||
            (app->render_classes != NULL &&
             mc_client_render_classes_free(app->render_classes) != MC_CLIENT_MODEL_OK))
            return false;
    }
    if (!tick_cleanup(s->world))
        return false;
    s->world = NULL;
    tick_chunk = NULL;
    tick_context = (mc_world_entity_tick_context){0};
    if (app != NULL) {
        if (mc_game_settings_retire(&app->settings, NULL) != MC_GAME_SETTINGS_OK)
            return false;
        mc_client_game_directory_free(app->diagnostic_game_directory);
        mc_resource_files_free(app->files);
        free(app);
        s->app = NULL;
    }
    free(s->error);
    s->error = NULL;
    return true;
}

static bool st_copy(FILE *input) {
    if (fflush(input) != 0 || fseek(input, 0, SEEK_SET) != 0)
        return false;
    unsigned char bytes[4096];
    size_t count;
    while ((count = fread(bytes, 1, sizeof(bytes), input)) != 0)
        if (fwrite(bytes, 1, count, stdout) != count)
            return false;
    return !ferror(input);
}

static bool st_site(mc_player_site site) {
    if (site.class_name == NULL)
        return printf("null") >= 0;
    return site.method != NULL && site.descriptor != NULL &&
           printf("{\"class\":\"%s\",\"method\":\"%s\",\"descriptor\":\"%s\",\"pc\":%u}",
                  site.class_name, site.method, site.descriptor, site.pc) >= 0;
}

static bool st_diagnostic(const mc_player_tick_error *error) {
    const bool has_wake = error->child == MC_PLAYER_TICK_CHILD_WAKE;
    const mc_player_sleep_error *wake = has_wake ? &error->child_error.wake.error : NULL;
    const char *status = error->status == MC_ENTITY_OK            ? "OK"
                         : error->status == MC_ENTITY_STATE_ERROR ? "STATE_ERROR"
                                                                  : "OTHER_FAILURE";
    if (printf("{\"status\":\"%s\",\"status_code\":%d,\"source_class\":", status,
               (int)error->status) < 0)
        return false;
    if ((error->source_class == NULL ? printf("null") : printf("\"%s\"", error->source_class)) <
            0 ||
        printf(",\"source_method\":") < 0 ||
        (error->source_method == NULL ? printf("null") : printf("\"%s\"", error->source_method)) <
            0 ||
        printf(",\"player_pc\":%u,\"living_pc\":%u,\"local_pc\":%u,\"bed_pc\":%u,\"wake\":",
               error->player_bytecode, error->living_bytecode, error->local_bytecode,
               error->bed_bytecode) < 0)
        return false;
    if (!has_wake)
        return printf("null}") >= 0;
    if (printf(
            "{\"source_null\":%s,\"source_exception\":%s,\"native_policy\":%s,\"exception_class\":",
            st_bool(wake->status == MC_PLAYER_SLEEP_SOURCE_NULL), st_bool(wake->source_exception),
            st_bool(wake->native_policy)) < 0 ||
        (wake->exception_class == NULL ? printf("null") : printf("\"%s\"", wake->exception_class)) <
            0 ||
        printf(",\"site\":") < 0 || !st_site(wake->site) || printf(",\"child\":") < 0 ||
        !st_site(wake->child))
        return false;
    return printf("}}") >= 0;
}

static bool st_profile(const char *root, const char *resource_path, const int32_t input[8]) {
    bool ok = false;
    st_fixture s = {0};
    st_snapshot before = {0}, after = {0};
    const bool pre111_fault = st_fault_input(input);
    FILE *before_json = NULL;
    char path[8192];
    const int written = snprintf(path, sizeof(path), "%s/case-%" PRId32, root, input[0]);
    CHECK(written > 0 && (size_t)written < sizeof(path) &&
          st_prepare(&s, path, resource_path, input));
    if (pre111_fault)
        CHECK(st_snapshot_take(&s, &before));
    else {
        before.actor_random = *s.actor->random;
        before.world_random = *s.world->worlds[0]->fields.r;
    }
#if defined(_MSC_VER)
    if (tmpfile_s(&before_json) != 0)
        before_json = NULL;
#else
    before_json = tmpfile();
#endif
    CHECK(before_json != NULL);
    st_stream = before_json;
    const bool rendered = st_state(&s);
    st_stream = stdout;
    CHECK(rendered && fflush(before_json) == 0 && fflush(stdout) == 0);
    const mc_player_action_context context = {
        .travel = {.code_registry = s.world->registry[0], .trig = &trig}};
    const int32_t entry_ticks = s.actor->base.bt;
    if (!pre111_fault && input[0] != 7)
        CHECK(s.actor->base.aH == NULL && s.actor->local_player->fields.player.private_bN != NULL);
    const mc_entity_status status = mc_player_tick(s.actor, &context, NULL, s.error);
    if ((!pre111_fault && status != MC_ENTITY_OK && input[0] != 7) ||
        (pre111_fault && status != MC_ENTITY_STATE_ERROR))
        fprintf(stderr, "parent case%" PRId32 " status%d gs%u ls%u dc%u bed%u %s\n", input[0],
                (int)status, s.error->player_bytecode, s.error->living_bytecode,
                s.error->local_bytecode, s.error->bed_bytecode,
                s.error->operation == NULL ? "-" : s.error->operation);
    CHECK(s.error->status == status);
    if (pre111_fault)
        CHECK(st_snapshot_take(&s, &after));
    else {
        after.actor_random = *s.actor->random;
        after.world_random = *s.world->worlds[0]->fields.r;
    }
    if (pre111_fault) {
        CHECK(status == MC_ENTITY_STATE_ERROR && s.error->source_class != NULL &&
              strcmp(s.error->source_class, "gs") == 0 && s.error->source_method != NULL &&
              strcmp(s.error->source_method, "w_()V") == 0 &&
              st_unselected_actor(&before, &after) && st_unselected_world(&before, &after) &&
              before.actor_random.state == after.actor_random.state &&
              before.world_random.state == after.world_random.state);
        if (s.error->player_bytecode == 73)
            CHECK(s.error->child == MC_PLAYER_TICK_CHILD_WAKE &&
                  s.error->child_error.wake.call != NULL &&
                  s.error->retained_registry == s.world->registry[0]);
    } else {
        /* Profile7 deliberately keeps its NULL World into Living111. Its real
         * later failure is recorded, with no fabricated parent-return claim. */
        if (input[0] == 7) {
            CHECK(status != MC_ENTITY_OK && s.actor->base.world == NULL &&
                  s.error->player_bytecode == 111 && s.error->living_bytecode != 0);
        } else {
            /* Native continuation controls only: the original snapshots stop
             * before these effects and do not qualify their equality. */
            CHECK(status == MC_ENTITY_OK && s.actor->base.bt == mc_java_i32_add(entry_ticks, 1) &&
                  s.actor->base.aH == NULL &&
                  s.actor->local_player->fields.player.private_bN == NULL);
        }
    }
    /* Only now start a complete row: genuine initializer writes during the
     * actual call cannot split a JSON record. Close/flush before retirement. */
    CHECK(printf("SLEEP_TICK {\"case\":%" PRId32 ",\"input\":[", input[0]) >= 0);
    for (size_t i = 0; i < 8; ++i)
        CHECK(printf("%s%" PRId32, i == 0 ? "" : ",", input[i]) >= 0);
    CHECK(printf("],\"before\":") >= 0 && st_copy(before_json) && printf(",\"after\":") >= 0 &&
          st_state(&s) && printf(",\"diagnostic\":") >= 0 && st_diagnostic(s.error) &&
          printf(",\"random_before\":[") >= 0 && st_random(&before.actor_random) &&
          putchar(',') != EOF && st_random(&before.world_random) &&
          printf("],\"random_after\":[") >= 0 && st_random(&after.actor_random) &&
          putchar(',') != EOF && st_random(&after.world_random) &&
          printf("],\"unselected_actor_preserved\":%s,\"unselected_world_preserved\":%s}\n",
                 pre111_fault ? "true" : "null", pre111_fault ? "true" : "null") >= 0 &&
          fflush(stdout) == 0);
    const mc_entity_status first_status = s.error->status;
    const unsigned int first_pc = s.error->player_bytecode;
    CHECK(mc_player_tick_error_retire(s.error) && mc_player_tick_error_retire(s.error) &&
          s.error->status == first_status && s.error->player_bytecode == first_pc &&
          s.error->retained_registry == NULL &&
          (s.error->child != MC_PLAYER_TICK_CHILD_WAKE || s.error->child_error.wake.call == NULL) &&
          s.world->worlds[0]->active_calls == 0 && s.world->worlds[0]->local_leases == 0);
    if (s.error->child == MC_PLAYER_TICK_CHILD_WAKE)
        CHECK(s.error->child_error.wake.error.acquisition == NULL &&
              s.error->child_error.wake.error.occupancy == NULL &&
              s.error->child_error.wake.error.bed == NULL &&
              s.error->child_error.wake.error.world_sleep == NULL &&
              s.error->child_error.wake.error.position == NULL);
    ok = true;
cleanup:
    st_stream = stdout;
    if (before_json != NULL && fclose(before_json) != 0)
        ok = false;
    if (!st_retire(&s))
        ok = false;
    return ok;
}

static bool st_profiles(const char *root, const char *path) {
    bool ok = false;
    FILE *file = fopen(path, "rb");
    char resource_path[8192], line[256];
    const int written = snprintf(resource_path, sizeof(resource_path), "%s/resources", root);
    CHECK(file != NULL && written > 0 && (size_t)written < sizeof(resource_path));
    int32_t number = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        int32_t input[8] = {0};
        char *cursor = line;
        for (size_t i = 0; i < 8; ++i) {
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
        CHECK(*cursor == '\0' && number < 36 && input[0] == number && input[1] >= 0 &&
              input[1] < 4 && input[2] >= 0 && input[2] < 2 && input[3] >= 0 && input[3] < 2 &&
              input[4] >= 0 && input[4] < 2 && input[5] >= 0 && input[5] < 2 && input[7] >= 0 &&
              input[7] < 8 && st_profile(root, resource_path, input));
        ++number;
    }
    CHECK(!ferror(file) && number == 36);
    ok = true;
cleanup:
    if (file != NULL && fclose(file) != 0)
        ok = false;
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    char root[8192];
    st_stream = stdout;
    if (argc != 4 || strcmp(argv[1], "sleep-tick") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 2;
    CHECK(st_profiles(root, argv[3]));
    result = 0;
cleanup:
    if (result == 0 && printf("PASS: %u Player sleep parent component checks\n", checks) < 0)
        return 1;
    return result;
}
