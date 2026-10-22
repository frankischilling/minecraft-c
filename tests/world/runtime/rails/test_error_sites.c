/* Constructed diagnostic records exercise the error transport contract. */
#include "../../../../src/world/runtime/mutation/mutation_internal.h"
#include "../../../../src/world/runtime/mutation/rails/error_sites_internal.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static unsigned int checks;
static unsigned int cases;

#define CHECK(expression)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(expression)) {                                                                       \
            fprintf(stderr, "FAIL rail.error_sites:%d: %s\n", __LINE__, #expression);              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

enum site_kind { DETECTOR, LIFECYCLE, CONNECTION, SEARCH };

typedef struct site_slot {
    uint32_t bit;
    size_t offset;
    enum site_kind kind;
    size_t cause;
} site_slot;

/* This table names record slots, not any gameplay transition or dispatch. */
#define SLOT(bit_name, member, type, cause_index)                                                  \
    { MC_WORLD_RAIL_SITE_##bit_name, offsetof(mc_world_mutation_error, member), type, cause_index }
static const site_slot slots[] = {
    SLOT(DETECTOR_CONTACT, rail_detector_contact, DETECTOR, 3),
    SLOT(DETECTOR_TICK, rail_detector_tick, DETECTOR, 3),
    SLOT(DETECTOR_UPDATE, rail_detector_update, DETECTOR, 3),
    SLOT(DETECTOR_CAUSE, rail_detector_cause, DETECTOR, 3),
    SLOT(ADDED, rail_added, LIFECYCLE, 7),
    SLOT(SHAPE, rail_shape, LIFECYCLE, 7),
    SLOT(NEIGHBOR, rail_neighbor, LIFECYCLE, 7),
    SLOT(LIFECYCLE_CAUSE, rail_lifecycle_cause, LIFECYCLE, 7),
    SLOT(CONNECTION, rail_connection, CONNECTION, 9),
    SLOT(CONNECTION_CAUSE, rail_connection_cause, CONNECTION, 9),
    SLOT(PREDICATE, rail_predicate, CONNECTION, 11),
    SLOT(PREDICATE_CAUSE, rail_predicate_cause, CONNECTION, 11),
    SLOT(ACCESSOR, rail_accessor, CONNECTION, 13),
    SLOT(ACCESSOR_CAUSE, rail_accessor_cause, CONNECTION, 13),
    SLOT(SEARCH, rail_search, SEARCH, 16),
    SLOT(VERIFY, rail_verify, SEARCH, 16),
    SLOT(SEARCH_CAUSE, rail_search_cause, SEARCH, 16),
};
#undef SLOT

static const size_t currents[] = {0, 1, 2, 4, 5, 6, 8, 10, 12, 14, 15};
static const size_t causes[] = {3, 7, 9, 11, 13, 16};
static const uint32_t all_current =
    MC_WORLD_RAIL_SITE_DETECTOR_CONTACT | MC_WORLD_RAIL_SITE_DETECTOR_TICK |
    MC_WORLD_RAIL_SITE_DETECTOR_UPDATE | MC_WORLD_RAIL_SITE_ADDED | MC_WORLD_RAIL_SITE_SHAPE |
    MC_WORLD_RAIL_SITE_NEIGHBOR | MC_WORLD_RAIL_SITE_CONNECTION | MC_WORLD_RAIL_SITE_PREDICATE |
    MC_WORLD_RAIL_SITE_ACCESSOR | MC_WORLD_RAIL_SITE_SEARCH | MC_WORLD_RAIL_SITE_VERIFY;
static const uint32_t all_cause =
    MC_WORLD_RAIL_SITE_DETECTOR_CAUSE | MC_WORLD_RAIL_SITE_LIFECYCLE_CAUSE |
    MC_WORLD_RAIL_SITE_CONNECTION_CAUSE | MC_WORLD_RAIL_SITE_PREDICATE_CAUSE |
    MC_WORLD_RAIL_SITE_ACCESSOR_CAUSE | MC_WORLD_RAIL_SITE_SEARCH_CAUSE;
static const uint32_t one_per_channel =
    MC_WORLD_RAIL_SITE_DETECTOR_TICK | MC_WORLD_RAIL_SITE_NEIGHBOR | MC_WORLD_RAIL_SITE_CONNECTION |
    MC_WORLD_RAIL_SITE_PREDICATE | MC_WORLD_RAIL_SITE_ACCESSOR | MC_WORLD_RAIL_SITE_VERIFY;

typedef struct site_value {
    bool entered;
    const char *declaring_class, *method;
    unsigned int bytecode;
    int32_t x, y, z, metadata, depth, axis;
    bool forward;
} site_value;

/* Access only the actual declared member type; do not compare struct padding. */
static site_value value(const mc_world_mutation_error *error, size_t slot) {
    const unsigned char *address = (const unsigned char *)error + slots[slot].offset;
    site_value out = {0};
    switch (slots[slot].kind) {
        case DETECTOR: {
            const mc_world_detector_site *s = (const mc_world_detector_site *)address;
            out = (site_value){
                s->entered, s->declaring_class, s->method, s->bytecode, s->x, s->y, s->z, 0, 0, 0,
                false};
            break;
        }
        case LIFECYCLE: {
            const mc_rail_lifecycle_site *s = (const mc_rail_lifecycle_site *)address;
            out = (site_value){
                s->entered, s->declaring_class, s->method, s->bytecode, s->x, s->y, s->z, 0, 0, 0,
                false};
            break;
        }
        case CONNECTION: {
            const mc_rail_connection_site *s = (const mc_rail_connection_site *)address;
            out = (site_value){s->entered, s->class_name, s->method, s->bytecode, 0, 0, 0, 0, 0,
                               0,          false};
            break;
        }
        case SEARCH: {
            const mc_rail_power_search_site *s = (const mc_rail_power_search_site *)address;
            out = (site_value){
                s->entered, s->declaring_class, s->method, s->bytecode, s->x,      s->y,
                s->z,       s->metadata,        s->depth,  s->axis,     s->forward};
            break;
        }
    }
    return out;
}

static bool same_value(site_value a, site_value b) {
    return a.entered == b.entered && a.declaring_class == b.declaring_class &&
           a.method == b.method && a.bytecode == b.bytecode && a.x == b.x && a.y == b.y &&
           a.z == b.z && a.metadata == b.metadata && a.depth == b.depth && a.axis == b.axis &&
           a.forward == b.forward;
}

static void fill_sites(mc_world_mutation_error *error, unsigned int variant) {
    for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
        const unsigned int pc = variant * 100u + (unsigned int)i;
        const int32_t x = (int32_t)pc;
        const char *class_name = variant == 1u ? "fixture.lower" : "fixture.outer";
        const char *method = variant == 1u ? "lower()" : "outer()";
        unsigned char *address = (unsigned char *)error + slots[i].offset;
        switch (slots[i].kind) {
            case DETECTOR:
                *(mc_world_detector_site *)address =
                    (mc_world_detector_site){true, class_name, method, pc, x, -x, x + 1};
                break;
            case LIFECYCLE:
                *(mc_rail_lifecycle_site *)address =
                    (mc_rail_lifecycle_site){true, class_name, method, pc, x, -x, x + 1};
                break;
            case CONNECTION:
                *(mc_rail_connection_site *)address =
                    (mc_rail_connection_site){true, class_name, method, pc};
                break;
            case SEARCH:
                *(mc_rail_power_search_site *)address = (mc_rail_power_search_site){
                    true, class_name, method, pc, x, -x, x + 1, 15, -x, 2, variant == 1u};
                break;
        }
    }
}

/* All values below are constructed error-transport fixtures. No runtime owner,
 * game error, source PC witness, Entity or power result is manufactured. */
static mc_world_mutation_error lower_fixture(void) {
    mc_world_mutation_error e = mc_world_mutation_empty_error();
    e.status = MC_WORLD_STORAGE_IO_ERROR;
    e.operation = "fixture.lower.operation";
    e.wrapper_bytecode = 31;
    e.bytecode = 32;
    e.chunk_bytecode = 33;
    e.pressure_bytecode = 34;
    e.piston_finalize_bytecode = 35;
    e.item = MC_ITEM_STACK_LIMIT;
    e.record = MC_SCHEDULED_TICK_LIMIT;
    e.membership = MC_SCHEDULED_MEMBERSHIP_HOLD_LIMIT;
    e.ordered = MC_SCHEDULED_ORDERED_EMPTY;
    e.entity.status = MC_ENTITY_LIMIT;
    e.entity.operation = "fixture.entity";
    e.entity.bytecode = 36;
    e.publication.status = MC_WORLD_STORAGE_BUSY;
    e.publication.observer_bytecode = 37;
    e.batch.index = 5;
    e.batch.publication.provider_bytecode = 38;
    e.tile.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    e.tile.tile.status = MC_TILE_LIMIT;
    e.tile.tile.piston_initializer_bytecode = 39;
    e.tile.acquisition_parent.inventory_bytecode = 40;
    e.load_entity.index = 6;
    e.load_entity.native.system_error = 45;
    e.bounds.status = MC_ENTITY_UNSUPPORTED_FP;
    e.bounds.bytecode = 46;
    e.bounds.pool.bytecode = 47;
    e.material.bytecode = 48;
    e.query.chunk_bytecode = 49;
    e.light.provider_bytecode = 50;
    e.population.reached = true;
    e.population.cause.system_error = 55;
    e.cause = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR, "fixture.storage", 1234};
    e.registry.block_id = 28;
    e.registry.initializer_pc = 41;
    e.registry.active_initializer_class = "fixture.registry";
    e.registry.active_initializer_pc = 42;
    e.registry.constructor_class = "fixture.constructor";
    e.registry.constructor_pc = 43;
    e.registry.item_argument = -7;
    e.nearby.query.status = MC_WORLD_STORAGE_LIMIT;
    e.nearby.query.provider_bytecode = 51;
    e.nearby.query.registry.block_id = 52;
    e.nearby.slice = 3;
    e.nearby.index = 4;
    e.nearby.activation_bytecode = 53;
    e.nearby.load_builder_bytecode = 54;
    e.schedule.status = MC_WORLD_STORAGE_BUSY;
    e.schedule.operation = "fixture.schedule";
    e.schedule.bytecode = 61;
    e.schedule.presence_bytecode = 62;
    e.schedule.provider_bytecode = 63;
    e.schedule.cause.system_error = 64;
    e.schedule.registry.item_argument = 65;
    e.power.status = MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    e.power.operation = "fixture.power";
    e.power.world_class = "fixture.World";
    e.power.cause.system_error = 71;
    e.power.weak_neighbors = (mc_world_power_site){true, 0, -1, 0, 1, -1};
    e.power.weak_at = (mc_world_power_site){true, 72, 7, 8, 9, 5};
    e.power.strong_neighbors = (mc_world_power_site){true, 73, 3, 2, 1, -1};
    e.power.strong_at = (mc_world_power_site){false, 0, 1, 2, 3, 4};
    e.power.weak = (mc_world_block_power_site){true, "fixture.Block", "weak()", 74};
    e.power.wire_connectivity = (mc_world_block_power_site){true, "fixture.Wire", "connect()", 75};
    e.power.support.bytecode = 76;
    e.power.support.registry.block_id = 77;
    e.power.access.query.provider_bytecode = 78;
    e.power.registry.constructor_pc = 79;
    e.rail_access.world_bytecode = 81;
    e.rail_access.block_bytecode = 82;
    e.rail_access.piston_bytecode = 83;
    e.rail_access.shifted_bytecode = 84;
    e.rail_access.block_class = "fixture.CollisionBlock";
    e.rail_access.x = INT32_MIN;
    e.rail_access.y = 127;
    e.rail_access.z = INT32_MAX;
    e.rail_access.nearby.query.bytecode = 85;
    e.rail_access.nearby.query.registry.initializer_pc = 86;
    e.rail_access.nearby.slice = 7;
    e.rail_support.status = MC_WORLD_STORAGE_MISSING;
    e.rail_support.operation = "fixture.support";
    e.rail_support.bytecode = 91;
    e.rail_support.cause.system_error = 92;
    e.rail_support.registry.active_initializer_pc = 93;
    e.hook_class = "fixture.lower.hook";
    e.hook_method = "lowerHook()";
    e.hook_bytecode = 0;
    e.cause_hook_class = "fixture.lower.deepest";
    e.cause_hook_method = "deepestHook()";
    e.cause_hook_bytecode = 94;
    fill_sites(&e, 1);
    return e;
}

static mc_world_mutation_error outer_fixture(void) {
    mc_world_mutation_error e = mc_world_mutation_empty_error();
    e.status = MC_WORLD_STORAGE_INVALID_ARGUMENT;
    e.operation = "fixture.outer.operation";
    e.power.status = MC_WORLD_STORAGE_BUSY;
    e.schedule.bytecode = 201;
    e.hook_class = "fixture.outer.saved.hook";
    e.hook_method = "savedOuterHook()";
    e.hook_bytecode = 202;
    fill_sites(&e, 2);
    return e;
}

static bool retained(const mc_world_mutation_error *out, const mc_world_mutation_error *lower) {
    CHECK(out->status == lower->status && out->operation == lower->operation);
    CHECK(out->wrapper_bytecode == lower->wrapper_bytecode && out->bytecode == lower->bytecode);
    CHECK(out->chunk_bytecode == lower->chunk_bytecode);
    CHECK(out->pressure_bytecode == lower->pressure_bytecode);
    CHECK(out->piston_finalize_bytecode == lower->piston_finalize_bytecode);
    CHECK(out->item == lower->item && out->record == lower->record);
    CHECK(out->membership == lower->membership && out->ordered == lower->ordered);
    /* These exact object representations were copied by the public transport
     * contract and are not overlaid. Padding is never used to compare sites. */
    CHECK(memcmp(&out->cause, &lower->cause, sizeof(out->cause)) == 0);
    CHECK(memcmp(&out->registry, &lower->registry, sizeof(out->registry)) == 0);
    CHECK(memcmp(&out->entity, &lower->entity, sizeof(out->entity)) == 0);
    CHECK(memcmp(&out->publication, &lower->publication, sizeof(out->publication)) == 0);
    CHECK(memcmp(&out->batch, &lower->batch, sizeof(out->batch)) == 0);
    CHECK(memcmp(&out->tile, &lower->tile, sizeof(out->tile)) == 0);
    CHECK(memcmp(&out->load_entity, &lower->load_entity, sizeof(out->load_entity)) == 0);
    CHECK(memcmp(&out->bounds, &lower->bounds, sizeof(out->bounds)) == 0);
    CHECK(memcmp(&out->material, &lower->material, sizeof(out->material)) == 0);
    CHECK(memcmp(&out->query, &lower->query, sizeof(out->query)) == 0);
    CHECK(memcmp(&out->light, &lower->light, sizeof(out->light)) == 0);
    CHECK(memcmp(&out->population, &lower->population, sizeof(out->population)) == 0);
    CHECK(memcmp(&out->nearby, &lower->nearby, sizeof(out->nearby)) == 0);
    CHECK(memcmp(&out->schedule, &lower->schedule, sizeof(out->schedule)) == 0);
    CHECK(memcmp(&out->power, &lower->power, sizeof(out->power)) == 0);
    CHECK(memcmp(&out->rail_access, &lower->rail_access, sizeof(out->rail_access)) == 0);
    CHECK(memcmp(&out->rail_support, &lower->rail_support, sizeof(out->rail_support)) == 0);
    return true;
}

static bool empty_frame(void) {
    ++cases;
    const mc_world_mutation_error e = mc_world_mutation_empty_error();
    const site_value zero = {0};
    CHECK(sizeof(slots) / sizeof(slots[0]) == 17);
    CHECK(e.rail_sites_selected == 0);
    for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i)
        CHECK(same_value(value(&e, i), zero));
    CHECK(e.status == MC_WORLD_STORAGE_OK && e.operation == NULL);
    CHECK(e.hook_class == NULL && e.hook_method == NULL && e.hook_bytecode == 0);
    CHECK(e.cause_hook_class == NULL && e.cause_hook_method == NULL && e.cause_hook_bytecode == 0);
    CHECK(e.registry.initializer_pc == -1 && e.registry.block_id == -1);
    CHECK(e.power.status == MC_WORLD_STORAGE_OK && e.power.operation == NULL);
    CHECK(!e.power.weak_neighbors.entered && !e.power.weak_at.entered);
    CHECK(!e.power.strong_neighbors.entered && !e.power.strong_at.entered);
    CHECK(e.power.registry.initializer_pc == -1 && e.power.registry.block_id == -1);
    CHECK(e.rail_access.world_bytecode == 0 && e.rail_access.block_bytecode == 0);
    CHECK(e.rail_access.nearby.query.registry.block_id == -1);
    CHECK(e.rail_support.status == MC_WORLD_STORAGE_OK && e.rail_support.operation == NULL);
    CHECK(e.rail_support.registry.initializer_pc == -1 && e.rail_support.registry.block_id == -1);
    return true;
}

static bool deepest_and_restore(void) {
    ++cases;
    mc_world_mutation_error lower = lower_fixture(), outer = outer_fixture(), out;
    lower.rail_sites_selected = all_current | all_cause;
    outer.rail_sites_selected = all_current | all_cause;
    const mc_world_rail_merge_plan plan = {.restore_current = all_current,
                                           .outer_origin = one_per_channel,
                                           .lower_cause_hook_selected = true};
    errno = ERANGE;
    const bool ok = mc_world_rail_error_merge(&out, &lower, &outer, plan);
    const int saved_errno = errno;
    CHECK(ok && saved_errno == ERANGE);
    CHECK(out.rail_sites_selected == (all_current | all_cause));
    for (size_t i = 0; i < sizeof(currents) / sizeof(currents[0]); ++i)
        CHECK(same_value(value(&out, currents[i]), value(&outer, currents[i])));
    for (size_t i = 0; i < sizeof(causes) / sizeof(causes[0]); ++i)
        CHECK(same_value(value(&out, causes[i]), value(&lower, causes[i])));
    CHECK(retained(&out, &lower));
    CHECK(out.cause_hook_class == lower.cause_hook_class);

    /* Restore two named currents only. Other selected outer records, including
     * apparent outer deepest causes, cannot overwrite unrelated lower slots. */
    ++cases;
    mc_world_rail_merge_plan narrow = {
        .restore_current = MC_WORLD_RAIL_SITE_SHAPE | MC_WORLD_RAIL_SITE_SEARCH,
        .outer_origin = MC_WORLD_RAIL_SITE_SHAPE | MC_WORLD_RAIL_SITE_SEARCH};
    CHECK(mc_world_rail_error_merge(&out, &lower, &outer, narrow));
    for (size_t i = 0; i < sizeof(slots) / sizeof(slots[0]); ++i) {
        const mc_world_mutation_error *from = i == 5 || i == 14 ? &outer : &lower;
        CHECK(same_value(value(&out, i), value(from, i)));
    }
    CHECK(retained(&out, &lower));
    return true;
}

static bool each_origin(void) {
    for (size_t i = 0; i < sizeof(currents) / sizeof(currents[0]); ++i) {
        const size_t slot = currents[i];
        const uint32_t bit = slots[slot].bit;
        const uint32_t cause = slots[slots[slot].cause].bit;
        mc_world_mutation_error lower = lower_fixture(), outer = outer_fixture(), out;
        lower.rail_sites_selected = bit;
        outer.rail_sites_selected = bit | cause;
        /* Selected-but-unentered and entered PC0 are both real record states.
         * Zero all numeric context, retain names; selection is still explicit. */
        unsigned char *address = (unsigned char *)&lower + slots[slot].offset;
        switch (slots[slot].kind) {
            case DETECTOR:
                *(mc_world_detector_site *)address =
                    (mc_world_detector_site){i % 2 == 0, "fixture.zero", "zero()", 0, 0, 0, 0};
                break;
            case LIFECYCLE:
                *(mc_rail_lifecycle_site *)address =
                    (mc_rail_lifecycle_site){i % 2 == 0, "fixture.zero", "zero()", 0, 0, 0, 0};
                break;
            case CONNECTION:
                *(mc_rail_connection_site *)address =
                    (mc_rail_connection_site){i % 2 == 0, "fixture.zero", "zero()", 0};
                break;
            case SEARCH:
                *(mc_rail_power_search_site *)address = (mc_rail_power_search_site){
                    i % 2 == 0, "fixture.zero", "zero()", 0, 0, 0, 0, 0, 0, 0, false};
                break;
        }
        const mc_world_rail_merge_plan plan = {
            .restore_current = bit, .lower_origin = bit, .outer_origin = bit};
        ++cases;
        CHECK(mc_world_rail_error_merge(&out, &lower, &outer, plan));
        CHECK(out.rail_sites_selected == (bit | cause));
        CHECK(same_value(value(&out, slot), value(&outer, slot)));
        CHECK(same_value(value(&out, slots[slot].cause), value(&lower, slot)));
        CHECK(retained(&out, &lower));
        /* An empty lower channel falls back to the explicitly named outer
         * current, never the separately selected saved outer cause. */
        lower.rail_sites_selected = 0;
        const mc_world_rail_merge_plan fallback = {.restore_current = bit, .outer_origin = bit};
        ++cases;
        CHECK(mc_world_rail_error_merge(&out, &lower, &outer, fallback));
        CHECK(out.rail_sites_selected == (bit | cause));
        CHECK(same_value(value(&out, slot), value(&outer, slot)));
        CHECK(same_value(value(&out, slots[slot].cause), value(&outer, slot)));
        CHECK(retained(&out, &lower));
    }
    return true;
}

static bool selected_causes_and_no_inference(void) {
    mc_world_mutation_error lower = lower_fixture(), outer = outer_fixture(), out;
    const mc_world_rail_merge_plan no_plan = {0};
    for (size_t i = 0; i < sizeof(causes) / sizeof(causes[0]); ++i) {
        const size_t slot = causes[i];
        unsigned char *address = (unsigned char *)&lower + slots[slot].offset;
        /* A selected deepest invocation need not have entered a body. */
        switch (slots[slot].kind) {
            case DETECTOR:
                *(mc_world_detector_site *)address =
                    (mc_world_detector_site){false, "fixture.unentered", "deep()", 0, 0, 0, 0};
                break;
            case LIFECYCLE:
                *(mc_rail_lifecycle_site *)address =
                    (mc_rail_lifecycle_site){false, "fixture.unentered", "deep()", 0, 0, 0, 0};
                break;
            case CONNECTION:
                *(mc_rail_connection_site *)address =
                    (mc_rail_connection_site){false, "fixture.unentered", "deep()", 0};
                break;
            case SEARCH:
                *(mc_rail_power_search_site *)address = (mc_rail_power_search_site){
                    false, "fixture.unentered", "deep()", 0, 0, 0, 0, 0, 0, 0, false};
                break;
        }
    }
    lower.rail_sites_selected = all_cause;
    outer.rail_sites_selected = all_cause;
    ++cases;
    CHECK(mc_world_rail_error_merge(&out, &lower, &outer, no_plan));
    CHECK(memcmp(&out, &lower, sizeof(out)) == 0);
    lower.rail_sites_selected = 0;
    lower.hook_class = "pc";
    lower.hook_method = "a";
    lower.hook_bytecode = 438;
    outer.rail_sites_selected = all_current | all_cause;
    ++cases;
    CHECK(mc_world_rail_error_merge(&out, &lower, &outer, no_plan));
    CHECK(out.rail_sites_selected == 0);
    CHECK(memcmp(&out, &lower, sizeof(out)) == 0);
    return true;
}

static bool explicit_choice_among_currents(void) {
    ++cases;
    mc_world_mutation_error lower = lower_fixture(), outer = outer_fixture(), out;
    lower.rail_sites_selected = all_current;
    outer.rail_sites_selected = all_current;
    const mc_world_rail_merge_plan plan = {.restore_current = all_current,
                                           .lower_origin = one_per_channel,
                                           .outer_origin = one_per_channel};
    CHECK(mc_world_rail_error_merge(&out, &lower, &outer, plan));
    CHECK(out.rail_sites_selected == (all_current | all_cause));
    /* Literal choices distinguish siblings in the three multi-current groups. */
    CHECK(same_value(value(&out, 3), value(&lower, 1)));
    CHECK(same_value(value(&out, 7), value(&lower, 6)));
    CHECK(same_value(value(&out, 9), value(&lower, 8)));
    CHECK(same_value(value(&out, 11), value(&lower, 10)));
    CHECK(same_value(value(&out, 13), value(&lower, 12)));
    CHECK(same_value(value(&out, 16), value(&lower, 15)));
    CHECK(retained(&out, &lower));
    return true;
}

static bool hook_selection(void) {
    for (unsigned int scenario = 0; scenario < 4; ++scenario) {
        ++cases;
        mc_world_mutation_error lower = lower_fixture(), outer = outer_fixture(), out;
        const mc_world_rail_merge_plan plan = {
            .outer_hook = {scenario != 3, "fixture.explicit.outer", "explicitHook()", 0},
            .lower_current_hook_selected = scenario == 1,
            .lower_cause_hook_selected = scenario == 0};
        CHECK(mc_world_rail_error_merge(&out, &lower, &outer, plan));
        CHECK(out.rail_sites_selected == 0);
        CHECK(retained(&out, &lower));
        if (scenario == 0 || scenario == 3) {
            CHECK(out.cause_hook_class == lower.cause_hook_class);
            CHECK(out.cause_hook_method == lower.cause_hook_method);
            CHECK(out.cause_hook_bytecode == lower.cause_hook_bytecode);
        } else if (scenario == 1) {
            CHECK(out.cause_hook_class == lower.hook_class);
            CHECK(out.cause_hook_method == lower.hook_method);
            CHECK(out.cause_hook_bytecode == 0);
        } else {
            CHECK(out.cause_hook_class == plan.outer_hook.declaring_class);
            CHECK(out.cause_hook_method == plan.outer_hook.method);
            CHECK(out.cause_hook_bytecode == 0);
        }
        if (plan.outer_hook.selected) {
            CHECK(out.hook_class == plan.outer_hook.declaring_class);
            CHECK(out.hook_method == plan.outer_hook.method && out.hook_bytecode == 0);
        } else {
            CHECK(out.hook_class == lower.hook_class && out.hook_method == lower.hook_method);
            CHECK(out.hook_bytecode == lower.hook_bytecode);
        }
    }
    return true;
}

static bool aliases(void) {
    /* All five partitions of the three full-record pointer arguments. */
    for (unsigned int partition = 0; partition < 5; ++partition) {
        ++cases;
        mc_world_mutation_error lower = lower_fixture(), outer = outer_fixture(), destination;
        lower.rail_sites_selected = outer.rail_sites_selected = all_current | all_cause;
        mc_world_mutation_error *out = &destination;
        const mc_world_mutation_error *child = &lower, *parent = &outer;
        if (partition == 1)
            out = &lower;
        else if (partition == 2)
            out = &outer;
        else if (partition == 3)
            parent = &lower;
        else if (partition == 4) {
            out = &lower;
            parent = &lower;
        }
        mc_world_mutation_error saved_lower, saved_outer;
        memcpy(&saved_lower, child, sizeof(saved_lower));
        memcpy(&saved_outer, parent, sizeof(saved_outer));
        const mc_world_rail_merge_plan plan = {
            .restore_current = all_current,
            .outer_origin = one_per_channel,
            .outer_hook = {true, "fixture.alias.outer", "aliasHook()", 0},
            .lower_current_hook_selected = true};
        CHECK(mc_world_rail_error_merge(out, child, parent, plan));
        CHECK(out->rail_sites_selected == (all_current | all_cause));
        for (size_t i = 0; i < sizeof(currents) / sizeof(currents[0]); ++i)
            CHECK(same_value(value(out, currents[i]), value(&saved_outer, currents[i])));
        for (size_t i = 0; i < sizeof(causes) / sizeof(causes[0]); ++i)
            CHECK(same_value(value(out, causes[i]), value(&saved_lower, causes[i])));
        CHECK(out->cause_hook_class == saved_lower.hook_class);
        CHECK(out->cause_hook_method == saved_lower.hook_method && out->cause_hook_bytecode == 0);
        CHECK(out->hook_class == plan.outer_hook.declaring_class);
        CHECK(retained(out, &saved_lower));
        if (child != out)
            CHECK(memcmp(child, &saved_lower, sizeof(saved_lower)) == 0);
        if (parent != out)
            CHECK(memcmp(parent, &saved_outer, sizeof(saved_outer)) == 0);
    }
    return true;
}

static bool refused(const mc_world_mutation_error *lower, const mc_world_mutation_error *outer,
                    mc_world_rail_merge_plan plan) {
    ++cases;
    mc_world_mutation_error out = outer_fixture(), before, saved_lower, saved_outer;
    memcpy(&before, &out, sizeof(before));
    memcpy(&saved_lower, lower, sizeof(saved_lower));
    memcpy(&saved_outer, outer, sizeof(saved_outer));
    errno = EDOM;
    const bool ok = mc_world_rail_error_merge(&out, lower, outer, plan);
    const int saved_errno = errno;
    CHECK(!ok && saved_errno == EDOM);
    CHECK(memcmp(&out, &before, sizeof(out)) == 0);
    CHECK(memcmp(lower, &saved_lower, sizeof(saved_lower)) == 0);
    CHECK(memcmp(outer, &saved_outer, sizeof(saved_outer)) == 0);
    return true;
}

static bool invalid_plans(void) {
    mc_world_mutation_error lower = lower_fixture(), outer = outer_fixture();
    mc_world_rail_merge_plan plan = {0};
    const uint32_t unknown = UINT32_C(1) << 31;
    lower.rail_sites_selected = unknown;
    CHECK(refused(&lower, &outer, plan));
    lower.rail_sites_selected = 0;
    outer.rail_sites_selected = unknown;
    CHECK(refused(&lower, &outer, plan));
    outer.rail_sites_selected = 0;
    plan.restore_current = unknown;
    CHECK(refused(&lower, &outer, plan));
    plan = (mc_world_rail_merge_plan){.lower_origin = unknown};
    CHECK(refused(&lower, &outer, plan));
    plan = (mc_world_rail_merge_plan){.outer_origin = unknown};
    CHECK(refused(&lower, &outer, plan));
    /* Every cause bit is forbidden independently in each current-only mask. */
    for (size_t i = 0; i < sizeof(causes) / sizeof(causes[0]); ++i) {
        const uint32_t bit = slots[causes[i]].bit;
        lower.rail_sites_selected = outer.rail_sites_selected = bit;
        plan = (mc_world_rail_merge_plan){.restore_current = bit};
        CHECK(refused(&lower, &outer, plan));
        plan = (mc_world_rail_merge_plan){.lower_origin = bit};
        CHECK(refused(&lower, &outer, plan));
        plan = (mc_world_rail_merge_plan){.outer_origin = bit};
        CHECK(refused(&lower, &outer, plan));
    }
    /* Apparent nonzero/entered fields cannot admit an unselected origin. */
    lower.rail_sites_selected = outer.rail_sites_selected = 0;
    plan = (mc_world_rail_merge_plan){.restore_current = MC_WORLD_RAIL_SITE_ADDED,
                                      .outer_origin = MC_WORLD_RAIL_SITE_ADDED};
    CHECK(refused(&lower, &outer, plan));
    plan = (mc_world_rail_merge_plan){.lower_origin = MC_WORLD_RAIL_SITE_CONNECTION};
    CHECK(refused(&lower, &outer, plan));
    outer.rail_sites_selected = MC_WORLD_RAIL_SITE_SEARCH;
    plan = (mc_world_rail_merge_plan){.outer_origin = MC_WORLD_RAIL_SITE_SEARCH};
    CHECK(refused(&lower, &outer, plan));
    /* Missing origins are refused channel by channel, even for PC0 callers. */
    for (size_t i = 0; i < sizeof(currents) / sizeof(currents[0]); ++i) {
        const uint32_t bit = slots[currents[i]].bit;
        lower.rail_sites_selected = bit;
        outer.rail_sites_selected = 0;
        plan = (mc_world_rail_merge_plan){0};
        CHECK(refused(&lower, &outer, plan));
        lower.rail_sites_selected = 0;
        outer.rail_sites_selected = bit;
        plan.restore_current = bit;
        CHECK(refused(&lower, &outer, plan));
    }
    /* Ambiguity remains invalid even when a selected lower cause would win. */
    const uint32_t ambiguous[] = {MC_WORLD_RAIL_SITE_DETECTOR_CONTACT |
                                      MC_WORLD_RAIL_SITE_DETECTOR_UPDATE,
                                  MC_WORLD_RAIL_SITE_ADDED | MC_WORLD_RAIL_SITE_NEIGHBOR,
                                  MC_WORLD_RAIL_SITE_SEARCH | MC_WORLD_RAIL_SITE_VERIFY};
    for (size_t i = 0; i < sizeof(ambiguous) / sizeof(ambiguous[0]); ++i) {
        lower.rail_sites_selected = outer.rail_sites_selected = all_current | all_cause;
        plan = (mc_world_rail_merge_plan){.lower_origin = ambiguous[i]};
        CHECK(refused(&lower, &outer, plan));
        plan = (mc_world_rail_merge_plan){.restore_current = ambiguous[i],
                                          .outer_origin = ambiguous[i]};
        CHECK(refused(&lower, &outer, plan));
    }
    /* Refusal also preserves a destination aliased to either input. */
    for (unsigned int alias = 0; alias < 2; ++alias) {
        ++cases;
        mc_world_mutation_error *destination = alias == 0 ? &lower : &outer;
        mc_world_mutation_error before;
        memcpy(&before, destination, sizeof(before));
        plan = (mc_world_rail_merge_plan){.restore_current = all_cause};
        CHECK(!mc_world_rail_error_merge(destination, &lower, &outer, plan));
        CHECK(memcmp(destination, &before, sizeof(before)) == 0);
    }
    ++cases;
    mc_world_mutation_error before;
    memcpy(&before, &lower, sizeof(before));
    plan = (mc_world_rail_merge_plan){0};
    CHECK(!mc_world_rail_error_merge(NULL, &lower, &outer, plan));
    CHECK(memcmp(&lower, &before, sizeof(before)) == 0);
    CHECK(!mc_world_rail_error_merge(&lower, NULL, &outer, plan));
    CHECK(memcmp(&lower, &before, sizeof(before)) == 0);
    CHECK(!mc_world_rail_error_merge(&lower, &outer, NULL, plan));
    CHECK(memcmp(&lower, &before, sizeof(before)) == 0);
    return true;
}

int main(void) {
    if (!empty_frame() || !deepest_and_restore() || !each_origin() ||
        !selected_causes_and_no_inference() || !explicit_choice_among_currents() ||
        !hook_selection() || !aliases() || !invalid_plans())
        return 1;
    printf("PASS rail.error_sites cases=%u checks=%u\n", cases, checks);
    return 0;
}
