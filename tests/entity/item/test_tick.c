/* Reuse genuine GAME World/Store/Registry/Chunk fixture; imported mains are
 * retained but never run. No Player or replacement World is constructed. */
#define MC_PLAYER_MOVEMENT_MAIN mc_item_tick_movement_fixture_main
#include "../movement/test_player_movement.c"
#undef MC_PLAYER_MOVEMENT_MAIN
#include "../../../src/entity/item/tick_internal.h"
#include "mc/entity/item/tick.h"

static bool item_reference;

typedef struct item_observer {
    mc_entity *item;
    int32_t scenario;
    size_t event, sounds, particles;
    bool fail_sound, fail_particle, mutate;
} item_observer;

static mc_world_storage_status item_sound(void *context, mc_world_runtime *world,
                                          const mc_registry_string *name, double x, double y,
                                          double z, float volume, float pitch,
                                          mc_world_storage_error *error) {
    item_observer *o = context;
    (void)world;
    if (item_reference) {
        printf("EVENT %" PRId32 " %zu S ", o->scenario, o->event);
        for (size_t i = 0; i < name->length; ++i)
            putchar((int)name->units[i]);
        printf(" %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %08" PRIx32 " %08" PRIx32 "\n",
               movement_double_bits(x), movement_double_bits(y), movement_double_bits(z),
               movement_float_bits(volume), movement_float_bits(pitch));
    }
    ++o->event;
    ++o->sounds;
    if (o->mutate) {
        (void)mc_entity_set_position(o->item, 8.25, 8.5, 8.75);
        (void)mc_entity_set_velocity(o->item, 0, 1.25);
        (void)mc_entity_set_velocity(o->item, 1, -.5);
        (void)mc_entity_set_velocity(o->item, 2, 2);
    }
    if (o->fail_sound) {
        if (error != NULL)
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                              "authored reached Item sound failure", 0};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status item_particle(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, double vx, double vy, double vz,
                                             mc_world_storage_error *error) {
    item_observer *o = context;
    (void)world;
    if (item_reference) {
        printf("EVENT %" PRId32 " %zu P ", o->scenario, o->event);
        for (size_t i = 0; i < name->length; ++i)
            putchar((int)name->units[i]);
        printf(" %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
               " %016" PRIx64 "\n",
               movement_double_bits(x), movement_double_bits(y), movement_double_bits(z),
               movement_double_bits(vx), movement_double_bits(vy), movement_double_bits(vz));
    }
    ++o->event;
    ++o->particles;
    if (o->fail_particle) {
        if (error != NULL)
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                              "authored reached Item particle failure", 0};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_entity_watch_record *item_record(mc_entity *p) {
    for (mc_entity_watch_node *n = p->watcher->map->head; n != NULL; n = n->next)
        if (n->key == 0)
            return n->record;
    return NULL;
}

static void item_fluid(fixture *f, uint8_t id) {
    for (int32_t x = 7; x <= 10; ++x)
        for (int32_t z = 7; z <= 10; ++z)
            for (int32_t y = 7; y <= 10; ++y)
                f->states[0]->data->blocks[boxes_index(x, y, z)] = id;
}

static bool item_reset(fixture *f, mc_entity *p, item_observer *o, int32_t scenario) {
    bool ok = false;
    movement_air(f, 0);
    p->base.aX = p->base.aY = p->base.aZ = p->base.ba = p->base.bb = p->base.bc = false;
    p->base.bd = true;
    p->base.be = p->base.bq = p->base.bx = p->base.bC = false;
    p->base.bp = p->base.y_size = p->base.bk = 0;
    p->base.offset = .125f;
    p->base.private_c = true;
    p->base.bt = 17;
    p->base.bj = .75f;
    p->base.bi = -77;
    p->base.aJ = -9;
    p->base.aK = -10;
    p->base.aL = -11;
    p->base.aU = -13;
    p->base.aV = -14;
    p->base.yaw = 25;
    p->base.aT = -17;
    p->base.bu = 5;
    p->base.bv = 0;
    p->base.aG = p->base.aH = NULL;
    p->item.b = p->item.e = 0;
    p->item.c = 0;
    p->item.f = 5;
    p->item.d = .25f;
    CHECK(mc_entity_set_size(p, .25f, .25f) == MC_ENTITY_OK &&
          mc_entity_set_position(p, 8.5, 8.5, 8.5) == MC_ENTITY_OK);
    p->base.velocity_x = .25;
    p->base.velocity_y = 0;
    p->base.velocity_z = -.125;
    mc_entity_watch_record *record = item_record(p);
    CHECK(record != NULL);
    record->byte_value = 0;
    record->dirty = false;
    p->watcher->dirty = false;
    f->worlds[0]->fields.B = false;
    mc_java_random_reseed(p->random, 38);
    CHECK(mc_entity_bounds_pool_reset(p->classes, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(p->classes, NULL) == MC_ENTITY_OK);
    *o = (item_observer){.item = p, .scenario = scenario};
    ok = true;
cleanup:
    return ok;
}

static bool item_row(mc_entity *p, item_observer *o, bool complete) {
    bool ok = false;
    mc_entity_watcher_snapshot w;
    mc_entity_vector_snapshot v = {0};
    CHECK(mc_entity_watcher_get_snapshot(p->watcher, &w) == MC_ENTITY_OK &&
          mc_entity_vector_get_snapshot(p->classes, &v) == MC_ENTITY_OK);
    if (item_reference) {
        printf("STATE %" PRId32 " %d %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32
               " %08" PRIx32,
               o->scenario, complete ? 1 : 0, p->base.bt, p->item.b, p->item.e, p->item.c,
               p->item.f, movement_float_bits(p->item.d));
#define ITEM_D(field) printf(" %016" PRIx64, movement_double_bits(p->base.field))
        ITEM_D(x);
        ITEM_D(y);
        ITEM_D(z);
        ITEM_D(velocity_x);
        ITEM_D(velocity_y);
        ITEM_D(velocity_z);
        ITEM_D(aJ);
        ITEM_D(aK);
        ITEM_D(aL);
#undef ITEM_D
#define ITEM_F(field) printf(" %08" PRIx32, movement_float_bits(p->base.field))
        ITEM_F(aU);
        ITEM_F(aV);
        ITEM_F(yaw);
        ITEM_F(aT);
        ITEM_F(offset);
        ITEM_F(width);
        ITEM_F(height);
        ITEM_F(bi);
        ITEM_F(bj);
        ITEM_F(bk);
        ITEM_F(y_size);
#undef ITEM_F
#define ITEM_BOX(field) printf(" %016" PRIx64, movement_double_bits(p->bounds->field))
        ITEM_BOX(min_x);
        ITEM_BOX(min_y);
        ITEM_BOX(min_z);
        ITEM_BOX(max_x);
        ITEM_BOX(max_y);
        ITEM_BOX(max_z);
#undef ITEM_BOX
        printf(" %d %d %d %d %d %d %d %d %d %" PRId32 " %d %d %d %d %" PRId32 " %" PRId32,
               p->base.aX ? 1 : 0, p->base.aY ? 1 : 0, p->base.aZ ? 1 : 0, p->base.ba ? 1 : 0,
               p->base.bb ? 1 : 0, p->base.bc ? 1 : 0, p->base.bd ? 1 : 0, p->base.be ? 1 : 0,
               p->base.bx ? 1 : 0, p->base.bv, p->base.private_c ? 1 : 0, (int)w.byte_value,
               w.record_dirty ? 1 : 0, w.dirty ? 1 : 0, p->classes->bounds_cursor, v.cursor);
        printf(" %012" PRIx64 " %016" PRIx64 " %d %zu %zu\n", p->random->state,
               movement_double_bits(p->random->next_next_gaussian),
               p->random->have_next_next_gaussian ? 1 : 0, o->sounds, o->particles);
    }
    ok = true;
cleanup:
    return ok;
}

static bool item_native_controls(fixture *f, mc_entity *p) {
    bool ok = false;
    item_observer o = {0};
    mc_entity *raw = NULL, *falling = NULL, *null_stack = NULL;
    mc_item_entity_tick_error e;
    CHECK(mc_item_entity_allocate(f->entity_classes[0], &raw, NULL) == MC_ENTITY_OK);
    CHECK(mc_item_entity_tick_in_context(raw, f->registry[0], NULL, &e) ==
              MC_ENTITY_INVALID_PHASE &&
          raw->phase == MC_ENTITY_RAW && e.child == MC_ITEM_TICK_CHILD_NONE);
    CHECK(mc_falling_entity_allocate(f->entity_classes[0], &falling, NULL) == MC_ENTITY_OK &&
          mc_falling_entity_construct_world(falling, NULL, NULL) == MC_ENTITY_OK);
    CHECK(mc_item_entity_tick_in_context(falling, f->registry[0], NULL, &e) ==
              MC_ENTITY_STATE_ERROR &&
          falling->base.bt == 0 && e.child == MC_ITEM_TICK_CHILD_NONE);
    CHECK(item_reset(f, p, &o, 100));
    size_t holds = p->holds;
    p->holds = SIZE_MAX;
    const mc_entity_status hold_status =
        mc_item_entity_tick_in_context(p, f->registry[0], NULL, &e);
    p->holds = holds;
    CHECK(hold_status == MC_ENTITY_LIMIT && p->base.bt == 17 && p->item.b == 0 &&
          e.child == MC_ITEM_TICK_CHILD_NONE);
    mc_item_entity_tick_limits limits = {
        .environment = {.max_particle_work = MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK,
                        .material = {.max_cell_work =
                                         MC_WORLD_MATERIAL_MOVEMENT_DEFAULT_CELL_WORK}},
        .movement = {.max_loop_work = MC_ENTITY_MOVE_DEFAULT_LOOP_WORK}};
    CHECK(item_reset(f, p, &o, 101));
    p->item.c = 3;
    limits.environment.material.max_cell_work = 0;
    CHECK(mc_item_entity_tick_in_context(p, f->registry[0], &limits, &e) != MC_ENTITY_OK &&
          e.child == MC_ITEM_TICK_CHILD_BASE &&
          e.child_error.base.environment.material.status == MC_WORLD_STORAGE_LIMIT &&
          p->base.bt == 18 && p->item.c == 3 && p->item.b == 0);
    CHECK(item_reset(f, p, &o, 102));
    item_fluid(f, 8);
    p->base.private_c = false;
    p->item.c = 3;
    limits.environment.material.max_cell_work = MC_WORLD_MATERIAL_MOVEMENT_DEFAULT_CELL_WORK;
    limits.environment.max_particle_work = 0;
    mc_java_random expected = *p->random;
    (void)mc_java_random_next_float(&expected);
    (void)mc_java_random_next_float(&expected);
    CHECK(mc_item_entity_tick_in_context(p, f->registry[0], &limits, &e) == MC_ENTITY_LIMIT &&
          e.child == MC_ITEM_TICK_CHILD_BASE && e.child_error.base.bytecode == 217 &&
          e.child_error.base.max_particle_work == 0 && e.child_error.base.particle_work == 0 &&
          mc_test_java_random_equal(p->random, &expected) && p->item.c == 3 && !p->base.bx &&
          !p->base.private_c);
    CHECK(item_reset(f, p, &o, 103));
    p->item.c = 3;
    limits.environment.max_particle_work = MC_ENTITY_TICK_DEFAULT_PARTICLE_WORK;
    limits.movement.max_loop_work = 0;
    CHECK(mc_item_entity_tick_in_context(p, f->registry[0], &limits, &e) == MC_ENTITY_LIMIT &&
          e.child == MC_ITEM_TICK_CHILD_MOVEMENT && e.child_error.movement.loop_work == 0 &&
          e.child_error.movement.max_loop_work == 0 && p->item.c == 2 && p->item.b == 0 &&
          p->item.e == 0);
    CHECK(item_reset(f, p, &o, 104));
    const unsigned saved_csr = _mm_getcsr();
    const mc_java_random random_before = *p->random;
    _mm_setcsr(saved_csr | 0x40u);
    const mc_entity_status fp_status = mc_item_entity_tick_in_context(p, f->registry[0], NULL, &e);
    _mm_setcsr(saved_csr);
    CHECK(fp_status != MC_ENTITY_OK && e.child == MC_ITEM_TICK_CHILD_BASE && p->base.bt == 18 &&
          p->item.b == 0 && mc_test_java_random_equal(p->random, &random_before));
    CHECK(mc_item_entity_allocate(f->entity_classes[0], &null_stack, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(null_stack, f->worlds[0], 8.5, 8.5, 8.5, NULL, NULL) ==
              MC_ENTITY_OK &&
          item_reset(f, null_stack, &o, 105));
    CHECK(mc_item_entity_tick_in_context(null_stack, f->registry[0], NULL, NULL) == MC_ENTITY_OK &&
          null_stack->stack == NULL && null_stack->item.b == 1 && null_stack->holds == 1);
    CHECK(item_reset(f, p, &o, 106) &&
          mc_item_entity_tick_in_context(p, f->registry[0], NULL, &e) == MC_ENTITY_OK);
    const size_t class_bytes = p->classes->allocated_bytes,
                 arena = f->registry[0]->allocation_bytes,
                 managed = f->registry[0]->managed_current_bytes,
                 direct = mc_entity_native_bytes(p);
    for (size_t i = 0; i < 128; i++) {
        CHECK(item_reset(f, p, &o, 106));
        const mc_java_random before = *p->random;
        CHECK(mc_item_entity_tick_in_context(p, f->registry[0], NULL, &e) == MC_ENTITY_OK &&
              p->item.b == 1 && p->item.e == 1 && p->holds == 1 &&
              p->classes->allocated_bytes == class_bytes &&
              f->registry[0]->allocation_bytes == arena &&
              f->registry[0]->managed_current_bytes == managed &&
              mc_entity_native_bytes(p) == direct &&
              mc_test_java_random_equal(p->random, &before) && e.child == MC_ITEM_TICK_CHILD_NONE &&
              p->classes->bounds_holds == 0 && p->classes->vector_holds == 0 &&
              f->worlds[0]->active_calls == 0 && f->worlds[0]->local_leases == 0);
    }
    ok = true;
cleanup:
    mc_entity_release(raw);
    mc_entity_release(falling);
    mc_entity_release(null_stack);
    return ok;
}

static bool item_cases(fixture *f, mc_entity *p, mc_entity *null_world) {
    bool ok = false, registered = false;
    item_observer observer = {0};
    mc_world_access *access = NULL;
    mc_item_entity_tick_error error;
    const mc_world_access_callbacks callbacks = {.sound = item_sound, .particle = item_particle};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->worlds[0], access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    for (int32_t scenario = 0; scenario < 37; ++scenario) {
        mc_entity *item = scenario == 30 ? null_world : p;
        CHECK(item_reset(f, item, &observer, scenario));
        if (scenario == 1 || scenario == 2) {
            for (int32_t x = 6; x <= 12; ++x)
                for (int32_t z = 6; z <= 12; ++z)
                    f->states[0]->data->blocks[boxes_index(x, 7, z)] =
                        (uint8_t)(scenario == 1 ? 1 : 79);
            CHECK(mc_entity_set_position(item, 8.5, 8.125, 8.5) == MC_ENTITY_OK);
        }
        if (scenario == 3)
            item->item.c = 3;
        if (scenario == 4)
            item->item.c = -1;
        if (scenario == 5)
            item->item.b = 5999;
        if (scenario == 6)
            item->item.b = INT32_MAX;
        if (scenario == 7)
            item->item.e = INT32_MAX;
        if (scenario == 8)
            item->base.be = true;
        if (scenario == 9) {
            item->base.bx = true;
            item->base.private_c = false;
        }
        if (scenario == 10) {
            item->base.bv = 20;
            item->item.f = 1;
        }
        if (scenario == 11) {
            item->base.bv = 3;
            item->base.bC = true;
        }
        if (scenario == 12 || scenario == 13 || scenario == 19 || scenario == 20) {
            item_fluid(f, 10);
            item->base.bC = scenario == 13;
        }
        if (scenario >= 14 && scenario <= 18) {
            item_fluid(f, 8);
            item->base.private_c = scenario == 14;
            item->base.bx = scenario == 16;
        }
        if (scenario == 17 || scenario == 19)
            observer.fail_sound = true;
        if (scenario == 18)
            observer.fail_particle = true;
        if (scenario == 20)
            observer.mutate = true;
        if (scenario >= 21 && scenario <= 28) {
            f->states[0]->data->blocks[boxes_index(8, 8, 8)] = 1;
            static const int32_t dx[] = {-1, 1, 0, 0, 0, 0}, dy[] = {0, 0, -1, 1, 0, 0},
                                 dz[] = {0, 0, 0, 0, -1, 1};
            for (int i = 0; i < 6; i++)
                if (scenario != 21 && !(scenario >= 23 && i == scenario - 23))
                    f->states[0]->data->blocks[boxes_index(8 + dx[i], 8 + dy[i], 8 + dz[i])] = 1;
        }
        if (scenario == 29)
            CHECK(mc_entity_set_position(item, 8.5, -65, 8.5) == MC_ENTITY_OK);
        if (scenario == 31)
            item->base.bt = INT32_MAX;
        if (scenario >= 32 && scenario <= 35) {
            item->base.bq = true;
            uint64_t bits[3] = {UINT64_C(0x8000000000000000), 0, UINT64_C(0x8000000000000000)};
            if (scenario == 32) {
                bits[0] = UINT64_C(0x7ff8123456789abc);
                bits[1] = UINT64_C(0xfff8456789abcdef);
                bits[2] = UINT64_C(0x7ff876543210abcd);
            }
            if (scenario == 33) {
                bits[0] = UINT64_C(0x7ff0000000000001);
                bits[1] = UINT64_C(0xfff0000000000001);
                bits[2] = UINT64_C(0x7ff0000000000002);
            }
            if (scenario == 34) {
                bits[0] = UINT64_C(0x7ff0000000000000);
                bits[1] = UINT64_C(0xfff0000000000000);
                bits[2] = UINT64_C(0x7ff0000000000000);
            }
            memcpy(&item->base.velocity_x, &bits[0], sizeof(double));
            memcpy(&item->base.velocity_y, &bits[1], sizeof(double));
            memcpy(&item->base.velocity_z, &bits[2], sizeof(double));
        }
        if (scenario == 36) {
            item_fluid(f, 8);
            item->base.private_c = false;
            observer.mutate = true;
        }

        const size_t holds = item->holds;
        const mc_entity_status status =
            mc_item_entity_tick_in_context(item, f->registry[0], NULL, &error);
        const bool expected_failure =
            scenario == 17 || scenario == 18 || scenario == 19 || scenario == 30;
        if ((status == MC_ENTITY_OK) != !expected_failure) {
            fprintf(stderr, "unexpected scenario %d status %d operation %s pc %u child %d\n",
                    scenario, (int)status, error.operation, error.bytecode, (int)error.child);
            if (error.child == MC_ITEM_TICK_CHILD_BASE)
                fprintf(stderr, "base %s pc %u work %zu/%zu\n", error.child_error.base.operation,
                        error.child_error.base.bytecode, error.child_error.base.particle_work,
                        error.child_error.base.max_particle_work);
        }
        CHECK((status == MC_ENTITY_OK) == !expected_failure && error.status == status &&
              item->holds == holds);
        CHECK(item_row(item, &observer, status == MC_ENTITY_OK));
        CHECK(mc_world_collision_owned_valid(f->worlds[0]) &&
              item->classes->bounds_holds == f->worlds[0]->collision_boxes.count &&
              item->classes->vector_holds == 0 && f->worlds[0]->active_calls == 0 &&
              f->worlds[0]->local_leases == 0);
        if (!expected_failure)
            CHECK(error.child == MC_ITEM_TICK_CHILD_NONE && error.bytecode == 0);
        if (scenario == 3)
            CHECK(item->item.c == 2);
        if (scenario == 4)
            CHECK(item->item.c == -1);
        if (scenario == 5)
            CHECK(item->item.b == 6000 && item->base.be);
        if (scenario == 6)
            CHECK(item->item.b == INT32_MIN && !item->base.be);
        if (scenario == 7)
            CHECK(item->item.e == INT32_MIN);
        if (scenario == 8)
            CHECK(item->base.be && item->item.b == 1);
        if (scenario == 29)
            CHECK(item->base.be && item->item.b == 1 && item->item.f == 5);
        if (scenario == 30)
            CHECK(error.bytecode == 1 && error.child == MC_ITEM_TICK_CHILD_BASE &&
                  item->item.b == 0 && item->base.bt == 18 && item->base.private_c);
    }
    {
        bool removed = false;
        CHECK(mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              removed);
    }
    registered = false;
    CHECK(mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    access = NULL;
    ok = true;
cleanup:
    if (registered) {
        bool removed = false;
        (void)mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL);
    }
    (void)mc_world_access_free(access, NULL);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_entity *item = NULL, *null_world = NULL;
    char root[8192];
    const uint16_t title[] = {'I', 't', 'e', 'm', 'T', 'i', 'c', 'k'};
    if (argc != 3 ||
        (strcmp(argv[1], "item-tick") != 0 && strcmp(argv[1], "item-tick-reference") != 0) ||
        !decode_path(argv[2], root, sizeof root) || !mc_math_trig_init(&trig))
        return 1;
    item_reference = strcmp(argv[1], "item-tick-reference") == 0;
    f = calloc(1, sizeof *f);
    CHECK(f != NULL && mc_materials_create(&f->materials));
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    CHECK(mc_item_stack_allocate(&f->stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(f->stack, 1, 1, 0) == MC_ITEM_STACK_OK);
    for (size_t side = 0; side < 2; side++) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
        r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
        const mc_registry_options options = {side == 0 ? MC_REGISTRY_CLIENT : MC_REGISTRY_SERVER,
                                             f->profile, f->materials, 0, &r->api};
        CHECK(mc_registry_classes_allocate(&options, &f->registry[side], NULL) == MC_REGISTRY_OK);
        CHECK(mc_world_runtime_classes_create(
                  side == 0 ? MC_WORLD_INFO_CLIENT : MC_WORLD_INFO_SERVER, f->profile, &trig,
                  &f->classes[side], NULL) == MC_WORLD_STORAGE_OK);
        CHECK(mc_world_runtime_classes_bind_registry(f->classes[side], f->registry[side], NULL) ==
                  MC_REGISTRY_OK &&
              mc_entity_classes_create(side == 0 ? MC_ENTITY_CLIENT : MC_ENTITY_SERVER, f->profile,
                                       NULL, &f->entity_classes[side]) == MC_ENTITY_OK &&
              mc_world_runtime_classes_bind_entities(f->classes[side], f->entity_classes[side]) ==
                  MC_ENTITY_OK);
        CHECK(mc_registry_classes_initialize_blocks(f->registry[side], NULL) == MC_REGISTRY_OK &&
              boxes_open_store(f, side, root) && boxes_create_world(f, side));
    }
    CHECK(mc_item_entity_allocate(f->entity_classes[0], &item, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(item, f->worlds[0], 8.5, 8.5, 8.5, f->stack, NULL) ==
              MC_ENTITY_OK);
    CHECK(mc_item_entity_allocate(f->entity_classes[0], &null_world, NULL) == MC_ENTITY_OK &&
          mc_item_entity_construct(null_world, NULL, 8.5, 8.5, 8.5, f->stack, NULL) ==
              MC_ENTITY_OK);
    CHECK(item_cases(f, item, null_world));
    CHECK(item_native_controls(f, item));
    result = 0;
cleanup:
    mc_entity_release(item);
    mc_entity_release(null_world);
    if (!cleanup_fixture(f))
        return 1;
    if (result == 0 && printf("Item tick: %u checks\n", checks) < 0)
        return 1;
    return result;
}
