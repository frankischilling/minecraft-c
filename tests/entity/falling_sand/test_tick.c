/* Authored native controls on genuine GAME/Registry/Chunk/FallingSand owners.
 * Included fixture mains are compiled but never executed. */
#define MC_PLAYER_MOVEMENT_MAIN mc_falling_tick_movement_fixture_main
#include "../movement/test_player_movement.c"
#undef MC_PLAYER_MOVEMENT_MAIN
#include "../../../src/entity/falling_sand/tick_internal.h"
#include "mc/world/runtime/entity_tick.h"
#include "mc/world/runtime/world_access.h"

static bool falling_reset(fixture *f, mc_entity *p, int32_t id, int32_t age) {
    bool ok = false;
    movement_air(f, 0);
    f->worlds[0]->fields.B = false;
    p->falling.a = id;
    p->falling.b = age;
    p->base.aX = p->base.aY = p->base.aZ = p->base.ba = p->base.bb = p->base.bc = false;
    p->base.be = p->base.bq = p->base.bx = p->base.bC = false;
    p->base.bd = true;
    p->base.bp = p->base.y_size = p->base.bk = 0;
    p->base.bu = 1;
    p->base.bv = 0;
    p->base.bt = 77;
    p->base.aJ = -1;
    p->base.aK = -2;
    p->base.aL = -3;
    p->base.private_c = true;
    p->base.offset = .98f / 2.f;
    CHECK(mc_entity_set_size(p, .98f, .98f) == MC_ENTITY_OK &&
          mc_entity_set_position(p, 8.5, 8.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_velocity(p, 0, .25) == MC_ENTITY_OK &&
          mc_entity_set_velocity(p, 1, 0) == MC_ENTITY_OK &&
          mc_entity_set_velocity(p, 2, -.125) == MC_ENTITY_OK &&
          mc_entity_bounds_pool_reset(p->classes, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(p->classes, NULL) == MC_ENTITY_OK);
    ok = true;
cleanup:
    return ok;
}

static bool falling_motion_cases(fixture *f, mc_entity *p) {
    bool ok = false;
    mc_falling_entity_tick_error e = {0};
    CHECK(falling_reset(f, p, 12, 0));
    const mc_java_random random = *p->random;
    const size_t holds = p->holds, borrowers = f->registry[0]->runtime_borrows;
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e) == MC_ENTITY_OK &&
          e.work == NULL && e.status == MC_ENTITY_OK && e.bytecode == 0 && p->falling.b == 1 &&
          !p->base.be && !p->base.aX && p->base.bt == 77 && p->base.private_c &&
          p->base.aJ == 8.5 && p->base.aK == 8.5 && p->base.aL == 8.5 && p->holds == holds &&
          f->registry[0]->runtime_borrows == borrowers &&
          mc_test_java_random_equal(p->random, &random));
    /* Independently evaluated binary64 constants and source operations. The
     * canonical mover does not collide in this real empty Chunk. */
    volatile double gravity = 0.0 - 0.03999999910593033;
    volatile double vx = .25 * 0.9800000190734863;
    volatile double vy = gravity * 0.9800000190734863;
    volatile double vz = -.125 * 0.9800000190734863;
    CHECK(movement_double_bits(p->base.velocity_x) == movement_double_bits(vx) &&
          movement_double_bits(p->base.velocity_y) == movement_double_bits(vy) &&
          movement_double_bits(p->base.velocity_z) == movement_double_bits(vz));
    CHECK(falling_reset(f, p, 12, INT32_MAX));
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e) == MC_ENTITY_OK &&
          p->falling.b == INT32_MIN && !p->base.be);
    /* Matching carried blocks are cleared on EVERY tick, including age8. */
    CHECK(falling_reset(f, p, 12, 7));
    p->base.bq = true; /* Explicit native no-clip control, not SDK input. */
    CHECK(boxes_cell(f, 0, 8, 8, 8, 12, 0));
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e) == MC_ENTITY_OK &&
          p->falling.b == 8 && f->states[0]->data->blocks[boxes_index(8, 8, 8)] == 0 &&
          !p->base.be && p->base.bt == 77);
    /* Exact FP admission prefix: previous positions and wrapped age precede
     * gravity; no super tick/Random/watcher work is invented. */
    CHECK(falling_reset(f, p, 12, 4));
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x40u);
    const mc_entity_status floating =
        mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e);
    _mm_setcsr(csr);
    CHECK(floating == MC_ENTITY_UNSUPPORTED_FP && e.bytecode == 54 && e.work != NULL &&
          p->falling.b == 5 && p->base.aJ == 8.5 && p->base.aK == 8.5 && p->base.velocity_y == 0 &&
          p->base.bt == 77);
    const mc_falling_entity_tick_work *pending = e.work;
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e) == MC_ENTITY_BUSY &&
          e.work == pending && e.status == MC_ENTITY_UNSUPPORTED_FP &&
          mc_falling_entity_tick_error_retire(&e) && e.work == NULL && p->holds == holds &&
          f->registry[0]->runtime_borrows == borrowers);
    CHECK(falling_reset(f, p, 12, 0));
    const mc_entity_move_limits limited = {.max_loop_work = 0};
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], &limited, &e) == MC_ENTITY_LIMIT &&
          e.bytecode == 71 && e.child == MC_FALLING_TICK_CHILD_MOVEMENT && e.movement != NULL &&
          e.movement->status == MC_ENTITY_LIMIT && p->falling.b == 1 && p->base.bt == 77 &&
          mc_falling_entity_tick_error_retire(&e) && p->holds == holds);
    ok = true;
cleanup:
    if (!mc_falling_entity_tick_error_retire(&e))
        ok = false;
    return ok;
}

static bool falling_landing_cases(fixture *f, mc_entity *p) {
    bool ok = false;
    mc_falling_entity_tick_error e = {0};
    /* Actual full-size bounds and canonical collision mover land on stone.
     * Dirt's inherited addition body avoids a declared scheduler quota; the
     * carried ID is an ordinary source int, not restricted to Sand/Gravel. */
    CHECK(falling_reset(f, p, 3, 0));
    movement_floor(f, 0);
    CHECK(mc_entity_set_position(p, 8.5, 8.5, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_velocity(p, 0, 0) == MC_ENTITY_OK &&
          mc_entity_set_velocity(p, 1, -.25) == MC_ENTITY_OK &&
          mc_entity_set_velocity(p, 2, 0) == MC_ENTITY_OK);
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e) == MC_ENTITY_OK &&
          p->base.aX && p->base.be && p->falling.b == 1 && p->base.bt == 77 &&
          f->states[0]->data->blocks[boxes_index(8, 8, 8)] == 3 &&
          movement_double_bits(p->base.velocity_y) == UINT64_C(0x8000000000000000));
    /* Remote gating is later than landing death and predicates. A declared
     * grounded no-clip input above passable air fails landing and skips drop. */
    CHECK(falling_reset(f, p, 12, 0));
    p->base.bq = p->base.aX = true;
    f->worlds[0]->fields.B = true;
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e) == MC_ENTITY_OK &&
          p->base.be && p->falling.b == 1 && f->states[0]->data->blocks[boxes_index(8, 8, 8)] == 0);
    CHECK(falling_reset(f, p, 12, 100));
    f->worlds[0]->fields.B = true;
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e) == MC_ENTITY_OK &&
          p->falling.b == 101 && !p->base.be);
    ok = true;
cleanup:
    f->worlds[0]->fields.B = false;
    if (!mc_falling_entity_tick_error_retire(&e))
        ok = false;
    return ok;
}

typedef struct falling_observer {
    mc_falling_entity_tick_error *tick;
    mc_entity *source, *item;
    size_t calls;
    bool fail, active_refused, expected_dead;
} falling_observer;

static mc_world_storage_status falling_added(void *context, mc_world_runtime *world,
                                             mc_entity *item, mc_world_storage_error *error) {
    falling_observer *o = context;
    ++o->calls;
    o->item = item;
    o->active_refused = !mc_falling_entity_tick_error_retire(o->tick) &&
                        o->tick->cleanup_status == MC_ENTITY_BUSY && o->tick->work != NULL;
    mc_item_stack_fields fields;
    if (!mc_entity_is_kind(item, MC_ENTITY_CLASS_KIND_ITEM) || item->base.world != world ||
        mc_item_stack_fields_get(item->stack, &fields) != MC_ITEM_STACK_OK || fields.id != 12 ||
        fields.count != 1 || fields.damage != 0 || item->item.c != 10 ||
        o->source->base.be != o->expected_dead || !o->active_refused) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_INVALID_DATA,
                                          "authored FallingSand publication observation", 0};
        return MC_WORLD_STORAGE_INVALID_DATA;
    }
    if (o->fail) {
        *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                          "authored FallingSand publication failure", 0};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool falling_drop_cases(fixture *f, mc_entity *p) {
    bool ok = false, registered = false;
    mc_world_access *access = NULL;
    mc_falling_entity_tick_error e = {0};
    falling_observer observer = {.tick = &e, .source = p};
    const mc_world_access_callbacks callbacks = {.entity_added = falling_added};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->worlds[0], access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    CHECK(falling_reset(f, p, 12, 99));
    p->base.bq = true;
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e) == MC_ENTITY_OK &&
          p->falling.b == 100 && !p->base.be && observer.calls == 0);
    const mc_java_random before = *p->random;
    CHECK(falling_reset(f, p, 12, 100));
    p->base.bq = true;
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e) == MC_ENTITY_OK &&
          p->falling.b == 101 && p->base.be && observer.calls == 1 && observer.active_refused &&
          observer.item != NULL && mc_test_java_random_equal(p->random, &before) && e.work == NULL);
    /* Timeout publication failure occurs BEFORE its K; the same concrete Item
     * is already in World/Chunk lists and remains owned after tick retirement. */
    CHECK(falling_reset(f, p, 12, 100));
    p->base.bq = true;
    observer.fail = true;
    observer.expected_dead = false;
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e) ==
              MC_ENTITY_WORLD_FAILED &&
          e.child == MC_FALLING_TICK_CHILD_PUBLICATION && e.bytecode == 312 &&
          e.drop_bytecode == 39 && strcmp(e.drop_method, "a(Liz;F)Lhl;") == 0 &&
          e.item == observer.item && e.publication != NULL &&
          e.publication->status == MC_WORLD_STORAGE_IO_ERROR && p->falling.b == 101 &&
          !p->base.be && e.item->base.world == f->worlds[0]);
    mc_entity *published = e.item;
    CHECK(mc_entity_retain(published) == MC_ENTITY_OK);
    const size_t held = published->holds;
    CHECK(mc_falling_entity_tick_error_retire(&e) && published->holds == held - 1 &&
          published->phase == MC_ENTITY_READY && published->item.c == 10 && !p->base.be);
    mc_entity_release(published);
    /* A failed landing kills BEFORE its same failed drop invocation. */
    CHECK(falling_reset(f, p, 12, 0));
    p->base.bq = p->base.aX = true;
    observer.expected_dead = true;
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, &e) ==
              MC_ENTITY_WORLD_FAILED &&
          e.bytecode == 280 && e.child == MC_FALLING_TICK_CHILD_PUBLICATION && p->base.be &&
          mc_falling_entity_tick_error_retire(&e));
    ok = true;
cleanup:
    if (!mc_falling_entity_tick_error_retire(&e))
        ok = false;
    if (registered) {
        bool removed = false;
        if (mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL) !=
                MC_WORLD_STORAGE_OK ||
            !removed)
            ok = false;
    }
    if (mc_world_access_free(access, NULL) != MC_WORLD_STORAGE_OK)
        ok = false;
    return ok;
}

static bool falling_admission(fixture *f, mc_entity *p) {
    bool ok = false;
    mc_entity *zero = NULL, *raw = NULL, *item = NULL;
    mc_falling_entity_tick_error e = {0};
    CHECK(mc_falling_entity_tick_in_context(p, f->registry[0], NULL, NULL) ==
          MC_ENTITY_INVALID_ARGUMENT);
    CHECK(mc_falling_entity_allocate(f->entity_classes[0], &zero, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct_world(zero, NULL, NULL) == MC_ENTITY_OK);
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x40u);
    const mc_entity_status status = mc_falling_entity_tick_in_context(zero, NULL, NULL, &e);
    _mm_setcsr(csr);
    CHECK(status == MC_ENTITY_OK && zero->base.be && zero->falling.b == 0 && zero->base.bt == 0 &&
          e.work == NULL);
    CHECK(mc_falling_entity_allocate(f->entity_classes[0], &raw, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_tick_in_context(raw, NULL, NULL, &e) == MC_ENTITY_INVALID_PHASE &&
          e.work == NULL && raw->phase == MC_ENTITY_RAW && mc_falling_entity_tick_error_retire(&e));
    CHECK(mc_item_entity_allocate(f->entity_classes[0], &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(item, NULL, 8.5, 8.5, 8.5, NULL, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_tick_in_context(item, NULL, NULL, &e) == MC_ENTITY_STATE_ERROR &&
          e.work == NULL && !item->base.be && mc_falling_entity_tick_error_retire(&e));
    /* Source no-clip mover needs no World, then ju's own query fails at141. */
    zero->falling.a = 12;
    zero->base.bq = true;
    zero->base.be = false;
    CHECK(mc_falling_entity_tick_in_context(zero, f->registry[0], NULL, &e) ==
              MC_ENTITY_STATE_ERROR &&
          e.bytecode == 141 && zero->falling.b == 1 && !zero->base.be &&
          mc_falling_entity_tick_error_retire(&e) && zero->holds == 1);
    ok = true;
cleanup:
    if (!mc_falling_entity_tick_error_retire(&e))
        ok = false;
    mc_entity_release(zero);
    mc_entity_release(raw);
    mc_entity_release(item);
    return ok;
}

static bool falling_world_dispatch(fixture *f, mc_entity *p) {
    bool ok = false;
    mc_world_entity_tick_error *error = calloc(1, sizeof(*error));
    mc_world_entity_tick_context context = {0};
    context.player.travel.code_registry = f->registry[0];
    CHECK(error != NULL && falling_reset(f, p, 12, 3));
    p->base.bq = true;
    bool added = false;
    CHECK(mc_world_runtime_add_weather_entity(f->worlds[0], p, &added, NULL) ==
              MC_WORLD_STORAGE_OK &&
          added);
    const double x = p->base.x, y = p->base.y, z = p->base.z;
    /* The real weather publication accepts the same SN instance. fd.g reaches
     * its virtual child directly, without inventing a surrounding Chunk grid.
     * NULL diagnostics refuse exactly that native ownership admission. */
    CHECK(mc_world_runtime_tick_entities(f->worlds[0], &context, NULL, NULL) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          p->falling.b == 3 && p->base.x == x && p->base.y == y && p->base.z == z && !p->base.be);
    CHECK(mc_world_runtime_tick_entities(f->worlds[0], &context, NULL, error) ==
              MC_WORLD_STORAGE_OK &&
          error->child == MC_WORLD_ENTITY_TICK_CHILD_NONE && p->falling.b == 4 && !p->base.be &&
          mc_world_entity_tick_error_retire(error));
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x40u);
    const mc_world_storage_status status =
        mc_world_runtime_tick_entities(f->worlds[0], &context, NULL, error);
    _mm_setcsr(csr);
    CHECK(status == MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          error->child == MC_WORLD_ENTITY_TICK_CHILD_FALLING_SAND &&
          error->child_error.falling.status == MC_ENTITY_UNSUPPORTED_FP &&
          error->child_error.falling.bytecode == 54 && error->child_error.falling.work != NULL &&
          p->falling.b == 5 && !p->base.be && f->worlds[0]->active_calls == 0 &&
          mc_world_entity_tick_error_retire(error) &&
          error->child == MC_WORLD_ENTITY_TICK_CHILD_NONE);
    ok = true;
cleanup:
    if (error != NULL && !mc_world_entity_tick_error_retire(error))
        return false;
    free(error);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_entity *falling = NULL;
    char root[8192];
    const uint16_t title[] = {'F', 'a', 'l', 'l', 'T', 'i', 'c', 'k'};
    if (argc != 3 || strcmp(argv[1], "falling-tick") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_materials_create(&f->materials) &&
          mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK &&
          mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
              MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK &&
              mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK &&
              mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
                  MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK &&
              mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              boxes_open_store(f, side, root) && boxes_create_world(f, side));
    }
    CHECK(mc_falling_entity_allocate(f->entity_classes[0], &falling, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct(falling, f->worlds[0], 8.5, 8.5, 8.5, 12, NULL) ==
              MC_ENTITY_OK);
    CHECK(falling_admission(f, falling) && falling_motion_cases(f, falling) &&
          falling_landing_cases(f, falling) && falling_drop_cases(f, falling) &&
          falling_world_dispatch(f, falling));
    result = 0;
cleanup:
    mc_entity_release(falling);
    if (!cleanup_fixture(f))
        return 1;
    if (result == 0 && printf("PASS: %u FallingSand tick checks\n", checks) < 0)
        return 1;
    return result;
}
