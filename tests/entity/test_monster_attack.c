/* GZ prefix through the closed real Mob runner; genuine GAME fixture owners. */
int mc_monster_world_fixture_main(int argc, char **argv);
#define main mc_monster_world_fixture_main
#include "../world/runtime/test_world_random_tick.c"
#undef main
#include "entity/mob/tick/internal.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Monster check%u line%d: %s\n", checks, __LINE__, #c);                 \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
#define REQUIRE(c)                                                                                 \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Monster check%u line%d: %s\n", checks, __LINE__, #c);                 \
            goto done;                                                                             \
        }                                                                                          \
    } while (0)

typedef struct monster_input {
    unsigned int id, bits, target_null, target_bounds_null, actor_bounds_null;
    int32_t cooldown;
    char name[80], min_y[24], max_y[24], projection[24];
} monster_input;

static bool monster_inputs(FILE *in, monster_input *p) {
    char line[1024];
    CHECK(fgets(line, sizeof(line), in) != NULL);
#ifdef _MSC_VER
    const int n = sscanf_s(line, "%u\t%79s\t%" SCNd32 "\t%x\t%u\t%u\t%u\t%23s\t%23s\t%23s", &p->id,
                           p->name, (unsigned int)sizeof(p->name), &p->cooldown, &p->bits,
                           &p->target_null, &p->target_bounds_null, &p->actor_bounds_null, p->min_y,
                           (unsigned int)sizeof(p->min_y), p->max_y, (unsigned int)sizeof(p->max_y),
                           p->projection, (unsigned int)sizeof(p->projection));
#else
    const int n = sscanf(line, "%u\t%79s\t%" SCNd32 "\t%x\t%u\t%u\t%u\t%23s\t%23s\t%23s", &p->id,
                         p->name, &p->cooldown, &p->bits, &p->target_null, &p->target_bounds_null,
                         &p->actor_bounds_null, p->min_y, p->max_y, p->projection);
#endif
    CHECK(n == 10 && p->id < 12 && p->target_null < 2 && p->target_bounds_null < 2 &&
          p->actor_bounds_null < 2);
    return true;
}

static double monster_double(const char *s) {
    if (strcmp(s, "NaN") == 0) {
        const uint64_t bits = UINT64_C(0x7ff8000000000000);
        double value;
        memcpy(&value, &bits, sizeof(value));
        return value;
    }
    return strtod(s, NULL);
}

static void monster_bounds(FILE *out, const mc_entity_bounds *b) {
    if (b == NULL) {
        fputs("null", out);
        return;
    }
    fprintf(out,
            "[\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\",\"%016" PRIx64
            "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"]",
            double_bits(b->min_x), double_bits(b->min_y), double_bits(b->min_z),
            double_bits(b->max_x), double_bits(b->max_y), double_bits(b->max_z));
}

static void monster_state(FILE *out, random_fixture *f, mc_entity *a, mc_entity *t) {
    fprintf(out,
            "{\"cooldown\":%" PRId32 ",\"damage\":%" PRId32 ",\"actor_health\":%" PRId32
            ",\"target_health\":%" PRId32
            ",\"attacking\":%s,\"actor_dead\":%s,\"target_dead\":%s,\"actor_bounds\":",
            a->mob->living.ae, a->mob->monster_damage, a->mob->living.Y, t->mob->living.Y,
            a->mob->creature_attacking ? "true" : "false", a->base.be ? "true" : "false",
            t->base.be ? "true" : "false");
    monster_bounds(out, a->bounds);
    fputs(",\"target_bounds\":", out);
    monster_bounds(out, t->bounds);
    fprintf(out,
            ",\"actor_seed\":\"%012" PRIx64 "\",\"target_seed\":\"%012" PRIx64
            "\",\"world_seed\":\"%012" PRIx64 "\",\"math_seed\":\"%012" PRIx64
            "\",\"U\":\"%016" PRIx64 "\"}",
            a->random->state, t->random->state, f->world->fields.r->state,
            atomic_load(&f->profile->math->state), atomic_load(&f->profile->uniquifier));
}

static bool monster_construct(random_fixture *f, mc_entity_class_kind kind, mc_entity **out) {
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error error;
    CHECK(mc_entity_class_literal_get(f->entities, kind, &literal) == MC_ENTITY_OK &&
          mc_mob_allocate(f->entities, f->registry, literal, out, &error) == MC_MOB_OK &&
          mc_mob_construct(*out, f->world, &error) == MC_MOB_OK);
    CHECK(*out && (*out)->phase == MC_ENTITY_READY && (*out)->mob && !(*out)->local_player);
    return true;
}

static bool monster_one(random_fixture *f, const monster_input *p, FILE *out) {
    bool passed = false;
    mc_entity *actor = NULL, *target = NULL;
    mc_entity_bounds *actor_owned = NULL, *target_owned = NULL;
    mc_mob_tick_error *e = calloc(1, sizeof(*e)), *cleanup = calloc(1, sizeof(*cleanup));
    const unsigned int holder_before = atomic_load(&f->profile->holder_phase);
    const uint64_t u_before = atomic_load(&f->profile->uniquifier);
    REQUIRE(e && cleanup);
    REQUIRE(monster_construct(f, MC_ENTITY_CLASS_KIND_ZOMBIE, &actor) &&
            monster_construct(f, MC_ENTITY_CLASS_KIND_PIG, &target));
    REQUIRE(atomic_load(&f->profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_READY &&
            f->profile->math != NULL);
    mc_entity_class_literal_view actor_class, target_class;
    REQUIRE(mc_entity_class_literal_inspect(actor->class_literal, &actor_class) == MC_ENTITY_OK &&
            mc_entity_class_literal_inspect(target->class_literal, &target_class) == MC_ENTITY_OK);
    fprintf(out,
            "{\"case\":%u,\"scenario\":\"%s\",\"actor_class\":\"%s\","
            "\"target_class\":\"%s\",\"constructor\":{\"rebound\":false,"
            "\"holder_before\":%u,\"U_before\":\"%016" PRIx64 "\",\"actor_F\":\"%08" PRIx32
            "\",\"actor_G\":\"%08" PRIx32 "\",\"actor_aj\":\"%08" PRIx32
            "\",\"actor_yaw\":\"%08" PRIx32 "\",\"target_F\":\"%08" PRIx32
            "\",\"target_G\":\"%08" PRIx32 "\",\"target_aj\":\"%08" PRIx32
            "\",\"target_yaw\":\"%08" PRIx32 "\"},",
            p->id, p->name, actor_class.client_name, target_class.client_name, holder_before,
            u_before, float_bits(actor->mob->living.F), float_bits(actor->mob->living.G),
            float_bits(actor->mob->living.aj), float_bits(actor->base.yaw),
            float_bits(target->mob->living.F), float_bits(target->mob->living.G),
            float_bits(target->mob->living.aj), float_bits(target->base.yaw));
    REQUIRE(mc_entity_set_position(actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK &&
            mc_entity_set_position(target, 9.5, 64.5, 8.5) == MC_ENTITY_OK);
    /* Each genuine constructor Entity occurrence protects its permanent box.
     * These saved allocation pointers are not standalone/pool box owners. */
    actor_owned = actor->bounds;
    target_owned = target->bounds;
    /* Declared source fields; no native hold/phase/active metadata is assigned. */
    actor_owned->min_y = 64.0;
    actor_owned->max_y = 66.0;
    target_owned->min_y = monster_double(p->min_y);
    target_owned->max_y = monster_double(p->max_y);
    actor->mob->living.ae = p->cooldown;
    if (p->actor_bounds_null)
        actor->bounds = NULL;
    if (p->target_bounds_null)
        target->bounds = NULL;
    REQUIRE(f->world->world_accesses.count == 0 && !actor->mob->tick_active &&
            !target->mob->tick_active && actor->holds == 1 && target->holds == 1);
    mc_entity_base_fields actor_base = actor->base, target_base = target->base;
    mc_mob_entity_state actor_expected, target_expected;
    memcpy(&actor_expected, actor->mob, sizeof(actor_expected));
    memcpy(&target_expected, target->mob, sizeof(target_expected));
    const bool returned = strcmp(p->projection, "returned") == 0;
    const bool hurt = strcmp(p->projection, "hurt-prefix61") == 0;
    const int32_t pc = returned ? 65 : hurt ? 61 : p->id == 8 ? 14 : p->id == 9 ? 17 : 24;
    if (hurt)
        actor_expected.living.ae = 20;
    const uint64_t actor_seed = actor->random->state, target_seed = target->random->state;
    const uint64_t world_seed = f->world->fields.r->state;
    const uint64_t math_seed = atomic_load(&f->profile->math->state);
    const uint64_t u = atomic_load(&f->profile->uniquifier);
    mc_entity_class_snapshot classes_before, classes_after;
    mc_math_class_state math_before, math_after;
    REQUIRE(mc_entity_classes_get_snapshot(f->entities, &classes_before) == MC_ENTITY_OK &&
            mc_math_classes_state(f->math, &math_before, NULL) == MC_MATH_CLASS_OK);
    fputs("\"before\":", out);
    monster_state(out, f, actor, target);
    float distance;
    const uint32_t bits = (uint32_t)p->bits;
    memcpy(&distance, &bits, sizeof(distance));
    mc_entity *argument = p->target_null ? NULL : target;
    const mc_entity_status status =
        mt_monster_component(actor, f->registry, f->math, NULL, argument, distance, e);
    REQUIRE((status == MC_ENTITY_OK) == returned && e->state.returned == returned &&
            e->state.attack.returned == returned && e->state.attack.stored &&
            !e->state.attack.unseen && e->state.attack.target_identity == (uintptr_t)argument &&
            float_bits(e->state.attack.distance) == bits && e->site.pc == pc &&
            e->site.class_name && strcmp(e->site.class_name, "gz") == 0 &&
            strcmp(e->site.method, "a") == 0 && strcmp(e->site.descriptor, "(Lsn;F)V") == 0);
    REQUIRE(!e->state.environment_returned && !e->state.action_returned &&
            !e->state.behavior_returned && !e->state.cleanup_pending &&
            e->state.concrete_random_calls == 0 && e->state.concrete_world_random_calls == 0 &&
            e->state.sounds == 0 && e->state.particles == 0 && e->state.path_calls == 0);
    if (hurt) {
        REQUIRE(e->native_policy && !e->source_exception && e->exception_class == NULL &&
                e->child == MC_MOB_TICK_CHILD_EFFECT && e->child_site.pc == 0 &&
                strcmp(e->child_site.class_name, "sn") == 0 &&
                strcmp(e->child_site.descriptor, "(Lsn;I)Z") == 0 &&
                e->detail.effect.cause == MC_ENTITY_EFFECT_CAUSE_UNSUPPORTED_VIRTUAL &&
                e->detail.effect.damage_arguments_stored &&
                e->detail.effect.damage_source_identity == (uintptr_t)actor &&
                e->detail.effect.damage_amount == 5 && actor->mob->living.ae == 20 &&
                target->mob->living.Y == 10);
    } else if (!returned) {
        REQUIRE(!e->native_policy && e->source_exception && e->exception_class &&
                strcmp(e->exception_class, "java/lang/NullPointerException") == 0);
    }
    REQUIRE(actor->holds == 1 && target->holds == 1 && !actor->mob->tick_active &&
            !target->mob->tick_active &&
            actor->bounds == (p->actor_bounds_null ? NULL : actor_owned) &&
            target->bounds == (p->target_bounds_null ? NULL : target_owned));
    REQUIRE(mc_mob_tick_cleanup(actor, cleanup) == MC_ENTITY_OK && !cleanup->state.cleanup_pending);
    REQUIRE(actor->bounds == (p->actor_bounds_null ? NULL : actor_owned) &&
            target->bounds == (p->target_bounds_null ? NULL : target_owned) &&
            actor->random->state == actor_seed && target->random->state == target_seed &&
            f->world->fields.r->state == world_seed &&
            atomic_load(&f->profile->math->state) == math_seed &&
            atomic_load(&f->profile->uniquifier) == u && actor->holds == 1 && target->holds == 1);
    REQUIRE(memcmp(&actor_base, &actor->base, sizeof(actor_base)) == 0 &&
            memcmp(&target_base, &target->base, sizeof(target_base)) == 0 &&
            memcmp(&actor_expected, actor->mob, sizeof(actor_expected)) == 0 &&
            memcmp(&target_expected, target->mob, sizeof(target_expected)) == 0);
    REQUIRE(mc_entity_classes_get_snapshot(f->entities, &classes_after) == MC_ENTITY_OK &&
            mc_math_classes_state(f->math, &math_after, NULL) == MC_MATH_CLASS_OK &&
            classes_before.bounds_pool_cursor == classes_after.bounds_pool_cursor &&
            classes_before.next_id == classes_after.next_id &&
            classes_before.live_entities == classes_after.live_entities &&
            classes_before.allocation_bytes == classes_after.allocation_bytes &&
            math_before.holds == math_after.holds);
    fputs(",\"after\":", out);
    monster_state(out, f, actor, target);
    const bool hurt_stored =
        e->child == MC_MOB_TICK_CHILD_EFFECT && e->detail.effect.damage_arguments_stored;
    fprintf(
        out,
        ",\"returned\":%s,\"native_policy\":%s,\"source_exception\":%s,\"pc\":%" PRId32
        ",\"hurt\":%s,\"damage\":%" PRId32 ",\"attacker_identity\":\"%016" PRIxPTR
        "\",\"target_identity\":\"%016" PRIxPTR "\",\"argument_identity\":\"%016" PRIxPTR
        "\",\"effect_source_identity\":\"%016" PRIxPTR "\",\"guard_released\":true,"
        "\"cleanup_returned\":true,\"null_fields_preserved\":true,\"entropy_unchanged\":true}\n",
        e->state.returned ? "true" : "false", e->native_policy ? "true" : "false",
        e->source_exception ? "true" : "false", e->site.pc, hurt_stored ? "true" : "false",
        hurt_stored ? e->detail.effect.damage_amount : 0, (uintptr_t)actor, (uintptr_t)target,
        (uintptr_t)argument, hurt_stored ? e->detail.effect.damage_source_identity : 0);
    passed = true;
done:
    /* Caller Entity occurrences stayed live through the call and cleanup.
     * Only final fixture disposal restores their original allocation pointers;
     * Entity-owned boxes never use the standalone/pool retain/release API. */
    if (actor_owned && actor)
        actor->bounds = actor_owned;
    if (target_owned && target)
        target->bounds = target_owned;
    mc_mob_error retired;
    if (target && mc_mob_retire(&target, &retired) != MC_MOB_OK)
        passed = false;
    if (actor && mc_mob_retire(&actor, &retired) != MC_MOB_OK)
        passed = false;
    free(cleanup);
    free(e);
    return passed;
}

int main(int argc, char **argv) {
    if (argc != 5)
        return 2;
    FILE *table = open_file(argv[4], "rb");
    char path[8192], line[1024];
    int n = snprintf(path, sizeof(path), "%s/native.ndjson", argv[3]);
    FILE *out = n > 0 && (size_t)n < sizeof(path) ? open_file(path, "wb") : NULL;
    if (!table || !out || !fgets(line, sizeof(line), table))
        return 2;
    for (unsigned int i = 0; i < 12; ++i) {
        monster_input input = {0};
        random_fixture *f = calloc(1, sizeof(*f));
        if (!f)
            return 2;
        n = snprintf(path, sizeof(path), "%s/case%u", argv[2], i);
        const bool passed = monster_inputs(table, &input) && input.id == i && n > 0 &&
                            (size_t)n < sizeof(path) && setup(f, argv[1], path) &&
                            monster_one(f, &input, out);
        const bool cleaned = teardown(f);
        free(f);
        if (!passed || !cleaned)
            return 1;
    }
    if (fgets(line, sizeof(line), table) || fclose(table) || fclose(out))
        return 1;
    printf("Monster attack: %u checks;12 genuine protected-body profiles\n", checks);
    return 0;
}
