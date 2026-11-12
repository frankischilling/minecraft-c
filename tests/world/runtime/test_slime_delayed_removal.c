/* Genuine cached World and concrete Mob runner. Death age advances through
 * twenty-one real ticks after fatal damage; no death-age store skips them. */
int slime_delayed_unused_world_main(int, char **);
#define MC_WORLD_ENTITY_TICK_MAIN slime_delayed_unused_world_main
#include "test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN
#include "entity/mob/internal.h"
#include "mc/entity/mob/tick.h"
#include "world/runtime/world_access_internal.h"

typedef struct delayed_fixture {
    fixture *world;
    mc_entity *parent;
    mc_math_classes *math;
    mc_world_access *access;
    uint64_t occurrence;
    mc_entity_effect_error damage;
    mc_mob_tick_error tick;
    size_t children, particles, ticks;
    bool valid;
} delayed_fixture;

static mc_world_storage_status delayed_added(void *context, mc_world_runtime *world,
                                             mc_entity *child, mc_world_storage_error *error) {
    delayed_fixture *s = context;
    const size_t i = s->children++;
    mc_mob_state state = {0};
    s->valid &= i < 4 && world == s->world->worlds[0] && child != s->parent &&
                mc_mob_inspect(child, &state) && state.kind == MC_ENTITY_CLASS_KIND_SLIME &&
                state.watcher_16 == 1 && state.living.Y == 1 && !s->parent->base.be &&
                s->parent->mob->living.ad == 21 && s->particles == 0 &&
                child->base.x == s->parent->base.x + (i % 2 == 0 ? -.25 : .25) &&
                child->base.y == s->parent->base.y + .5 &&
                child->base.z == s->parent->base.z + (i / 2 == 0 ? -.25 : .25) &&
                world->fields.b->count == i + 2 && world->fields.b->items[i + 1] == child;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status delayed_particle(void *context, mc_world_runtime *world,
                                                const mc_registry_string *name, double x, double y,
                                                double z, double vx, double vy, double vz,
                                                mc_world_storage_error *error) {
    delayed_fixture *s = context;
    mc_registry_string_view text = {0};
    static const uint16_t expected[] = {'e', 'x', 'p', 'l', 'o', 'd', 'e'};
    (void)x;
    (void)y;
    (void)z;
    (void)vx;
    (void)vy;
    (void)vz;
    ++s->particles;
    s->valid &= world == s->world->worlds[0] && mc_registry_string_read(name, &text) &&
                text.length == COUNT(expected) &&
                memcmp(text.units, expected, sizeof expected) == 0 && s->parent->base.be &&
                s->children == 4 && s->parent->mob->living.ad == 21 &&
                !mc_entity_effect_has_pending(s->parent);
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool delayed_run(delayed_fixture *s) {
    bool ok = false, inserted = false, accepted = false;
    const mc_entity_class_literal *kind = NULL;
    mc_mob_error mob = {0};
    CHECK(mc_entity_class_literal_get(s->world->entity_classes[0], MC_ENTITY_CLASS_KIND_SLIME,
                                      &kind) == MC_ENTITY_OK &&
          mc_mob_allocate(s->world->entity_classes[0], NULL, kind, &s->parent, &mob) == MC_MOB_OK &&
          mc_mob_construct(s->parent, s->world->worlds[0], &mob) == MC_MOB_OK &&
          mc_mob_slime_set_size(s->parent, 2, &mob) == MC_MOB_OK &&
          mc_entity_set_position(s->parent, 8.5, 64, 8.5) == MC_ENTITY_OK &&
          mc_world_runtime_insert_entity(s->world->worlds[0], s->parent, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    CHECK(mc_entity_effect_damage_with_math(s->parent, s->world->registry[0], s->math, NULL, 4,
                                            &accepted, &s->damage) == MC_ENTITY_OK &&
          accepted && s->parent->mob->living.Y == 0 && s->parent->mob->living.ad == 0 &&
          !s->parent->base.be && !mc_entity_effect_has_pending(s->parent));
    const mc_world_access_callbacks callbacks = {.entity_added = delayed_added,
                                                 .particle = delayed_particle};
    CHECK(mc_world_access_create(&callbacks, s, &s->access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_access_add_receipted(s->world->worlds[0], s->access, &s->occurrence, NULL) ==
              MC_WORLD_STORAGE_OK);
    for (size_t i = 0; i < 21; ++i) {
        const mc_entity_status status =
            mc_mob_tick(s->parent, s->world->registry[0], s->math, NULL, &s->tick);
        if (status != MC_ENTITY_OK)
            fprintf(stderr, "Slime delayed tick%zu status%u site=%s.%s:%d op=%s\n", i + 1,
                    (unsigned int)status, s->tick.site.class_name, s->tick.site.method,
                    s->tick.site.pc, s->tick.operation == NULL ? "none" : s->tick.operation);
        CHECK(status == MC_ENTITY_OK && s->parent->mob->living.ad == (int32_t)i + 1 && s->valid &&
              !mc_entity_effect_has_pending(s->parent));
        ++s->ticks;
        if (i < 20)
            CHECK(!s->parent->base.be && s->children == 0 && s->particles == 0 &&
                  s->world->worlds[0]->fields.b->count == 1);
    }
    CHECK(s->parent->base.be && s->children == 4 && s->particles == 20 &&
          s->world->worlds[0]->fields.b->count == 5 && s->parent->mob->living.Y == 0);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    char root[8192];
    if (argc != 2 || !decode_path(argv[1], root, sizeof root))
        return 2;
    delayed_fixture *s = calloc(1, sizeof *s);
    if (s == NULL)
        return 1;
    s->world = calloc(1, sizeof *s->world);
    s->valid = true;
    mc_entity_effect_error_init(&s->damage);
    mc_mob_tick_error_init(&s->tick);
    const bool passed =
        s->world != NULL && mc_math_classes_create(SIZE_MAX, &s->math, NULL) == MC_MATH_CLASS_OK &&
        mc_math_classes_ensure(s->math, NULL) == MC_MATH_CLASS_OK &&
        tick_setup_configured(s->world, root, 4, mc_math_classes_table(s->math)) && delayed_run(s);
    bool cleaned = mc_mob_tick_error_retire(&s->tick) && mc_entity_effect_error_retire(&s->damage);
    if (cleaned && s->access != NULL)
        cleaned = mc_world_access_detach_occurrence(s->world->worlds[0], s->access, s->occurrence,
                                                    NULL) == MC_WORLD_STORAGE_OK &&
                  mc_world_access_free(s->access, NULL) == MC_WORLD_STORAGE_OK;
    if (cleaned) {
        mc_entity_release(s->parent);
        s->parent = NULL;
        cleaned = tick_cleanup(s->world);
        if (cleaned)
            s->world = NULL;
    }
    if (cleaned)
        cleaned = mc_math_classes_release(&s->math, NULL) == MC_MATH_CLASS_OK;
    printf("SLIME_DELAYED ticks=%zu children=%zu particles=%zu valid=%u cleanup=%u passed=%u\n",
           s->ticks, s->children, s->particles, (unsigned int)s->valid, (unsigned int)cleaned,
           (unsigned int)passed);
    free(s);
    return passed && cleaned ? 0 : 1;
}
