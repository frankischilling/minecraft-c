#include "mc/entity/entity.h"
#include "mc/entity/registry/registry.h"
#include "mc/resources/translation/translation.h"
#include "mc/world/biome/registry.h"
#include "mc/world/runtime/entities.h"

#include "../../../src/block/registry/registry_internal.h"
#include "../../../src/core/java/random_profile_internal.h"
#include "../../../src/entity/classes/classes_internal.h"
#include "../../../src/entity/registry/registry_internal.h"
#include "../../../src/world/runtime/construction_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))
static unsigned int checks;
static bool retirement_failed;
static mc_math_trig trig;
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            goto cleanup;                                                                          \
        }                                                                                          \
    } while (0)
#define VERIFY(c)                                                                                  \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(c)) {                                                                                \
            fprintf(stderr, "%s:%d: failed: %s\n", __FILE__, __LINE__, #c);                        \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

/* Literal expectations from the complete paired jc/ga registrations. Names
 * below check diagnostics/value keys; actual Class assertions use pointers. */
static const struct expected_class {
    mc_entity_class_kind kind;
    int32_t id;
    const char *name, *client, *server;
} expected[] = {{MC_ENTITY_CLASS_KIND_ARROW, 10, "Arrow", "sl", "lp"},
                {MC_ENTITY_CLASS_KIND_SNOWBALL, 11, "Snowball", "by", "be"},
                {MC_ENTITY_CLASS_KIND_ITEM, 1, "Item", "hl", "ez"},
                {MC_ENTITY_CLASS_KIND_PAINTING, 9, "Painting", "qv", "kf"},
                {MC_ENTITY_CLASS_KIND_MOB, 48, "Mob", "ls", "hl"},
                {MC_ENTITY_CLASS_KIND_MONSTER, 49, "Monster", "gz", "ep"},
                {MC_ENTITY_CLASS_KIND_CREEPER, 50, "Creeper", "gb", "ec"},
                {MC_ENTITY_CLASS_KIND_SKELETON, 51, "Skeleton", "fr", "dv"},
                {MC_ENTITY_CLASS_KIND_SPIDER, 52, "Spider", "cn", "bn"},
                {MC_ENTITY_CLASS_KIND_GIANT, 53, "Giant", "nt", "ip"},
                {MC_ENTITY_CLASS_KIND_ZOMBIE, 54, "Zombie", "uz", "ne"},
                {MC_ENTITY_CLASS_KIND_SLIME, 55, "Slime", "uw", "nb"},
                {MC_ENTITY_CLASS_KIND_GHAST, 56, "Ghast", "bp", "aw"},
                {MC_ENTITY_CLASS_KIND_PIG_ZOMBIE, 57, "PigZombie", "ya", "pi"},
                {MC_ENTITY_CLASS_KIND_PIG, 90, "Pig", "wh", "oc"},
                {MC_ENTITY_CLASS_KIND_SHEEP, 91, "Sheep", "dl", "ce"},
                {MC_ENTITY_CLASS_KIND_COW, 92, "Cow", "bx", "bd"},
                {MC_ENTITY_CLASS_KIND_CHICKEN, 93, "Chicken", "ww", "or"},
                {MC_ENTITY_CLASS_KIND_SQUID, 94, "Squid", "xt", "pf"},
                {MC_ENTITY_CLASS_KIND_WOLF, 95, "Wolf", "gi", "eh"},
                {MC_ENTITY_CLASS_KIND_PRIMED_TNT, 20, "PrimedTnt", "qw", "kg"},
                {MC_ENTITY_CLASS_KIND_FALLING_SAND, 21, "FallingSand", "ju", "gl"},
                {MC_ENTITY_CLASS_KIND_MINECART, 40, "Minecart", "yl", "pr"},
                {MC_ENTITY_CLASS_KIND_BOAT, 41, "Boat", "fz", "eb"}};

static const struct {
    mc_entity_class_kind kind;
    const char *client, *server;
} query_only[] = {{MC_ENTITY_CLASS_KIND_ENTITY, "sn", "lq"},
                  {MC_ENTITY_CLASS_KIND_PLAYER, "gs", "em"}};

_Static_assert(COUNT(expected) == 24, "EntityList still registers exactly 24 Classes");
_Static_assert(MC_ENTITY_CLASS_KIND_BOAT == 23 && MC_ENTITY_CLASS_KIND_ENTITY == 24 &&
                   MC_ENTITY_CLASS_KIND_PLAYER == 25,
               "query Classes follow the unchanged registration selectors");
_Static_assert(COUNT(expected) + COUNT(query_only) == (size_t)MC_ENTITY_CLASS_KIND_COUNT,
               "one diagnostic record per admitted Entity Class");

/* Complete expected ancestor sets, independently stated rather than walking
 * the implementation's parent table. Intermediate classes are not literals. */
#define CLASS_BIT(kind) (UINT32_C(1) << MC_ENTITY_CLASS_KIND_##kind)
static const uint32_t expected_ancestors[] = {
    CLASS_BIT(ARROW) | CLASS_BIT(ENTITY),
    CLASS_BIT(SNOWBALL) | CLASS_BIT(ENTITY),
    CLASS_BIT(ITEM) | CLASS_BIT(ENTITY),
    CLASS_BIT(PAINTING) | CLASS_BIT(ENTITY),
    CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(MONSTER) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(CREEPER) | CLASS_BIT(MONSTER) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(SKELETON) | CLASS_BIT(MONSTER) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(SPIDER) | CLASS_BIT(MONSTER) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(GIANT) | CLASS_BIT(MONSTER) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(ZOMBIE) | CLASS_BIT(MONSTER) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(SLIME) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(GHAST) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(PIG_ZOMBIE) | CLASS_BIT(ZOMBIE) | CLASS_BIT(MONSTER) | CLASS_BIT(MOB) |
        CLASS_BIT(ENTITY),
    CLASS_BIT(PIG) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(SHEEP) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(COW) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(CHICKEN) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(SQUID) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(WOLF) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY),
    CLASS_BIT(PRIMED_TNT) | CLASS_BIT(ENTITY),
    CLASS_BIT(FALLING_SAND) | CLASS_BIT(ENTITY),
    CLASS_BIT(MINECART) | CLASS_BIT(ENTITY),
    CLASS_BIT(BOAT) | CLASS_BIT(ENTITY),
    CLASS_BIT(ENTITY),
    CLASS_BIT(PLAYER) | CLASS_BIT(MOB) | CLASS_BIT(ENTITY)};
_Static_assert(COUNT(expected_ancestors) == (size_t)MC_ENTITY_CLASS_KIND_COUNT,
               "complete Class-to-Class expectation matrix");
static const mc_entity_class_kind biome_kinds[] = {
    MC_ENTITY_CLASS_KIND_SPIDER,    MC_ENTITY_CLASS_KIND_ZOMBIE,  MC_ENTITY_CLASS_KIND_SKELETON,
    MC_ENTITY_CLASS_KIND_CREEPER,   MC_ENTITY_CLASS_KIND_SLIME,   MC_ENTITY_CLASS_KIND_SHEEP,
    MC_ENTITY_CLASS_KIND_PIG,       MC_ENTITY_CLASS_KIND_CHICKEN, MC_ENTITY_CLASS_KIND_COW,
    MC_ENTITY_CLASS_KIND_SQUID,     MC_ENTITY_CLASS_KIND_WOLF,    MC_ENTITY_CLASS_KIND_GHAST,
    MC_ENTITY_CLASS_KIND_PIG_ZOMBIE};

typedef struct profile_state {
    uint64_t uniquifier, math_state, gaussian_bits;
    unsigned int holder;
    mc_java_math_random_owner *math;
    bool gaussian, clock_ready;
    int64_t frequency, initial_sample;
    size_t bytes, limit;
} profile_state;

static profile_state profile_get(const mc_java_random_profile *p) {
    profile_state s = {.uniquifier = atomic_load_explicit(&p->uniquifier, memory_order_relaxed),
                       .holder = atomic_load_explicit(&p->holder_phase, memory_order_acquire),
                       .math = p->math,
                       .clock_ready = p->clock.initialized,
                       .frequency = p->clock.frequency,
                       .initial_sample = p->clock.initial_sample,
                       .bytes = p->allocated_bytes,
                       .limit = p->max_allocation_bytes};
    if (s.math != NULL) {
        s.math_state = atomic_load_explicit(&s.math->state, memory_order_relaxed);
        s.gaussian = s.math->have_next_next_gaussian;
        memcpy(&s.gaussian_bits, &s.math->next_next_gaussian, sizeof(s.gaussian_bits));
    }
    return s;
}

static bool profile_equal(profile_state a, profile_state b) {
    return a.uniquifier == b.uniquifier && a.math_state == b.math_state &&
           a.gaussian_bits == b.gaussian_bits && a.holder == b.holder && a.math == b.math &&
           a.gaussian == b.gaussian && a.clock_ready == b.clock_ready &&
           a.frequency == b.frequency && a.initial_sample == b.initial_sample &&
           a.bytes == b.bytes && a.limit == b.limit;
}

static bool counter_equal(const mc_entity_class_snapshot *a, const mc_entity_class_snapshot *b) {
    return a->source == b->source && a->entity_class == b->entity_class &&
           a->bounds_class == b->bounds_class && a->watcher_class == b->watcher_class &&
           a->next_id == b->next_id && a->bounds_pool_cursor == b->bounds_pool_cursor &&
           a->bounds_pool_identity == b->bounds_pool_identity &&
           a->watcher_types_identity == b->watcher_types_identity &&
           a->watcher_type_count == b->watcher_type_count && a->live_entities == b->live_entities;
}

static bool registry_error_equal(const mc_entity_registry_error *a,
                                 const mc_entity_registry_error *b) {
    return a->status == b->status && a->cause == b->cause && a->operation == b->operation &&
           a->initializer_bytecode == b->initializer_bytecode &&
           a->registration_bytecode == b->registration_bytecode && a->class_cause == b->class_cause;
}

static bool name_equal(mc_nbt_string value, const char *ascii) {
    if (value.length != strlen(ascii) || (value.length != 0 && value.units == NULL))
        return false;
    for (size_t i = 0; i < value.length; ++i)
        if (value.units[i] != (uint16_t)(unsigned char)ascii[i])
            return false;
    return true;
}

static mc_nbt_string name_input(const char *ascii, uint16_t units[16]) {
    size_t length = strlen(ascii);
    for (size_t i = 0; i < length; ++i)
        units[i] = (uint16_t)(unsigned char)ascii[i];
    return (mc_nbt_string){units, length};
}

static bool retire_entity_classes(mc_entity_classes **owner) {
    if (*owner == NULL)
        return true;
    if (mc_entity_classes_free(*owner) != MC_ENTITY_OK) {
        retirement_failed = true;
        fputs("Entity Class retirement refused; live dependencies retained\n", stderr);
        return false;
    }
    *owner = NULL;
    return true;
}

static bool retire_world_classes(mc_world_runtime_classes **owner) {
    if (*owner == NULL)
        return true;
    if (mc_world_runtime_classes_free(*owner, NULL) != MC_WORLD_STORAGE_OK) {
        retirement_failed = true;
        fputs("World Class retirement refused; live dependencies retained\n", stderr);
        return false;
    }
    *owner = NULL;
    return true;
}

static bool check_cells(mc_entity_classes *a, mc_entity_classes *b, mc_entity_source source) {
    const mc_entity_class_literal *seen[MC_ENTITY_CLASS_KIND_COUNT] = {0};
    mc_entity_class_snapshot before, after, other_before, other_after;
    VERIFY(mc_entity_classes_get_snapshot(a, &before) == MC_ENTITY_OK &&
           mc_entity_classes_get_snapshot(b, &other_before) == MC_ENTITY_OK);
    const profile_state p = profile_get(a->profile);
    const mc_entity_class_phase registry_phase = a->registry_phase, other_phase = b->registry_phase;
    for (size_t i = 0; i < (size_t)MC_ENTITY_CLASS_KIND_COUNT; ++i) {
        const mc_entity_class_literal *again = NULL, *other = NULL;
        mc_entity_class_literal_view view;
        const mc_entity_class_kind kind = (mc_entity_class_kind)i;
        const bool registered = i < COUNT(expected);
        const size_t extra = registered ? 0 : i - COUNT(expected);
        VERIFY((registered ? expected[i].kind : query_only[extra].kind) == kind);
        VERIFY(mc_entity_class_literal_get(a, kind, &seen[i]) == MC_ENTITY_OK);
        VERIFY(mc_entity_class_literal_get(a, kind, &again) == MC_ENTITY_OK && again == seen[i]);
        VERIFY(mc_entity_class_literal_get(b, kind, &other) == MC_ENTITY_OK && other != seen[i]);
        VERIFY(mc_entity_class_literal_inspect(seen[i], &view) == MC_ENTITY_OK && view.owner == a &&
               view.source == source && view.kind == kind);
        VERIFY(strcmp(view.client_name,
                      registered ? expected[i].client : query_only[extra].client) == 0 &&
               strcmp(view.server_name,
                      registered ? expected[i].server : query_only[extra].server) == 0);
        for (size_t j = 0; j < i; ++j)
            VERIFY(seen[i] != seen[j]);
    }
    const mc_entity_class_literal *out = seen[0];
    VERIFY(mc_entity_class_literal_get(NULL, expected[0].kind, &out) ==
               MC_ENTITY_INVALID_ARGUMENT &&
           out == seen[0]);
    VERIFY(mc_entity_class_literal_get(a, (mc_entity_class_kind)-1, &out) ==
               MC_ENTITY_INVALID_ARGUMENT &&
           out == seen[0]);
    VERIFY(mc_entity_class_literal_get(a, MC_ENTITY_CLASS_KIND_COUNT, &out) ==
               MC_ENTITY_INVALID_ARGUMENT &&
           out == seen[0]);
    VERIFY(mc_entity_class_literal_get(a, expected[0].kind, NULL) == MC_ENTITY_INVALID_ARGUMENT);
    mc_entity_class_literal_view view, old;
    VERIFY(mc_entity_class_literal_inspect(seen[0], &old) == MC_ENTITY_OK);
    view = old;
    VERIFY(mc_entity_class_literal_inspect(NULL, &view) == MC_ENTITY_INVALID_ARGUMENT &&
           view.owner == old.owner && view.source == old.source && view.kind == old.kind &&
           view.client_name == old.client_name && view.server_name == old.server_name);
    VERIFY(mc_entity_class_literal_inspect(seen[0], NULL) == MC_ENTITY_INVALID_ARGUMENT);
    VERIFY(mc_entity_classes_get_snapshot(a, &after) == MC_ENTITY_OK &&
           counter_equal(&before, &after) && before.allocation_bytes == after.allocation_bytes &&
           a->registry_phase == registry_phase && profile_equal(p, profile_get(a->profile)));
    VERIFY(mc_entity_classes_get_snapshot(b, &other_after) == MC_ENTITY_OK &&
           counter_equal(&other_before, &other_after) &&
           other_before.allocation_bytes == other_after.allocation_bytes &&
           b->registry_phase == other_phase);
    return true;
}

static bool test_assignability(mc_entity_source source, mc_java_random_profile *profile) {
    bool ok = false;
    mc_entity_classes *owner = NULL, *foreign = NULL, *other_side = NULL;
    const mc_entity_source opposite =
        source == MC_ENTITY_CLIENT ? MC_ENTITY_SERVER : MC_ENTITY_CLIENT;
    const profile_state before_profile = profile_get(profile);
    CHECK(mc_entity_classes_create(source, profile, NULL, &owner) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_create(source, profile, NULL, &foreign) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_create(opposite, profile, NULL, &other_side) == MC_ENTITY_OK);
    unsigned char before[sizeof(*owner)], foreign_before[sizeof(*foreign)],
        side_before[sizeof(*other_side)];
    memcpy(before, owner, sizeof(before));
    memcpy(foreign_before, foreign, sizeof(foreign_before));
    memcpy(side_before, other_side, sizeof(side_before));
    const mc_entity_class_literal *cells[MC_ENTITY_CLASS_KIND_COUNT],
        *foreign_cells[MC_ENTITY_CLASS_KIND_COUNT], *side_cells[MC_ENTITY_CLASS_KIND_COUNT];
    for (size_t i = 0; i < (size_t)MC_ENTITY_CLASS_KIND_COUNT; ++i) {
        const mc_entity_class_kind kind = (mc_entity_class_kind)i;
        CHECK(mc_entity_class_literal_get(owner, kind, &cells[i]) == MC_ENTITY_OK);
        CHECK(mc_entity_class_literal_get(foreign, kind, &foreign_cells[i]) == MC_ENTITY_OK);
        CHECK(mc_entity_class_literal_get(other_side, kind, &side_cells[i]) == MC_ENTITY_OK);
    }
    for (size_t candidate = 0; candidate < COUNT(cells); ++candidate)
        for (size_t target = 0; target < COUNT(cells); ++target) {
            const bool wanted = (expected_ancestors[candidate] & (UINT32_C(1) << target)) != 0;
            bool actual = !wanted;
            CHECK(mc_entity_class_literal_is_assignable_from(cells[target], cells[candidate],
                                                             &actual) == MC_ENTITY_OK &&
                  actual == wanted);
            actual = true;
            CHECK(mc_entity_class_literal_is_assignable_from(
                      foreign_cells[target], cells[candidate], &actual) == MC_ENTITY_OK &&
                  !actual);
            actual = true;
            CHECK(mc_entity_class_literal_is_assignable_from(
                      cells[target], foreign_cells[candidate], &actual) == MC_ENTITY_OK &&
                  !actual);
            actual = true;
            CHECK(mc_entity_class_literal_is_assignable_from(side_cells[target], cells[candidate],
                                                             &actual) == MC_ENTITY_OK &&
                  !actual);
        }
    const mc_entity_class_literal *entity = cells[MC_ENTITY_CLASS_KIND_ENTITY],
                                  *mob = cells[MC_ENTITY_CLASS_KIND_MOB],
                                  *player = cells[MC_ENTITY_CLASS_KIND_PLAYER],
                                  *zombie = cells[MC_ENTITY_CLASS_KIND_ZOMBIE],
                                  *pig_zombie = cells[MC_ENTITY_CLASS_KIND_PIG_ZOMBIE];
    bool actual = true;
    CHECK(mc_entity_class_literal_is_assignable_from(NULL, entity, &actual) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          actual);
    actual = false;
    CHECK(mc_entity_class_literal_is_assignable_from(entity, NULL, &actual) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          !actual);
    CHECK(mc_entity_class_literal_is_assignable_from(entity, entity, NULL) ==
          MC_ENTITY_INVALID_ARGUMENT);
    mc_entity_class_literal copied = *entity;
    actual = true;
    CHECK(mc_entity_class_literal_is_assignable_from(&copied, entity, &actual) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          actual);
    actual = false;
    CHECK(mc_entity_class_literal_is_assignable_from(entity, &copied, &actual) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          !actual);
    copied.owner = NULL;
    CHECK(mc_entity_class_literal_is_assignable_from(entity, &copied, &actual) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          !actual);
    copied = *entity;
    copied.kind = (mc_entity_class_kind)-1;
    CHECK(mc_entity_class_literal_is_assignable_from(&copied, entity, &actual) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          !actual);
    copied.kind = MC_ENTITY_CLASS_KIND_COUNT;
    CHECK(mc_entity_class_literal_is_assignable_from(entity, &copied, &actual) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          !actual);
    copied = *entity;
    copied.source = opposite;
    CHECK(mc_entity_class_literal_is_assignable_from(entity, &copied, &actual) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          !actual);
    CHECK(memcmp(before, owner, sizeof(before)) == 0 &&
          memcmp(foreign_before, foreign, sizeof(foreign_before)) == 0 &&
          memcmp(side_before, other_side, sizeof(side_before)) == 0);
    /* Restored native phase controls prove that this passive operation adds
     * no active-use/READY gate, including entered and failed class owners. */
    for (unsigned int phase = MC_ENTITY_CLASS_COLD; phase <= MC_ENTITY_CLASS_FAILED; ++phase) {
        owner->entity_phase = owner->bounds_phase = owner->watcher_phase = owner->registry_phase =
            (mc_entity_class_phase)phase;
        unsigned char phase_before[sizeof(*owner)];
        memcpy(phase_before, owner, sizeof(phase_before));
        CHECK(mc_entity_class_literal_is_assignable_from(mob, player, &actual) == MC_ENTITY_OK &&
              actual);
        CHECK(mc_entity_class_literal_is_assignable_from(zombie, pig_zombie, &actual) ==
                  MC_ENTITY_OK &&
              actual);
        CHECK(mc_entity_class_literal_is_assignable_from(entity, pig_zombie, &actual) ==
                  MC_ENTITY_OK &&
              actual);
        CHECK(mc_entity_class_literal_is_assignable_from(player, mob, &actual) == MC_ENTITY_OK &&
              !actual);
        CHECK(memcmp(phase_before, owner, sizeof(phase_before)) == 0);
    }
    owner->entity_phase = owner->bounds_phase = owner->watcher_phase = owner->registry_phase =
        MC_ENTITY_CLASS_COLD;
    CHECK(memcmp(before, owner, sizeof(before)) == 0 &&
          memcmp(foreign_before, foreign, sizeof(foreign_before)) == 0 &&
          memcmp(side_before, other_side, sizeof(side_before)) == 0 &&
          profile_equal(before_profile, profile_get(profile)));
    ok = true;
cleanup:
    if (owner != NULL)
        owner->entity_phase = owner->bounds_phase = owner->watcher_phase = owner->registry_phase =
            MC_ENTITY_CLASS_COLD;
    if (!retire_entity_classes(&other_side))
        ok = false;
    if (!retire_entity_classes(&foreign))
        ok = false;
    if (!retire_entity_classes(&owner))
        ok = false;
    return ok;
}

static bool initialize_namespace(mc_entity_classes *owner) {
    mc_entity_class_snapshot before, after;
    mc_entity_registry_view view;
    VERIFY(mc_entity_classes_get_snapshot(owner, &before) == MC_ENTITY_OK);
    const profile_state p = profile_get(owner->profile);
    VERIFY(mc_entity_classes_initialize_registry(owner, NULL) == MC_ENTITY_REGISTRY_OK);
    VERIFY(mc_entity_classes_get_snapshot(owner, &after) == MC_ENTITY_OK &&
           counter_equal(&before, &after));
    VERIFY(after.allocation_bytes == before.allocation_bytes + 4u * sizeof(mc_entity_registry_map) +
                                         COUNT(expected) * 4u * sizeof(mc_entity_registry_node));
    VERIFY(profile_equal(p, profile_get(owner->profile)));
    VERIFY(mc_entity_registry_inspect(owner, &view) == MC_ENTITY_REGISTRY_OK &&
           view.phase == MC_ENTITY_CLASS_READY);
    for (size_t i = 0; i < 4; ++i) {
        VERIFY(view.maps[i] != NULL && view.counts[i] == COUNT(expected));
        for (size_t j = 0; j < i; ++j)
            VERIFY(view.maps[i] != view.maps[j]);
    }
    return true;
}

static bool check_namespace(mc_entity_classes *owner, mc_entity_classes *other) {
    const profile_state p = profile_get(owner->profile);
    mc_entity_class_snapshot before, after;
    VERIFY(mc_entity_classes_get_snapshot(owner, &before) == MC_ENTITY_OK);
    for (size_t i = 0; i < COUNT(expected); ++i) {
        uint16_t units[16];
        const mc_entity_class_literal *literal = NULL, *named = NULL, *numeric = NULL,
                                      *foreign = NULL;
        mc_nbt_string name = {0};
        VERIFY(mc_entity_class_literal_get(owner, expected[i].kind, &literal) == MC_ENTITY_OK);
        VERIFY(mc_entity_registry_named_class(owner, name_input(expected[i].name, units), &named,
                                              NULL) == MC_ENTITY_REGISTRY_OK &&
               named == literal);
        VERIFY(mc_entity_registry_numeric_class(owner, expected[i].id, &numeric, NULL) ==
                   MC_ENTITY_REGISTRY_OK &&
               numeric == literal);
        VERIFY(mc_entity_registry_class_name(owner, literal, &name, NULL) ==
                   MC_ENTITY_REGISTRY_OK &&
               name_equal(name, expected[i].name));
        VERIFY(mc_entity_class_literal_get(other, expected[i].kind, &foreign) == MC_ENTITY_OK &&
               foreign != literal);
        VERIFY(mc_entity_registry_class_name(owner, foreign, &name, NULL) ==
                   MC_ENTITY_REGISTRY_OK &&
               name.units == NULL && name.length == 0);
    }
    for (size_t i = 0; i < COUNT(query_only); ++i) {
        const mc_entity_class_literal *literal = NULL;
        mc_nbt_string name = {0};
        VERIFY(mc_entity_class_literal_get(owner, query_only[i].kind, &literal) == MC_ENTITY_OK);
        VERIFY(mc_entity_registry_class_name(owner, literal, &name, NULL) ==
                   MC_ENTITY_REGISTRY_OK &&
               name.units == NULL && name.length == 0);
    }
    uint16_t units[16];
    const mc_entity_class_literal *missing = NULL;
    VERIFY(mc_entity_registry_named_class(owner, name_input("Unknown", units), &missing, NULL) ==
               MC_ENTITY_REGISTRY_OK &&
           missing == NULL);
    VERIFY(mc_entity_registry_named_class(owner, name_input("Entity", units), &missing, NULL) ==
               MC_ENTITY_REGISTRY_OK &&
           missing == NULL);
    VERIFY(mc_entity_registry_named_class(owner, name_input("Player", units), &missing, NULL) ==
               MC_ENTITY_REGISTRY_OK &&
           missing == NULL);
    VERIFY(mc_entity_registry_numeric_class(owner, -1, &missing, NULL) == MC_ENTITY_REGISTRY_OK &&
           missing == NULL);
    /* Inspect the actual ordered Maps, including the fourth Class-to-ID Map
     * which has no separate public lookup yet. No copied Map is substituted. */
    for (size_t map = 0; map < 4; ++map) {
        const mc_entity_registry_map *actual = owner->registry_maps[map];
        VERIFY(actual != NULL && actual->count == COUNT(expected));
        const mc_entity_registry_node *node = actual->head, *last = NULL;
        for (size_t i = 0; i < COUNT(expected); ++i) {
            const mc_entity_class_literal *literal = NULL;
            VERIFY(mc_entity_class_literal_get(owner, expected[i].kind, &literal) == MC_ENTITY_OK);
            VERIFY(node != NULL && node->type == literal && node->record != NULL &&
                   node->record->kind == expected[i].kind && node->record->id == expected[i].id &&
                   name_equal(node->record->name, expected[i].name));
            last = node;
            node = node->next;
        }
        VERIFY(node == NULL && actual->tail == last);
    }
    VERIFY(mc_entity_classes_get_snapshot(owner, &after) == MC_ENTITY_OK &&
           counter_equal(&before, &after) && before.allocation_bytes == after.allocation_bytes &&
           profile_equal(p, profile_get(owner->profile)));
    return true;
}

/* Synthetic streams match the existing Biome resource fixture pattern and
 * exercise real Registry initialization; no expected registry objects exist. */
static const uint8_t language[] = "stat.mineBlock=Mine %1$s\nstat.useItem=Use %1$s\n"
                                  "stat.breakItem=Break %1$s\nstat.craftItem=Craft %1$s\n";

typedef struct stream {
    const uint8_t *bytes;
    size_t length, offset, opens, closes;
    bool closed;
} stream;

typedef struct registry_fixture {
    mc_registry_classes *owner;
    mc_registry_resources resources;
    stream streams[3];
    mc_entity_classes *probe;
    size_t callbacks;
    bool bad;
} registry_fixture;

static mc_java_properties_status read_bytes(void *context, uint8_t *out, size_t capacity,
                                            size_t *count) {
    stream *s = context;
    if (out == NULL || count == NULL || capacity == 0 || s->closed || s->offset > s->length)
        return MC_JAVA_PROPERTIES_IO_ERROR;
    size_t take = s->length - s->offset;
    if (take > 17u)
        take = 17u;
    if (take > capacity)
        take = capacity;
    if (take != 0)
        memcpy(out, s->bytes + s->offset, take);
    s->offset += take;
    *count = take;
    return MC_JAVA_PROPERTIES_OK;
}

static mc_registry_status open_resource(void *context, const char *name,
                                        mc_java_properties_input *out) {
    registry_fixture *f = context;
    const char *const names[] = {"/lang/en_US.lang", "/lang/stats_US.lang", "/achievement/map.txt"};
    size_t index = COUNT(names);
    for (size_t i = 0; i < COUNT(names); ++i)
        if (name != NULL && strcmp(name, names[i]) == 0)
            index = i;
    if (index == COUNT(names) || out == NULL) {
        f->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    if (++f->streams[index].opens != 1 || index != f->callbacks)
        f->bad = true;
    /* Real entered initializer: BUSY must precede either binding/retirement
     * mutation, including the already-bound order. No entered count is forged. */
    const size_t holds = f->probe->borrowers, bytes = f->owner->allocation_bytes;
    mc_entity_classes *const binding = f->owner->entities;
    const profile_state p = profile_get(f->probe->profile);
    mc_entity_class_snapshot before, after;
    if (mc_entity_classes_get_snapshot(f->probe, &before) != MC_ENTITY_OK ||
        mc_registry_classes_bind_entities(f->owner, f->probe, NULL) != MC_REGISTRY_BUSY ||
        mc_registry_classes_free(f->owner, NULL) != MC_REGISTRY_BUSY ||
        mc_entity_classes_get_snapshot(f->probe, &after) != MC_ENTITY_OK ||
        !counter_equal(&before, &after) || before.allocation_bytes != after.allocation_bytes ||
        f->probe->borrowers != holds || f->owner->entities != binding ||
        f->owner->allocation_bytes != bytes || !profile_equal(p, profile_get(f->probe->profile)))
        f->bad = true;
    ++f->callbacks;
    *out = (mc_java_properties_input){&f->streams[index], read_bytes};
    return MC_REGISTRY_OK;
}

static mc_registry_status close_resource(void *context, const mc_java_properties_input *input) {
    registry_fixture *f = context;
    if (input == NULL || input->context != &f->streams[2] || input->read != read_bytes) {
        f->bad = true;
        return MC_REGISTRY_INVALID_ARGUMENT;
    }
    ++f->streams[2].closes;
    f->streams[2].closed = true;
    return MC_REGISTRY_OK;
}

static bool ready_resource(void *context, const mc_java_properties_input *input) {
    registry_fixture *f = context;
    if (input == NULL || input->read != read_bytes)
        f->bad = true;
    return false;
}

static registry_fixture *new_registry(mc_entity_source source, mc_materials *materials,
                                      mc_java_random_profile *profile, mc_entity_classes *probe) {
    registry_fixture *f = calloc(1, sizeof(*f));
    if (f == NULL)
        return NULL;
    f->probe = probe;
    f->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
    f->streams[1] = f->streams[2] = (stream){.bytes = language};
    f->resources = (mc_registry_resources){f, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {source == MC_ENTITY_CLIENT ? MC_REGISTRY_CLIENT
                                                                    : MC_REGISTRY_SERVER,
                                         profile, materials, 0, &f->resources};
    if (mc_registry_classes_allocate(&options, &f->owner, NULL) != MC_REGISTRY_OK) {
        free(f);
        return NULL;
    }
    return f;
}

static bool retire_registry(registry_fixture **owner) {
    if (*owner == NULL)
        return true;
    if (mc_registry_classes_free((*owner)->owner, NULL) != MC_REGISTRY_OK) {
        retirement_failed = true;
        fputs("Registry Class retirement refused; live dependencies retained\n", stderr);
        return false;
    }
    free(*owner);
    *owner = NULL;
    return true;
}

static bool logical_at(const mc_biome_spawn_list *list, size_t index,
                       const mc_biome_entity_class **out) {
    const mc_biome_spawn *spawn = mc_biome_spawn_list_at(list, index);
    const mc_biome_spawn_fields *fields = mc_biome_spawn_fields_get(spawn);
    VERIFY(fields != NULL && fields->a != NULL);
    *out = fields->a;
    return true;
}

static bool collect_logical(mc_registry_classes *owner, const mc_biome_entity_class *out[13]) {
    mc_biome_registry_view view;
    VERIFY(mc_biome_registry_inspect(owner, &view, NULL) == MC_REGISTRY_OK &&
           view.phase == MC_REGISTRY_READY);
    const mc_biome_fields *base = mc_biome_fields_get(view.c),
                          *forest = mc_biome_fields_get(view.d),
                          *hell = mc_biome_fields_get(view.l);
    VERIFY(base != NULL && forest != NULL && hell != NULL);
    for (size_t i = 0; i < 5; ++i)
        VERIFY(logical_at(base->s, i, &out[i]));
    for (size_t i = 0; i < 4; ++i)
        VERIFY(logical_at(base->t, i, &out[5u + i]));
    VERIFY(logical_at(base->u, 0, &out[9]));
    VERIFY(logical_at(forest->t, 4, &out[10]));
    VERIFY(logical_at(hell->s, 0, &out[11]) && logical_at(hell->s, 1, &out[12]));
    for (size_t i = 0; i < 13; ++i)
        for (size_t j = 0; j < i; ++j)
            VERIFY(out[i] != out[j]);
    return true;
}

static bool check_join(mc_registry_classes *registry, mc_entity_classes *entities,
                       const mc_biome_entity_class *const logical[13], bool names) {
    for (size_t i = 0; i < 13; ++i) {
        const mc_entity_class_literal *expected_literal = NULL, *canonical = NULL;
        VERIFY(mc_entity_class_literal_get(entities, biome_kinds[i], &expected_literal) ==
               MC_ENTITY_OK);
        VERIFY(mc_biome_entity_class_canonical(registry, logical[i], &canonical, NULL) ==
                   MC_REGISTRY_OK &&
               canonical == expected_literal);
        if (names) {
            mc_nbt_string name = {0};
            VERIFY(mc_entity_registry_class_name(entities, canonical, &name, NULL) ==
                       MC_ENTITY_REGISTRY_OK &&
                   name_equal(name, expected[(size_t)biome_kinds[i]].name));
        }
    }
    return true;
}

static bool test_passive(mc_entity_source source, mc_java_random_profile *profile) {
    bool ok = false;
    mc_entity_classes *a = NULL, *b = NULL;
    mc_entity *entity = NULL;
    const profile_state p = profile_get(profile);
    CHECK(mc_entity_classes_create(source, profile, NULL, &a) == MC_ENTITY_OK &&
          mc_entity_classes_create(source, profile, NULL, &b) == MC_ENTITY_OK);
    CHECK(profile_equal(p, profile_get(profile)) && check_cells(a, b, source));
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(a, &before) == MC_ENTITY_OK && before.next_id == 0 &&
          before.entity_class == MC_ENTITY_CLASS_COLD &&
          before.bounds_class == MC_ENTITY_CLASS_COLD &&
          before.watcher_class == MC_ENTITY_CLASS_COLD && before.allocation_bytes == sizeof(*a));
    CHECK(mc_item_entity_allocate(a, &entity, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct_world(entity, NULL, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_get_snapshot(a, &after) == MC_ENTITY_OK && after.next_id == 1 &&
          after.live_entities == 1 && after.entity_class == MC_ENTITY_CLASS_READY);
    const profile_state warm = profile_get(profile);
    CHECK(warm.math != NULL && warm.holder == MC_JAVA_RANDOM_HOLDER_READY);
    CHECK(check_cells(a, b, source) && mc_entity_classes_free(a) == MC_ENTITY_BUSY);
    mc_entity_release(entity);
    entity = NULL;
    CHECK(initialize_namespace(a) && check_namespace(a, b) && check_cells(a, b, source));
    CHECK(mc_entity_classes_get_snapshot(a, &after) == MC_ENTITY_OK && after.next_id == 1 &&
          after.live_entities == 0);
    ok = true;
cleanup:
    mc_entity_release(entity);
    if (!retire_entity_classes(&a))
        ok = false;
    if (!retire_entity_classes(&b))
        ok = false;
    return ok;
}

static bool test_binding(mc_entity_source source, bool registry_first, mc_materials *materials,
                         mc_java_random_profile *profile) {
    bool ok = false, external_hold = false;
    registry_fixture *f = NULL, *foreign = NULL;
    mc_entity_classes *entities = NULL, *other = NULL, *wrong_side = NULL, *wrong_profile = NULL;
    mc_java_random_profile *second_profile = NULL;
    const mc_biome_entity_class *logical[13], *again[13];
    const mc_entity_class_literal *sentinel = NULL, *out = NULL;
    CHECK(mc_entity_classes_create(source, profile, NULL, &entities) == MC_ENTITY_OK &&
          mc_entity_classes_create(source, profile, NULL, &other) == MC_ENTITY_OK &&
          mc_entity_classes_create(source == MC_ENTITY_CLIENT ? MC_ENTITY_SERVER : MC_ENTITY_CLIENT,
                                   profile, NULL, &wrong_side) == MC_ENTITY_OK);
    CHECK(mc_java_random_profile_create(sizeof(mc_java_random_profile) +
                                            sizeof(mc_java_math_random_owner),
                                        &second_profile) == MC_JAVA_RANDOM_PROFILE_OK &&
          mc_entity_classes_create(source, second_profile, NULL, &wrong_profile) == MC_ENTITY_OK);
    f = new_registry(source, materials, profile, entities);
    foreign = new_registry(source, materials, profile, other);
    CHECK(f != NULL && foreign != NULL &&
          mc_entity_class_literal_get(other, MC_ENTITY_CLASS_KIND_ITEM, &sentinel) == MC_ENTITY_OK);
    profile_state p = profile_get(profile), second = profile_get(second_profile);
    CHECK(mc_registry_classes_bind_entities(NULL, entities, NULL) == MC_REGISTRY_INVALID_ARGUMENT &&
          mc_registry_classes_bind_entities(f->owner, NULL, NULL) == MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_registry_classes_bind_entities(f->owner, wrong_side, NULL) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          mc_registry_classes_bind_entities(f->owner, wrong_profile, NULL) ==
              MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(f->owner->entities == NULL && wrong_side->borrowers == 0 &&
          wrong_profile->borrowers == 0 && entities->borrowers == 0 &&
          profile_equal(p, profile_get(profile)) &&
          profile_equal(second, profile_get(second_profile)));
    if (registry_first) {
        CHECK(mc_registry_classes_initialize_biomes(f->owner, NULL) == MC_REGISTRY_OK &&
              collect_logical(f->owner, logical));
        CHECK(entities->next_id == 0 && entities->entity_phase == MC_ENTITY_CLASS_COLD &&
              entities->bounds_phase == MC_ENTITY_CLASS_COLD &&
              entities->watcher_phase == MC_ENTITY_CLASS_COLD &&
              entities->registry_phase == MC_ENTITY_CLASS_COLD &&
              entities->allocated_bytes == sizeof(*entities));
        out = sentinel;
        CHECK(mc_biome_entity_class_canonical(f->owner, logical[0], &out, NULL) ==
                  MC_REGISTRY_UNIMPLEMENTED_DEPENDENCY &&
              out == sentinel);
        CHECK(initialize_namespace(entities) && check_namespace(entities, other));
    } else {
        CHECK(initialize_namespace(entities) && check_namespace(entities, other));
        p = profile_get(profile);
        CHECK(mc_registry_classes_bind_entities(f->owner, entities, NULL) == MC_REGISTRY_OK &&
              entities->borrowers == 1 && profile_equal(p, profile_get(profile)));
        CHECK(mc_registry_classes_initialize_biomes(f->owner, NULL) == MC_REGISTRY_OK &&
              collect_logical(f->owner, logical));
    }
    CHECK(entities->next_id == 0 && entities->entity_phase == MC_ENTITY_CLASS_COLD &&
          entities->bounds_phase == MC_ENTITY_CLASS_COLD &&
          entities->watcher_phase == MC_ENTITY_CLASS_COLD && entities->live_entities == 0);
    CHECK(!f->bad && f->callbacks == 3 && f->streams[0].opens == 1 && f->streams[1].opens == 1 &&
          f->streams[2].opens == 1 && f->streams[0].closes == 0 && f->streams[1].closes == 0 &&
          f->streams[2].closes == 1);
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(entities, &before) == MC_ENTITY_OK);
    p = profile_get(profile);
    CHECK(mc_registry_classes_bind_entities(f->owner, entities, NULL) == MC_REGISTRY_OK &&
          entities->borrowers == 1);
    CHECK(mc_registry_classes_bind_entities(f->owner, entities, NULL) == MC_REGISTRY_OK &&
          entities->borrowers == 1);
    CHECK(mc_registry_classes_bind_entities(f->owner, other, NULL) == MC_REGISTRY_BUSY &&
          other->borrowers == 0);
    CHECK(collect_logical(f->owner, again) && check_join(f->owner, entities, logical, true));
    for (size_t i = 0; i < 13; ++i)
        CHECK(logical[i] == again[i]);
    out = sentinel;
    CHECK(mc_biome_entity_class_canonical(NULL, logical[0], &out, NULL) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          out == sentinel);
    CHECK(mc_biome_entity_class_canonical(f->owner, NULL, &out, NULL) ==
              MC_REGISTRY_INVALID_ARGUMENT &&
          out == sentinel);
    CHECK(mc_biome_entity_class_canonical(f->owner, logical[0], NULL, NULL) ==
          MC_REGISTRY_INVALID_ARGUMENT);
    CHECK(mc_biome_entity_class_canonical(f->owner, &foreign->owner->biomes.entity_classes[0], &out,
                                          NULL) == MC_REGISTRY_INVALID_ARGUMENT &&
          out == sentinel);
    CHECK(mc_entity_classes_get_snapshot(entities, &after) == MC_ENTITY_OK &&
          counter_equal(&before, &after) && before.allocation_bytes == after.allocation_bytes &&
          profile_equal(p, profile_get(profile)));
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_BUSY && entities->borrowers == 1);
    CHECK(mc_entity_classes_retain(entities, source, profile) == MC_ENTITY_OK);
    external_hold = true;
    CHECK(entities->borrowers == 2);
    CHECK(mc_biome_entity_class_canonical(f->owner, logical[0], &out, NULL) == MC_REGISTRY_OK);
    CHECK(retire_registry(&f) && entities->borrowers == 1);
    mc_entity_class_literal_view literal_view;
    CHECK(mc_entity_class_literal_inspect(out, &literal_view) == MC_ENTITY_OK &&
          literal_view.owner == entities && literal_view.kind == MC_ENTITY_CLASS_KIND_SPIDER &&
          check_namespace(entities, other));
    CHECK(mc_entity_classes_free(entities) == MC_ENTITY_BUSY);
    CHECK(mc_entity_classes_release(entities) == MC_ENTITY_OK);
    external_hold = false;
    CHECK(entities->borrowers == 0 &&
          mc_entity_classes_release(entities) == MC_ENTITY_STATE_ERROR && entities->borrowers == 0);
    ok = true;
cleanup:
    if (!retire_registry(&foreign))
        ok = false;
    if (!retire_registry(&f))
        ok = false;
    if (f == NULL) {
        if (external_hold && mc_entity_classes_release(entities) != MC_ENTITY_OK) {
            retirement_failed = true;
            ok = false;
        }
        if (!retire_entity_classes(&entities))
            ok = false;
    }
    if (!retire_entity_classes(&other))
        ok = false;
    if (!retire_entity_classes(&wrong_side))
        ok = false;
    if (!retire_entity_classes(&wrong_profile))
        ok = false;
    if (!retirement_failed)
        mc_java_random_profile_free(second_profile);
    return ok;
}

static bool test_limits(mc_entity_source source, mc_materials *materials,
                        mc_java_random_profile *profile) {
    bool ok = false;
    mc_entity_classes *tight = NULL, *partial = NULL;
    registry_fixture *f = NULL;
    mc_entity_limits limits = {sizeof(mc_entity_classes) - 1u, SIZE_MAX};
    mc_entity_classes *candidate = NULL;
    const profile_state p = profile_get(profile);
    CHECK(mc_entity_classes_create(source, profile, &limits, &candidate) == MC_ENTITY_LIMIT &&
          candidate == NULL);
    limits.max_allocation_bytes = sizeof(mc_entity_classes);
    CHECK(mc_entity_classes_create(source, profile, &limits, &tight) == MC_ENTITY_OK);
    const mc_entity_class_literal *arrow = NULL, *again = NULL;
    CHECK(mc_entity_class_literal_get(tight, MC_ENTITY_CLASS_KIND_ARROW, &arrow) == MC_ENTITY_OK);
    mc_entity_registry_error first, retry;
    mc_entity_registry_view view, after;
    CHECK(mc_entity_classes_initialize_registry(tight, &first) == MC_ENTITY_REGISTRY_LIMIT &&
          first.initializer_bytecode == 0 && first.registration_bytecode == 0);
    CHECK(mc_entity_registry_inspect(tight, &view) == MC_ENTITY_REGISTRY_OK &&
          view.phase == MC_ENTITY_CLASS_FAILED);
    for (size_t i = 0; i < 4; ++i)
        CHECK(view.maps[i] == NULL && view.counts[i] == 0);
    CHECK(tight->allocated_bytes == sizeof(*tight) && profile_equal(p, profile_get(profile)));
    CHECK(mc_entity_class_literal_get(tight, MC_ENTITY_CLASS_KIND_ARROW, &again) == MC_ENTITY_OK &&
          again == arrow);
    limits.max_allocation_bytes = sizeof(mc_entity_classes) + 4u * sizeof(mc_entity_registry_map) +
                                  2u * sizeof(mc_entity_registry_node);
    CHECK(mc_entity_classes_create(source, profile, &limits, &partial) == MC_ENTITY_OK);
    CHECK(mc_entity_classes_initialize_registry(partial, &first) == MC_ENTITY_REGISTRY_LIMIT &&
          first.cause == MC_ENTITY_REGISTRY_LIMIT && first.class_cause == MC_ENTITY_OK &&
          first.initializer_bytecode == 46 && first.registration_bytecode == 30);
    CHECK(mc_entity_registry_inspect(partial, &view) == MC_ENTITY_REGISTRY_OK &&
          view.phase == MC_ENTITY_CLASS_FAILED && view.counts[0] == 1 && view.counts[1] == 1 &&
          view.counts[2] == 0 && view.counts[3] == 0 &&
          registry_error_equal(&view.first_failure, &first));
    CHECK(partial->allocated_bytes == limits.max_allocation_bytes &&
          profile_equal(p, profile_get(profile)));
    CHECK(mc_entity_classes_initialize_registry(partial, &retry) ==
              MC_ENTITY_REGISTRY_CLASS_FAILED &&
          retry.cause == first.cause && retry.operation == first.operation &&
          retry.class_cause == first.class_cause &&
          retry.initializer_bytecode == first.initializer_bytecode &&
          retry.registration_bytecode == first.registration_bytecode);
    CHECK(mc_entity_registry_inspect(partial, &after) == MC_ENTITY_REGISTRY_OK &&
          registry_error_equal(&after.first_failure, &first));
    for (size_t i = 0; i < 4; ++i)
        CHECK(after.maps[i] == view.maps[i] && after.counts[i] == view.counts[i]);
    CHECK(mc_entity_class_literal_get(partial, MC_ENTITY_CLASS_KIND_ARROW, &again) == MC_ENTITY_OK);
    const mc_entity_registry_map *a = view.maps[0], *b = view.maps[1];
    CHECK(a != NULL && b != NULL && a->head != NULL && b->head != NULL && a->head == a->tail &&
          b->head == b->tail && a->head->type == again && b->head->type == again &&
          a->head->next == NULL && b->head->next == NULL);
    CHECK(partial->next_id == 0 && partial->entity_phase == MC_ENTITY_CLASS_COLD &&
          partial->bounds_phase == MC_ENTITY_CLASS_COLD &&
          partial->watcher_phase == MC_ENTITY_CLASS_COLD && partial->live_entities == 0 &&
          profile_equal(p, profile_get(profile)));
    /* Failed EntityList class is not a failed target Class literal. Binding
     * must not replay Maps or reset the cached prefix to make it usable. */
    f = new_registry(source, materials, profile, partial);
    CHECK(f != NULL && mc_registry_classes_initialize_biomes(f->owner, NULL) == MC_REGISTRY_OK);
    const mc_biome_entity_class *logical[13];
    CHECK(collect_logical(f->owner, logical));
    const profile_state after_biomes = profile_get(profile);
    CHECK(mc_registry_classes_bind_entities(f->owner, partial, NULL) == MC_REGISTRY_OK &&
          check_join(f->owner, partial, logical, false));
    CHECK(mc_entity_registry_inspect(partial, &after) == MC_ENTITY_REGISTRY_OK &&
          after.phase == MC_ENTITY_CLASS_FAILED &&
          registry_error_equal(&after.first_failure, &first));
    for (size_t i = 0; i < 4; ++i)
        CHECK(after.maps[i] == view.maps[i] && after.counts[i] == view.counts[i]);
    CHECK(partial->allocated_bytes == limits.max_allocation_bytes &&
          profile_equal(after_biomes, profile_get(profile)) && !f->bad);
    ok = true;
cleanup:
    if (!retire_registry(&f))
        ok = false;
    if (f == NULL && !retire_entity_classes(&partial))
        ok = false;
    if (!retire_entity_classes(&tight))
        ok = false;
    if (!retire_entity_classes(&candidate))
        ok = false;
    return ok;
}

static bool test_world_binding(mc_entity_source source, bool registry_first,
                               mc_materials *materials, mc_java_random_profile *profile) {
    bool ok = false;
    mc_entity_classes *entities = NULL, *other = NULL;
    mc_world_runtime_classes *worlds = NULL, *conflict = NULL;
    registry_fixture *f = NULL;
    const profile_state p = profile_get(profile);
    const mc_world_info_side side =
        source == MC_ENTITY_CLIENT ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER;
    CHECK(mc_entity_classes_create(source, profile, NULL, &entities) == MC_ENTITY_OK &&
          mc_entity_classes_create(source, profile, NULL, &other) == MC_ENTITY_OK);
    f = new_registry(source, materials, profile, entities);
    CHECK(f != NULL);
    CHECK(mc_world_runtime_classes_create(side, profile, &trig, &worlds, NULL) ==
              MC_WORLD_STORAGE_OK &&
          mc_world_runtime_classes_create(side, profile, &trig, &conflict, NULL) ==
              MC_WORLD_STORAGE_OK);
    const size_t registry_bytes = f->owner->allocation_bytes;
    mc_entity_class_snapshot before, after;
    CHECK(mc_entity_classes_get_snapshot(entities, &before) == MC_ENTITY_OK);
    const mc_entity_class_literal *literal = NULL;
    CHECK(mc_entity_class_literal_get(entities, MC_ENTITY_CLASS_KIND_PIG, &literal) ==
          MC_ENTITY_OK);
    if (registry_first) {
        CHECK(mc_world_runtime_classes_bind_registry(worlds, f->owner, NULL) == MC_REGISTRY_OK &&
              f->owner->runtime_borrows == 1 && f->owner->entities == NULL &&
              entities->borrowers == 0);
        CHECK(mc_world_runtime_classes_bind_entities(worlds, entities) == MC_ENTITY_OK);
    } else {
        CHECK(mc_world_runtime_classes_bind_entities(worlds, entities) == MC_ENTITY_OK &&
              entities->borrowers == 1 && worlds->registry == NULL && f->owner->entities == NULL);
        CHECK(mc_world_runtime_classes_bind_registry(worlds, f->owner, NULL) == MC_REGISTRY_OK);
    }
    CHECK(worlds->registry == f->owner && worlds->entities == entities &&
          f->owner->entities == entities && entities->borrowers == 2 &&
          f->owner->runtime_borrows == 1);
    mc_world_runtime_class_state state;
    CHECK(mc_world_runtime_classes_get(worlds, &state) == MC_WORLD_STORAGE_OK &&
          state.side == side && !state.initialized && state.enqueue_depth == 0 &&
          state.live_worlds == 0);
    CHECK(mc_world_runtime_classes_bind_registry(worlds, f->owner, NULL) == MC_REGISTRY_BUSY &&
          mc_world_runtime_classes_bind_entities(worlds, entities) == MC_ENTITY_BUSY &&
          entities->borrowers == 2 && f->owner->runtime_borrows == 1);
    CHECK(mc_registry_classes_free(f->owner, NULL) == MC_REGISTRY_BUSY &&
          mc_entity_classes_free(entities) == MC_ENTITY_BUSY);
    /* Existing R->entities rejects a different actual owner at the second
     * World binding; only the just-acquired temporary hold is rolled back. */
    if (registry_first) {
        CHECK(mc_world_runtime_classes_bind_registry(conflict, f->owner, NULL) == MC_REGISTRY_OK &&
              f->owner->runtime_borrows == 2);
        CHECK(mc_world_runtime_classes_bind_entities(conflict, other) == MC_ENTITY_BUSY &&
              conflict->entities == NULL && other->borrowers == 0 &&
              conflict->registry == f->owner);
    } else {
        CHECK(mc_world_runtime_classes_bind_entities(conflict, other) == MC_ENTITY_OK &&
              other->borrowers == 1);
        CHECK(mc_world_runtime_classes_bind_registry(conflict, f->owner, NULL) ==
                  MC_REGISTRY_BUSY &&
              conflict->registry == NULL && f->owner->runtime_borrows == 1 &&
              conflict->entities == other && other->borrowers == 1);
    }
    CHECK(f->owner->entities == entities && entities->borrowers == 2 &&
          f->owner->allocation_bytes == registry_bytes && f->callbacks == 0 &&
          mc_entity_classes_get_snapshot(entities, &after) == MC_ENTITY_OK &&
          counter_equal(&before, &after) && before.allocation_bytes == after.allocation_bytes &&
          entities->registry_phase == MC_ENTITY_CLASS_COLD &&
          other->registry_phase == MC_ENTITY_CLASS_COLD && other->next_id == 0 &&
          other->entity_phase == MC_ENTITY_CLASS_COLD &&
          other->bounds_phase == MC_ENTITY_CLASS_COLD &&
          other->watcher_phase == MC_ENTITY_CLASS_COLD && profile_equal(p, profile_get(profile)));
    CHECK(retire_world_classes(&conflict) && f->owner->runtime_borrows == 1 &&
          other->borrowers == 0);
    CHECK(retire_world_classes(&worlds) && f->owner->runtime_borrows == 0 &&
          entities->borrowers == 1);
    mc_entity_class_literal_view view;
    CHECK(mc_entity_class_literal_inspect(literal, &view) == MC_ENTITY_OK &&
          view.owner == entities && view.kind == MC_ENTITY_CLASS_KIND_PIG &&
          mc_entity_classes_free(entities) == MC_ENTITY_BUSY);
    CHECK(retire_registry(&f) && entities->borrowers == 0 &&
          mc_entity_class_literal_inspect(literal, &view) == MC_ENTITY_OK &&
          view.owner == entities && profile_equal(p, profile_get(profile)));
    ok = true;
cleanup:
    if (!retire_world_classes(&conflict))
        ok = false;
    if (!retire_world_classes(&worlds))
        ok = false;
    if (conflict == NULL && worlds == NULL) {
        if (!retire_registry(&f))
            ok = false;
        if (f == NULL && !retire_entity_classes(&entities))
            ok = false;
        if (!retire_entity_classes(&other))
            ok = false;
    }
    return ok;
}

int main(void) {
    int result = 1;
    mc_materials *materials = NULL;
    mc_java_random_profile *profile = NULL;
    CHECK(mc_materials_create(&materials));
    CHECK(mc_java_random_profile_create(
              sizeof(mc_java_random_profile) + sizeof(mc_java_math_random_owner), &profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_math_trig_init(&trig));
    for (unsigned int side = 0; side < 2; ++side) {
        const mc_entity_source source = side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER;
        CHECK(test_assignability(source, profile));
        CHECK(test_passive(source, profile));
        CHECK(test_binding(source, true, materials, profile));
        CHECK(test_binding(source, false, materials, profile));
        CHECK(test_limits(source, materials, profile));
        CHECK(test_world_binding(source, true, materials, profile));
        CHECK(test_world_binding(source, false, materials, profile));
    }
    result = 0;
cleanup:
    if (!retirement_failed) {
        mc_java_random_profile_free(profile);
        mc_materials_destroy(materials);
    } else
        result = 1;
    if (result == 0 && printf("PASS: %u Entity Class identity checks\n", checks) < 0)
        result = 1;
    return result;
}
