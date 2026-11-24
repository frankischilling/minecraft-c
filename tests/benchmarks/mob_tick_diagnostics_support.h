#ifndef MC_BENCHMARK_MOB_TICK_DIAGNOSTICS_SUPPORT_H
#define MC_BENCHMARK_MOB_TICK_DIAGNOSTICS_SUPPORT_H

#include "../../src/core/java/random_profile_internal.h"
#include "../../src/entity/construction_internal.h"
#include "../../src/entity/mob/references_internal.h"
#include "../../src/entity/mob/tick/internal.h"
#include "../../src/entity/mob/tick/workspace_internal.h"
#include "../../src/world/chunk/object_state_internal.h"
#include "../../src/world/path/path_internal.h"
#include "../../src/world/runtime/entity_tick_internal.h"
#include "mc/entity/mob/construction.h"

#include <inttypes.h>

enum { MB_MAX_ACTORS = 64, MB_CHUNKS = 25, MB_TICKS = 16, MB_WARMUPS = 2, MB_SAMPLES = 21 };

typedef struct mb_fixture {
    fixture base;
    mc_math_classes *math;
    mc_entity *actors[MB_MAX_ACTORS];
    mc_chunk_state *chunks[MB_CHUNKS];
    mc_chunk_cache *cache;
    mc_chunk_pending *pending;
    mc_world_entity_tick_context context;
    size_t count;
} mb_fixture;

/* Pointer bits are used only to locate metadata entries, NEVER as digest
 * input. Ranks are assigned by deterministic typed traversal; Entity ranks
 * are explicit (unpublished Player1, actor i+2). Zero denotes source null. */
typedef struct mb_rank {
    const void *identity;
    uint64_t rank;
} mb_rank;

typedef struct mb_hasher {
    mb_rank *ranks;
    size_t capacity, used;
    uint64_t next;
    bool valid;
} mb_hasher;

typedef struct mb_guard {
    uint64_t semantic, raw, rng, chunks, ownership, constructor, player;
    uint64_t uniquifier;
    mc_java_random world_random;
    int32_t ticks[MB_MAX_ACTORS];
} mb_guard;

#define MB_INITIAL UINT64_C(1469598103934665603)

static void mb_word(uint64_t *hash, uint64_t value) {
    for (unsigned int byte = 0; byte < 8; ++byte) {
        *hash = (*hash ^ (value & UINT64_C(255))) * UINT64_C(1099511628211);
        value >>= 8;
    }
}

static uint64_t mb_float(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t mb_double(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t mb_rank_of(mb_hasher *h, const void *identity) {
    if (identity == NULL)
        return 0;
    uint64_t mixed = (uint64_t)(uintptr_t)identity;
    mixed ^= mixed >> 33;
    mixed *= UINT64_C(0xff51afd7ed558ccd);
    mixed ^= mixed >> 33;
    const size_t mask = h->capacity - 1;
    size_t slot = (size_t)mixed & mask;
    for (size_t probe = 0; probe < h->capacity; ++probe) {
        mb_rank *entry = &h->ranks[slot];
        if (entry->identity == identity)
            return entry->rank;
        if (entry->identity == NULL) {
            if (h->used == h->capacity) {
                h->valid = false;
                return UINT64_MAX;
            }
            entry->identity = identity;
            entry->rank = h->next++;
            ++h->used;
            return entry->rank;
        }
        slot = (slot + 1) & mask;
    }
    h->valid = false;
    return UINT64_MAX;
}

static void mb_begin(mb_hasher *h, const mb_fixture *f) {
    memset(h->ranks, 0, h->capacity * sizeof(*h->ranks));
    h->used = 0;
    h->next = 1;
    h->valid = true;
    (void)mb_rank_of(h, f->base.player);
    for (size_t i = 0; i < f->count; ++i)
        (void)mb_rank_of(h, f->actors[i]);
    /* Permanent Entity child ranks must not shift when a World query grows a
     * different native list between the before and after projections. */
    for (size_t i = 0; i <= f->count; ++i) {
        const mc_entity *p = i == 0 ? f->base.player : f->actors[i - 1];
        const mc_living_entity_fields *v =
            p->mob == NULL ? &p->local_player->fields.living : &p->mob->living;
        (void)mb_rank_of(h, p->bounds);
        (void)mb_rank_of(h, p->random);
        (void)mb_rank_of(h, p->watcher);
        (void)mb_rank_of(h, v->O);
        (void)mb_rank_of(h, v->R);
        if (p->local_player != NULL) {
            const mc_local_player_snapshot *snapshot = &p->local_player->fields;
            const mc_player_fields *player = &snapshot->player;
            const void *children[] = {player->c,
                                      player->d,
                                      player->e,
                                      player->l,
                                      player->n,
                                      player->v,
                                      player->private_b,
                                      player->private_bN,
                                      player->D,
                                      snapshot->input,
                                      snapshot->minecraft,
                                      snapshot->skin_url,
                                      snapshot->filters[0],
                                      snapshot->filters[1],
                                      snapshot->filters[2]};
            for (size_t child = 0; child < sizeof(children) / sizeof(*children); ++child)
                (void)mb_rank_of(h, children[child]);
        }
        if (p->watcher != NULL && p->watcher->map != NULL) {
            (void)mb_rank_of(h, p->watcher->map);
            size_t count = 0;
            for (const mc_entity_watch_node *node = p->watcher->map->head; node != NULL;
                 node = node->next) {
                if (count++ >= p->watcher->map->size) {
                    h->valid = false;
                    break;
                }
                (void)mb_rank_of(h, node);
                (void)mb_rank_of(h, node->record);
                if (node->record != NULL)
                    (void)mb_rank_of(h, node->record->string_value);
            }
        }
    }
    /* Chunk identity ranks are likewise independent of temporary path/box
     * growth. A changed source Chunk list or voxel still changes its digest. */
    for (size_t i = 0; i < MB_CHUNKS; ++i) {
        const mc_chunk_state *state = f->chunks[i];
        const mc_world_chunk *chunk = mc_world_store_chunk_object(f->base.store, state);
        if (state == NULL || state->data == NULL || chunk == NULL || chunk->m == NULL) {
            h->valid = false;
            continue;
        }
        (void)mb_rank_of(h, state);
        (void)mb_rank_of(h, chunk);
        (void)mb_rank_of(h, chunk->data);
        (void)mb_rank_of(h, chunk->d);
        (void)mb_rank_of(h, chunk->e);
        (void)mb_rank_of(h, chunk->f);
        (void)mb_rank_of(h, chunk->g);
        (void)mb_rank_of(h, chunk->h);
        (void)mb_rank_of(h, chunk->l);
        (void)mb_rank_of(h, chunk->m);
        (void)mb_rank_of(h, state->data->blocks);
        if (chunk->e != NULL)
            (void)mb_rank_of(h, chunk->e->a);
        if (chunk->f != NULL)
            (void)mb_rank_of(h, chunk->f->a);
        if (chunk->g != NULL)
            (void)mb_rank_of(h, chunk->g->a);
        for (size_t slice = 0; slice < CHUNK_ENTITY_SLICES; ++slice) {
            const mc_world_chunk_entity_list *list = chunk->m->items[slice];
            (void)mb_rank_of(h, list);
            if (list != NULL)
                (void)mb_rank_of(h, list->items);
        }
    }
}

static void mb_bytes(uint64_t *hash, const uint8_t *bytes, size_t length) {
    mb_word(hash, length);
    for (size_t i = 0; i < length; ++i)
        *hash = (*hash ^ bytes[i]) * UINT64_C(1099511628211);
}

static void mb_text(uint64_t *hash, const mc_player_string *text) {
    mc_player_string_view view = {0};
    mb_word(hash, text != NULL);
    if (text != NULL && mc_player_string_read(text, &view)) {
        mb_word(hash, view.length);
        for (size_t i = 0; i < view.length; ++i)
            mb_word(hash, view.units[i]);
    }
}

static void mb_random(uint64_t *hash, const mc_java_random *random) {
    mb_word(hash, random != NULL);
    if (random != NULL) {
        mb_word(hash, random->state);
        mb_word(hash, mb_double(random->next_next_gaussian));
        mb_word(hash, random->have_next_next_gaussian);
    }
}

static void mb_bounds(uint64_t *hash, const mc_entity_bounds *box) {
    mb_word(hash, box != NULL);
    if (box != NULL) {
        mb_word(hash, mb_double(box->min_x));
        mb_word(hash, mb_double(box->min_y));
        mb_word(hash, mb_double(box->min_z));
        mb_word(hash, mb_double(box->max_x));
        mb_word(hash, mb_double(box->max_y));
        mb_word(hash, mb_double(box->max_z));
    }
}

static uint64_t mb_path(mb_hasher *h, const mc_path *path) {
    uint64_t hash = MB_INITIAL;
    mb_word(&hash, mb_rank_of(h, path));
    if (path == NULL)
        return hash;
    mb_word(&hash, (uint64_t)(uint32_t)path->a);
    mb_word(&hash, (uint64_t)(uint32_t)path->c);
    mb_word(&hash, path->array_length);
    mb_word(&hash, path->holds);
    mb_word(&hash, path->constructed);
    mb_word(&hash, path->constructor_pc);
    mb_word(&hash, mb_rank_of(h, path->arena));
    if (path->arena == NULL) {
        h->valid = false;
        return hash;
    }
    mb_word(&hash, path->arena->holds);
    mb_word(&hash, path->arena->points);
    mb_word(&hash, path->arena->bytes);
    size_t count = 0;
    for (const mc_path_point *p = path->arena->point_list; p != NULL; p = p->allocation_next) {
        if (count++ >= path->arena->points) {
            h->valid = false;
            break;
        }
        mb_word(&hash, mb_rank_of(h, p));
        mb_word(&hash, (uint64_t)(uint32_t)p->a);
        mb_word(&hash, (uint64_t)(uint32_t)p->b);
        mb_word(&hash, (uint64_t)(uint32_t)p->c);
        mb_word(&hash, (uint64_t)(uint32_t)p->j);
        mb_word(&hash, (uint64_t)(uint32_t)p->d);
        mb_word(&hash, mb_float(p->e));
        mb_word(&hash, mb_float(p->f));
        mb_word(&hash, mb_float(p->g));
        mb_word(&hash, mb_rank_of(h, p->h));
        mb_word(&hash, p->i);
        mb_word(&hash, p->constructed);
        mb_word(&hash, p->constructor_pc);
    }
    if (count != path->arena->points)
        h->valid = false;
    mb_word(&hash, mb_rank_of(h, path->b));
    for (size_t i = 0; i < path->array_length; ++i)
        mb_word(&hash, mb_rank_of(h, path->b[i]));
    return hash;
}

static uint64_t mb_entity(mb_hasher *h, const mc_entity *p, uint64_t *constructor, uint64_t *raw) {
    uint64_t hash = MB_INITIAL;
    const mc_entity_base_fields *b = &p->base;
    const mc_living_entity_fields *v =
        p->mob == NULL ? &p->local_player->fields.living : &p->mob->living;
#define V(value) mb_word(&hash, (uint64_t)(value))
#define I(field) V((uint32_t)b->field)
#define B(field) V(b->field)
#define F(field) V(mb_float(b->field))
#define D(field) V(mb_double(b->field))
#define P(field) V(mb_rank_of(h, b->field))
    V(mb_rank_of(h, p));
    V(p->class_literal->kind);
    V(p->class_literal == &p->classes->literals[p->class_literal->kind]);
    V(p->phase);
    V(p->base_bytecode);
    V(p->item_bytecode);
    V(p->falling_bytecode);
    I(id);
    D(aE);
    B(aF);
    P(aG);
    P(aH);
    P(world);
    D(aJ);
    D(aK);
    D(aL);
    D(x);
    D(y);
    D(z);
    D(velocity_x);
    D(velocity_y);
    D(velocity_z);
    F(yaw);
    F(aT);
    F(aU);
    F(aV);
    B(aX);
    B(aY);
    B(aZ);
    B(ba);
    B(bb);
    B(bc);
    B(bd);
    B(be);
    F(offset);
    F(width);
    F(height);
    F(bi);
    F(bj);
    F(bk);
    I(private_b);
    D(bl);
    D(bm);
    D(bn);
    F(y_size);
    F(bp);
    B(bq);
    F(br);
    I(bt);
    I(bu);
    I(bv);
    I(bw);
    B(bx);
    I(by);
    I(bz);
    B(private_c);
    B(bC);
    F(bE);
    D(private_d);
    D(private_e);
    B(bF);
    I(bG);
    I(bH);
    I(bI);
    B(bM);
    P(client_only.bA);
    P(client_only.bB);
    I(client_only.bJ);
    I(client_only.bK);
    I(client_only.bL);
#undef I
#undef B
#undef F
#undef D
#undef P
#define I(field) V((uint32_t)v->field)
#define B(field) V(v->field)
#define F(field) V(mb_float(v->field))
#define D(field) V(mb_double(v->field))
#define P(field) V(mb_rank_of(h, v->field))
    I(E);
    /* Only these original constructor draws are unpaired across processes.
     * Their actual source evolution remains in the raw/constructor digests. */
    mb_word(constructor, mb_float(v->F));
    mb_word(constructor, mb_float(v->G));
    mb_word(constructor, mb_float(v->aj));
    F(H);
    F(I);
    F(J);
    F(K);
    F(L);
    F(M);
    B(N);
    P(O);
    B(P);
    F(Q);
    P(R);
    F(S);
    I(T);
    F(U);
    B(V);
    F(W);
    F(X);
    I(Y);
    I(Z);
    I(private_a);
    I(aa);
    I(ab);
    F(ac);
    I(ad);
    I(ae);
    F(af);
    F(ag);
    B(ah);
    I(ai);
    F(ak);
    F(al);
    F(am);
    I(an);
    D(ao);
    D(ap);
    D(aq);
    D(ar);
    D(as);
    F(at);
    I(au);
    I(av);
    F(aw);
    F(ax);
    F(ay);
    B(az);
    F(aA);
    F(aB);
    P(private_b);
    I(aC);
#undef I
#undef B
#undef F
#undef D
#undef P
    mb_text(&hash, v->O);
    mb_text(&hash, v->R);
    if (p->local_player != NULL) {
        const mc_local_player_snapshot *snapshot = &p->local_player->fields;
        const mc_player_fields *player = &snapshot->player;
#define PI(field) V((uint32_t)player->field)
#define PB(field) V(player->field)
#define PF(field) V(mb_float(player->field))
#define PD(field) V(mb_double(player->field))
#define PP(field) V(mb_rank_of(h, player->field))
        PP(c);
        PP(d);
        PP(e);
        PI(f);
        PI(g);
        PF(h);
        PF(i);
        PB(j);
        PI(k);
        PP(l);
        PI(m);
        PP(n);
        PD(o);
        PD(p);
        PD(q);
        PD(r);
        PD(s);
        PD(t);
        PB(u);
        PP(v);
        PI(private_a);
        PF(w);
        PF(x);
        PF(y);
        PP(private_b);
        PP(private_bN);
        PI(z);
        PB(A);
        PF(B);
        PF(C);
        PI(private_bO);
        PP(D);
#undef PI
#undef PB
#undef PF
#undef PD
#undef PP
        mb_text(&hash, player->l);
        mb_text(&hash, player->n);
        V(mb_rank_of(h, snapshot->input));
        V(mb_rank_of(h, snapshot->minecraft));
        V(mb_rank_of(h, snapshot->skin_url));
        mb_text(&hash, snapshot->skin_url);
        V(snapshot->living_pc);
        V(snapshot->player_pc);
        V(snapshot->local_pc);
        V(snapshot->watcher_hook_pc);
        for (size_t i = 0; i < 3; ++i) {
            V(mb_rank_of(h, snapshot->filters[i]));
            if (snapshot->filters[i] != NULL) {
                V(mb_float(snapshot->filters[i]->a));
                V(mb_float(snapshot->filters[i]->b));
                V(mb_float(snapshot->filters[i]->c));
            }
        }
    }
    V(mb_rank_of(h, p->bounds));
    mb_bounds(&hash, p->bounds);
    V(mb_rank_of(h, p->random));
    mb_random(&hash, p->random);
    V(mb_rank_of(h, p->watcher));
    if (p->watcher != NULL) {
        V(p->watcher->dirty);
        V(mb_rank_of(h, p->watcher->map));
        if (p->watcher->map != NULL) {
            V(p->watcher->map->size);
            size_t count = 0;
            for (const mc_entity_watch_node *node = p->watcher->map->head; node != NULL;
                 node = node->next) {
                if (count++ >= p->watcher->map->size) {
                    h->valid = false;
                    break;
                }
                V(mb_rank_of(h, node));
                V((uint32_t)node->key);
                V(mb_rank_of(h, node->record));
                if (node->record == NULL) {
                    h->valid = false;
                    break;
                }
                V((uint32_t)node->record->index);
                V((uint32_t)node->record->type);
                V((uint32_t)node->record->payload_kind);
                V((uint8_t)node->record->byte_value);
                V((uint32_t)node->record->integer_value);
                V(mb_rank_of(h, node->record->string_value));
                mb_text(&hash, node->record->string_value);
                V(node->record->dirty);
            }
            if (count != p->watcher->map->size)
                h->valid = false;
        }
    }
    V(mb_rank_of(h, p->mount_hold));
    V(mb_rank_of(h, p->passenger_hold));
    if (p->mob != NULL) {
        const mc_mob_entity_state *m = p->mob;
        V(mb_rank_of(h, m->creature_target));
        V(mb_rank_of(h, m->look_target_hold));
        V(mb_rank_of(h, m->creature_target_hold));
        V(mb_rank_of(h, m->creature_path_hold));
        V(mb_rank_of(h, m->pending_ray));
        V(mb_rank_of(h, m->pending_path));
        V(mb_rank_of(h, m->pending_death));
        V(m->tick_active);
        V(m->creature_attacking);
        V((uint32_t)m->monster_damage);
        V((uint32_t)m->creeper_a);
        V((uint32_t)m->creeper_b);
        V(m->living_pc);
        V(m->creature_pc);
        V(m->monster_pc);
        V(m->constructor_pc);
        V(m->watcher_hook_pc);
        V(m->animal_pc);
        V(m->water_creature_pc);
        V(m->flying_pc);
        V(mb_path(h, m->creature_path));
    }
    mb_word(raw, hash);
    mb_word(raw, mb_float(v->F));
    mb_word(raw, mb_float(v->G));
    mb_word(raw, mb_float(v->aj));
#undef V
    return hash;
}

static void mb_entity_list(mb_hasher *h, uint64_t *hash, const mc_world_reference_list *list) {
    mb_word(hash, mb_rank_of(h, list));
    if (list == NULL)
        return;
    mb_word(hash, list->count);
    mb_word(hash, list->capacity);
    mb_word(hash, (uint32_t)list->mod_count);
    mb_word(hash, mb_rank_of(h, list->items));
    for (size_t i = 0; i < list->count; ++i)
        mb_word(hash, mb_rank_of(h, list->items[i]));
}

static bool mb_snapshot(mb_fixture *f, mb_hasher *h, mb_guard *g) {
    mb_begin(h, f);
    *g = (mb_guard){.semantic = MB_INITIAL,
                    .raw = MB_INITIAL,
                    .rng = MB_INITIAL,
                    .chunks = MB_INITIAL,
                    .ownership = MB_INITIAL,
                    .constructor = MB_INITIAL};
    mc_world_runtime *w = f->base.world;
    mc_entity_classes *classes = f->base.app.entities;
    const mc_world_runtime_fields *v = &w->fields;
#define S(value)                                                                                   \
    do {                                                                                           \
        mb_word(&g->semantic, (uint64_t)(value));                                                  \
        mb_word(&g->raw, (uint64_t)(value));                                                       \
    } while (0)
#define O(value) mb_word(&g->ownership, (uint64_t)(value))
#define I(field) S((uint32_t)v->field)
#define B(field) S(v->field)
#define F(field) S(mb_float(v->field))
#define P(field) S(mb_rank_of(h, v->field))
    B(a);
    P(C);
    P(b);
    P(D);
    P(E);
    P(F);
    P(c);
    P(G);
    P(d);
    P(e);
    S((uint64_t)v->H);
    I(f);
    I(g);
    I(h);
    F(i);
    F(j);
    F(k);
    F(l);
    I(m);
    I(n);
    B(o);
    S((uint64_t)v->I);
    I(p);
    I(q);
    P(r);
    B(s);
    P(t);
    P(u);
    P(v);
    P(w);
    P(x);
    B(y);
    B(J);
    P(z);
    P(K);
    B(L);
    I(M);
    B(N);
    B(O);
    P(P);
    I(Q);
    P(R);
    B(B);
#undef I
#undef B
#undef F
#undef P
    mc_world_info_fields info;
    CHECK(mc_world_info_fields_get(v->x, &info) == MC_WORLD_INFO_OK && info.value.time == 13000);
    S((uint64_t)info.value.seed);
    S((uint32_t)info.value.spawn_x);
    S((uint32_t)info.value.spawn_y);
    S((uint32_t)info.value.spawn_z);
    S((uint64_t)info.value.time);
    S((uint64_t)info.value.last_played);
    S((uint64_t)info.value.size_on_disk);
    S((uint32_t)info.value.player_dimension);
    S((uint32_t)info.value.version);
    S((uint32_t)info.value.rain_time);
    S((uint32_t)info.value.thunder_time);
    S(info.value.raining);
    S(info.value.thundering);
    S(mb_rank_of(h, info.name));
    S(mb_rank_of(h, info.player));
    const mc_world_reference_list *entity_lists[] = {v->b, v->D, v->d, v->e, v->R,
                                                     v->C, v->c, v->G, v->K, v->u};
    for (size_t i = 0; i < sizeof(entity_lists) / sizeof(*entity_lists); ++i) {
        uint64_t digest = MB_INITIAL;
        mb_entity_list(h, &digest, entity_lists[i]);
        S(digest);
    }
    S(w->construction.phase);
    S(w->construction.last_store_pc);
    S(w->classes->enqueue_depth);
    S(classes->next_id);
    S((uint32_t)classes->bounds_cursor);
    S((uint32_t)classes->vector_cursor);
    S(classes->entity_phase);
    S(classes->bounds_phase);
    S(classes->vector_phase);
    S(classes->watcher_phase);
    for (size_t i = 0; i <= f->count; ++i) {
        const mc_entity *p = i == 0 ? f->base.player : f->actors[i - 1];
        const uint64_t semantic = mb_entity(h, p, &g->constructor, &g->raw);
        mb_word(&g->semantic, semantic);
        if (i == 0) {
            uint64_t player = semantic;
            const mc_living_entity_fields *living = &p->local_player->fields.living;
            mb_word(&player, mb_float(living->F));
            mb_word(&player, mb_float(living->G));
            mb_word(&player, mb_float(living->aj));
            g->player = player;
        } else
            g->ticks[i - 1] = p->base.bt;
        mb_random(&g->rng, p->random);
        O(mb_rank_of(h, p));
        O(p->holds);
        O(p->direct_bytes);
        O(p->phase);
    }
    mb_random(&g->rng, v->r);
    g->world_random = *v->r;
    mc_java_random_profile *profile = f->base.app.profile;
    CHECK(!profile->constructor_inputs_bound && profile->math != NULL);
    g->uniquifier = atomic_load(&profile->uniquifier);
    mb_word(&g->rng, g->uniquifier);
    mb_word(&g->rng, atomic_load(&profile->math->state));
    mb_word(&g->rng, mb_double(profile->math->next_next_gaussian));
    mb_word(&g->rng, profile->math->have_next_next_gaussian);
    S(g->rng);
    O(w->active_calls);
    O(w->local_leases);
    O(w->persistent_bindings);
    O(w->construction.direct_allocation_bytes);
    O(classes->allocated_bytes);
    O(classes->live_entities);
    O(classes->borrowers);
    O(classes->live_path_arenas);
    O(classes->bounds_holds);
    O(classes->vector_holds);
    O(classes->live_hit_results);
    O(classes->live_hit_calls);
    O(profile->allocated_bytes);
    O(f->base.app.registry->runtime_borrows);
    const mc_world_entity_list_ledger *ledgers[] = {&w->entity_membership, &w->entity_deferral,
                                                    &w->player_membership, &w->weather_membership,
                                                    &w->nearby_entities};
    for (size_t i = 0; i < sizeof(ledgers) / sizeof(*ledgers); ++i) {
        const mc_world_entity_list_ledger *ledger = ledgers[i];
        O(mb_rank_of(h, ledger->list));
        O(mb_rank_of(h, ledger->entries));
        O(ledger->count);
        O(ledger->capacity);
        for (size_t j = 0; j < ledger->count; ++j)
            O(mb_rank_of(h, ledger->entries[j]));
    }
    O(w->collision_boxes.count);
    O(w->collision_boxes.capacity);
    for (size_t i = 0; i < w->collision_boxes.count; ++i) {
        const mc_world_collision_reference *box = &w->collision_boxes.entries[i];
        O(mb_rank_of(h, box->bounds));
        O(mb_rank_of(h, box->pool));
        O(mb_rank_of(h, box->entity));
        uint64_t digest = MB_INITIAL;
        mb_bounds(&digest, box->bounds);
        S(digest);
    }
    if (classes->bounds_pool != NULL) {
        S(classes->bounds_pool->length);
        S(classes->bounds_pool->capacity);
        for (size_t i = 0; i < classes->bounds_pool->length; ++i) {
            S(mb_rank_of(h, classes->bounds_pool->elements[i]));
            uint64_t digest = MB_INITIAL;
            mb_bounds(&digest, classes->bounds_pool->elements[i]);
            S(digest);
        }
    } else
        S(0);
    if (classes->vector_pool != NULL) {
        S(classes->vector_pool->length);
        S(classes->vector_pool->capacity);
        for (size_t i = 0; i < classes->vector_pool->length; ++i) {
            const mc_entity_vector *p = classes->vector_pool->elements[i];
            S(mb_rank_of(h, p));
            S(mb_double(p->x));
            S(mb_double(p->y));
            S(mb_double(p->z));
        }
    } else
        S(0);
    for (size_t i = 0; i < MB_CHUNKS; ++i) {
        const mc_chunk_state *state = f->chunks[i];
        const mc_world_chunk *chunk = mc_world_store_chunk_object(f->base.store, state);
        CHECK(chunk != NULL && state->data != NULL && !state->blank && state->active &&
              state->terrain_populated);
#define C(value) mb_word(&g->chunks, (uint64_t)(value))
        C(mb_rank_of(h, state));
        C((uint32_t)state->x);
        C((uint32_t)state->z);
        C(state->blank);
        C(state->dirty);
        C(state->active);
        C(state->terrain_populated);
        C(state->never_save);
        C(state->has_entities);
        C((uint64_t)state->last_save_time);
        C(mb_rank_of(h, chunk));
        C(mb_rank_of(h, chunk->data));
        C(mb_rank_of(h, chunk->d));
        C(mb_rank_of(h, chunk->e));
        C(mb_rank_of(h, chunk->f));
        C(mb_rank_of(h, chunk->g));
        C(mb_rank_of(h, chunk->h));
        C(mb_rank_of(h, chunk->l));
        C(mb_rank_of(h, chunk->m));
        C(mb_rank_of(h, state->data->blocks));
        C(state->data->blocks_capacity);
        CHECK(chunk->e != NULL && chunk->f != NULL && chunk->g != NULL);
        C(mb_rank_of(h, chunk->e->a));
        C(chunk->e->length);
        C(mb_rank_of(h, chunk->f->a));
        C(chunk->f->length);
        C(mb_rank_of(h, chunk->g->a));
        C(chunk->g->length);
        C(chunk->m->length);
        C(chunk->l->count);
        C(mb_rank_of(h, chunk->l->backing));
        mb_bytes(&g->chunks, state->data->blocks, MC_CHUNK_BLOCKS);
        mb_bytes(&g->chunks, state->data->metadata, MC_CHUNK_NIBBLE_BYTES);
        mb_bytes(&g->chunks, state->data->skylight, MC_CHUNK_NIBBLE_BYTES);
        mb_bytes(&g->chunks, state->data->block_light, MC_CHUNK_NIBBLE_BYTES);
        mb_bytes(&g->chunks, state->data->heightmap, MC_CHUNK_HEIGHTMAP_BYTES);
        C((uint32_t)state->data->minimum_height);
        for (size_t slice = 0; slice < CHUNK_ENTITY_SLICES; ++slice) {
            const mc_world_chunk_entity_list *list = chunk->m->items[slice];
            CHECK(list != NULL);
            C(mb_rank_of(h, list));
            C(mb_rank_of(h, list->items));
            C(list->count);
            C(list->capacity);
            for (size_t j = 0; j < list->count; ++j)
                C(mb_rank_of(h, list->items[j]));
        }
#undef C
    }
    S(g->chunks);
    S(g->ownership);
#undef S
#undef O
    CHECK(h->valid && w->active_calls == 0 && w->local_leases == 0 &&
          w->player_membership.count == 0 && w->fields.d->count == 0 &&
          w->entity_membership.count == f->count && w->fields.b->count == f->count &&
          w->fields.D->count == 0 && w->fields.e->count == 0 && w->world_accesses.count == 0 &&
          f->cache->count == MB_CHUNKS);
    for (size_t i = 0; i < f->count; ++i) {
        const mc_entity *p = f->actors[i];
        CHECK(!p->base.be && p->base.bF && p->mob != NULL && !p->mob->tick_active &&
              p->mob->pending_ray == NULL && p->mob->pending_path == NULL &&
              p->mob->pending_death == NULL && p->mob->creature_target == NULL &&
              p->mob->creature_target_hold == NULL && p->mob->living.private_b == NULL &&
              p->mob->look_target_hold == NULL && !mc_entity_effect_has_pending(p));
    }
    return true;
}

#endif
