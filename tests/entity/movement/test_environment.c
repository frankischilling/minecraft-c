#define MC_COLLISION_BOXES_MAIN mc_environment_boxes_fixture_main
#include "../../world/runtime/collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN
#include "../../../src/entity/bounds/bounds_internal.h"
#include "../../../src/entity/movement/environment_internal.h"
#include "mc/entity/minecart/minecart.h"
#include "mc/entity/movement/environment.h"
#include "mc/entity/player/player.h"
#include <emmintrin.h>
#include <inttypes.h>

static uint64_t bits(double v) {
    uint64_t b;
    memcpy(&b, &v, sizeof(b));
    return b;
}

static float fvalue(uint32_t b) {
    float v;
    memcpy(&v, &b, sizeof(v));
    return v;
}

static double dvalue(uint64_t b) {
    double v;
    memcpy(&v, &b, sizeof(v));
    return v;
}

static void empty_cells(fixture *f, size_t side) {
    memset(f->states[side]->data->blocks, 0, MC_CHUNK_BLOCKS);
    memset(f->states[side]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
}

static bool flow_cells(fixture *f, size_t side, uint8_t id) {
    bool ok = false;
    for (int32_t y = 60; y < 69; ++y) {
        CHECK(boxes_cell(f, side, 6, y, 6, id, 0));
        CHECK(boxes_cell(f, side, 5, y, 6, id, 0));
        CHECK(boxes_cell(f, side, 6, y, 5, id, 0));
        CHECK(boxes_cell(f, side, 6, y, 7, id, 0));
        CHECK(boxes_cell(f, side, 7, y, 6, id, 7));
    }
    ok = true;
cleanup:
    return ok;
}

static bool reset_pools(mc_entity_classes *classes) {
    return mc_entity_bounds_pool_reset(classes, NULL) == MC_ENTITY_OK &&
           mc_entity_vector_pool_reset(classes, NULL) == MC_ENTITY_OK;
}

static bool record_probe(const char *label, size_t side, size_t kind, int c, bool value,
                         mc_entity *entity) {
    mc_entity_class_snapshot b;
    mc_entity_vector_snapshot v;
    if (mc_entity_classes_get_snapshot(entity->classes, &b) != MC_ENTITY_OK ||
        mc_entity_vector_get_snapshot(entity->classes, &v) != MC_ENTITY_OK || v.holds != 0)
        return false;
    return printf("%s %zu %zu %d %d %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %" PRId32
                  " %" PRId32 "\n",
                  label, side, kind, c, value, bits(entity->base.velocity_x),
                  bits(entity->base.velocity_y), bits(entity->base.velocity_z),
                  b.bounds_pool_cursor, v.cursor) > 0;
}

static bool environment_same_text(const char *a, const char *b) {
    return a == b || (a != NULL && b != NULL && strcmp(a, b) == 0);
}

static bool environment_same_error(const mc_entity_environment_error *a,
                                   const mc_entity_environment_error *b) {
    return a->status == b->status && a->cause == b->cause && a->bytecode == b->bytecode &&
           environment_same_text(a->operation, b->operation) &&
           environment_same_text(a->source_class, b->source_class) &&
           a->entity.status == b->entity.status && a->entity.bytecode == b->entity.bytecode &&
           a->entity.world == b->entity.world && a->bounds.status == b->bounds.status &&
           a->bounds.bytecode == b->bounds.bytecode &&
           a->bounds.factory.status == b->bounds.factory.status &&
           a->bounds.pool.status == b->bounds.pool.status &&
           a->registry.status == b->registry.status &&
           a->registry.initializer_pc == b->registry.initializer_pc &&
           a->collision.nearby.index == SIZE_MAX && b->collision.nearby.index == SIZE_MAX &&
           a->material.status == b->material.status &&
           environment_same_text(a->material.operation, b->material.operation) &&
           a->material.bytecode == b->material.bytecode &&
           a->material.cell_work == b->material.cell_work &&
           a->material.max_cell_work == b->material.max_cell_work &&
           a->material.block.x == b->material.block.x &&
           a->material.block.y == b->material.block.y &&
           a->material.block.z == b->material.block.z &&
           a->material.block.nearby.query.status == b->material.block.nearby.query.status &&
           a->material.block.nearby.query.bytecode == b->material.block.nearby.query.bytecode &&
           a->material.block.nearby.query.chunk_bytecode ==
               b->material.block.nearby.query.chunk_bytecode &&
           a->material.block.nearby.index == SIZE_MAX &&
           b->material.block.nearby.index == SIZE_MAX &&
           a->material.area.presentation.registry.initializer_pc == -1 &&
           b->material.area.presentation.registry.initializer_pc == -1;
}

static bool environment_canonical(const mc_entity_environment_error *e) {
    return e->status == MC_ENTITY_OK && e->cause == MC_ENTITY_ENVIRONMENT_NONE &&
           e->operation == NULL && e->source_class == NULL && e->bytecode == 0 &&
           e->entity.status == MC_ENTITY_OK && e->bounds.status == MC_ENTITY_OK &&
           e->registry.initializer_pc == -1 && e->collision.nearby.index == SIZE_MAX &&
           e->material.status == MC_WORLD_STORAGE_OK && e->material.bytecode == 0 &&
           e->material.cell_work == 0 && e->material.max_cell_work == 0 &&
           e->material.block.nearby.index == SIZE_MAX &&
           e->material.area.presentation.registry.initializer_pc == -1;
}

static bool prepared_cases(fixture *f, size_t side, mc_entity *entity) {
    bool ok = false;
    mc_entity_environment_error public_error, prepared_error;
    mc_chunk_data *const data = f->states[side]->data;
    const size_t capacity = data->blocks_capacity, holds = entity->holds,
                 borrows = f->registry[side]->runtime_borrows;
    const mc_entity_base_fields saved_base = entity->base;
    const mc_entity_bounds saved_bounds = *entity->bounds;
    const mc_java_random random = *entity->random;
    const mc_entity_bounds scan = {6.125, 64.125, 6.125, 6.875, 65.875, 7.875};
    const mc_world_material_movement_limits limits = {64};
    for (unsigned water = 0; water < 2; ++water)
        for (unsigned failure = 0; failure < 2; ++failure) {
            empty_cells(f, side);
            if (failure)
                CHECK(boxes_cell(f, side, 6, 64, 6, 1, 0));
            else
                CHECK(flow_cells(f, side, water ? 8 : 10));
            mc_entity_bounds_assign(entity->bounds, &scan);
            entity->base.velocity_x = 2;
            entity->base.velocity_y = -3;
            entity->base.velocity_z = 4;
            const mc_entity_base_fields input = entity->base;
            bool public_value = failure != 0, prepared_value = public_value;
            CHECK(reset_pools(entity->classes));
            /* First Stone succeeds; the next Z cell reaches the genuine
             * normal Chunk byte-backing limit. Restore before assertions. */
            data->blocks_capacity = failure ? boxes_index(6, 64, 7) : capacity;
            const mc_entity_status public_status =
                water ? mc_entity_environment_water(entity, f->registry[side], &limits,
                                                    &public_value, &public_error)
                      : mc_entity_environment_lava(entity, f->registry[side], &limits,
                                                   &public_value, &public_error);
            data->blocks_capacity = capacity;
            const uint64_t vx = bits(entity->base.velocity_x), vy = bits(entity->base.velocity_y),
                           vz = bits(entity->base.velocity_z);
            const int32_t public_bounds_cursor = entity->classes->bounds_cursor;
            mc_entity_vector_snapshot public_vectors, prepared_vectors;
            CHECK(mc_entity_vector_get_snapshot(entity->classes, &public_vectors) == MC_ENTITY_OK);
            entity->base = input;
            CHECK(reset_pools(entity->classes));
            memset(&prepared_error, 0xa5, sizeof(prepared_error));
            mc_entity_environment_error_init(&prepared_error);
            data->blocks_capacity = failure ? boxes_index(6, 64, 7) : capacity;
            const mc_entity_status prepared_status = mc_entity_environment_liquid_in_place(
                entity, f->registry[side], water != 0, &limits, &prepared_value, &prepared_error);
            data->blocks_capacity = capacity;
            CHECK(mc_entity_vector_get_snapshot(entity->classes, &prepared_vectors) ==
                  MC_ENTITY_OK);
            CHECK(public_status == (failure ? MC_ENTITY_WORLD_FAILED : MC_ENTITY_OK) &&
                  prepared_status == public_status && public_value && prepared_value &&
                  environment_same_error(&public_error, &prepared_error) &&
                  bits(entity->base.velocity_x) == vx && bits(entity->base.velocity_y) == vy &&
                  bits(entity->base.velocity_z) == vz && entity->holds == holds &&
                  entity->classes->bounds_cursor == public_bounds_cursor &&
                  prepared_vectors.cursor == public_vectors.cursor && public_vectors.holds == 0 &&
                  prepared_vectors.holds == 0 && f->registry[side]->runtime_borrows == borrows &&
                  f->worlds[side]->active_calls == 0 && f->worlds[side]->local_leases == 0);
            CHECK(failure ? public_error.material.cell_work == 2 &&
                                public_error.material.bytecode == (water ? 135u : 101u) &&
                                public_error.material.block.nearby.query.bytecode == 60 &&
                                public_error.material.block.nearby.query.chunk_bytecode == 15 &&
                                vx == bits(2) && vy == bits(-3) && vz == bits(4)
                          : environment_canonical(&public_error) &&
                                environment_canonical(&prepared_error));
        }
    for (unsigned failure = 0; failure < 2; ++failure) {
        entity->base.bx = true;
        bool public_value = false, prepared_value = false;
        mc_entity_environment_error_init(&prepared_error);
        if (failure)
            entity->holds = SIZE_MAX;
        const mc_entity_status public_status = mc_entity_environment_in_water(
            entity, f->registry[side], NULL, &public_value, &public_error);
        const size_t public_holds = entity->holds;
        const mc_entity_status prepared_status = mc_entity_environment_in_water_in_place(
            entity, f->registry[side], NULL, &prepared_value, &prepared_error);
        const size_t prepared_holds = entity->holds;
        entity->holds = holds;
        CHECK(public_status == (failure ? MC_ENTITY_LIMIT : MC_ENTITY_OK) &&
              prepared_status == public_status && public_value == !failure &&
              prepared_value == public_value && public_holds == (failure ? SIZE_MAX : holds) &&
              prepared_holds == public_holds &&
              environment_same_error(&public_error, &prepared_error) &&
              (failure ||
               (environment_canonical(&public_error) && environment_canonical(&prepared_error))));
        entity->base.velocity_x = 2;
        entity->base.velocity_y = -3;
        entity->base.velocity_z = 4;
        entity->base.yaw = 30;
        const mc_math_trig *const table = failure ? NULL : &trig;
        const mc_entity_status public_relative =
            mc_entity_environment_relative(entity, table, .7f, -.4f, .2f, &public_error);
        const uint64_t vx = bits(entity->base.velocity_x), vz = bits(entity->base.velocity_z);
        entity->base.velocity_x = 2;
        entity->base.velocity_z = 4;
        mc_entity_environment_error_init(&prepared_error);
        const mc_entity_status prepared_relative =
            mc_entity_environment_relative_in_place(entity, table, .7f, -.4f, .2f, &prepared_error);
        CHECK(public_relative == (failure ? MC_ENTITY_INVALID_ARGUMENT : MC_ENTITY_OK) &&
              prepared_relative == public_relative &&
              environment_same_error(&public_error, &prepared_error) &&
              bits(entity->base.velocity_x) == vx && bits(entity->base.velocity_z) == vz &&
              entity->base.velocity_y == -3 && entity->holds == holds &&
              (!failure || (vx == bits(2) && vz == bits(4))) &&
              (failure ||
               (environment_canonical(&public_error) && environment_canonical(&prepared_error))));
    }
    CHECK(mc_test_java_random_equal(&random, entity->random) && reset_pools(entity->classes) &&
          f->registry[side]->runtime_borrows == borrows);
    ok = true;
cleanup:
    data->blocks_capacity = capacity;
    entity->holds = holds;
    entity->base = saved_base;
    mc_entity_bounds_assign(entity->bounds, &saved_bounds);
    return ok;
}

static bool probes(fixture *f, size_t side, size_t kind, mc_entity *entity) {
    bool ok = false, value = false;
    mc_entity_environment_error error;
    CHECK(mc_entity_set_position(entity, 6.5, 64.1, 6.5) == MC_ENTITY_OK);
    for (int c = 0; c < 6; ++c) {
        empty_cells(f, side);
        if (c == 1 || c == 5)
            CHECK(flow_cells(f, side, 8));
        if (c == 2)
            CHECK(flow_cells(f, side, 9));
        if (c == 3)
            CHECK(flow_cells(f, side, 10));
        if (c == 4)
            CHECK(boxes_cell(f, side, 6, 64, 6, 1, 0));
        CHECK(reset_pools(entity->classes));
        CHECK(mc_entity_set_velocity(entity, 0, 0) == MC_ENTITY_OK &&
              mc_entity_set_velocity(entity, 1, 0) == MC_ENTITY_OK &&
              mc_entity_set_velocity(entity, 2, 0) == MC_ENTITY_OK);
        const size_t held = entity->holds;
        const mc_java_random random = *entity->random;
        const uint16_t ctor = entity->base_bytecode;
        const bool in_water = entity->base.bx;
        CHECK(mc_entity_environment_water(entity, f->registry[side], NULL, &value, &error) ==
              MC_ENTITY_OK);
        if (c == 5)
            CHECK(mc_entity_environment_water(entity, f->registry[side], NULL, &value, &error) ==
                  MC_ENTITY_OK);
        CHECK(entity->holds == held && entity->base_bytecode == ctor &&
              entity->base.bx == in_water && mc_test_java_random_equal(&random, entity->random));
        CHECK(record_probe("WATER", side, kind, c, value, entity));
        CHECK(value == (c == 1 || c == 2 || c == 5));
    }
    for (int c = 0; c < 4; ++c) {
        empty_cells(f, side);
        if (c)
            CHECK(flow_cells(f, side, c == 1 ? 10 : c == 2 ? 11 : 8));
        CHECK(reset_pools(entity->classes));
        CHECK(mc_entity_set_velocity(entity, 0, 2) == MC_ENTITY_OK &&
              mc_entity_set_velocity(entity, 1, -3) == MC_ENTITY_OK &&
              mc_entity_set_velocity(entity, 2, 4) == MC_ENTITY_OK);
        memset(&error, 0xa5, sizeof(error));
        CHECK(mc_entity_environment_lava(entity, f->registry[side], NULL, &value, &error) ==
                  MC_ENTITY_OK &&
              error.status == MC_ENTITY_OK && error.operation == NULL && error.bytecode == 0 &&
              error.material.status == MC_WORLD_STORAGE_OK && error.material.cell_work == 0 &&
              error.material.block.nearby.index == SIZE_MAX &&
              error.collision.nearby.index == SIZE_MAX);
        CHECK(record_probe("LAVA", side, kind, c, value, entity));
        CHECK(((c != 0 && c != 3) || !value) && entity->base.velocity_x == 2 &&
              entity->base.velocity_y == -3 && entity->base.velocity_z == 4);
    }
    const bool saved = entity->base.bx;
    const size_t holds = entity->holds;
    for (int flag = 0; flag < 2; ++flag) {
        entity->base.bx = flag != 0;
        memset(&error, 0xa5, sizeof(error));
        CHECK(mc_entity_environment_stored_in_water(entity, &value, &error) == MC_ENTITY_OK &&
              value == (flag != 0) && error.status == MC_ENTITY_OK && error.source_class == NULL &&
              error.material.status == MC_WORLD_STORAGE_OK &&
              error.material.block.nearby.index == SIZE_MAX &&
              error.collision.nearby.index == SIZE_MAX);
        CHECK(printf("FLAG %zu %zu %d %d\n", side, kind, flag, value) > 0);
        value = !entity->base.bx;
        CHECK(mc_entity_environment_stored_in_water(entity, &value, NULL) == MC_ENTITY_OK &&
              value == entity->base.bx && entity->holds == holds);
    }
    entity->base.bx = saved;
    ok = true;
cleanup:
    return ok;
}

static const uint32_t floats[] = {0,
                                  UINT32_C(0x80000000),
                                  UINT32_C(0x3c23d70a),
                                  UINT32_C(0x3c23d709),
                                  UINT32_C(0x3c23d70b),
                                  UINT32_C(0x3ca3d70a),
                                  UINT32_C(0x3f800000),
                                  UINT32_C(0xbf800000),
                                  UINT32_C(0x40000000),
                                  1,
                                  UINT32_C(0x7f7fffff),
                                  UINT32_C(0x7f800000),
                                  UINT32_C(0xff800000),
                                  UINT32_C(0x7fc00123),
                                  UINT32_C(0xffc00456),
                                  UINT32_C(0x7f800789)};
static const uint32_t yaws[] = {0,
                                UINT32_C(0x42b40000),
                                UINT32_C(0xc2b40000),
                                UINT32_C(0x43340000),
                                UINT32_C(0x43b3f333),
                                UINT32_C(0x7f7fffff),
                                UINT32_C(0x7fc00789),
                                UINT32_C(0xff800000)};
static const uint64_t velocities[] = {0,
                                      UINT64_C(0x8000000000000000),
                                      UINT64_C(0x4000000000000000),
                                      UINT64_C(0xc008000000000000),
                                      UINT64_C(0x7ff8000000000123),
                                      UINT64_C(0xfff8000000000456)};

static bool relative_cases(mc_entity *entity, size_t side, size_t kind) {
    bool ok = false;
    mc_entity_environment_error error;
    for (size_t i = 0; i < COUNT(floats); ++i)
        for (size_t j = 0; j < COUNT(floats); ++j) {
            entity->base.yaw = fvalue(yaws[(i + 2 * j) % COUNT(yaws)]);
            CHECK(mc_entity_set_velocity(entity, 0, dvalue(velocities[i % COUNT(velocities)])) ==
                      MC_ENTITY_OK &&
                  mc_entity_set_velocity(entity, 1, -0.0) == MC_ENTITY_OK &&
                  mc_entity_set_velocity(entity, 2, dvalue(velocities[j % COUNT(velocities)])) ==
                      MC_ENTITY_OK);
            const mc_java_random random = *entity->random;
            const size_t holds = entity->holds;
            mc_entity_base_fields before, after;
            memcpy(&before, &entity->base, sizeof(before));
            CHECK(mc_entity_environment_relative(
                      entity, &trig, fvalue(floats[i]), fvalue(floats[j]),
                      fvalue(floats[(3 * i + 5 * j + 5) % COUNT(floats)]), &error) == MC_ENTITY_OK);
            memcpy(&after, &entity->base, sizeof(after));
            before.velocity_x = after.velocity_x;
            before.velocity_z = after.velocity_z;
            CHECK(memcmp(&before, &after, sizeof(before)) == 0);
            CHECK(entity->holds == holds && mc_test_java_random_equal(&random, entity->random) &&
                  bits(entity->base.velocity_y) == UINT64_C(0x8000000000000000));
            CHECK(printf("REL %zu %zu %zu %016" PRIx64 " %016" PRIx64 " %016" PRIx64 "\n", side,
                         kind, i * COUNT(floats) + j, bits(entity->base.velocity_x),
                         bits(entity->base.velocity_y), bits(entity->base.velocity_z)) > 0);
        }
    ok = true;
cleanup:
    return ok;
}

static bool clearance_cases(fixture *f, size_t side, size_t kind, mc_entity *entity) {
    bool ok = false, value = false;
    mc_entity_environment_error error;
    CHECK(mc_entity_set_position(entity, 6.5, 64.1, 6.5) == MC_ENTITY_OK);
    for (int c = 0; c < 5; ++c) {
        empty_cells(f, side);
        if (c == 1)
            CHECK(boxes_cell(f, side, 6, 64, 6, 1, 0));
        if (c == 2 || c == 3)
            CHECK(flow_cells(f, side, c == 2 ? 8 : 10));
        if (c == 4)
            CHECK(boxes_cell(f, side, 7, 64, 6, 1, 0));
        CHECK(reset_pools(entity->classes));
        const mc_entity_bounds permanent = *entity->bounds;
        CHECK(mc_entity_environment_clearance(entity, c == 4 ? 1.0 : 0.0, 0, 0, NULL, &value,
                                              &error) == MC_ENTITY_OK);
        CHECK(memcmp(&permanent, entity->bounds, sizeof(permanent)) == 0);
        mc_entity_class_snapshot pool;
        CHECK(mc_entity_classes_get_snapshot(entity->classes, &pool) == MC_ENTITY_OK);
        CHECK(printf("CLEAR %zu %zu %d %d %" PRId32 " %zu\n", side, kind, c, value,
                     pool.bounds_pool_cursor, f->worlds[side]->fields.K->count) > 0);
        CHECK(value == (c == 0));
    }
    ok = true;
cleanup:
    return ok;
}

static bool advance_pool(mc_entity_classes *classes) {
    if (mc_entity_bounds_pool_reset(classes, NULL) != MC_ENTITY_OK)
        return false;
    size_t length = classes->bounds_pool->length;
    for (size_t i = 0; i < length; ++i) {
        mc_entity_bounds *b = NULL;
        if (mc_entity_bounds_pool_get(classes, 0, 0, 0, 0, 0, 0, &b, NULL) != MC_ENTITY_OK)
            return false;
        if (mc_entity_bounds_release(classes, b) != MC_ENTITY_OK)
            return false;
    }
    return true;
}

static bool failures(fixture *f, size_t side, mc_entity *ready) {
    bool ok = false, value = true;
    mc_entity *raw = NULL, *null_entity = NULL;
    mc_registry_classes *cold = NULL;
    mc_entity_classes *classes = f->entity_classes[side];
    mc_entity_environment_error error;
    const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                         f->profile, f->materials, 0, &f->sources[side].api};
    CHECK(mc_registry_classes_allocate(&options, &cold, NULL) == MC_REGISTRY_OK);
    CHECK(cold->blocks_phase == MC_REGISTRY_COLD && cold->runtime_borrows == 0);
    const int32_t initial_pc = cold->block_initializer_pc;
    CHECK(mc_falling_entity_allocate(classes, &raw, NULL) == MC_ENTITY_OK);
    memset(&error, 0xa5, sizeof(error));
    CHECK(mc_entity_environment_stored_in_water(raw, &value, &error) == MC_ENTITY_INVALID_PHASE &&
          value && error.status == MC_ENTITY_INVALID_PHASE &&
          error.cause == MC_ENTITY_ENVIRONMENT_OWNER && error.source_class == NULL &&
          error.bytecode == 0 && error.material.block.nearby.index == SIZE_MAX &&
          error.collision.nearby.index == SIZE_MAX);
    CHECK(mc_entity_environment_stored_in_water(NULL, &value, NULL) == MC_ENTITY_INVALID_ARGUMENT &&
          value);
    mc_entity_release(raw);
    raw = NULL;
    CHECK(mc_falling_entity_allocate(classes, &null_entity, NULL) == MC_ENTITY_OK);
    CHECK(mc_falling_entity_construct_world(null_entity, NULL, NULL) == MC_ENTITY_OK);
    for (int operation = 0; operation < 3; ++operation) {
        CHECK(reset_pools(classes));
        value = true;
        const mc_entity_status status =
            operation == 0 ? mc_entity_environment_water(null_entity, cold, NULL, &value, &error)
            : operation == 1
                ? mc_entity_environment_lava(null_entity, cold, NULL, &value, &error)
                : mc_entity_environment_clearance(null_entity, 0, 0, 0, NULL, &value, &error);
        CHECK(status == MC_ENTITY_WORLD_FAILED && value &&
              error.cause == MC_ENTITY_ENVIRONMENT_NULL_REFERENCE);
        CHECK(error.bytecode == (operation == 0 ? 32u : operation == 1 ? 23u : 20u));
        mc_entity_class_snapshot b;
        CHECK(mc_entity_classes_get_snapshot(classes, &b) == MC_ENTITY_OK);
        CHECK(b.bounds_pool_cursor == (operation == 0 ? 2 : 1));
        mc_entity_vector_snapshot vectors;
        CHECK(mc_entity_vector_get_snapshot(classes, &vectors) == MC_ENTITY_OK &&
              vectors.holds == 0);
        CHECK(printf("PREFIX %zu 1 %d %" PRId32 " %" PRId32 "\n", side, operation,
                     b.bounds_pool_cursor, vectors.cursor) > 0);
        CHECK(cold->blocks_phase == MC_REGISTRY_COLD && cold->block_initializer_pc == initial_pc &&
              cold->runtime_borrows == 0);
    }
    CHECK(relative_cases(null_entity, side, 4));
    mc_entity_bounds *saved_bounds = null_entity->bounds;
    null_entity->bounds = NULL;
    value = true;
    const mc_entity_status no_bounds =
        mc_entity_environment_water(null_entity, cold, NULL, &value, &error);
    null_entity->bounds = saved_bounds;
    CHECK(no_bounds == MC_ENTITY_STATE_ERROR && value && error.bytecode == 13 &&
          cold->runtime_borrows == 0);
    CHECK(reset_pools(classes));
    value = true;
    CHECK(mc_entity_environment_water(ready, cold, NULL, &value, &error) == MC_ENTITY_STATE_ERROR &&
          value && error.bytecode == 32 && error.cause == MC_ENTITY_ENVIRONMENT_NAMESPACE);
    CHECK(cold->blocks_phase == MC_REGISTRY_COLD && cold->runtime_borrows == 0);
    CHECK(advance_pool(classes));
    const size_t maximum = classes->limits.max_allocation_bytes;
    const int32_t cursor = classes->bounds_cursor;
    classes->limits.max_allocation_bytes = classes->allocated_bytes;
    value = true;
    const mc_entity_status allocation =
        mc_entity_environment_water(ready, cold, NULL, &value, &error);
    classes->limits.max_allocation_bytes = maximum;
    CHECK(allocation == MC_ENTITY_LIMIT && value && error.bytecode == 13 &&
          classes->bounds_cursor == cursor && cold->runtime_borrows == 0);
    CHECK(reset_pools(classes));
    value = true;
    const size_t borrows = f->registry[side]->runtime_borrows;
    const mc_world_material_movement_limits no_work = {0};
    CHECK(mc_entity_environment_water(ready, f->registry[side], &no_work, &value, &error) ==
              MC_ENTITY_WORLD_FAILED &&
          value && error.bytecode == 32 && error.material.status == MC_WORLD_STORAGE_LIMIT &&
          f->registry[side]->runtime_borrows == borrows);
    const unsigned csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8000u);
    value = true;
    const mc_entity_status stored = mc_entity_environment_stored_in_water(ready, &value, &error);
    const mc_entity_status fp = mc_entity_environment_relative(ready, &trig, 0, 0, 0, &error);
    _mm_setcsr(csr);
    CHECK(stored == MC_ENTITY_OK && value == ready->base.bx && fp == MC_ENTITY_UNSUPPORTED_FP &&
          error.bytecode == 2);
    mc_entity_base_fields before;
    memcpy(&before, &ready->base, sizeof(before));
    CHECK(mc_entity_environment_relative(ready, NULL, 1, 1, 1, &error) ==
              MC_ENTITY_INVALID_ARGUMENT &&
          memcmp(&before, &ready->base, sizeof(before)) == 0);
    ok = true;
cleanup:
    if (raw)
        mc_entity_release(raw);
    if (null_entity)
        mc_entity_release(null_entity);
    if (cold && mc_registry_classes_free(cold, NULL) != MC_REGISTRY_OK)
        ok = false;
    return ok;
}

static bool published_boxes(fixture *f, mc_entity *falling, mc_entity *player, mc_entity *cart) {
    bool ok = false, value = false;
    empty_cells(f, 0);
    bool inserted = false;
    CHECK(mc_entity_set_position(falling, 6.5, 64.1, 6.5) == MC_ENTITY_OK &&
          mc_entity_set_position(player, 6.5, 64.1, 6.5) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_insert_entity(f->worlds[0], falling, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    CHECK(mc_world_runtime_insert_entity(f->worlds[0], player, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    for (size_t c = 0; c < 2; ++c) {
        mc_entity *moving = c == 0 ? player : falling;
        CHECK(reset_pools(moving->classes));
        CHECK(mc_entity_environment_clearance(moving, 0, 0, 0, NULL, &value, NULL) ==
                  MC_ENTITY_OK &&
              value);
        mc_entity_class_snapshot pool;
        CHECK(mc_entity_classes_get_snapshot(moving->classes, &pool) == MC_ENTITY_OK);
        CHECK(printf("PUBLISHED 0 %zu %d %" PRId32 " %zu\n", c, value, pool.bounds_pool_cursor,
                     f->worlds[0]->fields.K->count) > 0);
    }
    CHECK(mc_entity_set_position(cart, 6.5, 64.1, 6.5) == MC_ENTITY_OK);
    CHECK(mc_world_runtime_insert_entity(f->worlds[0], cart, &inserted, NULL) ==
              MC_WORLD_STORAGE_OK &&
          inserted);
    value = true;
    mc_entity_environment_error error;
    const mc_entity_status candidate_status =
        mc_entity_environment_clearance(player, 0, 0, 0, NULL, &value, &error);
    /* Actual yl.f() returns null; the moving Player inherits the other null
     * body. Publishing the Minecart therefore does not block clearance. */
    CHECK(candidate_status == MC_ENTITY_OK && value && error.status == MC_ENTITY_OK &&
          error.bytecode == 0 && error.collision.world_bytecode == 0 &&
          f->worlds[0]->local_leases == 0 && f->worlds[0]->active_calls == 0);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    if (setvbuf(stdout, NULL, _IONBF, 0) != 0)
        return 1;
    int result = 1;
    fixture *f = NULL;
    char root[8192];
    mc_entity *actors[2][4] = {{NULL}};
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    const uint16_t title[] = {'E', 'n', 'v', 'i', 'r', 'o', 'n', 'm', 'e', 'n', 't'};
    if (argc != 3 || strcmp(argv[1], "entity-environment") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(4u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
        r->streams[1] = (stream){.bytes = language};
        r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
              MC_REGISTRY_OK);
        CHECK(mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK);
        CHECK(mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
              MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(boxes_open_store(f, side, root) && boxes_create_world(f, side));
        CHECK(mc_item_entity_allocate(f->entity_classes[side], &actors[side][0], NULL) ==
              MC_ENTITY_OK);
        f->entities[f->entity_count++] = actors[side][0];
        CHECK(mc_item_entity_construct(actors[side][0], f->worlds[side], 6.5, 64.1, 6.5, f->stack,
                                       NULL) == MC_ENTITY_OK);
        CHECK(mc_falling_entity_allocate(f->entity_classes[side], &actors[side][1], NULL) ==
              MC_ENTITY_OK);
        f->entities[f->entity_count++] = actors[side][1];
        CHECK(mc_falling_entity_construct(actors[side][1], f->worlds[side], 6.5, 64.1, 6.5, 1,
                                          NULL) == MC_ENTITY_OK);
        CHECK(mc_minecart_entity_allocate(f->entity_classes[side], &actors[side][2], NULL) ==
              MC_ENTITY_OK);
        f->entities[f->entity_count++] = actors[side][2];
        CHECK(mc_minecart_entity_construct(actors[side][2], f->worlds[side], 6.5, 64.1, 6.5, 0,
                                           NULL) == MC_ENTITY_OK);
        if (side == 0) {
            CHECK(mc_player_session_classes_create(f->entity_classes[side], f->registry[side],
                                                   &sessions, NULL) == MC_PLAYER_OK);
            CHECK(mc_player_session_create(sessions, NULL, NULL, &session, NULL) == MC_PLAYER_OK);
            CHECK(mc_local_player_allocate(f->entity_classes[side], &actors[side][3], NULL) ==
                  MC_ENTITY_OK);
            f->entities[f->entity_count++] = actors[side][3];
            CHECK(mc_local_player_construct(actors[side][3], f->worlds[side], NULL, session, 0,
                                            NULL, NULL) == MC_PLAYER_OK);
        }
        for (size_t kind = 0; kind < (side == 0 ? 4u : 3u); ++kind) {
            CHECK(probes(f, side, kind, actors[side][kind]));
            CHECK(clearance_cases(f, side, kind, actors[side][kind]));
            CHECK(relative_cases(actors[side][kind], side, kind));
        }
        CHECK(prepared_cases(f, side, actors[side][1]) && failures(f, side, actors[side][1]));
    }
    CHECK(published_boxes(f, actors[0][1], actors[0][3], actors[0][2]));
    result = 0;
cleanup:
    mc_player_session_retire(&session);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK)
        result = 1;
    if (!cleanup_fixture(f)) {
        fputs("environment cleanup refused\n", stderr);
        return 1;
    }
    if (result == 0 && printf("Entity environment: %u checks\n", checks) < 0)
        result = 1;
    return result;
}
