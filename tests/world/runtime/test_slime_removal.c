/* Asset-independent cached GAME World/Store/Chunk bootstrap. Imported main
 * never runs; this control reaches the genuine damage and virtual K entries. */
int slime_removal_unused_world_main(int, char **);
#define MC_WORLD_ENTITY_TICK_MAIN slime_removal_unused_world_main
#include "test_entity_tick.c"
#undef MC_WORLD_ENTITY_TICK_MAIN

#include "entity/mob/internal.h"
#include "entity/watcher_payload_internal.h"
#include "mc/core/java/java.h"
#include "mc/entity/mob/construction.h"
#include "mc/entity/mob/removal.h"
#include "world/runtime/world_access_internal.h"

typedef enum sk_callback_change {
    SK_CHANGE_NONE,
    SK_CHANGE_POSE,
    SK_CHANGE_SIZE,
    SK_CHANGE_RESEED,
    SK_CHANGE_NULL_WORLD
} sk_callback_change;

typedef struct sk_profile {
    const char *name;
    int32_t size, health, final_size, final_health;
    size_t publications, draws;
    bool remote, null_world, wrong_type;
    const char *exception;
    sk_callback_change change;
} sk_profile;

/* Authored direct-K operands on genuine constructor outputs. The two original
 * damage-then-K reds below keep their separate, independent-entropy inputs. */
static const sk_profile sk_profiles[] = {
    {"direct-split", 2, 0, 2, 0, 4, 4, false, false, false, "NONE", SK_CHANGE_NONE},
    {"size-one", 1, 0, 1, 0, 0, 0, false, false, false, "NONE", SK_CHANGE_NONE},
    {"size-zero", 0, 0, 0, 0, 0, 0, false, false, false, "NONE", SK_CHANGE_NONE},
    {"odd-size", 3, 0, 3, 0, 4, 4, false, false, false, "NONE", SK_CHANGE_NONE},
    {"signed-size", 128, 0, -128, 0, 0, 0, false, false, false, "NONE", SK_CHANGE_NONE},
    {"negative-size", -1, 0, -1, 0, 0, 0, false, false, false, "NONE", SK_CHANGE_NONE},
    {"remote", 2, 0, 2, 0, 0, 0, true, false, false, "NONE", SK_CHANGE_NONE},
    {"positive-health", 2, 4, 2, 4, 0, 0, false, false, false, "NONE", SK_CHANGE_NONE},
    {"null-world", 1, 1, 1, 1, 0, 0, false, true, false, "NPE", SK_CHANGE_NONE},
    {"watcher-type", 2, 0, 2, 0, 0, 0, false, true, true, "CCE", SK_CHANGE_NONE},
    {"callback-pose", 2, 0, 2, 0, 4, 4, false, false, false, "NONE", SK_CHANGE_POSE},
    {"callback-size", 2, 0, 4, 16, 4, 4, false, false, false, "NONE", SK_CHANGE_SIZE},
    {"callback-reseed", 2, 0, 2, 0, 4, 4, false, false, false, "NONE", SK_CHANGE_RESEED},
    {"callback-null-world", 2, 0, 2, 0, 1, 2, false, false, false, "NPE", SK_CHANGE_NULL_WORLD},
};

typedef struct sk_child_snapshot {
    int32_t size, health;
    double position[3];
    float yaw, pitch;
    uint64_t random_after;
    size_t chunk_members;
    bool primary_world, world_member, parent_dead;
} sk_child_snapshot;

#define SK_REQUIRE(condition)                                                                      \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "Slime removal line%d: %s\n", __LINE__, #condition);                   \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct slime_removal_fixture {
    fixture *world;
    mc_math_classes *math;
    mc_entity *parent, *children[4]; /* Children borrow actual World occurrences. */
    mc_world_access *observer;
    uint64_t observer_occurrence;
    mc_entity_effect_error damage_error, removal_error;
    mc_entity_status damage_status, removal_status;
    mc_java_random parent_before, parent_expected;
    float expected_yaw[4];
    sk_child_snapshot snapshots[4];
    const sk_profile *profile;
    const char *mode;
    int32_t amount, health, captured_size;
    size_t publications, world_count, chunk_count;
    bool accepted, valid, observed, parent_dead, random_matches, mutation_returned;
} slime_removal_fixture;

static uint64_t sk_double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool sk_random_same(const mc_java_random *a, const mc_java_random *b) {
    return a->state == b->state && a->have_next_next_gaussian == b->have_next_next_gaussian &&
           sk_double_bits(a->next_next_gaussian) == sk_double_bits(b->next_next_gaussian);
}

static double sk_x(const slime_removal_fixture *s, size_t index) {
    const bool moved = s->profile != NULL && s->profile->change == SK_CHANGE_POSE && index > 0;
    const double offset = s->captured_size == 3 ? .375 : .25;
    return (moved ? 10.5 : 8.5) + (index % 2 == 0 ? -offset : offset);
}

static double sk_y(const slime_removal_fixture *s, size_t index) {
    return s->profile != NULL && s->profile->change == SK_CHANGE_POSE && index > 0 ? 65.5 : 64.5;
}

static double sk_z(const slime_removal_fixture *s, size_t index) {
    const bool moved = s->profile != NULL && s->profile->change == SK_CHANGE_POSE && index > 0;
    const double offset = s->captured_size == 3 ? .375 : .25;
    return (moved ? 9.5 : 8.5) + (index / 2 == 0 ? -offset : offset);
}

static mc_entity_watch_record *sk_size_record(mc_entity *entity) {
    for (mc_entity_watch_node *node = entity->watcher->map->head; node != NULL; node = node->next)
        if (node->key == 16)
            return node->record;
    return NULL;
}

static size_t sk_chunk_members(const slime_removal_fixture *s, const mc_entity *child) {
    size_t count = 0;
    for (size_t slice = 0; slice < 8; ++slice)
        for (size_t index = 0; index < s->world->chunks[0]->entities[slice].count; ++index)
            count += s->world->chunks[0]->entities[slice].items[index] == child ? 1u : 0u;
    return count;
}

static bool sk_world_member(const mc_world_runtime *world, const mc_entity *child) {
    for (size_t i = 0; i < world->fields.b->count; ++i)
        if (world->fields.b->items[i] == child)
            return true;
    return false;
}

static mc_world_storage_status sk_added(void *context, mc_world_runtime *world, mc_entity *child,
                                        mc_world_storage_error *cause) {
    slime_removal_fixture *s = context;
    const size_t index = s->publications++;
    mc_mob_state child_state = {0}, parent_state = {0};
    const bool resized = s->profile != NULL && s->profile->change == SK_CHANGE_SIZE && index > 0;
    bool valid =
        index < 4 && world == s->world->worlds[0] && child != NULL && child != s->parent &&
        child->classes == s->parent->classes && child->phase == MC_ENTITY_READY &&
        child->base.world == world && !child->base.be && child->base.bF && child->base.bG == 0 &&
        child->base.bH == 4 && child->base.bI == 0 && mc_mob_inspect(child, &child_state) &&
        child_state.kind == MC_ENTITY_CLASS_KIND_SLIME && child_state.watcher_16_present &&
        child_state.watcher_16 == 1 && child_state.living.Y == 1 && child->base.width == .6f &&
        child->base.height == .6f && child->base.offset == 0.f &&
        mc_mob_inspect(s->parent, &parent_state) && parent_state.watcher_16_present &&
        parent_state.watcher_16 == (resized ? 4 : s->captured_size) &&
        parent_state.living.Y == (resized ? 16 : 0) && !s->parent->base.be &&
        s->observer->active_holds == 1;
    if (valid)
        valid = child->base.x == sk_x(s, index) && child->base.y == sk_y(s, index) &&
                child->base.z == sk_z(s, index) && child->base.aJ == child->base.x &&
                child->base.aK == child->base.y && child->base.aL == child->base.z &&
                child->base.yaw == s->expected_yaw[index] && child->base.aT == 0.f &&
                world->entity_membership.count == index + 2 &&
                world->entity_membership.entries[index + 1] == child &&
                world->fields.b->count == index + 2 && world->fields.b->items[index + 1] == child &&
                s->world->chunks[0]->entities[4].count == index + 2 &&
                s->world->chunks[0]->entities[4].items[index + 1] == child;
    for (size_t i = 0; valid && i < index; ++i)
        valid = s->children[i] != child && s->children[i]->base.id != child->base.id;
    s->valid &= valid;
    if (valid) {
        s->children[index] = child; /* No synthetic hold or publication record. */
        s->snapshots[index] =
            (sk_child_snapshot){.size = child_state.watcher_16,
                                .health = child_state.living.Y,
                                .position = {child->base.x, child->base.y, child->base.z},
                                .yaw = child->base.yaw,
                                .pitch = child->base.aT,
                                .random_after = s->parent->random->state,
                                .chunk_members = sk_chunk_members(s, child),
                                .primary_world = child->base.world == s->world->worlds[0],
                                .world_member = sk_world_member(world, child),
                                .parent_dead = s->parent->base.be};
    }
    if (!valid) {
        *cause = (mc_world_storage_error){MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                          "Slime actual child publication input", 0};
        return cause->status;
    }
    if (index == 0 && s->profile != NULL) {
        mc_mob_error error = {0};
        switch (s->profile->change) {
            case SK_CHANGE_NONE:
                break;
            case SK_CHANGE_POSE:
                s->mutation_returned =
                    mc_entity_set_position(s->parent, 10.5, 65, 9.5) == MC_ENTITY_OK;
                break;
            case SK_CHANGE_SIZE:
                s->mutation_returned = mc_mob_slime_set_size(s->parent, 4, &error) == MC_MOB_OK;
                break;
            case SK_CHANGE_RESEED:
                mc_java_random_reseed(s->parent->random, 12345);
                s->mutation_returned = true;
                break;
            case SK_CHANGE_NULL_WORLD: {
                mc_world_runtime *old = s->parent->base.world;
                s->parent->base.world = NULL;
                mc_world_runtime_binding_release(old); /* Actual owning Source assignment. */
                s->mutation_returned = true;
                break;
            }
        }
    }
    *cause = (mc_world_storage_error){0};
    return MC_WORLD_STORAGE_OK;
}

static bool sk_run(slime_removal_fixture *s) {
    fixture *f = s->world;
    const bool split = strcmp(s->mode, "split") == 0;
    const mc_entity_class_literal *kind = NULL;
    mc_mob_error mob = {0};
    mc_mob_state state = {0};
    bool inserted = false;
    SK_REQUIRE(mc_entity_class_literal_get(f->entity_classes[0], MC_ENTITY_CLASS_KIND_SLIME,
                                           &kind) == MC_ENTITY_OK &&
               mc_mob_allocate(f->entity_classes[0], f->registry[0], kind, &s->parent, &mob) ==
                   MC_MOB_OK &&
               mc_mob_construct(s->parent, f->worlds[0], &mob) == MC_MOB_OK &&
               mc_mob_slime_set_size(s->parent, 2, &mob) == MC_MOB_OK &&
               mc_entity_set_position(s->parent, 8.5, 64, 8.5) == MC_ENTITY_OK &&
               mc_mob_inspect(s->parent, &state) && state.watcher_16_present &&
               state.watcher_16 == 2 && state.living.Y == 4 &&
               mc_world_runtime_insert_entity(f->worlds[0], s->parent, &inserted, NULL) ==
                   MC_WORLD_STORAGE_OK &&
               inserted);
    s->amount = split ? 4 : 5;
    s->damage_status = mc_entity_effect_damage_with_math(s->parent, f->registry[0], s->math, NULL,
                                                         s->amount, &s->accepted, &s->damage_error);
    SK_REQUIRE(s->damage_status == MC_ENTITY_OK && s->accepted &&
               s->damage_error.living_damage.death_invoke_reached &&
               s->damage_error.death.returned && !mc_entity_effect_has_pending(s->parent) &&
               mc_mob_inspect(s->parent, &state) && state.living.Y == (split ? 0 : -1) &&
               state.living.ah && state.watcher_16_present && state.watcher_16 == 2 &&
               !s->parent->base.be && f->worlds[0]->entity_membership.count == 1 &&
               f->worlds[0]->fields.b->count == 1 && f->chunks[0]->entities[4].count == 1);
    /* Preserve the post-damage size and real private stream. Child constructors
     * consume their own Random/Java Math entropy, never these four parent draws. */
    s->captured_size = state.watcher_16;
    s->parent_before = s->parent_expected = *s->parent->random;
    if (split)
        for (size_t i = 0; i < 4; ++i) {
            const float draw = mc_java_random_next_float(&s->parent_expected);
            volatile float yaw = draw * 360.f;
            s->expected_yaw[i] = yaw;
        }
    const mc_world_access_callbacks callbacks = {.entity_added = sk_added};
    SK_REQUIRE(mc_world_access_create(&callbacks, s, &s->observer, NULL) == MC_WORLD_STORAGE_OK &&
               mc_world_access_add_receipted(f->worlds[0], s->observer, &s->observer_occurrence,
                                             NULL) == MC_WORLD_STORAGE_OK);
    /* Existing public K dispatcher. No tick, fake split, health repair or
     * caller-provided child return bridges its current UW dependency. */
    s->removal_status =
        mc_entity_effect_dead_with_context(s->parent, f->registry[0], s->math, &s->removal_error);
    s->health = s->parent->mob->living.Y;
    s->parent_dead = s->parent->base.be;
    s->world_count = f->worlds[0]->fields.b->count;
    s->chunk_count = f->chunks[0]->entities[4].count;
    s->random_matches = sk_random_same(s->parent->random, &s->parent_expected);
    s->observed =
        s->removal_status == MC_ENTITY_OK && s->valid && s->parent_dead &&
        s->publications == (split ? 4u : 0u) && s->world_count == (split ? 5u : 1u) &&
        s->chunk_count == (split ? 5u : 1u) && s->random_matches && s->health == (split ? 0 : -1) &&
        f->worlds[0]->entity_membership.count == s->world_count &&
        f->worlds[0]->entity_membership.entries[0] == s->parent &&
        f->worlds[0]->fields.b->items[0] == s->parent &&
        f->chunks[0]->entities[4].items[0] == s->parent && mc_mob_inspect(s->parent, &state) &&
        state.watcher_16 == s->captured_size && !mc_entity_effect_has_pending(s->parent);
    if (split)
        for (size_t i = 0; s->observed && i < 4; ++i) {
            mc_entity *child = s->children[i];
            s->observed &=
                child != NULL && f->worlds[0]->entity_membership.entries[i + 1] == child &&
                f->worlds[0]->fields.b->items[i + 1] == child &&
                f->chunks[0]->entities[4].items[i + 1] == child && child->base.x == sk_x(s, i) &&
                child->base.y == 64.5 && child->base.z == sk_z(s, i) &&
                child->base.yaw == s->expected_yaw[i] && mc_mob_inspect(child, &state) &&
                state.watcher_16 == 1 && state.living.Y == 1 && !child->base.be;
        }
    return true;
}

static const sk_profile *sk_find_profile(const char *mode) {
    for (size_t i = 0; i < COUNT(sk_profiles); ++i)
        if (strcmp(mode, sk_profiles[i].name) == 0)
            return &sk_profiles[i];
    return NULL;
}

static bool sk_run_direct(slime_removal_fixture *s) {
    fixture *f = s->world;
    const sk_profile *p = s->profile;
    const mc_entity_class_literal *kind = NULL;
    mc_mob_error mob = {0};
    mc_mob_state state = {0};
    SK_REQUIRE(
        p != NULL &&
        mc_entity_class_literal_get(f->entity_classes[0], MC_ENTITY_CLASS_KIND_SLIME, &kind) ==
            MC_ENTITY_OK &&
        mc_mob_allocate(f->entity_classes[0], f->registry[0], kind, &s->parent, &mob) ==
            MC_MOB_OK &&
        mc_mob_construct(s->parent, p->null_world ? NULL : f->worlds[0], &mob) == MC_MOB_OK &&
        mc_mob_slime_set_size(s->parent, p->size, &mob) == MC_MOB_OK &&
        mc_entity_set_position(s->parent, 8.5, 64, 8.5) == MC_ENTITY_OK &&
        mc_mob_inspect(s->parent, &state) && state.watcher_16_present &&
        state.watcher_16 == mc_java_i8_narrow(p->size) && state.living.Y == p->size * p->size);
    /* Declared prepared operands for a direct K call, not a forged child
     * return. Null-World parents were genuinely constructed that way and stay
     * unpublished; no owning World field is repaired for teardown. */
    s->parent->mob->living.Y = p->health;
    f->worlds[0]->fields.B = p->remote;
    s->captured_size = state.watcher_16;
    if (!p->null_world) {
        bool inserted = false;
        SK_REQUIRE(mc_world_runtime_insert_entity(f->worlds[0], s->parent, &inserted, NULL) ==
                       MC_WORLD_STORAGE_OK &&
                   inserted);
    }
    if (p->wrong_type) {
        mc_entity_watch_record *record = sk_size_record(s->parent);
        SK_REQUIRE(record != NULL && record->type == 0 &&
                   record->payload_kind == MC_ENTITY_WATCH_PAYLOAD_BYTE &&
                   mc_entity_watch_integer_replace(record, 16) && record->type == 0 &&
                   record->payload_kind == MC_ENTITY_WATCH_PAYLOAD_INTEGER);
    }
    mc_java_random_reseed(s->parent->random, 38);
    s->parent_before = s->parent_expected = *s->parent->random;
    for (size_t i = 0; i < p->draws; ++i) {
        const float draw = mc_java_random_next_float(&s->parent_expected);
        volatile float yaw = draw * 360.f;
        s->expected_yaw[i] = yaw;
        if (i == 0 && p->change == SK_CHANGE_RESEED)
            mc_java_random_reseed(&s->parent_expected, 12345);
    }
    const mc_world_access_callbacks callbacks = {.entity_added = sk_added};
    SK_REQUIRE(mc_world_access_create(&callbacks, s, &s->observer, NULL) == MC_WORLD_STORAGE_OK &&
               mc_world_access_add_receipted(f->worlds[0], s->observer, &s->observer_occurrence,
                                             NULL) == MC_WORLD_STORAGE_OK);
    s->removal_status =
        mc_entity_effect_dead_with_context(s->parent, f->registry[0], s->math, &s->removal_error);
    s->health = s->parent->mob->living.Y;
    s->parent_dead = s->parent->base.be;
    s->world_count = f->worlds[0]->fields.b->count;
    s->chunk_count = f->chunks[0]->entities[4].count;
    s->random_matches = sk_random_same(s->parent->random, &s->parent_expected);
    const bool failed = strcmp(p->exception, "NONE") != 0;
    const size_t expected_count = p->publications + (p->null_world ? 0u : 1u);
    const mc_slime_removal_state *receipt = &s->removal_error.slime_removal;
    SK_REQUIRE(receipt->entered && !receipt->active && receipt->returned == !failed &&
               receipt->be_stored == !failed && receipt->site.class_name != NULL &&
               strcmp(receipt->site.class_name, "uw") == 0 && receipt->site.method != NULL &&
               strcmp(receipt->site.method, "K") == 0 && receipt->site.descriptor != NULL &&
               strcmp(receipt->site.descriptor, "()V") == 0);
    if (failed) {
        const bool cast = p->wrong_type;
        SK_REQUIRE(s->removal_status == MC_ENTITY_STATE_ERROR && receipt->source_exception &&
                   !receipt->source_error && !receipt->native_policy &&
                   receipt->exception_class != NULL &&
                   strcmp(receipt->exception_class, cast ? "java/lang/ClassCastException"
                                                         : "java/lang/NullPointerException") == 0);
        if (cast)
            SK_REQUIRE(!receipt->size_read && !receipt->world_b_read && !receipt->health_read &&
                       receipt->removal_pc == 1 && receipt->watcher_pc == 19 &&
                       receipt->watcher_index == 16 && receipt->child.class_name != NULL &&
                       strcmp(receipt->child.class_name, "ud") == 0 && receipt->child.pc == 19);
        else if (p->null_world)
            SK_REQUIRE(receipt->size_read && receipt->size == 1 && !receipt->world_b_read &&
                       !receipt->health_read && receipt->removal_pc == 9 &&
                       receipt->children_attempted == 0 && receipt->parent_draws == 0);
        else
            SK_REQUIRE(receipt->size_read && receipt->size == 2 && receipt->world_b_read &&
                       receipt->health_read && receipt->health == 0 && receipt->removal_pc == 129 &&
                       receipt->constructors_returned == 2 && receipt->sizes_returned == 2 &&
                       receipt->poses_returned == 2 && receipt->parent_draws == 2 &&
                       receipt->constructor_worlds[1] == 0 && receipt->publication_worlds[1] == 0 &&
                       receipt->current_child != NULL &&
                       receipt->current_child->base.world == NULL && receipt->pending);
    } else {
        SK_REQUIRE(s->removal_status == MC_ENTITY_OK && receipt->size_read &&
                   receipt->size == s->captured_size && receipt->world_b_read &&
                   receipt->world_remote == p->remote && receipt->removal_pc == 143 &&
                   (size_t)receipt->children_completed == p->publications &&
                   (size_t)receipt->parent_draws == p->draws && !receipt->pending &&
                   receipt->health_read == (!p->remote && s->captured_size > 1));
    }
    s->observed = s->valid && s->parent_dead == !failed && s->publications == p->publications &&
                  s->world_count == expected_count && s->chunk_count == expected_count &&
                  f->worlds[0]->entity_membership.count == expected_count && s->random_matches &&
                  s->health == p->final_health &&
                  (p->change == SK_CHANGE_NONE || s->mutation_returned) &&
                  mc_mob_inspect(s->parent, &state) && state.watcher_16 == p->final_size;
    for (size_t i = 0; s->observed && i < p->publications; ++i) {
        mc_entity *child = s->children[i];
        s->observed &= child != NULL && f->worlds[0]->entity_membership.entries[i + 1] == child &&
                       f->worlds[0]->fields.b->items[i + 1] == child &&
                       f->chunks[0]->entities[4].items[i + 1] == child &&
                       child->base.x == sk_x(s, i) && child->base.y == sk_y(s, i) &&
                       child->base.z == sk_z(s, i) && child->base.yaw == s->expected_yaw[i] &&
                       mc_mob_inspect(child, &state) && state.watcher_16 == 1 &&
                       state.living.Y == 1 && !child->base.be;
    }
    return true;
}

static uint32_t sk_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void sk_emit_json(const slime_removal_fixture *s) {
    const mc_slime_removal_state *r = &s->removal_error.slime_removal;
    const char *exception = s->removal_status == MC_ENTITY_OK ? "NONE" : "UNRESOLVED";
    if (r->source_exception && r->exception_class != NULL)
        exception = strcmp(r->exception_class, "java/lang/NullPointerException") == 0 ? "NPE"
                    : strcmp(r->exception_class, "java/lang/ClassCastException") == 0
                        ? "CCE"
                        : "UNRESOLVED";
    printf("SLIME_REMOVAL {\"profile\":\"%s\",\"step\":0,\"set_size\":%d,\"size\":", s->mode,
           s->profile == NULL ? 2 : s->profile->size);
    const mc_entity_watch_record *record = s->parent == NULL ? NULL : sk_size_record(s->parent);
    if (record == NULL || record->payload_kind != MC_ENTITY_WATCH_PAYLOAD_BYTE)
        printf("null");
    else
        printf("%d", (int)record->byte_value);
    printf(",\"health\":%d,\"remote\":", s->health);
    if (s->parent == NULL || s->parent->base.world == NULL)
        printf("null");
    else
        printf("%s", s->parent->base.world->fields.B ? "true" : "false");
    printf(",\"exception\":\"%s\",\"dead\":%s,\"random\":", exception,
           s->parent_dead ? "true" : "false");
    if (s->parent == NULL || s->parent->random == NULL)
        printf("null");
    else
        printf("\"%016" PRIx64 "\"", s->parent->random->state);
    printf(",\"world_loaded\":%zu,\"children\":[", s->world_count);
    for (size_t i = 0; i < s->publications && i < COUNT(s->snapshots); ++i) {
        const sk_child_snapshot *c = &s->snapshots[i];
        printf("%s{\"index\":%zu,\"size\":%d,\"health\":%d,\"position\":[\"%016" PRIx64
               "\",\"%016" PRIx64 "\",\"%016" PRIx64 "\"],\"yaw\":\"%08" PRIx32
               "\",\"pitch\":\"%08" PRIx32 "\",\"world\":%s,\"world_member\":%s,"
               "\"chunk_members\":%zu,\"parent_dead\":%s,\"random_after\":\"%016" PRIx64 "\"}",
               i == 0 ? "" : ",", i, c->size, c->health, sk_double_bits(c->position[0]),
               sk_double_bits(c->position[1]), sk_double_bits(c->position[2]),
               sk_float_bits(c->yaw), sk_float_bits(c->pitch),
               c->primary_world ? "\"PRIMARY\"" : "null", c->world_member ? "true" : "false",
               c->chunk_members, c->parent_dead ? "true" : "false", c->random_after);
    }
    printf("]}\n");
}

static bool sk_cleanup(slime_removal_fixture *s) {
    if (!mc_entity_effect_error_retire(&s->removal_error) ||
        !mc_entity_effect_error_retire(&s->damage_error))
        return false;
    if (s->observer != NULL &&
        (mc_world_access_detach_occurrence(s->world->worlds[0], s->observer, s->observer_occurrence,
                                           NULL) != MC_WORLD_STORAGE_OK ||
         mc_world_access_free(s->observer, NULL) != MC_WORLD_STORAGE_OK))
        return false;
    s->observer = NULL;
    for (size_t i = 0; i < 4; ++i)
        s->children[i] = NULL; /* Expire World borrows before terminal work. */
    mc_entity_release(s->parent);
    s->parent = NULL;
    if (!tick_cleanup(s->world))
        return false;
    s->world = NULL; /* The imported checked teardown consumes its fixture. */
    if (mc_math_classes_release(&s->math, NULL) != MC_MATH_CLASS_OK)
        return false;
    return true;
}

int main(int argc, char **argv) {
    char root[8192];
    if (argc != 3 ||
        (strcmp(argv[1], "split") != 0 && strcmp(argv[1], "negative-health") != 0 &&
         sk_find_profile(argv[1]) == NULL) ||
        !decode_path(argv[2], root, sizeof(root)))
        return 2;
    slime_removal_fixture *s = calloc(1, sizeof(*s));
    if (s == NULL)
        return 1;
    s->world = calloc(1, sizeof(*s->world));
    s->mode = argv[1];
    s->profile = sk_find_profile(s->mode);
    s->valid = true;
    mc_entity_effect_error_init(&s->damage_error);
    mc_entity_effect_error_init(&s->removal_error);
    printf("SLIME_REMOVAL_BEGIN mode=%s\n", s->mode);
    fflush(stdout);
    const bool prepared =
        s->world != NULL && mc_math_classes_create(SIZE_MAX, &s->math, NULL) == MC_MATH_CLASS_OK &&
        mc_math_classes_ensure(s->math, NULL) == MC_MATH_CLASS_OK &&
        tick_setup_configured(s->world, root, 4, mc_math_classes_table(s->math)) &&
        (s->profile == NULL ? sk_run(s) : sk_run_direct(s));
    sk_emit_json(s);
    const bool cleaned = sk_cleanup(s);
    printf("SLIME_REMOVAL_OBSERVATION mode=%s prepared=%u damage_status=%u accepted=%u "
           "amount=%d health=%d K_status=%u K_cause=%u K_pc=%u children=%zu world=%zu "
           "chunk=%zu dead=%u random_matches=%u callback_valid=%u cleanup=%u\n",
           s->mode, (unsigned int)prepared, (unsigned int)s->damage_status,
           (unsigned int)s->accepted, s->amount, s->health, (unsigned int)s->removal_status,
           (unsigned int)s->removal_error.cause, (unsigned int)s->removal_error.close_bytecode,
           s->publications, s->world_count, s->chunk_count, (unsigned int)s->parent_dead,
           (unsigned int)s->random_matches, (unsigned int)s->valid, (unsigned int)cleaned);
    const bool passed = prepared && cleaned && s->observed;
    if (!passed)
        fprintf(stderr, "Slime actual K contract failed;mode=%s prepared=%u cleanup=%u\n", s->mode,
                (unsigned int)prepared, (unsigned int)cleaned);
    free(s);
    if (!passed)
        return 1;
    printf("PASS: %u actual Slime removal checks;mode=%s\n", checks, argv[1]);
    return 0;
}
