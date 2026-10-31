/* Protected GB bodies through the same canonical native admission as Mob tick.
 * The included fixture constructs real GAME owners; its unrelated main is unused. */
int mc_creeper_world_fixture_main(int argc, char **argv);
#define main mc_creeper_world_fixture_main
#include "../world/runtime/test_world_random_tick.c"
#undef main
#include "entity/mob/tick/internal.h"

#undef CHECK
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "Creeper check%u line%d: %s\n", checks, __LINE__, #c);                 \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct attack_input {
    unsigned int id, unseen, remote, null_world, target_self, bits, attacking;
    int32_t fuse, previous, direction, powered;
    int32_t expected_fuse, expected_direction;
    unsigned int expected_attacking, expected_dirty, sounds, pc;
    char name[80], projection[24];
} attack_input;

static mc_entity_watch_record *watch_record(mc_entity *p, int32_t index) {
    if (!p || !p->watcher || !p->watcher->map)
        return NULL;
    for (mc_entity_watch_node *n = p->watcher->map->head; n; n = n->next)
        if (n->key == index)
            return n->record;
    return NULL;
}

static bool inputs(FILE *in, attack_input *p) {
    char line[1024];
    CHECK(fgets(line, sizeof(line), in) != NULL);
#ifdef _MSC_VER
    const int fields = sscanf_s(
        line,
        "%u\t%79s\t%u\t%u\t%u\t%u\t%x\t%" SCNd32 "\t%" SCNd32 "\t%" SCNd32 "\t%" SCNd32
        "\t%u\t%" SCNd32 "\t%" SCNd32 "\t%u\t%u\t%u\t%23s\t%u",
        &p->id, p->name, (unsigned int)sizeof(p->name), &p->unseen, &p->remote, &p->null_world,
        &p->target_self, &p->bits, &p->fuse, &p->previous, &p->direction, &p->powered,
        &p->attacking, &p->expected_fuse, &p->expected_direction, &p->expected_attacking,
        &p->expected_dirty, &p->sounds, p->projection, (unsigned int)sizeof(p->projection), &p->pc);
#else
    const int fields =
        sscanf(line,
               "%u\t%79s\t%u\t%u\t%u\t%u\t%x\t%" SCNd32 "\t%" SCNd32 "\t%" SCNd32 "\t%" SCNd32
               "\t%u\t%" SCNd32 "\t%" SCNd32 "\t%u\t%u\t%u\t%23s\t%u",
               &p->id, p->name, &p->unseen, &p->remote, &p->null_world, &p->target_self, &p->bits,
               &p->fuse, &p->previous, &p->direction, &p->powered, &p->attacking, &p->expected_fuse,
               &p->expected_direction, &p->expected_attacking, &p->expected_dirty, &p->sounds,
               p->projection, &p->pc);
#endif
    CHECK(fields == 19);
    CHECK(p->id < 12 && p->unseen < 2 && p->remote < 2 && p->null_world < 2 && p->target_self < 2 &&
          p->attacking < 2);
    return true;
}

static void state(FILE *out, const mc_entity *p) {
    const mc_mob_entity_state *m = p->mob;
    const mc_entity_watch_record *a = watch_record((mc_entity *)p, 16);
    const mc_entity_watch_record *b = watch_record((mc_entity *)p, 17);
    fprintf(out,
            "{\"fuse\":%" PRId32 ",\"previous\":%" PRId32
            ",\"attacking\":%s,\"dead\":%s,\"world_null\":%s,"
            "\"direction\":%d,\"powered\":%d,\"dirty16\":%s,\"dirty17\":%s,"
            "\"dirty\":%s,\"x\":\"%016" PRIx64 "\",\"y\":\"%016" PRIx64 "\",\"z\":\"%016" PRIx64
            "\"}",
            m->creeper_a, m->creeper_b, m->creature_attacking ? "true" : "false",
            p->base.be ? "true" : "false", p->base.world ? "false" : "true", (int)a->byte_value,
            (int)b->byte_value, a->dirty ? "true" : "false", b->dirty ? "true" : "false",
            p->watcher->dirty ? "true" : "false", double_bits(p->base.x), double_bits(p->base.y),
            double_bits(p->base.z));
}

static bool one(random_fixture *f, const attack_input *p, FILE *out) {
    mc_entity *actor = NULL;
    const mc_entity_class_literal *literal = NULL;
    mc_mob_error construction;
    const unsigned int holder_before = atomic_load(&f->profile->holder_phase);
    CHECK((holder_before == MC_JAVA_RANDOM_HOLDER_COLD && f->profile->math == NULL) ||
          (holder_before == MC_JAVA_RANDOM_HOLDER_READY && f->profile->math != NULL));
    /* Source's observation helper prepares its holder with Class.forName.
     * Native has no no-draw prepare API. Retain its actual independent COLD
     * context, with no invented earlier stream or extra preparatory draw. */
    const bool math_cold = holder_before == MC_JAVA_RANDOM_HOLDER_COLD;
    const uint64_t math_before = math_cold ? 0 : atomic_load(&f->profile->math->state);
    const uint64_t u_before = atomic_load(&f->profile->uniquifier);
    CHECK(mc_entity_class_literal_get(f->entities, MC_ENTITY_CLASS_KIND_CREEPER, &literal) ==
              MC_ENTITY_OK &&
          mc_mob_allocate(f->entities, f->registry, literal, &actor, &construction) == MC_MOB_OK &&
          mc_mob_construct(actor, f->world, &construction) == MC_MOB_OK);
    CHECK(actor && actor->phase == MC_ENTITY_READY && actor->mob && !actor->local_player);
    CHECK(atomic_load(&f->profile->holder_phase) == MC_JAVA_RANDOM_HOLDER_READY &&
          f->profile->math != NULL);
    /* Pinned JDK17 Random.seedUniquifier: ldc2_w8, then lmul11. */
    uint64_t expected_u = u_before * UINT64_C(1181783497276652981);
    if (math_cold)
        expected_u *= UINT64_C(1181783497276652981);
    CHECK(atomic_load(&f->profile->uniquifier) == expected_u);
    mc_entity_watch_record *a = watch_record(actor, 16), *b = watch_record(actor, 17);
    CHECK(a && b && a->type == 0 && b->type == 0);
    fprintf(out, "{\"case\":%u,\"scenario\":\"%s\",\"constructor\":{\"raw\":", p->id, p->name);
    state(out, actor);
    fprintf(out,
            ",\"F\":\"%08" PRIx32 "\",\"G\":\"%08" PRIx32 "\",\"aj\":\"%08" PRIx32
            "\",\"yaw\":\"%08" PRIx32 "\",\"own_seed\":\"%012" PRIx64 "\",\"math_before\":",
            float_bits(actor->mob->living.F), float_bits(actor->mob->living.G),
            float_bits(actor->mob->living.aj), float_bits(actor->base.yaw), actor->random->state);
    if (math_cold)
        fputs("null", out);
    else
        fprintf(out, "\"%012" PRIx64 "\"", math_before);
    fprintf(out,
            ",\"math_holder_before\":\"%s\",\"math_holder_after\":\"ready\",\"math_after\":\"%"
            "012" PRIx64 "\",\"U_before\":\"%016" PRIx64 "\",\"U_after\":\"%016" PRIx64
            "\",\"rebound\":false},\"before\":",
            math_cold ? "cold" : "ready", atomic_load(&f->profile->math->state), u_before,
            atomic_load(&f->profile->uniquifier));
    CHECK(mc_entity_set_position(actor, 8.5, 64.5, 8.5) == MC_ENTITY_OK);
    /* Declared mutable Source inputs, never guard/hold/phase metadata. */
    actor->mob->creeper_a = p->fuse;
    actor->mob->creeper_b = p->previous;
    actor->mob->creature_attacking = p->attacking != 0;
    a->byte_value = (int8_t)p->direction;
    b->byte_value = (int8_t)p->powered;
    a->dirty = b->dirty = actor->watcher->dirty = false;
    f->world->fields.B = p->remote != 0;
    if (p->null_world)
        actor->base.world = NULL;
    CHECK(f->world->world_accesses.count == 0 && !actor->mob->tick_active && actor->holds == 1);
    state(out, actor);
    const uint64_t own = actor->random->state, world = f->world->fields.r->state;
    const uint64_t math = atomic_load(&f->profile->math->state);
    const uint64_t u = atomic_load(&f->profile->uniquifier);
    const mc_entity_base_fields before_base = actor->base;
    mc_mob_entity_state expected_mob = *actor->mob;
    expected_mob.creeper_a = p->expected_fuse;
    expected_mob.creature_attacking = p->expected_attacking != 0;
    mc_entity_class_snapshot before_classes, after_classes;
    mc_math_class_state before_math, after_math;
    CHECK(mc_entity_classes_get_snapshot(f->entities, &before_classes) == MC_ENTITY_OK &&
          mc_math_classes_state(f->math, &before_math, NULL) == MC_MATH_CLASS_OK);
    mc_mob_tick_error *e = malloc(sizeof(*e));
    CHECK(e != NULL);
    float distance;
    const uint32_t distance_bits = (uint32_t)p->bits;
    memcpy(&distance, &distance_bits, sizeof(distance));
    const mc_entity_status status =
        mt_creeper_component(actor, f->registry, f->math, NULL, p->target_self ? actor : NULL,
                             distance, p->unseen != 0, e);
    const bool returned = strcmp(p->projection, "return") == 0;
    CHECK((status == MC_ENTITY_OK) == returned && e->state.returned == returned &&
          e->state.attack.stored && e->state.attack.returned == returned &&
          e->state.attack.unseen == (p->unseen != 0) &&
          e->state.attack.target_identity == (uintptr_t)(p->target_self ? actor : NULL) &&
          float_bits(e->state.attack.distance) == p->bits && e->site.pc == (int32_t)p->pc);
    CHECK(!e->state.environment_returned && !e->state.action_returned &&
          !e->state.behavior_returned && !e->state.cleanup_pending &&
          e->state.concrete_random_calls == 0 && e->state.concrete_world_random_calls == 0 &&
          e->state.sounds == p->sounds && e->state.particles == 0);
    CHECK(actor->mob->creeper_a == p->expected_fuse && actor->mob->creeper_b == p->previous &&
          a->byte_value == p->expected_direction && b->byte_value == p->powered &&
          actor->mob->creature_attacking == (p->expected_attacking != 0) &&
          a->dirty == (p->expected_dirty != 0) && !b->dirty &&
          actor->watcher->dirty == (p->expected_dirty != 0) && !actor->base.be);
    CHECK(actor->random->state == own && f->world->fields.r->state == world &&
          atomic_load(&f->profile->math->state) == math &&
          atomic_load(&f->profile->uniquifier) == u && actor->holds == 1 &&
          !actor->mob->tick_active && actor->base.world == (p->null_world ? NULL : f->world));
    CHECK(memcmp(&before_base, &actor->base, sizeof(before_base)) == 0 &&
          memcmp(&expected_mob, actor->mob, sizeof(expected_mob)) == 0);
    CHECK(mc_entity_classes_get_snapshot(f->entities, &after_classes) == MC_ENTITY_OK &&
          mc_math_classes_state(f->math, &after_math, NULL) == MC_MATH_CLASS_OK &&
          before_math.holds == after_math.holds &&
          before_classes.bounds_pool_cursor == after_classes.bounds_pool_cursor);
    const bool explosion = strcmp(p->projection, "explosion") == 0;
    CHECK(e->state.explosion.stored == explosion);
    if (explosion) {
        CHECK(e->native_policy && !e->source_exception && e->exception_class == NULL &&
              e->child_site.class_name && strcmp(e->child_site.class_name, "fd") == 0 &&
              strcmp(e->child_site.descriptor, "(Lsn;DDDF)Lqx;") == 0 && e->child_site.pc == 0 &&
              e->state.explosion.invoke_reached && !e->state.explosion.returned &&
              e->state.explosion.world_identity == (uintptr_t)f->world &&
              e->state.explosion.source_identity == (uintptr_t)actor &&
              e->state.explosion.x == actor->base.x && e->state.explosion.y == actor->base.y &&
              e->state.explosion.z == actor->base.z &&
              e->state.explosion.radius == (p->powered == 1 ? 6.0f : 3.0f));
    } else if (!returned) {
        CHECK(!e->native_policy && e->source_exception && e->exception_class &&
              strcmp(e->exception_class, "java/lang/NullPointerException") == 0);
    }
    fputs(",\"after\":", out);
    state(out, actor);
    fprintf(out,
            ",\"returned\":%s,\"native_policy\":%s,\"source_exception\":%s,\"pc\":%d,"
            "\"sounds\":%zu,\"explosion\":%s,\"radius_bits\":\"%08" PRIx32
            "\",\"actor_identity\":\"%016" PRIxPTR "\",\"world_identity\":\"%016" PRIxPTR
            "\",\"entropy_unchanged\":true,"
            "\"guard_released\":true,\"null_input_preserved\":true}\n",
            returned ? "true" : "false", e->native_policy ? "true" : "false",
            e->source_exception ? "true" : "false", e->site.pc, e->state.sounds,
            explosion ? "true" : "false", float_bits(e->state.explosion.radius), (uintptr_t)actor,
            (uintptr_t)f->world);
    free(e);
    /* Source NULL remains through the real component cleanup above. Only final
     * fixture disposal restores its already-owned World occurrence. */
    if (p->null_world)
        actor->base.world = f->world;
    CHECK(mc_mob_retire(&actor, &construction) == MC_MOB_OK && actor == NULL);
    return true;
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
        attack_input input = {0};
        random_fixture *f = calloc(1, sizeof(*f));
        if (!f)
            return 2;
        n = snprintf(path, sizeof(path), "%s/case%u", argv[2], i);
        bool passed = inputs(table, &input) && input.id == i && n > 0 && (size_t)n < sizeof(path) &&
                      setup(f, argv[1], path) && one(f, &input, out);
        if (!teardown(f)) {
            fputs("Creeper cleanup refused; actual owners remain live\n", stderr);
            return 1;
        }
        free(f);
        if (!passed)
            return 1;
    }
    if (fgets(line, sizeof(line), table) || fclose(table) || fclose(out))
        return 1;
    printf("Creeper attack: %u checks;12 genuine protected-body profiles\n", checks);
    return 0;
}
