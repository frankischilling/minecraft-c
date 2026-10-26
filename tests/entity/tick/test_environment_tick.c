#define MC_PLAYER_MOVEMENT_MAIN mc_base_tick_movement_fixture_main
#include "../movement/test_player_movement.c"
#undef MC_PLAYER_MOVEMENT_MAIN
#include "mc/entity/tick/environment.h"
#include <xmmintrin.h>

static bool tick_reference;

typedef struct tick_observer {
    mc_entity *player;
    int32_t scenario;
    size_t sounds, particles, event;
    bool fail_sound, shrink, change_particle_fp;
    size_t fail_particle; /* SIZE_MAX means no failure. */
} tick_observer;

static mc_world_storage_status tick_sound(void *context, mc_world_runtime *world,
                                          const mc_registry_string *name, double x, double y,
                                          double z, float volume, float pitch,
                                          mc_world_storage_error *error) {
    tick_observer *o = context;
    (void)world;
    if (tick_reference) {
        printf("EVENT %" PRId32 " %zu S ", o->scenario, o->event);
        for (size_t i = 0; i < name->length; ++i)
            putchar((int)name->units[i]);
        printf(" %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %08" PRIx32 " %08" PRIx32 "\n",
               movement_double_bits(x), movement_double_bits(y), movement_double_bits(z),
               movement_float_bits(volume), movement_float_bits(pitch));
    }
    ++o->event;
    ++o->sounds;
    if (o->shrink) {
        (void)mc_entity_set_size(o->player, .25f, 1.0f);
        (void)mc_entity_set_velocity(o->player, 0, 1.25);
        (void)mc_entity_set_velocity(o->player, 1, -.5);
        (void)mc_entity_set_velocity(o->player, 2, 2.0);
    }
    if (o->fail_sound) {
        if (error != NULL)
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                              "authored reached splash sound failure", 0};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_world_storage_status tick_particle(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, double vx, double vy, double vz,
                                             mc_world_storage_error *error) {
    tick_observer *o = context;
    (void)world;
    if (tick_reference) {
        printf("EVENT %" PRId32 " %zu P ", o->scenario, o->event);
        for (size_t i = 0; i < name->length; ++i)
            putchar((int)name->units[i]);
        printf(" %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
               " %016" PRIx64 "\n",
               movement_double_bits(x), movement_double_bits(y), movement_double_bits(z),
               movement_double_bits(vx), movement_double_bits(vy), movement_double_bits(vz));
    }
    ++o->event;
    const size_t index = o->particles++;
    if (o->change_particle_fp)
        _mm_setcsr(_mm_getcsr() | 0x40u);
    if (index == o->fail_particle) {
        if (error != NULL)
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                              "authored reached particle failure", 0};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static mc_entity_watch_record *tick_record(mc_entity *p) {
    mc_entity_watch_node *node = p->watcher->map->head;
    while (node != NULL && node->key != 0)
        node = node->next;
    return node == NULL ? NULL : node->record;
}

static bool tick_reset(fixture *f, mc_entity *p, tick_observer *o, int32_t scenario) {
    bool ok = false;
    movement_air(f, 0);
    CHECK(movement_input(p, 8.5, 8, 8.5));
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
    p->base.bw = 300;
    p->base.bz = 123;
    p->base.by = 3;
    p->base.bk = 1.25f;
    mc_living_entity_fields *v = &p->local_player->fields.living;
    v->W = -31;
    v->X = .25f;
    v->H = .375f;
    v->I = -32;
    v->L = .5f;
    v->M = -33;
    v->af = -34;
    v->ag = .75f;
    v->ae = 3;
    v->aa = 4;
    v->ad = 0;
    v->private_a = 0;
    v->Y = 20;
    v->av = 19;
    p->local_player->fields.player.u = false;
    mc_entity_watch_record *record = tick_record(p);
    CHECK(record != NULL);
    record->byte_value = 0;
    record->dirty = false;
    p->watcher->dirty = false;
    f->worlds[0]->fields.B = false;
    mc_java_random_reseed(p->random, 38);
    CHECK(mc_entity_bounds_pool_reset(p->classes, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(p->classes, NULL) == MC_ENTITY_OK);
    *o = (tick_observer){.player = p, .scenario = scenario, .fail_particle = SIZE_MAX};
    ok = true;
cleanup:
    return ok;
}

static void tick_fluid(fixture *f, uint8_t id) {
    for (int32_t x = 7; x <= 10; ++x)
        for (int32_t z = 7; z <= 10; ++z)
            for (int32_t y = 7; y <= 10; ++y)
                f->states[0]->data->blocks[boxes_index(x, y, z)] = id;
}

static bool tick_row(mc_entity *p, tick_observer *o, bool complete) {
    bool ok = false;
    mc_entity_watcher_snapshot w;
    mc_entity_vector_snapshot vector = {0};
    CHECK(mc_entity_watcher_get_snapshot(p->watcher, &w) == MC_ENTITY_OK &&
          mc_entity_vector_get_snapshot(p->classes, &vector) == MC_ENTITY_OK);
    mc_living_entity_fields *v = &p->local_player->fields.living;
    if (tick_reference) {
        printf("STATE %" PRId32 " %d %" PRId32 " %08" PRIx32 " %016" PRIx64 " %016" PRIx64
               " %016" PRIx64 " %08" PRIx32 " %08" PRIx32,
               o->scenario, complete ? 1 : 0, p->base.bt, movement_float_bits(p->base.bi),
               movement_double_bits(p->base.aJ), movement_double_bits(p->base.aK),
               movement_double_bits(p->base.aL), movement_float_bits(p->base.aU),
               movement_float_bits(p->base.aV));
        printf(" %d %" PRId32 " %" PRId32 " %d %" PRId32 " %d %d %d", p->base.bx ? 1 : 0,
               p->base.bv, p->base.bz, p->base.private_c ? 1 : 0, (int32_t)w.byte_value,
               w.record_dirty ? 1 : 0, w.dirty ? 1 : 0, p->base.be ? 1 : 0);
        printf(" %08" PRIx32 " %" PRId32 " %08" PRIx32 " %08" PRIx32 " %" PRId32 " %" PRId32
               " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32,
               movement_float_bits(v->W), v->private_a, movement_float_bits(v->af),
               movement_float_bits(v->M), v->ae, v->aa, p->base.by, v->ad, v->av, v->Y);
        printf(" %08" PRIx32 " %08" PRIx32 " %012" PRIx64 " %016" PRIx64 " %d %zu %zu",
               movement_float_bits(v->I), movement_float_bits(p->base.bk), p->random->state,
               movement_double_bits(p->random->next_next_gaussian),
               p->random->have_next_next_gaussian ? 1 : 0, o->sounds, o->particles);
        printf(" %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %08" PRIx32 " %" PRId32 " %" PRId32
               "\n",
               movement_double_bits(p->base.velocity_x), movement_double_bits(p->base.velocity_y),
               movement_double_bits(p->base.velocity_z), movement_float_bits(p->base.width),
               p->classes->bounds_cursor, vector.cursor);
    }
    ok = true;
cleanup:
    return ok;
}

static bool tick_cases(fixture *f, mc_entity *p) {
    bool ok = false, registered = false;
    tick_observer observer = {0};
    mc_world_access *access = NULL;
    mc_entity_tick_error error;
    const mc_world_access_callbacks callbacks = {.sound = tick_sound, .particle = tick_particle};
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->worlds[0], access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    for (int32_t scenario = 0; scenario < 31; ++scenario) {
        CHECK(tick_reset(f, p, &observer, scenario));
        mc_living_entity_fields *v = &p->local_player->fields.living;
        if (scenario == 1)
            p->base.private_c = false;
        if (scenario == 2 || scenario == 3 || scenario == 4 || scenario == 15 || scenario == 16 ||
            scenario == 17 || scenario == 18 || scenario == 19) {
            tick_fluid(f, 8);
            p->base.private_c = scenario == 2;
            p->base.bx = scenario == 4 || scenario == 15 || scenario == 16;
        }
        if (scenario == 5) {
            p->base.bx = true;
            p->base.private_c = false;
        }
        if (scenario == 6)
            p->base.bv = 7;
        if (scenario == 7)
            p->base.bv = 20;
        if (scenario == 8) {
            p->base.bv = 20;
            v->Y = 0;
        }
        if (scenario == 9) {
            p->base.bv = 3;
            p->base.bC = true;
        }
        if (scenario == 10) {
            p->base.bv = 20;
            f->worlds[0]->fields.B = true;
        }
        if (scenario == 11 || scenario == 12) {
            tick_fluid(f, 10);
            p->base.bC = scenario == 12;
        }
        if (scenario == 13)
            CHECK(mc_entity_set_position(p, 8.5, -65, 8.5) == MC_ENTITY_OK);
        if (scenario == 14)
            f->states[0]->data->blocks[boxes_index(8, 8, 8)] = 1;
        if (scenario == 15 || scenario == 16) {
            p->base.bz = -19;
            if (scenario == 16)
                observer.fail_particle = 0;
        }
        if (scenario == 17)
            observer.fail_sound = true;
        if (scenario == 18)
            observer.fail_particle = 0;
        if (scenario == 19)
            observer.shrink = true;
        if (scenario == 20)
            v->private_a = 2000;
        if (scenario == 21)
            v->private_a = INT32_MAX;
        if (scenario == 22)
            v->private_a = INT32_MIN;
        if (scenario == 23) {
            v->Y = 0;
            v->ad = 19;
        }
        if (scenario >= 24 && scenario <= 28) {
            tick_fluid(f, 8);
            p->base.private_c = false;
        }
        if (scenario == 24) {
            const uint64_t bits[] = {UINT64_C(0x7ff8123456789abc), UINT64_C(0xfff8456789abcdef),
                                     UINT64_C(0x7ff876543210abcd)};
            memcpy(&p->base.velocity_x, &bits[0], sizeof(double));
            memcpy(&p->base.velocity_y, &bits[1], sizeof(double));
            memcpy(&p->base.velocity_z, &bits[2], sizeof(double));
        }
        if (scenario == 25) {
            const uint64_t positive = UINT64_C(0x7ff0000000000000),
                           negative = UINT64_C(0xfff0000000000000);
            memcpy(&p->base.velocity_x, &positive, sizeof(double));
            memcpy(&p->base.velocity_y, &negative, sizeof(double));
            memcpy(&p->base.velocity_z, &positive, sizeof(double));
        }
        if (scenario == 26) {
            const uint32_t bits = UINT32_C(0x7fc12345);
            memcpy(&p->base.width, &bits, sizeof(float));
        }
        if (scenario == 27)
            p->base.width = -.5f;
        if (scenario == 28)
            p->base.velocity_x = p->base.velocity_y = p->base.velocity_z = -0.0;
        if (scenario == 29) {
            f->states[0]->data->blocks[boxes_index(8, 8, 8)] = 8;
            f->states[0]->data->blocks[boxes_index(9, 7, 8)] = 8;
            p->base.private_c = false;
            p->base.bx = true;
        }
        const bool expected = !(scenario == 7 || scenario == 11 || scenario == 13 ||
                                scenario == 14 || scenario == 15 || scenario == 16 ||
                                scenario == 17 || scenario == 18 || scenario == 30);
        mc_world_runtime *saved_world = p->base.world;
        if (scenario == 30)
            p->base.world = NULL;
        const mc_entity_status status =
            mc_entity_tick_update_in_context(p, f->registry[0], NULL, &error);
        p->base.world = saved_world;
        if ((status == MC_ENTITY_OK) != expected)
            fprintf(stderr, "tick scenario %" PRId32 ": status%d %s base%u living%u\n", scenario,
                    (int)status, error.operation == NULL ? "-" : error.operation,
                    error.base_bytecode, error.living_bytecode);
        CHECK((status == MC_ENTITY_OK) == expected && p->holds == 1 &&
              f->worlds[0]->active_calls == 0 && f->worlds[0]->local_leases == 0);
        CHECK(tick_row(p, &observer, status == MC_ENTITY_OK));
        if (scenario == 3)
            CHECK(observer.sounds == 1 && observer.particles == 42 && p->base.bx &&
                  !p->base.private_c && p->base.bz == 122);
        if (scenario == 7)
            CHECK(error.base_bytecode == 501 && error.effect.player_bytecode == 152 &&
                  p->base.bv == 20 && v->private_a == 0);
        if (scenario == 15)
            CHECK(error.living_bytecode == 249 && observer.particles == 8 && p->base.bz == 0);
        if (scenario == 16)
            CHECK(error.living_bytecode == 237 && observer.particles == 1 && p->base.bz == 0);
        if (scenario == 17)
            CHECK(error.base_bytecode == 181 && observer.sounds == 1 && observer.particles == 0 &&
                  !p->base.bx && p->base.bk == 1.25f);
        if (scenario == 18)
            CHECK(error.base_bytecode == 299 && observer.particles == 1 && !p->base.bx);
        if (scenario == 19)
            CHECK(observer.sounds == 1 && observer.particles == 12 && p->base.width == .25f);
        if (scenario == 20 || scenario == 21)
            CHECK(v->private_a == -80 && observer.sounds == 0);
    }
    /* These are explicit native dependency/limit controls, not comparisons
     * with a completed original U method. */
    tick_reference = false;
    CHECK(tick_reset(f, p, &observer, 24));
    p->local_player->fields.living.Y = 0;
    p->local_player->fields.living.ad = 20;
    CHECK(mc_entity_tick_update_in_context(p, f->registry[0], NULL, &error) ==
              MC_ENTITY_STATE_ERROR &&
          error.living_bytecode == 359 && error.effect.close_bytecode == 9 && p->base.be &&
          observer.particles == 0);
    CHECK(tick_reset(f, p, &observer, 25));
    tick_fluid(f, 8);
    p->base.private_c = false;
    const mc_entity_tick_limits cap = {.max_particle_work = 1,
                                       .material = {MC_WORLD_MATERIAL_MOVEMENT_DEFAULT_CELL_WORK}};
    CHECK(mc_entity_tick_update_in_context(p, f->registry[0], &cap, &error) == MC_ENTITY_LIMIT &&
          error.base_bytecode == 209 && error.particle_work == 1 && observer.sounds == 1 &&
          observer.particles == 1 && !p->base.bx);
    CHECK(tick_reset(f, p, &observer, 26));
    mc_entity_watch_record *record = tick_record(p);
    CHECK(mc_entity_tick_flag_set(p, 0, false, &error) == MC_ENTITY_OK && record->byte_value == 0 &&
          !record->dirty && !p->watcher->dirty);
    CHECK(mc_entity_tick_flag_set(p, 2, true, &error) == MC_ENTITY_OK && record->byte_value == 4 &&
          record->dirty && p->watcher->dirty);
    record->dirty = false;
    p->watcher->dirty = false;
    CHECK(mc_entity_tick_flag_set(p, 34, true, &error) == MC_ENTITY_OK && record->byte_value == 4 &&
          !record->dirty && !p->watcher->dirty);
    CHECK(mc_entity_tick_flag_set(p, 2, false, &error) == MC_ENTITY_OK && record->byte_value == 0 &&
          record->dirty && p->watcher->dirty);
    CHECK(tick_reset(f, p, &observer, 27));
    p->local_player->fields.player.u = true;
    mc_world_runtime *saved = p->base.world;
    p->base.world = NULL;
    bool suffocating = true;
    const mc_entity_status sleeping = mc_entity_tick_suffocating(p, &suffocating, &error);
    p->base.world = saved;
    CHECK(sleeping == MC_ENTITY_OK && !suffocating);
    CHECK(tick_reset(f, p, &observer, 28));
    mc_entity *rider = NULL;
    CHECK(mc_item_entity_allocate(f->entity_classes[0], &rider, NULL) == MC_ENTITY_OK);
    CHECK(f->entity_count < ENTITY_COUNT);
    f->entities[f->entity_count++] = rider;
    CHECK(mc_item_entity_construct(rider, f->worlds[0], 8.5, 8, 8.5, NULL, NULL) == MC_ENTITY_OK);
    p->base.aH = rider;
    const mc_java_random before = *p->random;
    CHECK(mc_entity_tick_base_body_in_context(p, f->registry[0], NULL, &error) == MC_ENTITY_OK &&
          p->base.aH == rider && tick_record(p)->byte_value == 4 &&
          mc_test_java_random_equal(p->random, &before) && p->local_player->fields.living.W == -31);
    CHECK(mc_entity_effect_dead(rider, NULL) == MC_ENTITY_OK && rider->base.be);
    CHECK(mc_entity_tick_base_body_in_context(p, f->registry[0], NULL, &error) == MC_ENTITY_OK &&
          p->base.aH == NULL && tick_record(p)->byte_value == 0);
    CHECK(tick_reset(f, p, &observer, 30));
    tick_fluid(f, 8);
    p->base.private_c = false;
    p->base.bx = true;
    p->base.bz = -19;
    observer.change_particle_fp = true;
    mc_java_random fp_expected = *p->random;
    int32_t ambient;
    CHECK(mc_java_random_next_bounded(&fp_expected, 1000, &ambient));
    for (size_t draw = 0; draw < 8; ++draw)
        (void)mc_java_random_next_float(&fp_expected);
    const unsigned int previous_fp = _mm_getcsr();
    const mc_entity_status fp_status =
        mc_entity_tick_update_in_context(p, f->registry[0], NULL, &error);
    _mm_setcsr(previous_fp);
    CHECK(fp_status == MC_ENTITY_UNSUPPORTED_FP && error.living_bytecode == 162 &&
          observer.particles == 1 && error.particle_work == 2 &&
          mc_test_java_random_equal(p->random, &fp_expected));
    CHECK(tick_reset(f, p, &observer, 29));
    CHECK(mc_entity_tick_update_in_context(p, f->registry[0], NULL, &error) == MC_ENTITY_OK);
    const size_t allocated = p->classes->allocated_bytes, arena = f->registry[0]->allocation_bytes,
                 managed = f->registry[0]->managed_current_bytes;
    const size_t direct = mc_entity_native_bytes(p);
    mc_java_random expected_random = *p->random;
    mc_entity_watch_node *node16 = p->watcher->map->head;
    while (node16 != NULL && node16->key != 16)
        node16 = node16->next;
    CHECK(node16 != NULL && node16->record != NULL);
    const mc_entity_watch_record record16 = *node16->record;
    for (size_t iteration = 0; iteration < 256; ++iteration) {
        CHECK(mc_entity_bounds_pool_reset(p->classes, NULL) == MC_ENTITY_OK &&
              mc_entity_vector_pool_reset(p->classes, NULL) == MC_ENTITY_OK);
        int32_t draw;
        CHECK(mc_java_random_next_bounded(&expected_random, 1000, &draw));
        CHECK(mc_entity_tick_update_in_context(p, f->registry[0], NULL, &error) == MC_ENTITY_OK &&
              mc_test_java_random_equal(p->random, &expected_random) &&
              error.status == MC_ENTITY_OK && error.source_class == NULL &&
              error.registry.initializer_pc == -1 && p->classes->allocated_bytes == allocated &&
              f->registry[0]->allocation_bytes == arena &&
              f->registry[0]->managed_current_bytes == managed &&
              mc_entity_native_bytes(p) == direct && p->holds == 1 &&
              p->classes->bounds_holds == 0 && p->classes->vector_holds == 0 &&
              f->worlds[0]->active_calls == 0 && f->worlds[0]->local_leases == 0 &&
              node16->record->index == record16.index && node16->record->type == record16.type &&
              node16->record->byte_value == record16.byte_value &&
              node16->record->dirty == record16.dirty);
    }
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    registered = false;
    CHECK(mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    access = NULL;
    ok = true;
cleanup:
    if (registered) {
        bool removed_cleanup = false;
        (void)mc_world_runtime_remove_world_access(f->worlds[0], access, &removed_cleanup, NULL);
    }
    (void)mc_world_access_free(access, NULL);
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_entity *player = NULL;
    char root[8192];
    const uint16_t title[] = {'T', 'i', 'c', 'k'};
    if (argc != 3 ||
        (strcmp(argv[1], "player-base-tick") != 0 &&
         strcmp(argv[1], "player-base-tick-reference") != 0) ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    tick_reference = strcmp(argv[1], "player-base-tick-reference") == 0;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_materials_create(&f->materials));
    CHECK(mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK &&
          mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
              MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
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
    CHECK(mc_registry_classes_initialize_statistics(f->registry[0], NULL) == MC_REGISTRY_OK &&
          mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &sessions, NULL) ==
              MC_PLAYER_OK &&
          mc_player_session_create(sessions, NULL, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], &player, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(player, f->worlds[0], NULL, session, 0, NULL, NULL) ==
              MC_PLAYER_OK);
    mc_player_session_retire(&session);
    CHECK(tick_cases(f, player));
    result = 0;
cleanup:
    mc_entity_release(player);
    mc_player_session_retire(&session);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !cleanup_fixture(f))
        return 1;
    if (result == 0 && printf("Player base tick: %u checks\n", checks) < 0)
        return 1;
    return result;
}
