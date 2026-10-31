/* Genuine GAME Zombie NULL-source damage and actual nested Mob environment.
 * Proposed only; no constructor entropy or Source observation is an input. */
int mc_zombie_damage_world_fixture_main(int argc, char **argv);
#define main mc_zombie_damage_world_fixture_main
#include "../world/runtime/test_world_random_tick.c"
#undef main
#include "entity/mob/internal.h"
#include "entity/mob/references_internal.h"
#include "mc/entity/mob/tick.h"
#include "mc/entity/movement/effects.h"
#include "mc/world/runtime/entities.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Zombie damage check%u line%d: %s\n", checks, __LINE__, #c);           \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
#define fbits float_bits
#define dbits double_bits
#include "zombie_damage_transport.inc"

typedef struct damage_input {
    unsigned int id;
    char entry[32], scene[64], world[12], attacker[12], callback[32], scope[64];
    int32_t amount, health, by, au;
} damage_input;

typedef struct damage_fixture {
    random_fixture base;
    damage_input input;
    mc_entity *actor, *cow;
    mc_world_runtime *source_null_world;
    mc_java_random *source_null_random;
    mc_world_access *observer;
    bool observer_registered, callback_bad, source_inputs_observed;
    size_t sounds, actor_holds, cow_holds;
    uint64_t math_before, own_before, world_before, uniquifier_before;
    uint64_t callback_math, callback_own;
    bool callback_seen;
    mc_entity_effect_error *effect;
    mc_mob_tick_error *tick, *cleanup;
} damage_fixture;

static uint32_t oracle_bits(uint64_t *state, unsigned int count) {
    *state = (*state * UINT64_C(0x5deece66d) + UINT64_C(11)) & UINT64_C(0xffffffffffff);
    return (uint32_t)(*state >> (48u - count));
}

static double oracle_double(uint64_t *state) {
    const uint64_t high = oracle_bits(state, 26), low = oracle_bits(state, 27);
    return (double)((high << 27) | low) / 9007199254740992.0;
}

static float oracle_float(uint64_t *state) {
    return (float)oracle_bits(state, 24) / 16777216.0f;
}

static float oracle_pitch(float a, float b) {
    volatile float difference = a - b;
    volatile float scaled = difference * .2f;
    return scaled + 1.0f;
}

static bool read_damage_input(FILE *in, damage_input *p) {
    char line[1024];
    CHECK(fgets(line, sizeof(line), in) != NULL);
#ifdef _MSC_VER
    const int n = sscanf_s(
        line,
        "%u\t%31s\t%63s\t%" SCNd32 "\t%11s\t%" SCNd32 "\t%" SCNd32 "\t%" SCNd32
        "\t%11s\t%31s\t%63s",
        &p->id, p->entry, (unsigned int)sizeof(p->entry), p->scene, (unsigned int)sizeof(p->scene),
        &p->amount, p->world, (unsigned int)sizeof(p->world), &p->health, &p->by, &p->au,
        p->attacker, (unsigned int)sizeof(p->attacker), p->callback,
        (unsigned int)sizeof(p->callback), p->scope, (unsigned int)sizeof(p->scope));
#else
    const int n = sscanf(line,
                         "%u\t%31s\t%63s\t%" SCNd32 "\t%11s\t%" SCNd32 "\t%" SCNd32 "\t%" SCNd32
                         "\t%11s\t%31s\t%63s",
                         &p->id, p->entry, p->scene, &p->amount, p->world, &p->health, &p->by,
                         &p->au, p->attacker, p->callback, p->scope);
#endif
    CHECK(n == 11 && p->id < 12);
    return true;
}

static bool snapshot_entity(mc_entity *entity) {
    mc_mob_state s;
    CHECK(mc_mob_inspect(entity, &s));
    fputs("{\"fields\":{", output);
    emit_fields(entity, &s);
    fputs("},\"bounds\":", output);
    if (entity->bounds) {
        fputc('{', output);
        emit_bounds(entity->bounds);
        fputc('}', output);
    } else
        fputs("null", output);
    fputs(",\"watcher\":", output);
    CHECK(emit_watcher(entity));
    fputs(",\"own_seed\":", output);
    if (entity->random)
        fprintf(output, "\"%012" PRIx64 "\"", entity->random->state);
    else
        fputs("\"-\"", output);
    fputc('}', output);
    return true;
}

static void role_value(const char *name, const void *pointer, const char *cls, bool *comma) {
    if (!pointer)
        return;
    fprintf(output, "%s\"%s\":", *comma ? "," : "", name);
    *comma = true;
    emit_object(pointer, cls);
}

static bool state(damage_fixture *f, const char *phase) {
    mc_entity_class_snapshot c;
    mc_entity_vector_snapshot v;
    mc_mob_state s;
    CHECK(mc_entity_classes_get_snapshot(f->base.entities, &c) == MC_ENTITY_OK &&
          mc_entity_vector_get_snapshot(f->base.entities, &v) == MC_ENTITY_OK &&
          mc_mob_inspect(f->actor, &s));
    fprintf(output, "{\"event\":\"state\",\"case\":%u,\"phase\":\"%s\",\"fields\":{", f->input.id,
            phase);
    emit_fields(f->actor, &s);
    fputs("},\"bounds\":{", output);
    emit_bounds(f->actor->bounds);
    fputs("},\"watcher\":", output);
    CHECK(emit_watcher(f->actor));
    fputs(",\"own_seed\":", output);
    if (f->actor->random)
        fprintf(output, "\"%012" PRIx64 "\"", f->actor->random->state);
    else
        fputs("\"-\"", output);
    fprintf(output,
            ",\"world_seed\":\"%012" PRIx64 "\",\"math_seed\":\"%012" PRIx64
            "\",\"uniquifier\":\"%016" PRIx64
            "\",\"box_cursor\":%d,\"vector_cursor\":%d,\"loaded_entities\":%zu,\"players\":%zu,"
            "\"cache_size\":%zu,\"secondary_entities\":[",
            f->base.world->fields.r->state, atomic_load(&f->base.profile->math->state),
            atomic_load(&f->base.profile->uniquifier), c.bounds_pool_cursor, v.cursor,
            f->base.world->fields.b->count, f->base.world->fields.d->count,
            f->base.provider.cache->count);
    if (f->cow)
        CHECK(snapshot_entity(f->cow));
    fputs("],\"reference_roles\":{", output);
    bool comma = false;
    role_value("actor", f->actor, "uz", &comma);
    role_value("world", f->base.world, "fd", &comma);
    role_value("actor.bounds", f->actor->bounds, "eq", &comma);
    role_value("actor.random", f->actor->random, "java.util.Random", &comma);
    role_value("actor.watcher", f->actor->watcher, "ud", &comma);
    /* The real whole Mob tick in case9 may genuinely produce a Path from its
     * independent private stream. This later graph is Native-only; retain its
     * actual role rather than constrain or rebind that random branch. */
    if (f->actor->mob->creature_path)
        role_value("actor.path", f->actor->mob->creature_path, "dh", &comma);
    if (f->cow) {
        role_value("secondary0", f->cow, "bx", &comma);
        role_value("secondary0.bounds", f->cow->bounds, "eq", &comma);
        role_value("secondary0.random", f->cow->random, "java.util.Random", &comma);
        role_value("secondary0.watcher", f->cow->watcher, "ud", &comma);
    }
    fputs("}}\n", output);
    return fflush(output) == 0;
}

static bool construct(damage_fixture *f, mc_entity_class_kind kind, mc_entity **out,
                      unsigned int ordinal) {
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    const unsigned int holder = atomic_load(&f->base.profile->holder_phase);
    const uint64_t u = atomic_load(&f->base.profile->uniquifier);
    CHECK(mc_entity_class_literal_get(f->base.entities, kind, &literal) == MC_ENTITY_OK &&
          mc_mob_allocate(f->base.entities, f->base.registry, literal, out, &error) == MC_MOB_OK &&
          mc_mob_construct(*out, f->base.world, &error) == MC_MOB_OK &&
          (*out)->phase == MC_ENTITY_READY && !(*out)->local_player);
    fprintf(output,
            "{\"event\":\"constructor\",\"case\":%u,\"ordinal\":%u,\"class\":\"%s\",\"default_"
            "profile_rebound\":false,\"holder_before\":%u,\"uniquifier_before\":\"%016" PRIx64
            "\",\"raw\":",
            f->input.id, ordinal, entity_reference_class(*out), holder, u);
    CHECK(snapshot_entity(*out));
    fputs("}\n", output);
    return true;
}

static bool pose_and_publish(damage_fixture *f, mc_entity *p, double x, double y, double z,
                             float yaw, float pitch) {
    /* Declared caller stores matching SN.c; only actual position/bounds leaf is
     * invoked here. This fixture does not add Source SN.c qualification. */
    p->base.x = p->base.aJ = p->base.bl = x;
    p->base.y = p->base.aK = p->base.bm = y;
    p->base.z = p->base.aL = p->base.bn = z;
    p->base.yaw = yaw;
    p->base.aT = pitch;
    CHECK(mc_entity_set_position(p, x, y, z) == MC_ENTITY_OK);
    bool published = false;
    mc_world_entity_error error;
    CHECK(mc_world_runtime_insert_entity(f->base.world, p, &published, &error) ==
              MC_WORLD_STORAGE_OK &&
          published);
    return true;
}

static mc_world_storage_status damage_sound(void *owner, mc_world_runtime *world,
                                            const mc_registry_string *name, double x, double y,
                                            double z, float volume, float pitch,
                                            mc_world_storage_error *error) {
    damage_fixture *f = owner;
    mc_registry_string_view text = {0};
    if (world != f->base.world || !mc_registry_string_read(name, &text) || text.length >= 64 ||
        f->actor->base.bv != (f->input.id == 9 ? 20 : 0))
        f->callback_bad = true;
    char copy[64] = {0};
    if (!f->callback_bad)
        for (size_t i = 0; i < text.length; ++i) {
            if (text.units[i] > 127)
                f->callback_bad = true;
            else
                copy[i] = (char)text.units[i];
        }
    if (mc_world_access_free(f->observer, NULL) != MC_WORLD_STORAGE_BUSY ||
        mc_world_runtime_retire_world(world, NULL) != MC_WORLD_STORAGE_BUSY)
        f->callback_bad = true;
    f->callback_math = atomic_load(&f->base.profile->math->state);
    f->callback_own = f->actor->random ? f->actor->random->state : 0;
    f->callback_seen = true;
    fprintf(
        output,
        "{\"event\":\"sound\",\"case\":%u,\"ordinal\":%zu,\"name\":\"%s\",\"xyz\":[\"%016" PRIx64
        "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"volume\":{\"F\":\"%08" PRIx32
        "\"},\"pitch\":{\"F\":\"%08" PRIx32
        "\"},\"World_retire_BUSY\":true,\"access_free_BUSY\":true}\n",
        f->input.id, f->sounds++, copy, dbits(x), dbits(y), dbits(z), fbits(volume), fbits(pitch));
    if (!state(f, "sound_callback"))
        f->callback_bad = true;
    if (f->input.id == 10) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored native Zombie sound observer failure", 0};
        return error->status;
    }
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool prepare(damage_fixture *f, const char *resources, const char *directory) {
    CHECK(setup(&f->base, resources, directory));
    for (int32_t x = -2; x <= 2; ++x)
        for (int32_t z = -2; z <= 2; ++z) {
            mc_chunk_state *chunk = NULL;
            CHECK(load_chunk(&f->base, x, z, &chunk) &&
                  mc_chunk_cache_publish(f->base.provider.cache, x, z, chunk));
        }
    CHECK(f->base.provider.cache->count == 25 && f->base.world->fields.d->count == 0 &&
          construct(f, MC_ENTITY_CLASS_KIND_ZOMBIE, &f->actor, 0) &&
          pose_and_publish(f, f->actor, 8.5, 64.5, 8.5, 30.f, 15.f));
    f->base.world->fields.B = !strcmp(f->input.world, "remote");
    f->base.world->fields.q = 1;
    f->base.world->fields.f = 0;
    mc_living_entity_fields *l = &f->actor->mob->living;
    l->Y = f->input.health;
    l->av = 17;
    l->E = 20;
    f->actor->base.by = f->input.by;
    l->au = f->input.au;
    l->aa = 3;
    l->ab = 4;
    l->ac = 99.f;
    l->al = .25f;
    f->actor->base.bb = false;
    CHECK(f->actor->mob->creature_target == NULL);
    if (!strcmp(f->input.attacker, "COW") || f->input.id == 11) {
        CHECK(construct(f, MC_ENTITY_CLASS_KIND_COW, &f->cow, 1) &&
              pose_and_publish(f, f->cow, 11.5, 64.5, 12.5, 0, 0));
        if (f->input.id == 11) {
            mc_entity_error e;
            CHECK(mc_mob_creature_target_store(f->actor, f->cow, &e) == MC_ENTITY_OK &&
                  f->actor->mob->creature_target == f->cow);
        }
    }
    if (!strcmp(f->input.world, "NULL")) {
        f->source_null_world = f->actor->base.world;
        f->actor->base.world = NULL;
    }
    if (f->input.id == 6) {
        f->source_null_random = f->actor->random;
        f->actor->random = NULL;
    }
    if (f->input.id == 9) {
        f->actor->base.bv = 20;
        f->actor->base.bC = false;
        l->private_a = 0;
    }
    if (f->input.id == 9 || f->input.id == 10) {
        const mc_world_access_callbacks c = {.sound = damage_sound};
        CHECK(mc_world_access_create(&c, f, &f->observer, NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_add_world_access(f->base.world, f->observer, NULL) ==
                  MC_WORLD_STORAGE_OK);
        f->observer_registered = true;
    }
    CHECK(mc_entity_bounds_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(f->base.entities, NULL) == MC_ENTITY_OK);
    f->effect = calloc(1, sizeof(*f->effect));
    f->tick = calloc(1, sizeof(*f->tick));
    f->cleanup = calloc(1, sizeof(*f->cleanup));
    CHECK(f->effect && f->tick && f->cleanup);
    f->actor_holds = f->actor->holds;
    f->cow_holds = f->cow ? f->cow->holds : 0;
    f->math_before = atomic_load(&f->base.profile->math->state);
    f->own_before = f->actor->random ? f->actor->random->state : f->source_null_random->state;
    f->world_before = f->base.world->fields.r->state;
    f->uniquifier_before = atomic_load(&f->base.profile->uniquifier);
    return state(f, "before");
}

static void damage_receipt(const mc_entity_effect_error *e) {
    const mc_entity_living_damage_state *s = &e->living_damage;
    fprintf(output,
            "{\"entered\":%s,\"living_returned\":%s,\"returned\":%s,\"result_stored\":%s,"
            "\"result\":%s,\"full_hit\":%s,\"wrapper_pc\":%u,\"living_pc\":%u,\"health_pc\":%u,"
            "\"native_policy\":%s,\"source_exception\":%s,\"shared_attempts\":%u,\"shared_calls\":%"
            "u,\"private_attempts\":%u,\"private_calls\":%u,\"shared_value\":{\"D\":\"%016" PRIx64
            "\"},\"private_values\":[{\"F\":\"%08" PRIx32 "\"},{\"F\":\"%08" PRIx32
            "\"}],\"status_returned\":%s,\"attacked_returned\":%s,\"sound_arguments_stored\":%s,"
            "\"sound_returned\":%s,\"death_invoke_reached\":%s,\"sound_world\":\"%016" PRIxPTR
            "\",\"sound_name\":\"%016" PRIxPTR "\",\"sound_literal\":",
            s->entered ? "true" : "false", s->living_returned ? "true" : "false",
            s->returned ? "true" : "false", s->result_stored ? "true" : "false",
            s->result ? "true" : "false", s->full_hit ? "true" : "false", s->wrapper_pc,
            s->living_pc, s->health_pc, s->native_policy ? "true" : "false",
            s->source_exception ? "true" : "false", s->shared_attempts, s->shared_calls,
            s->private_attempts, s->private_calls, dbits(s->shared_value),
            fbits(s->private_values[0]), fbits(s->private_values[1]),
            s->status_returned ? "true" : "false", s->attacked_returned ? "true" : "false",
            s->sound_arguments_stored ? "true" : "false", s->sound_returned ? "true" : "false",
            s->death_invoke_reached ? "true" : "false", s->sound_world_identity,
            s->sound_name_identity);
    if (s->sound_literal)
        fprintf(output, "\"%s\"", s->sound_literal);
    else
        fputs("null", output);
    fprintf(output,
            ",\"volume\":{\"F\":\"%08" PRIx32 "\"},\"pitch\":{\"F\":\"%08" PRIx32
            "\"},\"exception_class\":",
            fbits(s->volume), fbits(s->pitch));
    if (s->exception_class)
        fprintf(output, "\"%s\"", s->exception_class);
    else
        fputs("null", output);
    fprintf(output,
            ",\"sound_world_captured\":%s,\"sound_name_returned\":%s,\"volume_returned\":%s,"
            "\"child_site\":",
            s->sound_world_captured ? "true" : "false", s->sound_name_returned ? "true" : "false",
            s->volume_returned ? "true" : "false");
    if (s->child_site.class_name)
        fprintf(output, "{\"class\":\"%s\",\"method\":\"%s\",\"descriptor\":\"%s\",\"pc\":%u}",
                s->child_site.class_name, s->child_site.method, s->child_site.descriptor,
                s->child_site.pc);
    else
        fputs("null", output);
    fputc('}', output);
}

static bool check_direct(damage_fixture *f, mc_entity_status status, bool before, bool answer) {
    const unsigned int i = f->input.id;
    const mc_entity_living_damage_state *r = &f->effect->living_damage;
    mc_living_entity_fields *l = &f->actor->mob->living;
    const bool returned = i == 0 || i == 1 || i == 2 || i == 3 || i == 4 || i == 11;
    CHECK((status == MC_ENTITY_OK) == returned && r->entered == (i != 8) &&
          r->returned == returned);
    if (returned) {
        CHECK(r->result_stored && r->living_returned && answer == r->result &&
              r->wrapper_pc == ((i >= 1 && i <= 3) ? 40 : 26) &&
              r->living_pc == (i == 1   ? 11
                               : i == 2 ? 25
                               : i == 3 ? 59
                                        : 363));
    } else
        CHECK(answer == before && !r->result_stored && !r->living_returned);
    const bool fresh = i == 0 || i == 6 || i == 7 || i == 10 || i == 11;
    CHECK(l->av == (i == 1 || i == 5 || i == 8 ? 17 : 0) &&
          fbits(l->al) == fbits(i == 1 || i == 2 || i == 5 || i == 8 ? .25f : 1.5f));
    CHECK(l->Y == (fresh ? f->input.health - 1 : i == 4 ? 18 : f->input.health));
    CHECK(l->au == (fresh    ? 1
                    : i == 4 ? 3
                             : f->input.au) &&
          l->aa == (fresh ? 10 : 3) && l->ab == (fresh ? 10 : 4) &&
          f->actor->base.by == (fresh ? 20 : f->input.by) && f->actor->base.bb == fresh);
    uint64_t math = f->math_before, own = f->own_before;
    double shared = 0;
    if (fresh) {
        shared = oracle_double(&math);
        CHECK(r->shared_attempts == 1 && r->shared_calls == 1 &&
              dbits(shared) == dbits(r->shared_value) &&
              l->ac == (float)((int32_t)(shared * 2.0) * 180) && r->status_returned &&
              r->attacked_returned);
    } else
        CHECK(r->shared_attempts == 0 && r->shared_calls == 0 &&
              fbits(l->ac) == fbits(i == 4 ? 0.f : 99.f));
    const bool floats = fresh && i != 6;
    if (floats) {
        const float a = oracle_float(&own), b = oracle_float(&own);
        CHECK(r->private_calls == 2 && r->private_attempts == 2 &&
              fbits(a) == fbits(r->private_values[0]) && fbits(b) == fbits(r->private_values[1]) &&
              fbits(r->pitch) == fbits(oracle_pitch(a, b)) && r->sound_arguments_stored &&
              r->sound_name_returned && r->volume_returned && r->volume == 1.f &&
              r->sound_literal &&
              strcmp(r->sound_literal, i == 7 ? "mob.zombiedeath" : "mob.zombiehurt") == 0);
    } else
        CHECK(r->private_calls == 0 && r->private_attempts == (i == 6 ? 1 : 0));
    CHECK(atomic_load(&f->base.profile->math->state) == math &&
          (f->actor->random ? f->actor->random : f->source_null_random)->state == own &&
          f->base.world->fields.r->state == f->world_before &&
          atomic_load(&f->base.profile->uniquifier) == f->uniquifier_before);
    if (i == 5 || i == 6)
        CHECK(r->source_exception && !r->native_policy && r->exception_class &&
              strcmp(r->exception_class, "java/lang/NullPointerException") == 0 &&
              r->living_pc == (i == 5 ? 4 : 343));
    if (i == 7)
        CHECK(r->native_policy && !r->source_exception && r->living_pc == 316 &&
              r->death_invoke_reached && r->sound_returned && !f->actor->base.be);
    if (i == 8)
        CHECK(f->effect->cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL &&
              f->effect->damage_source_identity == (uintptr_t)f->cow &&
              f->effect->damage_amount == 1 && f->effect->damage_bytecode == 3);
    if (i == 10) {
        CHECK(f->callback_seen && f->sounds == 1 && !f->callback_bad);
        CHECK(r->native_policy && !r->source_exception && !r->sound_returned);
        CHECK(r->living_pc == 359);
        CHECK(f->effect->world.bytecode == 53);
        CHECK(f->effect->world.world_access_bytecode == 53);
        CHECK(f->effect->world.cause.status == MC_WORLD_STORAGE_IO_ERROR);
        /* Ordinary native callbacks return storage errors, without a borrowed
         * concrete Source invocation receipt or its receiver identity. */
        CHECK(f->effect->world.world_access_failure.receiver == NULL);
        CHECK(f->effect->world.world_access_failure.receipt == NULL);
        CHECK(f->effect->world.world_access_failure.render_global == NULL);
    }
    if (i == 11) {
        mc_entity *target = NULL;
        mc_entity_error e;
        CHECK(mc_mob_creature_target_get(f->actor, &target, &e) == MC_ENTITY_OK &&
              target == f->cow && f->actor->mob->creature_target_hold == f->cow &&
              f->cow->holds == f->cow_holds && r->wrapper_pc == 26);
    }
    CHECK((!f->source_null_world || f->actor->base.world == NULL) &&
          (!f->source_null_random || f->actor->random == NULL));
    return true;
}

static bool execute(damage_fixture *f) {
    mc_entity_status status;
    bool answer = f->input.id != 0 && f->input.id != 4 && f->input.id != 11, before = answer;
    const uint16_t base_pc = f->actor->base_bytecode;
    const unsigned int ctor_pc = f->actor->mob->constructor_pc;
    mc_mob_error ctor_error;
    memcpy(&ctor_error, &f->actor->mob->failure, sizeof(ctor_error));
    if (f->input.id == 9) {
        status = mc_mob_tick(f->actor, f->base.registry, f->base.math, NULL, f->tick);
        CHECK(status == MC_ENTITY_OK && f->tick->state.returned &&
              f->tick->state.environment_returned && f->callback_seen && f->sounds == 1 &&
              !f->callback_bad && f->actor->base.bv == 19 && f->actor->mob->living.Y == 19 &&
              f->actor->mob->living.aa == 9 && f->actor->base.by == 19);
        uint64_t math = f->math_before, own = f->own_before;
        double draw = oracle_double(&math);
        (void)oracle_float(&own);
        (void)oracle_float(&own);
        CHECK(f->callback_math == math && f->callback_own == own &&
              f->actor->mob->living.ac == (float)((int32_t)(draw * 2.0) * 180));
    } else {
        status = mc_entity_effect_damage_in_context(f->actor, f->base.registry,
                                                    f->input.id == 8 ? f->cow : NULL,
                                                    f->input.amount, &answer, f->effect);
        CHECK(check_direct(f, status, before, answer));
    }
    CHECK(state(f, "after") && f->actor->holds == f->actor_holds &&
          (!f->cow || f->cow->holds == f->cow_holds) && !f->actor->mob->tick_active &&
          f->actor->base_bytecode == base_pc && f->actor->mob->constructor_pc == ctor_pc &&
          memcmp(&ctor_error, &f->actor->mob->failure, sizeof(ctor_error)) == 0);
    fprintf(output,
            "{\"event\":\"result\",\"case\":%u,\"status\":%d,\"answer_before\":%s,\"answer\":%s,"
            "\"scope\":\"%s\",\"effect\":",
            f->input.id, (int)status, before ? "true" : "false", answer ? "true" : "false",
            f->input.id == 9    ? "real_Mob_tick_nested_damage;later_body_native_only"
            : f->input.id == 10 ? "native_adapter_IO_failure;Original_Throwable_identity_unpaired"
                                : "direct_effect_facade");
    damage_receipt(f->effect);
    fprintf(output,
            ",\"effect_cause\":%d,\"effect_source\":\"%016" PRIxPTR
            "\",\"effect_amount\":%d,\"effect_damage_pc\":%u,\"world_status\":%d,\"world_pc\":%u,"
            "\"world_access_pc\":%u,\"world_cause_status\":%d,\"world_cause_operation\":",
            (int)f->effect->cause, f->effect->damage_source_identity, f->effect->damage_amount,
            f->effect->damage_bytecode, (int)f->effect->world.status, f->effect->world.bytecode,
            f->effect->world.world_access_bytecode, (int)f->effect->world.cause.status);
    if (f->effect->world.cause.operation)
        fprintf(output, "\"%s\"", f->effect->world.cause.operation);
    else
        fputs("null", output);
    fprintf(output, ",\"effect_operation\":");
    if (f->effect->operation)
        fprintf(output, "\"%s\"", f->effect->operation);
    else
        fputs("null", output);
    fprintf(output, ",\"effect_source_class\":");
    if (f->effect->source_class)
        fprintf(output, "\"%s\"", f->effect->source_class);
    else
        fputs("null", output);
    fprintf(output,
            ",\"effect_source_pc\":%u,\"effect_living_pc\":%u,\"world_system_error\":%" PRIu32
            ",\"world_"
            "operation\":",
            f->effect->source_bytecode, f->effect->living_bytecode,
            f->effect->world.cause.system_error);
    if (f->effect->world.operation)
        fprintf(output, "\"%s\"", f->effect->world.operation);
    else
        fputs("null", output);
    fprintf(output,
            ",\"world_failure_receiver\":\"%016" PRIxPTR
            "\",\"world_failure_receipt\":\"%016" PRIxPTR
            "\",\"tick_returned\":%s,\"environment_returned\":%s}\n",
            (uintptr_t)f->effect->world.world_access_failure.receiver,
            (uintptr_t)f->effect->world.world_access_failure.receipt,
            f->tick->state.returned ? "true" : "false",
            f->tick->state.environment_returned ? "true" : "false");
    CHECK(mc_mob_tick_cleanup(f->actor, f->cleanup) == MC_ENTITY_OK &&
          !f->cleanup->state.cleanup_pending &&
          (!f->source_null_world || f->actor->base.world == NULL) &&
          (!f->source_null_random || f->actor->random == NULL) &&
          f->actor->holds == f->actor_holds);
    CHECK(state(f, "after_cleanup"));
    f->source_inputs_observed = true;
    fprintf(output,
            "{\"event\":\"cleanup\",\"case\":%u,\"native_returned\":true,\"source_prefix_"
            "preserved\":true,\"ctor_diagnostic_preserved\":true,\"actual_target_preserved\":%s}\n",
            f->input.id, f->input.id == 11 ? "true" : "false");
    return true;
}

static bool dispose(damage_fixture *f) {
    CHECK(f->source_inputs_observed && (!f->source_null_world || f->actor->base.world == NULL) &&
          (!f->source_null_random || f->actor->random == NULL));
    if (f->observer_registered) {
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(f->base.world, f->observer, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed);
        f->observer_registered = false;
    }
    if (f->observer)
        CHECK(mc_world_access_free(f->observer, NULL) == MC_WORLD_STORAGE_OK);
    f->observer = NULL;
    /* End of all Source observations and checked cleanup. Restore only saved
     * constructor allocation pointers for final allocator destruction. No call
     * is retried; no RNG state, health, hold or phase is repaired/rebound. */
    if (f->source_null_world)
        f->actor->base.world = f->source_null_world;
    if (f->source_null_random)
        f->actor->random = f->source_null_random;
    mc_entity_release(f->actor);
    f->actor = NULL;
    if (f->cow) {
        mc_entity_release(f->cow);
        f->cow = NULL;
    }
    free(f->effect);
    free(f->tick);
    free(f->cleanup);
    return teardown(&f->base);
}

int main(int argc, char **argv) {
    if (argc != 5)
        return 2;
    FILE *table = open_file(argv[4], "rb");
    char path[8192], line[1024];
    int n = snprintf(path, sizeof(path), "%s/native.ndjson", argv[3]);
    if (!table || n < 0 || (size_t)n >= sizeof(path) || !(output = open_file(path, "wb")) ||
        !fgets(line, sizeof(line), table))
        return 2;
    for (unsigned int i = 0; i < 12; ++i) {
        damage_fixture *f = calloc(1, sizeof(*f));
        if (!f)
            return 2;
        n = snprintf(path, sizeof(path), "%s/case%u", argv[2], i);
        if (!read_damage_input(table, &f->input) || f->input.id != i || n < 0 ||
            (size_t)n >= sizeof(path) || !prepare(f, argv[1], path) || !execute(f) || !dispose(f)) {
            fputs("Zombie damage failure: actual failed graph retained; no source repair/cleanup "
                  "credit\n",
                  stderr);
            return 1;
        }
        free(f);
    }
    if (fgets(line, sizeof(line), table) || fclose(table) || ferror(output) || fclose(output))
        return 1;
    printf("Zombie NULL damage: %u checks;12 genuine profiles\n", checks);
    return 0;
}
