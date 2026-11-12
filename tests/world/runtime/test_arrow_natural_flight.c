/* NEW natural-flight cohort. The old permanent expected-RED opportunity is untouched. */
#define MC_WORLD_ENTITY_TICK_MAIN mc_arrow_dispatch_unused_fixture_main
#include "test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN

#include "../../../src/entity/arrow/internal.h"
#include "../../../src/world/runtime/collision/entities_internal.h"
#include "mc/core/math/classes.h"
#include "mc/entity/arrow/construction.h"

static size_t nf_calls;

static uint64_t ad_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool nf_first_air(fixture *f, mc_entity *arrow) {
    bool ok = false;
    mc_world_runtime *world = f->worlds[0];
    const mc_java_random random_before = *arrow->random;
    const uint64_t world_random_before = world->fields.r->state;
    const size_t arrow_holds_before = arrow->holds;
    const size_t world_bindings_before = world->persistent_bindings;
    const uint64_t initial_x = ad_bits(8.5);
    const double speed = (double)0.6f;
    /* Literal SL common-tail operands, independently derived from source:
     * position uses the pre-drag velocity, then .99f drag and .03f gravity. */
    volatile double expected_x = 8.5 + speed;
    volatile double expected_vx = speed * (double)0.99f;
    volatile double expected_vy = 0.0 - (double)0.03f;

    CHECK(arrow->base.bt == 0 && arrow->arrow->fields.k == 0 && !arrow->arrow->fields.i &&
          !arrow->base.be && !arrow->base.bx && ad_bits(arrow->base.x) == initial_x &&
          ad_bits(arrow->base.y) == ad_bits(66.0) && ad_bits(arrow->base.z) == initial_x &&
          ad_bits(arrow->base.velocity_x) == ad_bits(speed) &&
          ad_bits(arrow->base.velocity_y) == ad_bits(0.0) &&
          ad_bits(arrow->base.velocity_z) == ad_bits(0.0));
    CHECK(world->fields.b->count == 1 && world->fields.b->items[0] == arrow &&
          f->chunks[0]->entities[4].count == 1 && f->chunks[0]->entities[4].items[0] == arrow &&
          arrow->base.bF && arrow->base.bG == 0 && arrow->base.bH == 4 && arrow->base.bI == 0);

    /* Actual FD.g loaded-Entity loop through the NEW default Arrow dispatch. */
    ++nf_calls;
    const mc_world_storage_status status =
        mc_world_runtime_tick_entities(world, &tick_context, NULL, tick_error);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr,
                "Arrow World dispatch refused: status=%d method=%s pc=%u update=%u "
                "loop=%u operation=%s\n",
                (int)status, tick_error->source_method == NULL ? "-" : tick_error->source_method,
                tick_error->bytecode, tick_error->update_bytecode, tick_error->loop_bytecode,
                tick_error->operation == NULL ? "-" : tick_error->operation);
    CHECK(status == MC_WORLD_STORAGE_OK);

    CHECK(ad_bits(arrow->base.x) == UINT64_C(0x4022333334000000) &&
          ad_bits(arrow->base.velocity_x) == UINT64_C(0x3fe3020c5978d500) &&
          ad_bits(arrow->base.velocity_y) == UINT64_C(0xbf9eb851e0000000));
    CHECK(arrow->base.bt == 1 && arrow->arrow->fields.k == 1 && !arrow->arrow->fields.i &&
          !arrow->base.be && !arrow->base.bx && !arrow->base.private_c);
    CHECK(ad_bits(arrow->base.x) == ad_bits(expected_x) &&
          ad_bits(arrow->base.y) == ad_bits(66.0) && ad_bits(arrow->base.z) == initial_x &&
          ad_bits(arrow->base.velocity_x) == ad_bits(expected_vx) &&
          ad_bits(arrow->base.velocity_y) == ad_bits(expected_vy) &&
          ad_bits(arrow->base.velocity_z) == ad_bits(0.0));
    CHECK(arrow->random->state == random_before.state &&
          arrow->random->have_next_next_gaussian == random_before.have_next_next_gaussian &&
          ad_bits(arrow->random->next_next_gaussian) == ad_bits(random_before.next_next_gaussian) &&
          world->fields.r->state == world_random_before);
    CHECK(world->fields.b->count == 1 && world->fields.b->items[0] == arrow &&
          f->chunks[0]->entities[4].count == 1 && f->chunks[0]->entities[4].items[0] == arrow &&
          arrow->base.bF && arrow->holds == arrow_holds_before &&
          world->persistent_bindings == world_bindings_before &&
          tick_error->child == MC_WORLD_ENTITY_TICK_CHILD_NONE);
    ok = true;
cleanup:
    return ok;
}

#include "../../../src/entity/living/fields_internal.h"
#include "mc/entity/mob/construction.h"

typedef struct nf_fixture {
    fixture *world;
    mc_math_classes *math;
    mc_entity *arrow, *victim;
    bool owns_error;
    mc_world_access *victim_access;
    size_t victim_sounds, victim_impacts;
    bool victim_sound_valid;
} nf_fixture;

static mc_world_storage_status nf_victim_sound(void *context, mc_world_runtime *world,
                                               const mc_registry_string *name, double x, double y,
                                               double z, float volume, float pitch,
                                               mc_world_storage_error *error) {
    nf_fixture *s = context;
    const bool impact = s->victim_sounds != 0;
    const char *expected = impact ? "random.drr" : "mob.pig";
    mc_registry_string_view text = {0};
    mc_arrow_tick_state state = {0};
    s->victim_sound_valid &=
        world == s->world->worlds[0] && s->victim_sounds < 2 && volume == 1.f && pitch > 0.f &&
        mc_registry_string_read(name, &text) && text.length == strlen(expected) &&
        mc_entity_living_fields(s->victim)->Y == 6 && !s->arrow->base.be &&
        s->arrow->arrow->tick_call != NULL &&
        mc_arrow_tick_inspect(s->arrow->arrow->tick_call, &state) && state.active &&
        state.hurt_invoked && state.hurt_returned == impact && state.hurt_continued == impact &&
        (impact ? state.hurt_result && state.hurt_bytes == 0 : state.hurt_bytes != 0);
    for (size_t i = 0; s->victim_sound_valid && i < text.length; ++i)
        s->victim_sound_valid &= text.units[i] == (uint16_t)expected[i];
    const mc_entity *p = impact ? s->arrow : s->victim;
    s->victim_sound_valid &=
        x == p->base.x && y == p->base.y - (double)p->base.offset && z == p->base.z;
    ++s->victim_sounds;
    s->victim_impacts += impact;
    *error = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

/* Includable by a separate genuine SDK-cleanup control. A supplied error is
 * the actual caller output from the first call, never a copied owning record. */
static bool nf_setup(nf_fixture *s, const char *root, mc_world_entity_tick_error *output) {
    bool ok = false;
    s->world = calloc(1, sizeof(*s->world));
    s->owns_error = output == NULL;
    tick_error = output == NULL ? calloc(1, sizeof(*tick_error)) : output;
    CHECK(s->world != NULL && tick_error != NULL &&
          mc_math_classes_create(SIZE_MAX, &s->math, NULL) == MC_MATH_CLASS_OK &&
          mc_math_classes_ensure(s->math, NULL) == MC_MATH_CLASS_OK);
    const mc_math_trig *table = mc_math_classes_table(s->math);
    CHECK(table != NULL && tick_setup_configured(s->world, root, 64, table) &&
          tick_presence(s->world));
    tick_context.player.travel.code_registry = s->world->registry[0];
    tick_context.player.travel.trig = table;
    tick_context.player.travel.math = s->math;
    tick_context.math = s->math;
    tick_context.arrow_code_classes = s->world->classes[0];
    ok = true;
cleanup:
    return ok;
}

static bool nf_construct(nf_fixture *s) {
    bool ok = false, inserted = false;
    mc_entity_error allocation = {0};
    mc_arrow_error arrow_error;
    mc_world_entity_error publication;
    CHECK(mc_arrow_allocate(s->world->entity_classes[0], &s->arrow, &allocation) == MC_ENTITY_OK &&
          mc_arrow_construct_position(s->arrow, s->world->worlds[0], 8.5, 66.0, 8.5,
                                      &arrow_error) == MC_ENTITY_OK &&
          mc_arrow_heading(s->arrow, s->math, 1.0, 0.0, 0.0, 0.6f, 0.0f, &arrow_error) ==
              MC_ENTITY_OK &&
          mc_world_runtime_insert_entity(s->world->worlds[0], s->arrow, &inserted, &publication) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    ok = true;
cleanup:
    return ok;
}

/* Genuine authored placement BEFORE Arrow construction. These are actual
 * complete quiet setters, not Source plane/field repairs or returned values. */
static bool nf_prepare_block(nf_fixture *s) {
    bool ok = false;
    mc_world_mutation_error *error = calloc(1, sizeof(*error));
    CHECK(error != NULL);
    for (int32_t y = 65; y <= 66; ++y) {
        bool changed = false;
        CHECK(mc_world_runtime_block_metadata_set_quiet(s->world->worlds[0], 10, y, 8, 1, 0,
                                                        &changed, error) == MC_WORLD_STORAGE_OK &&
              changed);
    }
    ok = true;
cleanup:
    free(error);
    return ok;
}

static bool nf_tick(nf_fixture *s, bool whole_world) {
    ++nf_calls;
    const mc_world_storage_status status =
        whole_world
            ? mc_world_runtime_tick_entities(s->world->worlds[0], &tick_context, NULL, tick_error)
            : mc_world_runtime_tick_entity(s->world->worlds[0], s->arrow, &tick_context, NULL,
                                           tick_error);
    if (status != MC_WORLD_STORAGE_OK)
        fprintf(stderr, "Natural Arrow status=%u operation=%s\n", (unsigned int)status,
                tick_error->operation == NULL ? "-" : tick_error->operation);
    return status == MC_WORLD_STORAGE_OK;
}

static bool nf_more_air(nf_fixture *s) {
    bool ok = false;
    mc_entity *p = s->arrow;
    for (int32_t tick = 2; tick <= 8; ++tick) {
        const mc_java_random random = *p->random;
        const size_t holds = p->holds, bindings = s->world->worlds[0]->persistent_bindings;
        volatile double x = p->base.x + p->base.velocity_x;
        volatile double y = p->base.y + p->base.velocity_y;
        volatile double vx = p->base.velocity_x * (double).99f;
        volatile double vy = p->base.velocity_y * (double).99f - (double).03f;
        CHECK(nf_tick(s, true) && p->base.bt == tick && p->arrow->fields.k == tick &&
              ad_bits(p->base.x) == ad_bits(x) && ad_bits(p->base.y) == ad_bits(y) &&
              ad_bits(p->base.velocity_x) == ad_bits(vx) &&
              ad_bits(p->base.velocity_y) == ad_bits(vy) && !p->arrow->fields.i && !p->base.be &&
              p->arrow->tick_call == NULL && p->random->state == random.state &&
              p->random->have_next_next_gaussian == random.have_next_next_gaussian &&
              ad_bits(p->random->next_next_gaussian) == ad_bits(random.next_next_gaussian) &&
              p->holds == holds && s->world->worlds[0]->persistent_bindings == bindings &&
              tick_error->child == MC_WORLD_ENTITY_TICK_CHILD_NONE);
    }
    ok = true;
cleanup:
    return ok;
}

static bool nf_block(nf_fixture *s) {
    bool ok = false;
    mc_entity *p = s->arrow;
    CHECK(nf_first_air(s->world, p) && nf_tick(s, true) && !p->arrow->fields.i);
    CHECK(nf_tick(s, true) && p->base.bt == 3 && p->arrow->fields.k == 3 && p->arrow->fields.i &&
          p->arrow->fields.b == 7 && p->arrow->fields.g == 1 && p->arrow->fields.h == 0 &&
          p->arrow->fields.d == 10 && p->arrow->fields.e == 65 && p->arrow->fields.f == 8 &&
          p->arrow->tick_call == NULL && tick_error->child == MC_WORLD_ENTITY_TICK_CHILD_NONE);
    const uint64_t x = ad_bits(p->base.x), y = ad_bits(p->base.y);
    const uint64_t vx = ad_bits(p->base.velocity_x), vy = ad_bits(p->base.velocity_y);
    const mc_java_random random = *p->random;
    for (int32_t count = 1; count <= 2; ++count)
        CHECK(nf_tick(s, true) && p->base.bt == 3 + count && p->arrow->fields.j == count &&
              p->arrow->fields.b == 7 - count && p->arrow->fields.i && p->arrow->fields.k == 3 &&
              ad_bits(p->base.x) == x && ad_bits(p->base.y) == y &&
              ad_bits(p->base.velocity_x) == vx && ad_bits(p->base.velocity_y) == vy &&
              p->random->state == random.state && p->arrow->tick_call == NULL);
    ok = true;
cleanup:
    return ok;
}

static bool nf_victim(nf_fixture *s) {
    bool ok = false, inserted = false;
    mc_mob_error error;
    const mc_entity_class_literal *kind = NULL;
    CHECK(mc_entity_class_literal_get(s->world->entity_classes[0], MC_ENTITY_CLASS_KIND_PIG,
                                      &kind) == MC_ENTITY_OK &&
          mc_mob_allocate(s->world->entity_classes[0], s->world->registry[0], kind, &s->victim,
                          &error) == MC_MOB_OK &&
          mc_mob_construct(s->victim, s->world->worlds[0], &error) == MC_MOB_OK &&
          mc_entity_set_position(s->victim, 9.6, 65.75, 8.5) == MC_ENTITY_OK &&
          mc_world_runtime_insert_entity(s->world->worlds[0], s->victim, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    CHECK(mc_entity_living_fields(s->victim)->Y == 10);
    mc_java_random expected_victim = *s->victim->random, expected_arrow = *s->arrow->random;
    (void)mc_java_random_next_float(&expected_victim);
    (void)mc_java_random_next_float(&expected_victim);
    (void)mc_java_random_next_float(&expected_arrow);
    const size_t ah = s->arrow->holds, vh = s->victim->holds;
    const size_t bindings = s->world->worlds[0]->persistent_bindings;
    const size_t code = s->world->classes[0]->native_borrows;
    mc_world_runtime *const world = s->world->worlds[0];
    const mc_world_reference_list *const source_r = world->fields.R;
    const size_t local_leases = world->local_leases;
    CHECK(mc_world_nearby_owned_valid(world) && source_r != NULL &&
          source_r == world->nearby_entities.list);
    s->victim_sound_valid = true;
    const mc_world_access_callbacks callbacks = {.sound = nf_victim_sound};
    CHECK(mc_world_access_create(&callbacks, s, &s->victim_access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(world, s->victim_access, NULL) == MC_WORLD_STORAGE_OK);
    size_t occurrences_before = 0;
    for (size_t i = 0; i < world->nearby_entities.count; ++i)
        if (world->nearby_entities.entries[i] == s->victim)
            ++occurrences_before;
    ++nf_calls;
    CHECK(mc_world_runtime_tick_entity(s->world->worlds[0], s->arrow, &tick_context, NULL,
                                       tick_error) == MC_WORLD_STORAGE_OK &&
          tick_error->child == MC_WORLD_ENTITY_TICK_CHILD_NONE &&
          s->arrow->arrow->tick_call == NULL);
    const mc_living_entity_fields *living = mc_entity_living_fields(s->victim);
    CHECK(living->Y == 6 && living->Z == 10 && living->au == 4 && living->aa == 10 &&
          living->ab == 10 && living->al == 1.5f && !living->ah &&
          s->victim->base.by == living->E && s->victim_sound_valid && s->victim_sounds == 2 &&
          s->victim_impacts == 1 && s->arrow->base.be && !s->arrow->arrow->fields.i &&
          s->arrow->base.bt == 1 && s->arrow->arrow->fields.k == 1 &&
          ad_bits(s->arrow->base.x) == UINT64_C(0x4022333334000000) &&
          ad_bits(s->arrow->base.velocity_x) == UINT64_C(0x3fe3020c5978d500) &&
          ad_bits(s->arrow->base.velocity_y) == UINT64_C(0xbf9eb851e0000000) &&
          s->victim->random->state == expected_victim.state &&
          s->victim->random->have_next_next_gaussian == expected_victim.have_next_next_gaussian &&
          ad_bits(s->victim->random->next_next_gaussian) ==
              ad_bits(expected_victim.next_next_gaussian) &&
          s->arrow->random->state == expected_arrow.state &&
          s->arrow->random->have_next_next_gaussian == expected_arrow.have_next_next_gaussian &&
          ad_bits(s->arrow->random->next_next_gaussian) ==
              ad_bits(expected_arrow.next_next_gaussian));
    CHECK(mc_world_entity_tick_error_retire(tick_error));
    CHECK(mc_world_nearby_owned_valid(world) && world->fields.R == source_r &&
          world->nearby_entities.list == source_r);
    size_t occurrences_after = 0;
    for (size_t i = 0; i < world->nearby_entities.count; ++i)
        if (world->nearby_entities.entries[i] == s->victim)
            ++occurrences_after;
    /* Source World.R retains its actual selected occurrences after the local
     * query view retires. Account only those measured ledger holds; do not
     * clear/requery R or mistake its surviving Pig hold for a frame leak. */
    CHECK(world->nearby_entities.count == 1 && source_r->count == 1 && occurrences_after == 1 &&
          world->nearby_entities.entries[0] == s->victim && source_r->items[0] == s->victim &&
          world->local_leases == local_leases);
    CHECK(s->arrow->arrow->tick_call == NULL && s->arrow->holds == ah && vh >= occurrences_before &&
          s->victim->holds >= vh - occurrences_before &&
          s->victim->holds - (vh - occurrences_before) == occurrences_after &&
          s->world->worlds[0]->persistent_bindings == bindings &&
          s->world->classes[0]->native_borrows == code && living->Y == 6);
    ok = true;
cleanup:
    return ok;
}

static bool nf_cleanup(nf_fixture *s) {
    if (tick_error != NULL && !mc_world_entity_tick_error_retire(tick_error))
        return false;
    if (s->victim_access != NULL) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(s->world->worlds[0], s->victim_access, &removed,
                                                 NULL) != MC_WORLD_STORAGE_OK ||
            !removed || mc_world_access_free(s->victim_access, NULL) != MC_WORLD_STORAGE_OK)
            return false;
        s->victim_access = NULL;
    }
    mc_entity_release(s->arrow);
    s->arrow = NULL;
    mc_entity_release(s->victim);
    s->victim = NULL;
    if (!tick_cleanup(s->world))
        return false;
    s->world = NULL;
    if (mc_math_classes_release(&s->math, NULL) != MC_MATH_CLASS_OK)
        return false;
    if (s->owns_error)
        free(tick_error);
    tick_error = NULL;
    return true;
}

#ifndef MC_ARROW_NATURAL_FLIGHT_MAIN
#define MC_ARROW_NATURAL_FLIGHT_MAIN main
#endif
int MC_ARROW_NATURAL_FLIGHT_MAIN(int argc, char **argv) {
    int result = 1;
    nf_fixture s = {0};
    char root[8192];
    if (argc != 3 ||
        (strcmp(argv[1], "air-first") != 0 && strcmp(argv[1], "air-multi") != 0 &&
         strcmp(argv[1], "block") != 0 && strcmp(argv[1], "victim-frontier") != 0) ||
        !decode_path(argv[2], root, sizeof(root)))
        return 2;
    printf("ARROW_NATURAL_FLIGHT_BEGIN schema=1 mode=%s\n", argv[1]);
    fflush(stdout);
    CHECK(nf_setup(&s, root, NULL));
    if (strcmp(argv[1], "block") == 0)
        CHECK(nf_prepare_block(&s));
    CHECK(nf_construct(&s));
    printf("ARROW_NATURAL_FLIGHT_CASE %s BEGIN\n", argv[1]);
    fflush(stdout);
    if (strcmp(argv[1], "block") == 0)
        CHECK(nf_block(&s));
    else if (strcmp(argv[1], "victim-frontier") == 0)
        CHECK(nf_victim(&s));
    else {
        CHECK(nf_first_air(s.world, s.arrow));
        if (strcmp(argv[1], "air-multi") == 0)
            CHECK(nf_more_air(&s));
    }
    result = 0;
cleanup:
    if (!nf_cleanup(&s)) {
        fputs("Natural Arrow cleanup refused; surviving owners retained\n", stderr);
        return 1;
    }
    if (result == 0) {
        printf("ARROW_NATURAL_FLIGHT_CASE %s END calls=%zu\n", argv[1], nf_calls);
        puts("ARROW_NATURAL_FLIGHT_OPEN full-victim-damage/original/render/pickup/rider");
        printf("PASS: %u actual default Arrow flight checks;mode=%s\n", checks, argv[1]);
    }
    return result;
}
