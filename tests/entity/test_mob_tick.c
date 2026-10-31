/* Genuine constructed and ordinarily published Mob tick contracts. The shared
 * World fixture supplies actual GAME owners; its unrelated FD.n main is unused. */
int mc_mob_tick_world_fixture_main(int argc, char **argv);
#define main mc_mob_tick_world_fixture_main
#include "../world/runtime/test_world_random_tick.c"
#undef main
#include "../support/java_random.h"
#include "entity/mob/internal.h"
#include "entity/mob/references_internal.h"
#include "mc/entity/mob/tick.h"
#include "mc/entity/movement/material.h"
#include "mc/world/runtime/entity_tick.h"
#include "world/runtime/tile_entities/piston_list_internal.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Mob tick check%u line%d: %s\n", checks, __LINE__, #c);                \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define fbits float_bits
#define dbits double_bits

static FILE *output;
static bool first;

static void key(const char *name) {
    fprintf(output, "%s\"%s\":", first ? "" : ",", name);
    first = false;
}

static void emit_i(const char *name, int32_t v) {
    key(name);
    fprintf(output, "%" PRId32, v);
}

static void emit_z(const char *name, bool v) {
    key(name);
    fputs(v ? "true" : "false", output);
}

static const char *entity_reference_class(const mc_entity *entity) {
    if (!entity)
        return NULL;
    mc_entity_class_literal_view value;
    if (mc_entity_class_literal_inspect(entity->class_literal, &value) != MC_ENTITY_OK ||
        value.owner != entity->classes || value.source != MC_ENTITY_CLIENT)
        exit(3);
    return value.client_name;
}

static void emit_object(const void *value, const char *class_name) {
    if (!value) {
        fputs("null", output);
        return;
    }
    if (!class_name)
        exit(3);
    /* Comparison-only native pointer value, never a Java hash or source input. */
    fprintf(output, "{\"class\":\"%s\",\"identity\":\"%016" PRIxPTR "\"}", class_name,
            (uintptr_t)value);
}

static void emit_r(const char *name, const void *value, const char *class_name) {
    key(name);
    emit_object(value, class_name);
}

static void emit_f(const char *name, float v) {
    key(name);
    fprintf(output, "{\"F\":\"%08" PRIx32 "\"}", fbits(v));
}

static void emit_d(const char *name, double v) {
    key(name);
    fprintf(output, "{\"D\":\"%016" PRIx64 "\"}", dbits(v));
}

static void emit_s(const char *name, const mc_player_string *v) {
    key(name);
    if (!v) {
        fputs("null", output);
        return;
    }
    mc_player_string_view s;
    if (!mc_player_string_read(v, &s))
        exit(3);
    fputs("{\"utf16\":[", output);
    for (size_t i = 0; i < s.length; ++i)
        fprintf(output, "%s%u", i ? "," : "", (unsigned)s.units[i]);
    fputs("]}", output);
}

static void emit_bounds(const mc_entity_bounds *b) {
    first = true;
    emit_d("eq.a", b->min_x);
    emit_d("eq.b", b->min_y);
    emit_d("eq.c", b->min_z);
    emit_d("eq.d", b->max_x);
    emit_d("eq.e", b->max_y);
    emit_d("eq.f", b->max_z);
}

static void emit_fields(mc_entity *entity, const mc_mob_state *value) {
    const mc_mob_state state = *value;
    first = true;
    /* Authored observation transport; original field order from pinned declarations. */
    emit_i("sn.aD", entity->base.id);
    emit_d("sn.aE", entity->base.aE);
    emit_z("sn.aF", entity->base.aF);
    emit_r("sn.aG", entity->base.aG, entity_reference_class(entity->base.aG));
    emit_r("sn.aH", entity->base.aH, entity_reference_class(entity->base.aH));
    emit_r("sn.aI", entity->base.world, "fd");
    emit_d("sn.aJ", entity->base.aJ);
    emit_d("sn.aK", entity->base.aK);
    emit_d("sn.aL", entity->base.aL);
    emit_d("sn.aM", entity->base.x);
    emit_d("sn.aN", entity->base.y);
    emit_d("sn.aO", entity->base.z);
    emit_d("sn.aP", entity->base.velocity_x);
    emit_d("sn.aQ", entity->base.velocity_y);
    emit_d("sn.aR", entity->base.velocity_z);
    emit_f("sn.aS", entity->base.yaw);
    emit_f("sn.aT", entity->base.aT);
    emit_f("sn.aU", entity->base.aU);
    emit_f("sn.aV", entity->base.aV);
    emit_r("sn.aW", entity->bounds, "eq");
    emit_z("sn.aX", entity->base.aX);
    emit_z("sn.aY", entity->base.aY);
    emit_z("sn.aZ", entity->base.aZ);
    emit_z("sn.ba", entity->base.ba);
    emit_z("sn.bb", entity->base.bb);
    emit_z("sn.bc", entity->base.bc);
    emit_z("sn.bd", entity->base.bd);
    emit_z("sn.be", entity->base.be);
    emit_f("sn.bf", entity->base.offset);
    emit_f("sn.bg", entity->base.width);
    emit_f("sn.bh", entity->base.height);
    emit_f("sn.bi", entity->base.bi);
    emit_f("sn.bj", entity->base.bj);
    emit_f("sn.bk", entity->base.bk);
    emit_i("sn.b", entity->base.private_b);
    emit_d("sn.bl", entity->base.bl);
    emit_d("sn.bm", entity->base.bm);
    emit_d("sn.bn", entity->base.bn);
    emit_f("sn.bo", entity->base.y_size);
    emit_f("sn.bp", entity->base.bp);
    emit_z("sn.bq", entity->base.bq);
    emit_f("sn.br", entity->base.br);
    emit_r("sn.bs", entity->random, "java.util.Random");
    emit_i("sn.bt", entity->base.bt);
    emit_i("sn.bu", entity->base.bu);
    emit_i("sn.bv", entity->base.bv);
    emit_i("sn.bw", entity->base.bw);
    emit_z("sn.bx", entity->base.bx);
    emit_i("sn.by", entity->base.by);
    emit_i("sn.bz", entity->base.bz);
    emit_z("sn.c", entity->base.private_c);
    emit_s("sn.bA", (const mc_player_string *)entity->base.client_only.bA);
    emit_s("sn.bB", entity->base.client_only.bB);
    emit_z("sn.bC", entity->base.bC);
    emit_r("sn.bD", entity->watcher, "ud");
    emit_f("sn.bE", entity->base.bE);
    emit_d("sn.d", entity->base.private_d);
    emit_d("sn.e", entity->base.private_e);
    emit_z("sn.bF", entity->base.bF);
    emit_i("sn.bG", entity->base.bG);
    emit_i("sn.bH", entity->base.bH);
    emit_i("sn.bI", entity->base.bI);
    emit_i("sn.bJ", entity->base.client_only.bJ);
    emit_i("sn.bK", entity->base.client_only.bK);
    emit_i("sn.bL", entity->base.client_only.bL);
    emit_z("sn.bM", entity->base.bM);
    emit_i("ls.E", state.living.E);
    emit_f("ls.F", state.living.F);
    emit_f("ls.G", state.living.G);
    emit_f("ls.H", state.living.H);
    emit_f("ls.I", state.living.I);
    emit_f("ls.J", state.living.J);
    emit_f("ls.K", state.living.K);
    emit_f("ls.L", state.living.L);
    emit_f("ls.M", state.living.M);
    emit_z("ls.N", state.living.N);
    emit_s("ls.O", state.living.O);
    emit_z("ls.P", state.living.P);
    emit_f("ls.Q", state.living.Q);
    emit_s("ls.R", state.living.R);
    emit_f("ls.S", state.living.S);
    emit_i("ls.T", state.living.T);
    emit_f("ls.U", state.living.U);
    emit_z("ls.V", state.living.V);
    emit_f("ls.W", state.living.W);
    emit_f("ls.X", state.living.X);
    emit_i("ls.Y", state.living.Y);
    emit_i("ls.Z", state.living.Z);
    emit_i("ls.a", state.living.private_a);
    emit_i("ls.aa", state.living.aa);
    emit_i("ls.ab", state.living.ab);
    emit_f("ls.ac", state.living.ac);
    emit_i("ls.ad", state.living.ad);
    emit_i("ls.ae", state.living.ae);
    emit_f("ls.af", state.living.af);
    emit_f("ls.ag", state.living.ag);
    emit_z("ls.ah", state.living.ah);
    emit_i("ls.ai", state.living.ai);
    emit_f("ls.aj", state.living.aj);
    emit_f("ls.ak", state.living.ak);
    emit_f("ls.al", state.living.al);
    emit_f("ls.am", state.living.am);
    emit_i("ls.an", state.living.an);
    emit_d("ls.ao", state.living.ao);
    emit_d("ls.ap", state.living.ap);
    emit_d("ls.aq", state.living.aq);
    emit_d("ls.ar", state.living.ar);
    emit_d("ls.as", state.living.as);
    emit_f("ls.at", state.living.at);
    emit_i("ls.au", state.living.au);
    emit_i("ls.av", state.living.av);
    emit_f("ls.aw", state.living.aw);
    emit_f("ls.ax", state.living.ax);
    emit_f("ls.ay", state.living.ay);
    emit_z("ls.az", state.living.az);
    emit_f("ls.aA", state.living.aA);
    emit_f("ls.aB", state.living.aB);
    emit_r("ls.b", state.living.private_b, entity_reference_class(state.living.private_b));
    emit_i("ls.aC", state.living.aC);

    if (state.kind != MC_ENTITY_CLASS_KIND_SLIME) {
        emit_r("ii.a", state.creature_path, "dh");
        emit_r("ii.d", state.creature_target, entity_reference_class(state.creature_target));
        emit_z("ii.e", state.creature_attacking);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_SPIDER || state.kind == MC_ENTITY_CLASS_KIND_ZOMBIE ||
        state.kind == MC_ENTITY_CLASS_KIND_SKELETON || state.kind == MC_ENTITY_CLASS_KIND_CREEPER) {
        emit_i("gz.c", state.monster_damage);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_CREEPER) {
        emit_i("gb.a", state.creeper_a);
        emit_i("gb.b", state.creeper_b);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_SLIME) {
        emit_f("uw.a", state.slime_a);
        emit_f("uw.b", state.slime_b);
        emit_i("uw.c", state.slime_jump_delay);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_CHICKEN) {
        emit_z("ww.a", state.chicken.a);
        emit_f("ww.b", state.chicken.b);
        emit_f("ww.c", state.chicken.c);
        emit_f("ww.f", state.chicken.f);
        emit_f("ww.g", state.chicken.g);
        emit_f("ww.h", state.chicken.h);
        emit_i("ww.i", state.chicken.i);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_WOLF) {
        emit_z("gi.a", state.wolf.a);
        emit_f("gi.b", state.wolf.b);
        emit_f("gi.c", state.wolf.c);
        emit_z("gi.f", state.wolf.f);
        emit_z("gi.g", state.wolf.g);
        emit_f("gi.h", state.wolf.h);
        emit_f("gi.i", state.wolf.i);
    }
    if (state.kind == MC_ENTITY_CLASS_KIND_SQUID) {
        emit_f("xt.a", state.squid.a);
        emit_f("xt.b", state.squid.b);
        emit_f("xt.c", state.squid.c);
        emit_f("xt.f", state.squid.f);
        emit_f("xt.g", state.squid.g);
        emit_f("xt.h", state.squid.h);
        emit_f("xt.i", state.squid.i);
        emit_f("xt.j", state.squid.j);
        emit_f("xt.k", state.squid.k);
        emit_f("xt.l", state.squid.l);
        emit_f("xt.m", state.squid.m);
        emit_f("xt.n", state.squid.n);
        emit_f("xt.o", state.squid.o);
        emit_f("xt.p", state.squid.p);
    }
}

static bool emit_watcher(const mc_entity *entity) {
    CHECK(entity->watcher && entity->watcher->map);
    fprintf(output, "{\"size\":%zu,\"dirty\":%s,\"records\":[", entity->watcher->map->size,
            entity->watcher->dirty ? "true" : "false");
    size_t seen = 0;
    for (int32_t index = 0; index < 32; ++index)
        for (const mc_entity_watch_node *n = entity->watcher->map->head; n; n = n->next)
            if (n->key == index) {
                CHECK(n->record && n->record->index == index &&
                      (n->record->type == 0 || n->record->type == 2 || n->record->type == 4));
                fprintf(output, "%s{\"index\":%d,\"type\":%d,\"value\":", seen++ ? "," : "", index,
                        n->record->type);
                if (n->record->type == 4) {
                    mc_player_string_view text;
                    CHECK(mc_player_string_read(n->record->string_value, &text));
                    fputs("{\"utf16\":[", output);
                    for (size_t j = 0; j < text.length; ++j)
                        fprintf(output, "%s%u", j ? "," : "", (unsigned int)text.units[j]);
                    fputs("]}", output);
                } else
                    fprintf(output, "%d",
                            n->record->type == 0 ? (int)n->record->byte_value
                                                 : n->record->integer_value);
                fprintf(output, ",\"dirty\":%s}", n->record->dirty ? "true" : "false");
            }
    CHECK(seen == entity->watcher->map->size);
    fputs("]}", output);
    return true;
}

typedef struct tick_profile {
    unsigned int id;
    char kind[4], scene[80], callback[48];
    int64_t own_seed, world_seed;
} tick_profile;

typedef struct tick_operand {
    unsigned int id;
    char operation[16], owner[12], member[32], type[24], value[128];
} tick_operand;

typedef struct mob_fixture {
    random_fixture base;
    tick_profile input;
    mc_entity *actor, *other;
    mc_mob_tick_error *error;
    mc_world_access *observer;
    FILE *events;
    size_t event_count, sounds, particles, actor_holds, registry_holds, math_holds;
    bool callback_bad, actor_owned, other_owned, native_only, terminal_committed,
        observer_registered;
    mc_java_random *owned_random;
    mc_world_runtime *owned_world;
    mc_entity_watcher *owned_watcher;
    mc_entity_bounds *owned_bounds;
    mc_path *control_path_hold;
} mob_fixture;

static const char *const mob_names[] = {"cn", "uz", "fr", "gb", "uw", "wh",
                                        "dl", "bx", "ww", "gi", "xt"};
static const mc_entity_class_kind mob_kinds[] = {
    MC_ENTITY_CLASS_KIND_SPIDER,  MC_ENTITY_CLASS_KIND_ZOMBIE, MC_ENTITY_CLASS_KIND_SKELETON,
    MC_ENTITY_CLASS_KIND_CREEPER, MC_ENTITY_CLASS_KIND_SLIME,  MC_ENTITY_CLASS_KIND_PIG,
    MC_ENTITY_CLASS_KIND_SHEEP,   MC_ENTITY_CLASS_KIND_COW,    MC_ENTITY_CLASS_KIND_CHICKEN,
    MC_ENTITY_CLASS_KIND_WOLF,    MC_ENTITY_CLASS_KIND_SQUID};

static void emit_role(const char *name, const void *value, const char *class_name,
                      bool *first_role) {
    if (!value)
        return;
    fprintf(output, "%s\"%s\":", *first_role ? "" : ",", name);
    *first_role = false;
    emit_object(value, class_name);
}

static void emit_roles(const mob_fixture *f) {
    bool first_role = true;
    fputc('{', output);
    emit_role("actor", f->actor, entity_reference_class(f->actor), &first_role);
    emit_role("world", f->base.world, "fd", &first_role);
    emit_role("actor.bounds", f->actor->bounds, "eq", &first_role);
    emit_role("actor.random", f->actor->random, "java.util.Random", &first_role);
    emit_role("actor.watcher", f->actor->watcher, "ud", &first_role);
    if (!mc_entity_is_kind(f->actor, MC_ENTITY_CLASS_KIND_SLIME)) {
        mc_path *path = NULL;
        if (mc_mob_path_get(f->actor, &path, NULL) != MC_ENTITY_OK)
            exit(3);
        emit_role("actor.path", path, "dh", &first_role);
    }
    if (f->other) {
        emit_role("secondary0", f->other, entity_reference_class(f->other), &first_role);
        emit_role("secondary0.bounds", f->other->bounds, "eq", &first_role);
        emit_role("secondary0.random", f->other->random, "java.util.Random", &first_role);
        emit_role("secondary0.watcher", f->other->watcher, "ud", &first_role);
        mc_path *path = NULL;
        if (mc_mob_path_get(f->other, &path, NULL) != MC_ENTITY_OK)
            exit(3);
        emit_role("secondary0.path", path, "dh", &first_role);
    }
    fputc('}', output);
}

static bool profile_read(const char *path, tick_profile *profiles, size_t *count) {
    FILE *file = open_file(path, "rb");
    CHECK(file != NULL);
    char line[512];
    CHECK(fgets(line, sizeof(line), file) != NULL);
    size_t used = 0;
    while (fgets(line, sizeof(line), file)) {
        CHECK(used < 256);
        tick_profile *p = &profiles[used];
#ifdef _MSC_VER
        const int fields =
            sscanf_s(line, "%u %3s %79s %" SCNd64 " %" SCNd64 " %47s", &p->id, p->kind,
                     (unsigned int)sizeof(p->kind), p->scene, (unsigned int)sizeof(p->scene),
                     &p->own_seed, &p->world_seed, p->callback, (unsigned int)sizeof(p->callback));
#else
        const int fields = sscanf(line, "%u %3s %79s %" SCNd64 " %" SCNd64 " %47s", &p->id, p->kind,
                                  p->scene, &p->own_seed, &p->world_seed, p->callback);
#endif
        CHECK(fields == 6 && p->id == used);
        ++used;
    }
    CHECK(!ferror(file) && fclose(file) == 0 && used >= 11);
    *count = used;
    return true;
}

static bool operands_read(const char *path, tick_operand *values, size_t *count) {
    FILE *file = open_file(path, "rb");
    CHECK(file != NULL);
    char line[512];
    CHECK(fgets(line, sizeof(line), file) != NULL);
    size_t used = 0;
    while (fgets(line, sizeof(line), file)) {
        CHECK(used < 256);
        tick_operand *v = &values[used];
#ifdef _MSC_VER
        const int fields =
            sscanf_s(line, "%u %15s %11s %31s %23s %127s", &v->id, v->operation,
                     (unsigned int)sizeof(v->operation), v->owner, (unsigned int)sizeof(v->owner),
                     v->member, (unsigned int)sizeof(v->member), v->type,
                     (unsigned int)sizeof(v->type), v->value, (unsigned int)sizeof(v->value));
#else
        const int fields = sscanf(line, "%u %15s %11s %31s %23s %127s", &v->id, v->operation,
                                  v->owner, v->member, v->type, v->value);
#endif
        CHECK(fields == 6);
        ++used;
    }
    CHECK(!ferror(file) && fclose(file) == 0 && used > 0);
    *count = used;
    return true;
}

static bool construct_mob(mob_fixture *f, const char *name, mc_entity **out) {
    size_t kind = 0;
    while (kind < COUNT(mob_names) && strcmp(name, mob_names[kind]))
        ++kind;
    CHECK(kind < COUNT(mob_names));
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->base.entities, mob_kinds[kind], &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->base.entities, f->base.registry, literal, out, &error) == MC_MOB_OK &&
          mc_mob_construct(*out, f->base.world, &error) == MC_MOB_OK &&
          (*out)->phase == MC_ENTITY_READY && (*out)->mob != NULL && (*out)->local_player == NULL);
    return true;
}

static bool emit_state(mob_fixture *f, const char *phase) {
    mc_mob_state state;
    mc_entity_class_snapshot classes;
    mc_entity_vector_snapshot vectors;
    CHECK(mc_mob_inspect(f->actor, &state) &&
          mc_entity_classes_get_snapshot(f->base.entities, &classes) == MC_ENTITY_OK &&
          mc_entity_vector_get_snapshot(f->base.entities, &vectors) == MC_ENTITY_OK);
    fprintf(output,
            "{\"event\":\"%s\",\"case\":%u,\"kind\":\"%s\",\"scene\":\"%s\",\"phase\":\"%s\","
            "\"fields\":{",
            f->native_only ? "native_state" : "state", f->input.id, f->input.kind, f->input.scene,
            phase);
    emit_fields(f->actor, &state);
    fputs("},\"bounds\":", output);
    if (f->actor->bounds) {
        fputc('{', output);
        emit_bounds(f->actor->bounds);
        fputc('}', output);
    } else
        fputs("null", output);
    fputs(",\"watcher\":", output);
    if (f->actor->watcher) {
        CHECK(emit_watcher(f->actor));
    } else
        fputs("null", output);
    fputs(",\"own_seed\":", output);
    if (f->actor->random)
        fprintf(output, "\"%012" PRIx64 "\"", f->actor->random->state);
    else
        fputs("null", output);
    fprintf(output,
            ",\"world_seed\":\"%012" PRIx64 "\",\"math_seed\":\"%012" PRIx64
            "\",\"box_cursor\":%d,\"vector_cursor\":%d,\"loaded_entities\":%zu,\"players\":%zu,"
            "\"cache_size\":%zu,\"secondary_entities\":[",
            f->base.world->fields.r->state,
            f->base.profile->math ? atomic_load(&f->base.profile->math->state) : 0,
            classes.bounds_pool_cursor, vectors.cursor, f->base.world->fields.b->count,
            f->base.world->fields.d->count, f->base.provider.cache->count);
    if (f->other) {
        mc_mob_state secondary;
        CHECK(mc_mob_inspect(f->other, &secondary));
        fputs("{\"fields\":{", output);
        emit_fields(f->other, &secondary);
        fputs("},\"bounds\":{", output);
        emit_bounds(f->other->bounds);
        fputs("},\"watcher\":", output);
        CHECK(emit_watcher(f->other));
        fprintf(output, ",\"own_seed\":\"%012" PRIx64 "\"}", f->other->random->state);
    }
    fputs("],\"reference_roles\":", output);
    emit_roles(f);
    fputs("}\n", output);
    CHECK(fflush(output) == 0);
    return true;
}

static bool source_inputs_intact(const mob_fixture *f) {
    return (!f->owned_random || f->actor->random == NULL) &&
           (!f->owned_world || f->actor->base.world == NULL) &&
           (!f->owned_watcher || f->actor->watcher == NULL) &&
           (!f->owned_bounds || f->actor->bounds == NULL);
}

static bool prepare_mob(mob_fixture *f, const char *resources, const char *directory) {
    CHECK(setup(&f->base, resources, directory));
    /* The shared construction fixture owns an independent caller occurrence.
     * This probe audits the sole World.t ownership graph, so end that caller
     * hold now. The real World field and provider.a backedge stay intact. */
    CHECK(f->base.dimension == f->base.world->fields.t &&
          mc_world_provider_hold_count(f->base.dimension) == 2 &&
          mc_world_provider_world(f->base.dimension) == f->base.world);
    const size_t provider_binding = f->base.world->persistent_bindings;
    mc_world_provider_release(f->base.dimension);
    f->base.dimension = NULL;
    CHECK(mc_world_provider_hold_count(f->base.world->fields.t) == 1 &&
          mc_world_provider_world(f->base.world->fields.t) == f->base.world &&
          f->base.world->persistent_bindings == provider_binding);
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z) {
            mc_chunk_state *chunk = NULL;
            CHECK(load_chunk(&f->base, x, z, &chunk) &&
                  mc_chunk_cache_publish(f->base.provider.cache, x, z, chunk));
        }
    CHECK(f->base.provider.cache->count == 25 && f->base.owned_count == 25 &&
          construct_mob(f, f->input.kind, &f->actor));
    f->actor_owned = true;
    CHECK(emit_state(f, "constructor"));
    mc_entity *actor = f->actor;
    if (mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_SLIME)) {
        mc_mob_error error;
        CHECK(mc_mob_slime_set_size(actor, 2, &error) == MC_MOB_OK);
        actor->mob->slime_jump_delay = 15;
    }
    actor->mob->living.F = .25f;
    actor->mob->living.G = .015625f;
    actor->mob->living.aj = .5f;
    if (mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_CHICKEN))
        actor->mob->chicken.i = 6000;
    if (mc_entity_is_kind(actor, MC_ENTITY_CLASS_KIND_SQUID))
        actor->mob->squid.l = .1f;
    /* Declared caller operands matching SN.c's stores; actual e(DDD) recomputes
     * the same permanent bounds. This fixture does not qualify SN.c itself. */
    actor->base.x = actor->base.aJ = actor->base.bl = 8.5;
    actor->base.y = actor->base.aK = actor->base.bm = 64.5 + (double)actor->base.offset;
    actor->base.z = actor->base.aL = actor->base.bn = 8.5;
    actor->base.yaw = 30.f;
    actor->base.aT = actor->base.aU = actor->base.aV = 0.f;
    CHECK(mc_entity_set_position(actor, actor->base.x, actor->base.y, actor->base.z) ==
          MC_ENTITY_OK);
    bool published = false;
    mc_world_entity_error publication;
    CHECK(mc_world_runtime_insert_entity(f->base.world, actor, &published, &publication) ==
              MC_WORLD_STORAGE_OK &&
          published && f->base.world->entity_membership.count == 1 &&
          f->base.world->weather_membership.count == 0 && f->base.world->fields.b->count == 1 &&
          f->base.world->fields.d->count == 0);
    f->base.world->fields.B = false;
    f->base.world->fields.q = 1;
    f->base.world->fields.f = 0;
    f->base.world->fields.g = 17;
    f->base.world->fields.Q = 6001;
    mc_java_random_reseed(actor->random, f->input.own_seed);
    mc_java_random_reseed(f->base.world->fields.r, f->input.world_seed);
    CHECK(mc_entity_bounds_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
    return true;
}

static bool observer_guard(mob_fixture *f, mc_world_runtime *world) {
    mc_math_class_state math;
    mc_mob_tick_error *nested = calloc(1, sizeof(*nested));
    if (!nested)
        return false;
    const bool valid =
        world == f->base.world && f->actor->mob->tick_active && f->actor->holds > f->actor_holds &&
        world->active_calls > 0 && f->base.registry->runtime_borrows > f->registry_holds &&
        mc_math_classes_state(f->base.math, &math, NULL) == MC_MATH_CLASS_OK &&
        math.holds > f->math_holds &&
        mc_mob_tick(f->actor, f->base.registry, f->base.math, NULL, nested) == MC_ENTITY_BUSY &&
        mc_mob_tick_cleanup(f->actor, nested) == MC_ENTITY_BUSY && f->actor->mob->tick_active &&
        mc_world_runtime_retire_world(world, NULL) == MC_WORLD_STORAGE_BUSY &&
        mc_world_runtime_terminal_admit(world, NULL, &(mc_world_terminal_inventory){0}, NULL) ==
            MC_WORLD_STORAGE_BUSY;
    free(nested);
    return valid;
}

static bool print_name(FILE *file, const mc_registry_string *name) {
    mc_registry_string_view text;
    if (!mc_registry_string_read(name, &text))
        return false;
    for (size_t i = 0; i < text.length; ++i) {
        if (text.units[i] <= 32 || text.units[i] >= 127)
            return false;
        fputc((int)text.units[i], file);
    }
    return true;
}

static mc_world_storage_status mob_sound(void *context, mc_world_runtime *world,
                                         const mc_registry_string *name, double x, double y,
                                         double z, float volume, float pitch,
                                         mc_world_storage_error *error) {
    mob_fixture *f = context;
    if (!observer_guard(f, world))
        f->callback_bad = true;
    fprintf(f->events, "MOB_SOUND %u %zu ", f->input.id, f->event_count++);
    if (!print_name(f->events, name))
        f->callback_bad = true;
    fprintf(f->events,
            " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %08" PRIx32 " %08" PRIx32 "\n", dbits(x),
            dbits(y), dbits(z), fbits(volume), fbits(pitch));
    ++f->sounds;
    if (!strcmp(f->input.callback, "fail_sound")) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored Mob sound observer failure", 0};
        return error->status;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status mob_particle(void *context, mc_world_runtime *world,
                                            const mc_registry_string *name, double x, double y,
                                            double z, double vx, double vy, double vz,
                                            mc_world_storage_error *error) {
    mob_fixture *f = context;
    if (!observer_guard(f, world))
        f->callback_bad = true;
    fprintf(f->events, "MOB_PARTICLE %u %zu ", f->input.id, f->event_count++);
    if (!print_name(f->events, name))
        f->callback_bad = true;
    fprintf(f->events,
            " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
            " %016" PRIx64 "\n",
            dbits(x), dbits(y), dbits(z), dbits(vx), dbits(vy), dbits(vz));
    ++f->particles;
    if (!strcmp(f->input.callback, "fail_particle")) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored Mob particle observer failure", 0};
        return error->status;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool observer_attach(mob_fixture *f) {
    const mc_world_access_callbacks callbacks = {.sound = mob_sound, .particle = mob_particle};
    CHECK(mc_world_access_create(&callbacks, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->observer_registered = true;
    return true;
}

static bool same_path_point(const mc_path_point_state *a, const mc_path_point_state *b) {
    return a->a == b->a && a->b == b->b && a->c == b->c && a->j == b->j && a->d == b->d &&
           fbits(a->e) == fbits(b->e) && fbits(a->f) == fbits(b->f) && fbits(a->g) == fbits(b->g) &&
           a->h == b->h && a->i == b->i && a->constructed == b->constructed &&
           a->constructor_pc == b->constructor_pc;
}

static bool cleanup_checks(mob_fixture *f) {
    CHECK(!f->error->state.cleanup_pending);
    const mc_entity_base_fields base = f->actor->base;
    const mc_mob_entity_state mob = *f->actor->mob;
    const size_t holds = f->actor->holds, bindings = f->base.world->persistent_bindings,
                 borrows = f->base.registry->runtime_borrows;
    const int32_t boxes = f->base.entities->bounds_cursor,
                  vectors = f->base.entities->vector_cursor;
    const mc_java_random *actual = f->actor->random ? f->actor->random : f->owned_random;
    CHECK(actual != NULL);
    const mc_java_random own = *actual, world = *f->base.world->fields.r;
    mc_path *path = f->actor->mob->creature_path;
    mc_path_state path_before = {0}, path_after = {0};
    mc_path_point **path_array = NULL;
    mc_path_point_state *points = NULL;
    if (path) {
        CHECK(mc_path_inspect(path, &path_before) && path_before.array_length > 0 &&
              path_before.array_length <= SIZE_MAX / sizeof(*points));
        path_array = malloc(path_before.array_length * sizeof(*path_array));
        points = malloc(path_before.array_length * sizeof(*points));
        CHECK(path_array && points);
        memcpy(path_array, path_before.b, path_before.array_length * sizeof(*path_array));
        for (size_t index = 0; index < path_before.array_length; ++index)
            CHECK(mc_path_point_inspect(path, path_array[index], &points[index]));
    }
    mc_mob_tick_error *saved = malloc(sizeof(*saved));
    mc_mob_tick_error *cleanup = calloc(1, sizeof(*cleanup));
    CHECK(saved != NULL && cleanup != NULL);
    memcpy(saved, f->error, sizeof(*saved));
    CHECK(mc_mob_tick_cleanup(f->actor, cleanup) == MC_ENTITY_OK &&
          mc_mob_tick_cleanup(f->actor, cleanup) == MC_ENTITY_OK && source_inputs_intact(f) &&
          !memcmp(&base, &f->actor->base, sizeof(base)) &&
          !memcmp(&mob, f->actor->mob, sizeof(mob)) && !memcmp(saved, f->error, sizeof(*saved)) &&
          mc_test_java_random_equal(&own, actual) &&
          mc_test_java_random_equal(&world, f->base.world->fields.r) && f->actor->holds == holds &&
          f->base.world->persistent_bindings == bindings &&
          f->base.registry->runtime_borrows == borrows &&
          f->base.entities->bounds_cursor == boxes && f->base.entities->vector_cursor == vectors &&
          f->base.world->active_calls == 0 && f->base.world->local_leases == 0);
    if (path) {
        CHECK(mc_path_inspect(path, &path_after) && path_after.a == path_before.a &&
              path_after.c == path_before.c && path_after.b == path_before.b &&
              path_after.array_length == path_before.array_length &&
              path_after.holds == path_before.holds &&
              path_after.arena_holds == path_before.arena_holds &&
              path_after.arena_identity == path_before.arena_identity &&
              path_after.arena_points == path_before.arena_points &&
              path_after.arena_bytes == path_before.arena_bytes &&
              path_after.classes == path_before.classes &&
              path_after.constructed == path_before.constructed &&
              path_after.constructor_pc == path_before.constructor_pc &&
              !memcmp(path_array, path_after.b, path_after.array_length * sizeof(*path_array)));
        for (size_t index = 0; index < path_before.array_length; ++index) {
            mc_path_point_state after;
            CHECK(mc_path_point_inspect(path, path_array[index], &after) &&
                  same_path_point(&points[index], &after));
        }
    }
    free(path_array);
    free(points);
    free(cleanup);
    free(saved);
    return true;
}

static bool tick_call_context(mob_fixture *f, const mc_mob_tick_limits *limits,
                              bool expected_success, mc_math_classes *actual_math) {
    mc_mob_state before;
    mc_math_class_state math;
    CHECK(mc_mob_inspect(f->actor, &before) &&
          mc_math_classes_state(f->base.math, &math, NULL) == MC_MATH_CLASS_OK);
    const mc_mob_error constructor_failure = *before.failure;
    const uint64_t shared_u = atomic_load(&f->base.profile->uniquifier);
    const uint64_t shared_math =
        f->base.profile->math ? atomic_load(&f->base.profile->math->state) : 0;
    const unsigned int shared_phase = atomic_load(&f->base.profile->holder_phase);
    const size_t bindings = f->base.world->persistent_bindings;
    f->actor_holds = f->actor->holds;
    f->registry_holds = f->base.registry->runtime_borrows;
    f->math_holds = math.holds;
    CHECK(emit_state(f, "before"));
    const mc_entity_status status =
        mc_mob_tick(f->actor, f->base.registry, actual_math, limits, f->error);
    CHECK(emit_state(f, "after"));
    const mc_mob_tick_state *s = &f->error->state;
    fprintf(output,
            "{\"event\":\"%s\",\"case\":%u,\"scene\":\"%s\",\"outcome\":%d,\"status\":%u,\"site\":{"
            "\"class\":\"%s\",\"method\":\"%s\",\"pc\":%d},\"child_pc\":%d,\"native_policy\":%s,"
            "\"returned\":%s,\"environment_returned\":%s,\"action_returned\":%s,\"behavior_"
            "returned\":%s,\"living_pc\":%u,\"source_pc\":%u,\"work\":%zu,\"concrete_random_"
            "calls\":%zu,\"concrete_world_random_calls\":%zu,\"sounds\":%zu,\"particles\":%zu",
            f->native_only ? "native_control" : "result", f->input.id, f->input.scene,
            status == MC_ENTITY_OK ? 1 : -1, (unsigned int)status,
            f->error->site.class_name ? f->error->site.class_name : "-",
            f->error->site.method ? f->error->site.method : "-", f->error->site.pc,
            f->error->child_site.pc, f->error->native_policy ? "true" : "false",
            s->returned ? "true" : "false", s->environment_returned ? "true" : "false",
            s->action_returned ? "true" : "false", s->behavior_returned ? "true" : "false",
            s->living_pc, s->source_pc, s->work, s->concrete_random_calls,
            s->concrete_world_random_calls, f->sounds, f->particles);
    fprintf(output,
            ",\"source_exception\":%s,\"exception_class\":\"%s\",\"cleanup_pending\":%s,\"ray_"
            "calls\":%zu,\"ray_returned\":%s,\"environment_callee_pc\":%u,\"ray_start\":",
            f->error->source_exception ? "true" : "false",
            f->error->exception_class ? f->error->exception_class : "-",
            s->cleanup_pending ? "true" : "false", s->ray_calls, s->ray_returned ? "true" : "false",
            f->error->child == MC_MOB_TICK_CHILD_ENVIRONMENT
                ? f->error->detail.environment.environment.bytecode
                : 0);
    if (s->ray_arguments_stored) {
        fprintf(output,
                "[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
                "\"],\"ray_end\":[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]",
                dbits(s->ray_start[0]), dbits(s->ray_start[1]), dbits(s->ray_start[2]),
                dbits(s->ray_end[0]), dbits(s->ray_end[1]), dbits(s->ray_end[2]));
    } else
        fputs("null,\"ray_end\":null", output);
    fprintf(output,
            ",\"path\":{\"stored\":%s,\"kind\":%u,\"receiver_is_world\":%s,\"target_is_secondary\":"
            "%s,\"x\":%d,\"y\":%d,\"z\":%d,\"range\":{\"F\":\"%08" PRIx32 "\"}}",
            s->path.stored ? "true" : "false", (unsigned int)s->path.kind,
            s->path.receiver_identity == (uintptr_t)f->base.world ? "true" : "false",
            f->other && s->path.target_identity == (uintptr_t)f->other ? "true" : "false",
            s->path.x, s->path.y, s->path.z, fbits(s->path.range));
    mc_path_state path_state = {0};
    mc_path *current_path = NULL;
    if (!mc_entity_is_kind(f->actor, MC_ENTITY_CLASS_KIND_SLIME))
        CHECK(mc_mob_path_get(f->actor, &current_path, NULL) == MC_ENTITY_OK);
    if (current_path)
        CHECK(mc_path_inspect(current_path, &path_state) && path_state.classes == f->base.entities);
    fprintf(output,
            ",\"path_execution\":{\"calls\":%zu,\"returned\":%s,\"stored\":%s,\"waypoints\":%zu,"
            "\"advances\":%zu,\"present\":%s,\"length\":%d,\"cursor\":%d,\"holds\":%zu}",
            s->path_calls, s->path_returned ? "true" : "false",
            s->path_store_completed ? "true" : "false", s->path_waypoints, s->path_advances,
            current_path ? "true" : "false", path_state.a, path_state.c, path_state.holds);
    const mc_entity_effect_error *damage =
        f->error->child == MC_MOB_TICK_CHILD_ENVIRONMENT ? &f->error->detail.environment.effect
        : f->error->child == MC_MOB_TICK_CHILD_EFFECT    ? &f->error->detail.effect
                                                         : NULL;
    fprintf(output,
            ",\"drop\":{\"stored\":%s,\"item_id\":%d,\"count\":%d},\"damage\":{\"stored\":%s,"
            "\"source_null\":%s,\"amount\":%d}}\n",
            s->drop.stored ? "true" : "false", s->drop.item_id, s->drop.count,
            damage && damage->damage_arguments_stored ? "true" : "false",
            damage && damage->damage_source_identity == 0 ? "true" : "false",
            damage ? damage->damage_amount : 0);
    CHECK(fflush(output) == 0 && fflush(f->events) == 0 && !f->callback_bad &&
          source_inputs_intact(f) && (status == MC_ENTITY_OK) == expected_success &&
          s->returned == expected_success && !f->actor->mob->tick_active);
    if (expected_success)
        CHECK(s->environment_returned && s->action_returned &&
              (before.living.V || before.living.Y <= 0 || s->behavior_returned) &&
              s->living_pc == 528);
    CHECK(f->actor->holds == f->actor_holds &&
          f->base.registry->runtime_borrows == f->registry_holds &&
          mc_math_classes_state(f->base.math, &math, NULL) == MC_MATH_CLASS_OK &&
          math.holds == f->math_holds && f->base.world->persistent_bindings == bindings &&
          f->base.world->active_calls == 0 && f->base.world->local_leases == 0 &&
          atomic_load(&f->base.profile->uniquifier) == shared_u &&
          atomic_load(&f->base.profile->holder_phase) == shared_phase &&
          (!f->base.profile->math || atomic_load(&f->base.profile->math->state) == shared_math));
    mc_mob_state after;
    CHECK(mc_mob_inspect(f->actor, &after) && after.phase == MC_ENTITY_READY &&
          !memcmp(after.failure, &constructor_failure, sizeof(constructor_failure)) &&
          after.living_pc == before.living_pc && after.constructor_pc == before.constructor_pc &&
          after.watcher_hook_pc == before.watcher_hook_pc);
    if (!f->native_only && f->input.id >= 53 && f->input.id <= 56)
        CHECK(f->error->source_exception && !f->error->native_policy && f->error->exception_class &&
              !strcmp(f->error->exception_class, "java/lang/NullPointerException"));
    if (s->path.stored) {
        CHECK(s->path.receiver_identity == (uintptr_t)f->base.world);
        if (s->path.kind == MC_MOB_TICK_PATH_TARGET)
            CHECK(f->other && s->path.target_identity == (uintptr_t)f->other &&
                  s->path.range == 16.f);
        else
            CHECK(s->path.kind == MC_MOB_TICK_PATH_COORDINATES && s->path.range == 10.f);
    }
    if (!f->native_only && f->input.id == 34)
        CHECK(s->drop.stored && s->drop.item_id == 344 && s->drop.count == 1);
    if (!f->native_only && f->input.id >= 47 && f->input.id <= 50)
        CHECK(s->path_calls == 1 && s->path_returned && s->path_store_completed &&
              s->path_waypoints > 0 && current_path && path_state.holds == 1 &&
              path_state.arena_holds == 1 && !s->cleanup_pending);
    if (!f->native_only && (f->input.id == 40 || f->input.id == 42 || f->input.id == 44))
        CHECK(damage && damage->damage_arguments_stored && damage->damage_source_identity == 0 &&
              damage->damage_amount == (f->input.id == 40   ? 1
                                        : f->input.id == 42 ? 2
                                                            : 4));
    CHECK(cleanup_checks(f));
    return true;
}

static bool tick_call(mob_fixture *f, const mc_mob_tick_limits *limits, bool expected_success) {
    return tick_call_context(f, limits, expected_success, f->base.math);
}

static bool mob_teardown(mob_fixture *f) {
    CHECK(source_inputs_intact(f) &&
          (f->actor ? !f->actor->mob->tick_active : f->terminal_committed));
    /* Only terminal fixture disposal restores explicitly retained caller inputs.
     * The completed tick/error and all ownership checks above saw the genuine
     * failed prefix; no state or entropy repair is credited to the operation. */
    if (f->owned_random)
        f->actor->random = f->owned_random;
    if (f->owned_world)
        f->actor->base.world = f->owned_world;
    if (f->owned_watcher)
        f->actor->watcher = f->owned_watcher;
    if (f->owned_bounds)
        f->actor->bounds = f->owned_bounds;
    bool removed = false;
    if (f->observer) {
        if (f->observer_registered) {
            CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed,
                                                       NULL) == MC_WORLD_STORAGE_OK &&
                  removed);
            f->observer_registered = false;
        }
        CHECK(mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
    }
    f->observer = NULL;
    if (f->control_path_hold) {
        mc_path_release(f->control_path_hold);
        f->control_path_hold = NULL;
    }
    if (f->other_owned)
        mc_entity_release(f->other);
    if (f->actor_owned)
        mc_entity_release(f->actor);
    f->actor_owned = f->other_owned = false;
    mc_world_terminal_inventory inventory;
    mc_world_storage_error terminal_error = {0};
    const mc_world_storage_status terminal =
        f->terminal_committed
            ? MC_WORLD_STORAGE_OK
            : mc_world_runtime_terminal_admit(f->base.world, NULL, &inventory, &terminal_error);
    if (terminal != MC_WORLD_STORAGE_OK)
        fprintf(stderr,
                "[mob-tick-terminal] case%u status%d operation=%s bindings=%zu provider_holds=%zu "
                "phase%d\n",
                f->input.id, (int)terminal,
                terminal_error.operation ? terminal_error.operation : "-",
                f->base.world->persistent_bindings,
                mc_world_provider_hold_count(f->base.world->fields.t),
                (int)f->base.world->construction.phase);
    CHECK(terminal == MC_WORLD_STORAGE_OK);
    CHECK(teardown(&f->base));
    free(f->error);
    return true;
}

static bool fixture_cell(mob_fixture *f, int32_t x, int32_t y, int32_t z, uint8_t id) {
    mc_chunk_state *chunk = NULL;
    CHECK(y >= 0 && y < 128 &&
          mc_chunk_cache_lookup(f->base.provider.cache, mc_java_i32_shr(x, 4),
                                mc_java_i32_shr(z, 4), &chunk) &&
          chunk);
    const size_t lx = (uint32_t)x & 15u, lz = (uint32_t)z & 15u;
    chunk->data->blocks[(lx << 11) | (lz << 7) | (size_t)y] = id;
    if (id && chunk->data->heightmap[(lz << 4) | lx] < y + 1)
        chunk->data->heightmap[(lz << 4) | lx] = (uint8_t)(y + 1);
    return true;
}

static bool csv_values(const char *text, double *values, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        char *end = NULL;
        values[i] = strtod(text, &end);
        CHECK(end != text && (i + 1 == count ? !*end : *end == ','));
        text = end + (i + 1 == count ? 0 : 1);
    }
    return true;
}

static bool source_pose(mc_entity *actor, const char *text) {
    double v[5];
    CHECK(csv_values(text, v, COUNT(v)));
    actor->base.x = actor->base.aJ = actor->base.bl = v[0];
    actor->base.y = actor->base.aK = actor->base.bm = v[1] + (double)actor->base.offset;
    actor->base.z = actor->base.aL = actor->base.bn = v[2];
    actor->base.yaw = (float)v[3];
    actor->base.aT = (float)v[4];
    CHECK(mc_entity_set_position(actor, actor->base.x, actor->base.y, actor->base.z) ==
          MC_ENTITY_OK);
    return true;
}

static bool raw_field(mob_fixture *f, const tick_operand *v) {
    mc_entity *actor = f->actor;
    mc_living_entity_fields *ls = &actor->mob->living;
    char *end = NULL;
    const bool is_i = !strcmp(v->type, "I"), is_f = !strcmp(v->type, "F"),
               is_d = !strcmp(v->type, "D"), is_z = !strcmp(v->type, "Z"),
               is_r = !strcmp(v->type, "R");
    int32_t integer = 0;
    float floating = 0;
    double real = 0;
    bool boolean = false;
    if (is_i) {
        const long long value = strtoll(v->value, &end, 10);
        CHECK(end && !*end && value >= INT32_MIN && value <= INT32_MAX);
        integer = (int32_t)value;
    } else if (is_f || is_d) {
        const uint64_t bits = strtoull(v->value, &end, 16);
        CHECK(end && !*end && strlen(v->value) == (is_f ? 8u : 16u));
        if (is_f) {
            const uint32_t word = (uint32_t)bits;
            memcpy(&floating, &word, sizeof(floating));
        } else
            memcpy(&real, &bits, sizeof(real));
    } else if (is_z) {
        CHECK(!strcmp(v->value, "true") || !strcmp(v->value, "false"));
        boolean = !strcmp(v->value, "true");
    } else
        CHECK(is_r);
#define INPUT_FIELD(source_name, field, admitted, input)                                           \
    if (!strcmp(v->member, source_name) && (admitted)) {                                           \
        field = input;                                                                             \
        return true;                                                                               \
    }
    if (!strcmp(v->owner, "ls")) {
        INPUT_FIELD("a", ls->private_a, is_i, integer)
        INPUT_FIELD("Y", ls->Y, is_i, integer)
        INPUT_FIELD("ad", ls->ad, is_i, integer)
        INPUT_FIELD("an", ls->an, is_i, integer)
        INPUT_FIELD("aC", ls->aC, is_i, integer)
        INPUT_FIELD("V", ls->V, is_z, boolean)
        INPUT_FIELD("az", ls->az, is_z, boolean)
        INPUT_FIELD("H", ls->H, is_f, floating)
        INPUT_FIELD("ay", ls->ay, is_f, floating)
        INPUT_FIELD("ao", ls->ao, is_d, real)
        INPUT_FIELD("ap", ls->ap, is_d, real)
        INPUT_FIELD("aq", ls->aq, is_d, real)
        INPUT_FIELD("ar", ls->ar, is_d, real)
        INPUT_FIELD("as", ls->as, is_d, real)
        if (!strcmp(v->member, "b") && is_r) {
            CHECK(!strcmp(v->value, "secondary") && f->other &&
                  mc_mob_look_target_store(actor, f->other, NULL) == MC_ENTITY_OK);
            return true;
        }
    } else if (!strcmp(v->owner, "sn")) {
        INPUT_FIELD("aX", actor->base.aX, is_z, boolean)
        INPUT_FIELD("aP", actor->base.velocity_x, is_d, real)
        INPUT_FIELD("aQ", actor->base.velocity_y, is_d, real)
        INPUT_FIELD("aR", actor->base.velocity_z, is_d, real)
        INPUT_FIELD("bz", actor->base.bz, is_i, integer)
        if (is_r && !strcmp(v->value, "NULL")) {
            if (!strcmp(v->member, "bs")) {
                f->owned_random = actor->random;
                actor->random = NULL;
                return true;
            }
            if (!strcmp(v->member, "aI")) {
                f->owned_world = actor->base.world;
                actor->base.world = NULL;
                return true;
            }
            if (!strcmp(v->member, "aW")) {
                f->owned_bounds = actor->bounds;
                actor->bounds = NULL;
                return true;
            }
            if (!strcmp(v->member, "bD")) {
                f->owned_watcher = actor->watcher;
                actor->watcher = NULL;
                return true;
            }
        }
    } else if (!strcmp(v->owner, "fd")) {
        INPUT_FIELD("q", f->base.world->fields.q, is_i, integer)
        INPUT_FIELD("B", f->base.world->fields.B, is_z, boolean)
    } else if (!strcmp(v->owner, "uw")) {
        INPUT_FIELD("c", actor->mob->slime_jump_delay, is_i, integer)
    } else if (!strcmp(v->owner, "ww")) {
        INPUT_FIELD("i", actor->mob->chicken.i, is_i, integer)
    } else if (!strcmp(v->owner, "ii") && !strcmp(v->member, "d") && is_r) {
        CHECK(!strcmp(v->value, "secondary") && f->other &&
              mc_mob_creature_target_store(actor, f->other, NULL) == MC_ENTITY_OK);
        return true;
    }
#undef INPUT_FIELD
    CHECK(false);
    return false;
}

static bool secondary_prepare(mob_fixture *f, const tick_operand *v) {
    CHECK(!f->other && !strcmp(v->member, "construct_publish") && !strcmp(v->type, "entity") &&
          construct_mob(f, v->owner, &f->other));
    f->other_owned = true;
    f->other->mob->living.F = .25f;
    f->other->mob->living.G = .015625f;
    f->other->mob->living.aj = .5f;
    f->other->base.aU = f->other->base.aV = 0.f;
    CHECK(source_pose(f->other, v->value));
    bool published = false;
    mc_world_entity_error error;
    CHECK(mc_world_runtime_insert_entity(f->base.world, f->other, &published, &error) ==
              MC_WORLD_STORAGE_OK &&
          published);
    mc_java_random_reseed(f->other->random, 17);
    mc_java_random_reseed(f->base.world->fields.r, f->input.world_seed);
    return true;
}

static bool profile_prepare(mob_fixture *f, const tick_operand *values, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        const tick_operand *v = &values[i];
        if (v->id != f->input.id)
            continue;
        if (!strcmp(v->operation, "field")) {
            CHECK(raw_field(f, v));
        } else if (!strcmp(v->operation, "pose")) {
            CHECK(!strcmp(v->owner, "sn") && !strcmp(v->member, "c") &&
                  !strcmp(v->type, "(DDDFF)V") && source_pose(f->actor, v->value));
        } else if (!strcmp(v->operation, "invoke")) {
            CHECK(!strcmp(v->owner, "uw") && !strcmp(v->member, "e") && !strcmp(v->type, "(I)V"));
            char *end = NULL;
            const long long value = strtoll(v->value, &end, 10);
            mc_mob_error error;
            CHECK(end && !*end && value >= INT32_MIN && value <= INT32_MAX &&
                  mc_mob_slime_set_size(f->actor, (int32_t)value, &error) == MC_MOB_OK);
        } else if (!strcmp(v->operation, "secondary")) {
            CHECK(secondary_prepare(f, v));
        } else if (!strcmp(v->operation, "watcher")) {
            CHECK(!strcmp(v->owner, "ud"));
            char *end = NULL;
            const long index = strtol(v->member, &end, 10);
            CHECK(end && !*end && (index == 16 || index == 18));
            const long value = strtol(v->value, &end, 10);
            CHECK(end && !*end);
            mc_entity_watch_record *record = NULL;
            for (mc_entity_watch_node *node = f->actor->watcher->map->head; node; node = node->next)
                if (node->key == index)
                    record = node->record;
            CHECK(record != NULL);
            if (!strcmp(v->type, "Byte")) {
                CHECK(record->type == 0 && value >= -128 && value <= 127);
                if (record->byte_value != value) {
                    record->byte_value = (int8_t)value;
                    record->dirty = true;
                    f->actor->watcher->dirty = true;
                }
            } else {
                CHECK(!strcmp(v->type, "Integer") && record->type == 2 && value >= INT32_MIN &&
                      value <= INT32_MAX);
                if (record->integer_value != value) {
                    record->integer_value = (int32_t)value;
                    record->dirty = true;
                    f->actor->watcher->dirty = true;
                }
            }
        } else if (!strcmp(v->operation, "cell") || !strcmp(v->operation, "fill")) {
            CHECK(!strcmp(v->owner, "world") && !strcmp(v->type, "raw"));
            double n[8];
            const bool fill = !strcmp(v->operation, "fill");
            CHECK(csv_values(v->value, n, fill ? 8u : 5u));
            for (size_t k = 0; k < (fill ? 8u : 5u); ++k)
                CHECK(n[k] >= INT32_MIN && n[k] <= INT32_MAX && n[k] == (double)(int32_t)n[k]);
            CHECK(n[fill ? 7 : 4] == 0 && n[fill ? 6 : 3] >= 0 && n[fill ? 6 : 3] <= 255);
            const int32_t last_x = (int32_t)n[fill ? 3 : 0], last_y = (int32_t)n[fill ? 4 : 1],
                          last_z = (int32_t)n[fill ? 5 : 2];
            for (int32_t x = (int32_t)n[0]; x <= last_x; ++x)
                for (int32_t y = (int32_t)n[1]; y <= last_y; ++y)
                    for (int32_t z = (int32_t)n[2]; z <= last_z; ++z)
                        CHECK(fixture_cell(f, x, y, z, (uint8_t)n[fill ? 6 : 3]));
        } else
            CHECK(false);
    }
    CHECK(!strcmp(f->input.callback, "none") || !strcmp(f->input.callback, "fail_sound") ||
          !strcmp(f->input.callback, "fail_particle"));
    return true;
}

static mc_mob_tick_limits full_limits(void) {
    const mc_mob_tick_limits limits = {
        .max_work = MC_MOB_TICK_DEFAULT_WORK,
        .environment = {.max_particle_work = MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK,
                        .material = {.max_cell_work =
                                         MC_WORLD_MATERIAL_MOVEMENT_DEFAULT_CELL_WORK}},
        .action = {.max_loop_work = MC_PLAYER_ACTION_DEFAULT_LOOP_WORK},
        .movement = {.max_loop_work = MC_ENTITY_MOVE_DEFAULT_LOOP_WORK},
        .path = mc_world_path_default_limits()};
    return limits;
}

static bool shared_control_path(mob_fixture *f, mc_path **out) {
    CHECK(f->other && !*out);
    mc_world_path_call *call = NULL;
    CHECK(mc_world_runtime_path_to_coordinates_begin(f->base.world, f->actor, 12, 64, 8, 16.f,
                                                     f->base.math, NULL,
                                                     &call) == MC_WORLD_PATH_OK);
    mc_world_path_call_state state;
    CHECK(mc_world_path_call_inspect(call, &state) && state.source_returned && state.result &&
          mc_mob_path_store(f->actor, state.result, NULL) == MC_ENTITY_OK &&
          mc_mob_path_store(f->other, state.result, NULL) == MC_ENTITY_OK);
    *out = state.result;
    CHECK(mc_world_path_call_retire(&call) == MC_WORLD_PATH_OK && !call);
    mc_path_state path;
    CHECK(mc_path_inspect(*out, &path) && path.holds == 2 && path.arena_holds == 1 && path.a == 5 &&
          path.c == 0 && path.classes == f->base.entities && f->base.world->local_leases == 0 &&
          f->base.world->active_calls == 0);
    return true;
}

static bool control_path_is(mc_path *path, int32_t cursor, size_t holds) {
    mc_path_state state;
    CHECK(mc_path_inspect(path, &state) && state.c == cursor && state.holds == holds &&
          state.arena_holds == 1 && state.a == 5 && state.array_length == 5);
    return true;
}

static bool target_controls(mob_fixture *f) {
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    f->observer_registered = false;
    CHECK(construct_mob(f, "dl", &f->other));
    f->other_owned = true;
    CHECK(mc_entity_set_position(f->other, 9.5, 64.5, 8.5) == MC_ENTITY_OK);
    bool inserted = false;
    mc_world_entity_error error;
    CHECK(mc_world_runtime_insert_entity(f->base.world, f->other, &inserted, &error) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    mc_path *shared_path = NULL;
    CHECK(shared_control_path(f, &shared_path) &&
          mc_path_advance(shared_path, NULL) == MC_PATH_OK && control_path_is(shared_path, 1, 2));
    const size_t first_holds = f->actor->holds, other_holds = f->other->holds;
    CHECK(mc_mob_look_target_store(f->actor, f->other, NULL) == MC_ENTITY_OK &&
          mc_mob_creature_target_store(f->actor, f->other, NULL) == MC_ENTITY_OK &&
          mc_mob_look_target_store(f->other, f->actor, NULL) == MC_ENTITY_OK &&
          f->actor->holds == first_holds + 1 && f->other->holds == other_holds + 2);
    mc_entity *look = NULL, *target = NULL;
    CHECK(mc_mob_look_target_get(f->actor, &look, NULL) == MC_ENTITY_OK && look == f->other &&
          mc_mob_creature_target_get(f->actor, &target, NULL) == MC_ENTITY_OK &&
          target == f->other && f->actor->mob->living.private_b == f->other &&
          f->actor->mob->creature_target == f->other &&
          f->other->mob->living.private_b == f->actor &&
          f->actor->mob->creature_path == shared_path &&
          f->other->mob->creature_path == shared_path && control_path_is(shared_path, 1, 2));
    /* Repeated alias stores retain before release and must have zero net hold. */
    CHECK(mc_mob_look_target_store(f->actor, f->other, NULL) == MC_ENTITY_OK &&
          mc_mob_creature_target_store(f->actor, f->other, NULL) == MC_ENTITY_OK &&
          f->actor->holds == first_holds + 1 && f->other->holds == other_holds + 2);
    mc_entity *strict = f->actor;
    CHECK(mc_mob_retire(&strict, NULL) == MC_MOB_BUSY && strict == f->actor);
    mc_world_terminal_inventory inventory = {0};
    const size_t bindings = f->base.world->persistent_bindings;
    CHECK(mc_world_runtime_terminal_admit(f->base.world, NULL, &inventory, NULL) ==
              MC_WORLD_STORAGE_BUSY &&
          f->base.world->persistent_bindings == bindings &&
          f->actor->mob->living.private_b == f->other &&
          f->other->mob->living.private_b == f->actor);
    const mc_world_runtime_phase phase = f->base.world->construction.phase;
    mc_world_reference_list *const published = f->base.world->fields.b;
    mc_world_reference_list *const nearby = f->base.world->fields.R;
    mc_world_reference_list *const collisions = f->base.world->fields.K;
    const size_t published_count = published->count, nearby_count = nearby->count,
                 collision_count = collisions->count;
    const mc_world_entity_list_ledger membership = f->base.world->entity_membership;
    mc_world_storage_error refused;
    CHECK(mc_world_runtime_retire_entity_containers(f->base.world, &refused) ==
              MC_WORLD_STORAGE_BUSY &&
          refused.operation &&
          !strcmp(refused.operation, "World terminal external owner of Mob reference fields") &&
          !f->base.world->entity_containers_retired &&
          !f->base.world->tile_entity_containers_retired &&
          f->base.world->construction.phase == phase &&
          f->base.world->persistent_bindings == bindings && f->base.world->fields.b == published &&
          published->count == published_count && f->base.world->fields.R == nearby &&
          nearby->count == nearby_count && f->base.world->fields.K == collisions &&
          collisions->count == collision_count &&
          !memcmp(&membership, &f->base.world->entity_membership, sizeof(membership)) &&
          f->actor->holds == first_holds + 1 && f->other->holds == other_holds + 2 &&
          f->actor->mob->living.private_b == f->other &&
          f->actor->mob->creature_target == f->other &&
          f->other->mob->living.private_b == f->actor);
    mc_entity_release(f->actor);
    mc_entity_release(f->other);
    f->actor_owned = f->other_owned = false;
    CHECK(mc_world_runtime_terminal_admit(f->base.world, NULL, &inventory, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inventory.entities == 2 && inventory.expected_world_bindings >= 2 &&
          inventory.paths == 1 && inventory.path_occurrences == 2 && inventory.path_arenas == 1);
    CHECK(mc_entity_retain(f->actor) == MC_ENTITY_OK &&
          mc_world_runtime_terminal_admit(f->base.world, NULL, &inventory, NULL) ==
              MC_WORLD_STORAGE_BUSY);
    mc_entity_release(f->actor);
    CHECK(
        mc_world_runtime_terminal_admit(f->base.world, NULL, &inventory, NULL) ==
            MC_WORLD_STORAGE_OK &&
        f->actor->mob->living.private_b == f->other && f->actor->mob->creature_target == f->other &&
        f->other->mob->living.private_b == f->actor &&
        mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) == MC_WORLD_STORAGE_OK);
    f->observer_registered = true;
    fprintf(output,
            "{\"event\":\"native_control\",\"case\":%u,\"scene\":\"target_alias_cycle\","
            "\"entities\":2,\"external_hold_retry\":true,\"retire_busy_unchanged\":true,\"source_"
            "fields_preserved\":true,\"paths\":1,\"path_occurrences\":2,\"shared_cursor\":1}\n",
            f->input.id);
    return true;
}

static bool eye_controls(mob_fixture *f) {
    CHECK(mc_entity_is_kind(f->actor, MC_ENTITY_CLASS_KIND_WOLF) && fixture_cell(f, 8, 65, 8, 1) &&
          mc_entity_set_position(f->actor, 8.5, 64.29, 8.5) == MC_ENTITY_OK);
    const int32_t health = f->actor->mob->living.Y;
    /* Correct GI.w height*.8 keeps every actual SN.L sample below Y65;
     * inherited LS.w height*.85 would enter the genuine damage boundary. */
    CHECK(tick_call(f, NULL, true) && f->actor->mob->living.Y == health && f->actor->base.bt == 1 &&
          f->error->state.environment_returned);
    return true;
}

static bool raw_chain(mob_fixture *f, size_t count, mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    CHECK(mc_entity_class_literal_get(f->base.entities, MC_ENTITY_CLASS_KIND_COW, &literal) ==
          MC_ENTITY_OK);
    mc_entity *root = NULL, *tail = NULL;
    for (size_t index = 0; index < count; ++index) {
        mc_entity *next = NULL;
        mc_mob_error error;
        CHECK(mc_mob_allocate(f->base.entities, f->base.registry, literal, &next, &error) ==
                  MC_MOB_OK &&
              next->phase == MC_ENTITY_RAW && next->mob && !next->local_player &&
              !next->base.world && !next->random && !next->bounds && !next->watcher &&
              next->holds == 1);
        if (!root)
            root = next;
        else {
            CHECK((index & 1u ? mc_mob_look_target_store(tail, next, NULL)
                              : mc_mob_creature_target_store(tail, next, NULL)) == MC_ENTITY_OK &&
                  next->holds == 2);
            mc_entity_release(next);
            CHECK(next->holds == 1);
        }
        tail = next;
    }
    *out = root;
    return true;
}

static bool chain_controls(mob_fixture *f) {
    const size_t depth = 16384;
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(f->base.entities, &before) == MC_ENTITY_OK &&
          sizeof(mc_mob_entity_object) <=
              (f->base.entities->limits.max_allocation_bytes - before.allocation_bytes) /
                  (depth + 2));
    const uint64_t u = atomic_load(&f->base.profile->uniquifier);
    const uint64_t math = atomic_load(&f->base.profile->math->state);
    mc_entity *root = NULL;
    CHECK(raw_chain(f, depth, &root) && root &&
          mc_entity_classes_get_snapshot(f->base.entities, &after) == MC_ENTITY_OK &&
          after.live_entities == before.live_entities + depth);
    mc_entity_release(root);
    root = NULL;
    CHECK(mc_entity_classes_get_snapshot(f->base.entities, &after) == MC_ENTITY_OK &&
          after.live_entities == before.live_entities &&
          after.allocation_bytes == before.allocation_bytes);
    CHECK(raw_chain(f, depth, &root));
    const mc_entity_class_literal *literal = NULL;
    mc_entity *anchor = NULL, *replacement = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->base.entities, MC_ENTITY_CLASS_KIND_COW, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->base.entities, f->base.registry, literal, &anchor, &error) ==
              MC_MOB_OK &&
          mc_mob_allocate(f->base.entities, f->base.registry, literal, &replacement, &error) ==
              MC_MOB_OK &&
          mc_mob_look_target_store(anchor, root, NULL) == MC_ENTITY_OK &&
          mc_mob_creature_target_store(anchor, root, NULL) == MC_ENTITY_OK && root->holds == 3);
    mc_entity_release(root);
    CHECK(root->holds == 2 && mc_mob_look_target_store(anchor, NULL, NULL) == MC_ENTITY_OK &&
          root->holds == 1 &&
          mc_entity_classes_get_snapshot(f->base.entities, &after) == MC_ENTITY_OK &&
          after.live_entities == before.live_entities + depth + 2);
    /* Actual replacement retains its new occurrence before releasing the old
     * target-only chain. No source World owns these genuine RAW allocations. */
    CHECK(mc_mob_creature_target_store(anchor, replacement, NULL) == MC_ENTITY_OK &&
          replacement->holds == 2 && anchor->mob->creature_target == replacement &&
          anchor->mob->creature_target_hold == replacement &&
          anchor->mob->living.private_b == NULL);
    root = NULL;
    mc_entity_release(replacement);
    CHECK(replacement->holds == 1 &&
          mc_entity_classes_get_snapshot(f->base.entities, &after) == MC_ENTITY_OK &&
          after.live_entities == before.live_entities + 2 &&
          after.allocation_bytes == before.allocation_bytes + mc_entity_native_bytes(anchor) +
                                        mc_entity_native_bytes(replacement));
    mc_entity_release(anchor);
    anchor = replacement = NULL;
    CHECK(mc_entity_classes_get_snapshot(f->base.entities, &after) == MC_ENTITY_OK &&
          after.live_entities == before.live_entities &&
          after.allocation_bytes == before.allocation_bytes && after.next_id == before.next_id &&
          atomic_load(&f->base.profile->uniquifier) == u &&
          atomic_load(&f->base.profile->math->state) == math);
    fprintf(output,
            "{\"event\":\"native_control\",\"case\":%u,\"scene\":\"deep_raw_target_disposal\","
            "\"depth\":%zu,\"direct_release\":true,\"alias_replacement_release\":true,\"native_"
            "bytes_restored\":true,\"live_entities_restored\":true}\n",
            f->input.id, depth);
    return true;
}

static bool piston_root_controls(mob_fixture *f) {
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    f->observer_registered = false;
    CHECK(mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
    f->observer = NULL;
    CHECK(construct_mob(f, "dl", &f->other));
    f->other_owned = true;
    CHECK(mc_entity_set_position(f->other, 9.5, 64.5, 8.5) == MC_ENTITY_OK);
    bool inserted = false;
    mc_world_entity_error publication;
    CHECK(mc_world_runtime_insert_entity(f->base.world, f->other, &inserted, &publication) ==
              MC_WORLD_STORAGE_OK &&
          inserted && mc_mob_look_target_store(f->actor, f->other, NULL) == MC_ENTITY_OK &&
          mc_mob_creature_target_store(f->actor, f->other, NULL) == MC_ENTITY_OK &&
          mc_mob_look_target_store(f->other, f->actor, NULL) == MC_ENTITY_OK);
    mc_path *shared_path = NULL;
    CHECK(shared_control_path(f, &shared_path) && mc_path_advance(shared_path, NULL) == MC_PATH_OK);
    mc_world_entity_list_view view = {0};
    mc_world_nearby_error *query = calloc(1, sizeof(*query));
    mc_world_entity_tick_error *tick = calloc(1, sizeof(*tick));
    CHECK(query && tick);
    const mc_entity_bounds occupied = {7, 64, 7, 11, 67, 10}, empty = {24, 64, 24, 25, 67, 25};
    size_t count = 0;
    CHECK(mc_world_runtime_nearby_entities(f->base.world, NULL, &occupied, &view, query) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_entity_list_view_count(&view, &count) == MC_WORLD_STORAGE_OK && count == 2);
    mc_entity *a = NULL, *b = NULL;
    CHECK(mc_world_entity_list_view_get(&view, 0, &a) == MC_WORLD_STORAGE_OK && a == f->actor &&
          mc_world_entity_list_view_get(&view, 1, &b) == MC_WORLD_STORAGE_OK && b == f->other);
    mc_tile_entity *piston = NULL;
    mc_piston_list_error append;
    CHECK(mc_tile_entity_allocate(f->base.world, MC_TILE_ENTITY_PISTON, &piston) ==
              MC_TILE_ENTITY_OK &&
          mc_tile_entity_construct(piston) == MC_TILE_ENTITY_OK &&
          mc_tile_entity_piston_entities_append(piston, &view, &append) == MC_WORLD_STORAGE_OK);
    mc_world_entity_list_view_release(&view);
    CHECK(mc_world_runtime_nearby_entities(f->base.world, NULL, &empty, &view, query) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_entity_list_view_count(&view, &count) == MC_WORLD_STORAGE_OK && count == 0);
    mc_world_entity_list_view_release(&view);
    mc_tile_entity_release(piston);
    piston = NULL;
    mc_tile_entity_piston_class_snapshot classes;
    CHECK(
        mc_tile_entity_piston_class_get_snapshot(f->base.classes, &classes) == MC_TILE_ENTITY_OK &&
        classes.live_tile_entities == 0 && classes.list_count == 2 &&
        classes.list_identity->items[0] == f->actor && classes.list_identity->items[1] == f->other);
    /* Declared lifetime-control operands: the actual World Entity loop removes
     * both dead occurrences without entering active Mob ticks. Static m stays. */
    f->actor->base.be = f->other->base.be = true;
    CHECK(mc_world_runtime_tick_entities(f->base.world, NULL, NULL, tick) == MC_WORLD_STORAGE_OK &&
          f->base.world->fields.b->count == 0 && f->base.world->entity_membership.count == 0 &&
          f->base.world->fields.R->count == 0 && f->actor->base.bt == 0 && f->other->base.bt == 0 &&
          f->actor->mob->living.private_b == f->other &&
          f->actor->mob->creature_target == f->other &&
          f->other->mob->living.private_b == f->actor);
    const size_t actor_holds = f->actor->holds, other_holds = f->other->holds,
                 bindings = f->base.world->persistent_bindings;
    mc_world_storage_error error;
    CHECK(
        mc_world_runtime_retire_entity_containers(f->base.world, &error) == MC_WORLD_STORAGE_BUSY &&
        error.operation &&
        !strcmp(error.operation, "World terminal external owner of Mob reference fields") &&
        !f->base.world->entity_containers_retired &&
        f->base.world->construction.phase == MC_WORLD_RUNTIME_READY &&
        f->actor->holds == actor_holds && f->other->holds == other_holds &&
        f->base.world->persistent_bindings == bindings &&
        f->actor->mob->living.private_b == f->other && f->actor->mob->creature_target == f->other &&
        f->other->mob->living.private_b == f->actor);
    mc_entity_release(f->actor);
    mc_entity_release(f->other);
    f->actor_owned = f->other_owned = false;
    mc_world_terminal_inventory inventory;
    CHECK(mc_world_runtime_terminal_admit(f->base.world, NULL, &inventory, &error) ==
              MC_WORLD_STORAGE_OK &&
          inventory.entities == 2 && inventory.entity_occurrences == 5 &&
          inventory.tile_entities == 0 && inventory.piston_discard_required &&
          inventory.expected_world_bindings == bindings && inventory.paths == 1 &&
          inventory.path_occurrences == 2 && inventory.path_arenas == 1);
    CHECK(mc_world_runtime_retire_entity_containers(f->base.world, &error) == MC_WORLD_STORAGE_OK &&
          f->base.world->construction.phase == MC_WORLD_RUNTIME_RETIRED && f->actor->holds == 1 &&
          f->other->holds == 1 && f->actor->mob->living.private_b == NULL &&
          f->actor->mob->creature_target == NULL && f->other->mob->living.private_b == NULL &&
          mc_tile_entity_piston_class_get_snapshot(f->base.classes, &classes) ==
              MC_TILE_ENTITY_OK &&
          classes.list_count == 2);
    CHECK(f->actor->mob->creature_path == shared_path &&
          f->other->mob->creature_path == shared_path && control_path_is(shared_path, 1, 2));
    CHECK(mc_tile_entity_piston_class_discard_entities(f->base.classes, &error) ==
              MC_WORLD_STORAGE_OK &&
          mc_tile_entity_piston_class_get_snapshot(f->base.classes, &classes) ==
              MC_TILE_ENTITY_OK &&
          classes.list_count == 0 && classes.live_tile_entities == 0 &&
          f->base.world->persistent_bindings == 1);
    mc_entity_class_snapshot retired;
    CHECK(mc_entity_classes_get_snapshot(f->base.entities, &retired) == MC_ENTITY_OK &&
          retired.live_path_arenas == 0);
    f->actor = f->other = NULL;
    f->terminal_committed = true;
    free(tick);
    free(query);
    fprintf(output,
            "{\"event\":\"native_control\",\"case\":%u,\"scene\":\"piston_only_target_cycle\","
            "\"actual_query_append\":true,\"actual_dead_removal\":true,\"external_retire_refused\":"
            "true,\"joint_retire\":true,\"class_discard\":true,\"paths\":1,\"path_occurrences\":2,"
            "\"shared_cursor\":1,\"arena_released\":true}\n",
            f->input.id);
    return true;
}

static bool foreign_literal_controls(mob_fixture *f) {
    mc_entity_classes *foreign = NULL;
    const mc_entity_class_literal *literal = NULL;
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->base.profile, NULL, &foreign) ==
              MC_ENTITY_OK &&
          mc_entity_class_literal_get(foreign, MC_ENTITY_CLASS_KIND_COW, &literal) ==
              MC_ENTITY_OK &&
          literal != f->actor->class_literal);
    const mc_entity_class_literal *const original = f->actor->class_literal;
    /* Native metadata fault, using an actual separate class owner. Keep the
     * mismatched literal installed throughout rejection and no-effects proof. */
    f->actor->class_literal = literal;
    const mc_entity actor = *f->actor;
    const mc_mob_entity_state mob = *f->actor->mob;
    const mc_world_runtime world = *f->base.world;
    const mc_entity_bounds bounds = *f->actor->bounds;
    const mc_java_random own = *f->actor->random, world_random = *f->base.world->fields.r;
    const uint64_t shared_u = atomic_load(&f->base.profile->uniquifier);
    const uint64_t shared_math = atomic_load(&f->base.profile->math->state);
    const size_t borrows = f->base.registry->runtime_borrows;
    const int32_t boxes = f->base.entities->bounds_cursor,
                  vectors = f->base.entities->vector_cursor;
    mc_entity_effect_error *effect = calloc(1, sizeof(*effect));
    mc_entity_environment_error *environment = calloc(1, sizeof(*environment));
    mc_entity_move_error *movement = calloc(1, sizeof(*movement));
    mc_entity_material_error *material = calloc(1, sizeof(*material));
    CHECK(effect && environment && movement && material);
    bool result = true;
    CHECK(mc_entity_effect_sneaking(f->actor, &result, effect) == MC_ENTITY_STATE_ERROR && result &&
          effect->cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL &&
          effect->source_bytecode == 0 &&
          mc_entity_environment_water(f->actor, f->base.registry, NULL, &result, environment) ==
              MC_ENTITY_STATE_ERROR &&
          result && environment->cause == MC_ENTITY_ENVIRONMENT_UNSUPPORTED_VIRTUAL &&
          environment->bytecode == 0 &&
          mc_entity_move_in_context(f->actor, f->base.registry, 1., 2., 3., NULL, movement) ==
              MC_ENTITY_STATE_ERROR &&
          movement->bytecode == 0 &&
          mc_entity_inside_material(f->actor, mc_materials_get(f->base.materials, MC_MATERIAL_g),
                                    &result, material) == MC_ENTITY_STATE_ERROR &&
          result && material->bytecode == 5);
    CHECK(f->actor->class_literal == literal && !memcmp(&actor, f->actor, sizeof(actor)) &&
          !memcmp(&mob, f->actor->mob, sizeof(mob)) &&
          !memcmp(&world, f->base.world, sizeof(world)) &&
          !memcmp(&bounds, f->actor->bounds, sizeof(bounds)) &&
          mc_test_java_random_equal(&own, f->actor->random) &&
          mc_test_java_random_equal(&world_random, f->base.world->fields.r) &&
          atomic_load(&f->base.profile->uniquifier) == shared_u &&
          atomic_load(&f->base.profile->math->state) == shared_math &&
          f->base.registry->runtime_borrows == borrows &&
          f->base.entities->bounds_cursor == boxes && f->base.entities->vector_cursor == vectors);
    /* Only fixture metadata is restored after every rejected entry was proven. */
    f->actor->class_literal = original;
    CHECK(mc_entity_classes_free(foreign) == MC_ENTITY_OK);
    free(effect);
    free(environment);
    free(movement);
    free(material);
    fprintf(output,
            "{\"event\":\"native_control\",\"case\":%u,\"scene\":\"foreign_class_literal\","
            "\"rejected_entries\":4,\"source_state_unchanged\":true,\"streams_unchanged\":true}\n",
            f->input.id);
    return true;
}

static bool missing_math_controls(mob_fixture *f) {
    mc_entity_effect_error effect;
    CHECK(f->actor->mob->living.Y > 0 && !f->actor->mob->living.V &&
          mc_entity_effect_dead(f->actor, &effect) == MC_ENTITY_OK && f->actor->base.be);
    mc_java_random expected = *f->actor->random;
    int32_t ambient = -1, behavior = -1, candidate = -1;
    CHECK(mc_java_random_next_bounded(&expected, 1000, &ambient) && ambient >= 0 &&
          mc_java_random_next_bounded(&expected, 80, &behavior) && behavior == 0 &&
          mc_java_random_next_bounded(&expected, 13, &candidate) && candidate >= 0);
    CHECK(tick_call_context(f, NULL, false, NULL) && f->error->status == MC_ENTITY_STATE_ERROR &&
          f->error->native_policy && !f->error->source_exception && f->error->site.class_name &&
          !strcmp(f->error->site.class_name, "ii") && f->error->site.method &&
          !strcmp(f->error->site.method, "E") && f->error->site.pc == 42 &&
          f->error->state.environment_returned && !f->error->state.action_returned &&
          !f->error->state.behavior_returned && f->error->math.status == MC_MATH_CLASS_OK &&
          mc_test_java_random_equal(&expected, f->actor->random));
    return true;
}

static bool facing_limit_controls(mob_fixture *f) {
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    f->observer_registered = false;
    const tick_operand operand = {.operation = "secondary",
                                  .owner = "bx",
                                  .member = "construct_publish",
                                  .type = "entity",
                                  .value = "10.5,64.5,8.5,0,0"};
    CHECK(secondary_prepare(f, &operand) &&
          mc_mob_look_target_store(f->actor, f->other, NULL) == MC_ENTITY_OK &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->observer_registered = true;
    f->actor->base.yaw = 1000.f;
    f->actor->base.aT = 20.f;
    f->actor->mob->living.aC = 20;
    mc_mob_tick_limits limits = full_limits();
    limits.max_work = 0;
    CHECK(tick_call(f, &limits, false) && f->error->status == MC_ENTITY_LIMIT &&
          f->error->site.class_name && !strcmp(f->error->site.class_name, "ls") &&
          f->error->site.method && !strcmp(f->error->site.method, "f_") &&
          f->error->site.pc == 127 && f->error->child_site.class_name &&
          !strcmp(f->error->child_site.class_name, "ls") && f->error->child_site.method &&
          !strcmp(f->error->child_site.method, "b") && f->error->child_site.descriptor &&
          !strcmp(f->error->child_site.descriptor, "(FFF)F") && f->error->child_site.pc == 16 &&
          fbits(f->actor->base.aT) == UINT32_C(0x80000000) && f->actor->base.yaw == 1000.f &&
          f->actor->mob->living.aC == 20 && f->actor->mob->living.private_b == f->other &&
          f->actor->random->state == UINT64_C(0x9b3970be8d41));
    return true;
}

static bool following_controls(mob_fixture *f) {
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    f->observer_registered = false;
    const tick_operand secondary = {.operation = "secondary",
                                    .owner = "bx",
                                    .member = "construct_publish",
                                    .type = "entity",
                                    .value = "10.5,64.5,8.5,30,0"};
    CHECK(secondary_prepare(f, &secondary));
    mc_java_random_reseed(f->actor->random, 0);
    mc_java_random_reseed(f->other->random, 0);
    mc_path *shared = NULL;
    CHECK(shared_control_path(f, &shared) && mc_path_retain(shared, NULL) == MC_PATH_OK);
    f->control_path_hold = shared;
    mc_world_entity_tick_error *world_error = calloc(1, sizeof(*world_error));
    mc_mob_tick_error *second = calloc(1, sizeof(*second)), *cleanup = calloc(1, sizeof(*cleanup));
    CHECK(world_error && second && cleanup);
    const mc_mob_tick_limits limits = full_limits();
    const mc_world_entity_tick_context context = {
        .player = {.travel = {.code_registry = f->base.registry}},
        .math = f->base.math,
        .mob_limits = &limits};
    CHECK(mc_entity_bounds_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
    const mc_world_storage_status status =
        mc_world_runtime_tick_entities(f->base.world, &context, NULL, world_error);
    mc_path_state observed_path;
    CHECK(mc_path_inspect(shared, &observed_path));
    CHECK(status == MC_WORLD_STORAGE_OK);
    CHECK(f->actor->base.bt == 1 && f->other->base.bt == 1);
    CHECK(f->actor->mob->creature_path == shared && f->other->mob->creature_path == shared);
    CHECK(control_path_is(shared, 4, 3));
    CHECK(f->actor->random->state == UINT64_C(0x9b3970be8d41));
    CHECK(f->other->random->state == UINT64_C(0x9b3970be8d41));
    /* Declared caller pose input, outside either tick: place the first genuine
     * actor at its goal. Its next actual following body finishes/clears at340.
     * The second Mob still observes the same now-complete shared DH. */
    CHECK(mc_entity_set_position(f->actor, 12.5, 64.5, 8.5) == MC_ENTITY_OK &&
          mc_mob_tick(f->actor, f->base.registry, f->base.math, &limits, f->error) ==
              MC_ENTITY_OK &&
          f->error->state.returned && f->error->state.path_calls == 0 &&
          f->error->state.path_advances == 1 && f->error->state.path_waypoints == 1 &&
          f->error->state.creature_pc == 711 && !f->actor->mob->creature_path &&
          f->other->mob->creature_path == shared && control_path_is(shared, 5, 2));
    CHECK(mc_mob_tick(f->other, f->base.registry, f->base.math, &limits, second) ==
              MC_ENTITY_STATE_ERROR &&
          second->source_exception && !second->native_policy && second->exception_class &&
          !strcmp(second->exception_class, "java/lang/ArrayIndexOutOfBoundsException") &&
          second->site.class_name && !strcmp(second->site.class_name, "ii") &&
          second->site.pc == 272 && second->child_site.class_name &&
          !strcmp(second->child_site.class_name, "dh") && second->child_site.method &&
          !strcmp(second->child_site.method, "a") && second->child_site.pc == 8 &&
          !second->state.cleanup_pending && f->other->mob->creature_path == shared &&
          control_path_is(shared, 5, 2));
    const mc_entity_base_fields fields = f->other->base;
    const mc_mob_entity_state mob = *f->other->mob;
    const mc_java_random own = *f->other->random, world = *f->base.world->fields.r;
    CHECK(mc_mob_tick_cleanup(f->other, cleanup) == MC_ENTITY_OK &&
          !memcmp(&fields, &f->other->base, sizeof(fields)) &&
          !memcmp(&mob, f->other->mob, sizeof(mob)) &&
          mc_test_java_random_equal(&own, f->other->random) &&
          mc_test_java_random_equal(&world, f->base.world->fields.r) &&
          control_path_is(shared, 5, 2) && f->base.world->active_calls == 0 &&
          f->base.world->local_leases == 0);
    mc_path_release(shared);
    f->control_path_hold = NULL;
    CHECK(control_path_is(shared, 5, 1) &&
          mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
              MC_WORLD_STORAGE_OK);
    f->observer_registered = true;
    free(world_error);
    free(second);
    free(cleanup);
    fprintf(output,
            "{\"event\":\"native_control\",\"case\":%u,\"scene\":\"shared_path_following\","
            "\"actual_World_loops\":1,\"actual_World_entity_ticks\":2,\"direct_following_ticks\":2,"
            "\"shared_cursor_after_World\":4,\"finished_cursor\":5,"
            "\"clear_after_actual_advance\":true,\"other_alias_source_index_failure\":true,"
            "\"failed_prefix_cleanup_without_repair\":true}\n",
            f->input.id);
    return true;
}

static bool native_control(mob_fixture *f, size_t control) {
    CHECK(control < 13);
    if (control == 5)
        return target_controls(f);
    if (control == 6)
        return eye_controls(f);
    if (control == 7)
        return chain_controls(f);
    if (control == 8)
        return piston_root_controls(f);
    if (control == 9)
        return foreign_literal_controls(f);
    if (control == 10)
        return missing_math_controls(f);
    if (control == 11)
        return facing_limit_controls(f);
    if (control == 12)
        return following_controls(f);
    mc_mob_tick_limits limits = full_limits();
    switch (control) {
        case 0:
            f->actor->mob->living.H = 1000.f;
            limits.max_work = 0;
            break;
        case 1:
            f->owned_random = f->actor->random;
            f->actor->random = NULL;
            break;
        case 2:
            f->owned_world = f->actor->base.world;
            f->actor->base.world = NULL;
            break;
        case 3:
            f->owned_watcher = f->actor->watcher;
            f->actor->watcher = NULL;
            break;
        case 4:
            f->owned_bounds = f->actor->bounds;
            f->actor->bounds = NULL;
            break;
        default:
            return false;
    }
    CHECK(tick_call(f, &limits, false) && source_inputs_intact(f));
    if (control == 0) {
        CHECK(f->error->status == MC_ENTITY_LIMIT && f->error->site.class_name &&
              !strcmp(f->error->site.class_name, "ls") && f->error->site.method &&
              !strcmp(f->error->site.method, "w_") && f->error->site.pc == 224 &&
              f->error->state.environment_returned && f->error->state.action_returned &&
              f->error->state.behavior_returned && f->actor->base.bt == 1 &&
              f->actor->random->state == UINT64_C(0xa32dc9f68ab8));
    }
    return true;
}

static bool profile_returns(const tick_profile *p) {
    if (strcmp(p->callback, "none"))
        return false;
    /* Source call sites outside this finite native child closure. The optional
     * comparison pairs actual JDI invocation prefixes, not source full return. */
    const char *const boundaries[] = {"egg",        "drowning",    "lava",        "null_own",
                                      "null_world", "null_bounds", "null_watcher"};
    for (size_t i = 0; i < COUNT(boundaries); ++i)
        if (!strcmp(p->scene, boundaries[i]))
            return !strcmp(p->kind, "xt") && !strcmp(p->scene, "drowning");
    if (!strcmp(p->scene, "eye_block") && !strcmp(p->kind, "bx"))
        return false;
    return true;
}

int main(int argc, char **argv) {
    if (argc != 7)
        return 2;
    tick_profile profiles[256] = {0};
    tick_operand operands[256] = {0};
    size_t count = 0, operand_count = 0;
    if (!profile_read(argv[5], profiles, &count) ||
        !operands_read(argv[6], operands, &operand_count))
        return 2;
    output = open_file(argv[3], "wb");
    FILE *events = open_file(argv[4], "wb");
    if (!output || !events)
        return 2;
    for (size_t index = 0; index < count + 13; ++index) {
        mob_fixture *f = calloc(1, sizeof(*f));
        if (!f)
            return 2;
        f->native_only = index >= count;
        f->input = f->native_only ? (tick_profile){.id = (unsigned int)index,
                                                   .kind = "bx",
                                                   .scene = "native_prefix",
                                                   .own_seed = 0,
                                                   .world_seed = 89,
                                                   .callback = "none"}
                                  : profiles[index];
        if (f->native_only && index - count == 6) {
            memcpy(f->input.kind, "gi", 3);
            memcpy(f->input.scene, "native_wolf_eye", sizeof("native_wolf_eye"));
        }
        if (f->native_only && index - count == 10) {
            f->input.own_seed = 288;
            memcpy(f->input.scene, "native_missing_math", sizeof("native_missing_math"));
        }
        if (f->native_only && index - count == 11)
            memcpy(f->input.scene, "native_facing_limit", sizeof("native_facing_limit"));
        f->events = events;
        f->error = calloc(1, sizeof(*f->error));
        char directory[8192];
        const int n = snprintf(directory, sizeof(directory), "%s/case%zu", argv[2], index);
        const bool passed =
            f->error && n > 0 && (size_t)n < sizeof(directory) &&
            prepare_mob(f, argv[1], directory) &&
            (f->native_only
                 ? observer_attach(f) && native_control(f, index - count)
                 : profile_prepare(f, operands, operand_count) &&
                       mc_entity_bounds_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK &&
                       mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK &&
                       observer_attach(f) && tick_call(f, NULL, profile_returns(&f->input)));
        if (!mob_teardown(f)) {
            fputs("Mob tick cleanup refused; live dependencies retained\n", stderr);
            return 1;
        }
        free(f);
        if (!passed)
            return 1;
    }
    if (fclose(output) || fclose(events))
        return 1;
    printf("Mob tick: %u checks;%zu paired profiles\n", checks, count);
    return 0;
}
