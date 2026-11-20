/* Real GAME/Registry/Store/loaded-Chunk fixture; imported main is not run. */
#define MC_COLLISION_BOXES_MAIN mc_travel_boxes_main
#include "../../world/runtime/collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN
#include "../../../src/entity/living/travel_math_internal.h"
#include "../../../src/entity/player/player_internal.h"
#include "mc/entity/living/travel.h"
#include "mc/entity/player/input.h"
#include <inttypes.h>
#include <math.h>

static uint32_t fbits(float v) {
    uint32_t b;
    memcpy(&b, &v, 4);
    return b;
}

static uint64_t dbits(double v) {
    uint64_t b;
    memcpy(&b, &v, 8);
    return b;
}

static bool reset_actor(fixture *f, mc_entity *p) {
    boxes_air(f, 0);
    memset(f->states[0]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
    p->base.offset = p->base.y_size = p->base.bp = p->base.bk = 0;
    p->base.aX = p->base.aY = p->base.aZ = p->base.ba = p->base.bb = p->base.bc = p->base.be =
        false;
    p->base.bd = false;
    p->base.bq = p->base.bx = p->base.bC = false;
    p->base.bu = 5;
    p->base.bv = 0;
    p->base.aG = p->base.aH = NULL;
    p->base.bj = 0;
    p->base.private_b = 0;
    p->base.yaw = 37;
    p->local_player->fields.player.u = false;
    p->local_player->fields.living.ak = .125f;
    p->local_player->fields.living.al = .25f;
    p->local_player->fields.living.am = .5f;
    const mc_movement_input_fields input = {0};
    mc_registry_block_view blocks;
    if (mc_registry_classes_inspect_blocks(f->registry[0], &blocks, NULL) != MC_REGISTRY_OK)
        return false;
    ((mc_block *)(void *)blocks.m[1])->fields.bB = .6f;
    return mc_movement_input_set_fields(p->local_player->fields.input, &input, NULL) ==
               MC_MOVEMENT_INPUT_OK &&
           mc_entity_set_size(p, 1, 1) == MC_ENTITY_OK &&
           mc_entity_set_position(p, 8.5, 8, 8.5) == MC_ENTITY_OK &&
           mc_entity_set_velocity(p, 0, .2) == MC_ENTITY_OK &&
           mc_entity_set_velocity(p, 1, -.1) == MC_ENTITY_OK &&
           mc_entity_set_velocity(p, 2, -.3) == MC_ENTITY_OK &&
           (p->base.aJ = 8.25, p->base.aK = 7.5, p->base.aL = 8.75, true) &&
           mc_entity_bounds_pool_reset(p->classes, NULL) == MC_ENTITY_OK &&
           mc_entity_vector_pool_reset(p->classes, NULL) == MC_ENTITY_OK;
}

static void floor_cells(fixture *f, uint8_t id) {
    for (int x = 6; x <= 12; ++x)
        for (int z = 6; z <= 12; ++z)
            f->states[0]->data->blocks[boxes_index(x, 7, z)] = id;
}

static void fluid_cells(fixture *f, uint8_t id) {
    for (int x = 6; x <= 12; ++x)
        for (int z = 6; z <= 12; ++z)
            for (int y = 6; y <= 10; ++y)
                f->states[0]->data->blocks[boxes_index(x, y, z)] = id;
}

static bool prepare(fixture *f, mc_entity *p, int c) {
    if (!reset_actor(f, p))
        return false;
    if (c == 1 || c == 2 || c == 3) {
        floor_cells(f, c == 2 ? 79 : 1);
        p->base.aX = true;
    }
    if (c == 4 || c == 6 || (c >= 13 && c <= 15))
        p->base.bx = true;
    if (c == 5 || c == 6 || c == 16)
        fluid_cells(f, 10);
    if (c == 7)
        fluid_cells(f, 8);
    if (c >= 8 && c <= 10) {
        f->states[0]->data->blocks[boxes_index(8, 8, 8)] = 65;
        p->base.velocity_y = -1;
        p->base.bk = 7;
    }
    if (c == 9) {
        const mc_movement_input_fields input = {.sneak = true};
        if (mc_movement_input_set_fields(p->local_player->fields.input, &input, NULL) !=
            MC_MOVEMENT_INPUT_OK)
            return false;
    }
    if (c == 10) {
        f->states[0]->data->blocks[boxes_index(8, 7, 8)] = 65;
        for (int y = 7; y <= 10; ++y)
            for (int z = 7; z <= 9; ++z)
                f->states[0]->data->blocks[boxes_index(9, y, z)] = 1;
        p->base.velocity_x = 1;
    }
    if (c == 11) {
        p->base.velocity_x = p->base.velocity_y = p->base.velocity_z = 0;
    }
    if (c == 12)
        p->base.yaw = 90;
    if (c == 5 || c == 6 || c == 13 || c == 14 || c == 15 || c == 16)
        p->base.bq = true;
    if (c == 14) {
        p->base.aJ = -4;
        p->base.aL = 3;
    }
    if (c == 15 || c == 16)
        p->base.aY = true;
    if (c >= 17 && c <= 19) {
        floor_cells(f, c == 18 ? 79 : 1);
        p->base.aX = true;
        p->base.bq = true;
        if (c == 19) {
            mc_registry_block_view blocks;
            if (mc_registry_classes_inspect_blocks(f->registry[0], &blocks, NULL) != MC_REGISTRY_OK)
                return false;
            ((mc_block *)(void *)blocks.m[1])->fields.bB = .3f;
        }
    }
    if (c >= 20) {
        p->base.bq = true;
        p->base.bx = true;
        const uint64_t vx = c == 20   ? UINT64_C(0x7ff8000000000123)
                            : c == 21 ? UINT64_C(0xfff8000000000456)
                                      : UINT64_C(0x7ff0000000000000);
        memcpy(&p->base.velocity_x, &vx, sizeof(vx));
    }
    return true;
}

static bool row(const char *tag, int c, mc_entity *p, const mc_player_travel_error *e) {
    mc_living_entity_fields *l = &p->local_player->fields.living;
    return printf("CMP %s %d %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
                  " %016" PRIx64 " %016" PRIx64 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32
                  " %08" PRIx32 " %d %d %d %d %s %" PRId32 "\n",
                  tag, c, dbits(p->base.x), dbits(p->base.y), dbits(p->base.z),
                  dbits(p->base.velocity_x), dbits(p->base.velocity_y), dbits(p->base.velocity_z),
                  fbits(l->ak), fbits(l->al), fbits(l->am), fbits(p->base.bk), p->base.aX ? 1 : 0,
                  p->base.aY ? 1 : 0, p->base.aZ ? 1 : 0, p->base.ba ? 1 : 0,
                  e->cause == MC_PLAYER_TRAVEL_STATISTIC ? e->statistic_field : "-",
                  e->cause == MC_PLAYER_TRAVEL_STATISTIC ? e->statistic_amount : 0) > 0;
}

static bool travel_cases(fixture *f, mc_entity *p) {
    bool ok = false;
    mc_player_travel_error e;
    const mc_player_travel_context context = {.code_registry = f->registry[0], .trig = &trig};
    for (int c = 0; c < 23; ++c) {
        CHECK(prepare(f, p, c));
        const size_t holds = p->holds, borrows = f->registry[0]->runtime_borrows;
        const mc_java_random rng = *p->random;
        const mc_entity_status status =
            mc_living_travel(p, &context, c == 12 ? .00001f : .7f, c == 12 ? -.00001f : -.4f, &e);
        if (status != MC_ENTITY_OK)
            fprintf(stderr, "travel case %d: %s L%u move%u: %s effect: %s\n", c, e.operation,
                    e.living_bytecode, e.movement.bytecode, e.movement.operation,
                    e.movement.effect.operation);
        CHECK(status == MC_ENTITY_OK);
        CHECK(e.status == MC_ENTITY_OK && e.cause == MC_PLAYER_TRAVEL_NONE && p->holds == holds &&
              f->registry[0]->runtime_borrows == borrows &&
              mc_test_java_random_equal(&rng, p->random));
        CHECK(row("LIVING", c, p, &e));
    }
    for (int c = 0; c < 3; ++c) {
        CHECK(prepare(f, p, c == 0 ? 11 : c == 1 ? 0 : 4));
        const mc_entity_status status = mc_player_travel(p, &context, 0, 0, &e);
        CHECK(c == 0 ? status == MC_ENTITY_OK
                     : status == MC_ENTITY_STATE_ERROR && e.cause == MC_PLAYER_TRAVEL_STATISTIC &&
                           e.statistic != NULL && e.statistic_amount > 0 &&
                           e.statistic_bytecode == 85 &&
                           e.statistic_error.status == MC_PLAYER_STATISTIC_NULL_REFERENCE);
        CHECK(row("PLAYER", c, p, &e));
    }
    ok = true;
cleanup:
    return ok;
}

static bool helpers(fixture *f, mc_entity *p) {
    bool ok = false, value = true;
    mc_entity *mount = NULL;
    mc_player_travel_error e;
    const mc_player_travel_context context = {.code_registry = f->registry[0], .trig = &trig};
    /* Lava-contact damage genuinely reaches the null-App statistic descendant.
     * Do not run the remaining drag/limb tail or claim a successful no-op. */
    CHECK(prepare(f, p, 5));
    p->base.bq = false;
    CHECK(mc_living_travel(p, &context, .7f, -.4f, &e) == MC_ENTITY_STATE_ERROR &&
          e.cause == MC_PLAYER_TRAVEL_MOVEMENT && e.living_bytecode == 164 &&
          e.movement.effect.statistic != NULL && e.movement.effect.statistic_amount > 0 &&
          p->local_player->fields.living.ak == .125f);
    for (int c = 0; c < 3; ++c) {
        CHECK(reset_actor(f, p));
        if (c != 0)
            f->states[0]->data->blocks[boxes_index(8, 8, 8)] = c == 1 ? 65 : 1;
        CHECK(mc_living_on_ladder(p, &context, &value, &e) == MC_ENTITY_OK && value == (c == 1));
        printf("CMP LADDER %d %d\n", c, value ? 1 : 0);
    }
    CHECK(reset_actor(f, p) && mc_living_jump(p, &e) == MC_ENTITY_OK &&
          dbits(p->base.velocity_y) == UINT64_C(0x3fdae147a0000000));
    CHECK(row("JUMP", 0, p, &e));
    p->base.velocity_y = -9;
    CHECK(mc_player_jump(p, &context, &e) == MC_ENTITY_STATE_ERROR &&
          e.cause == MC_PLAYER_TRAVEL_STATISTIC && e.player_bytecode == 9 &&
          e.statistic_amount == 1 && strcmp(e.statistic_field, "u") == 0 && e.statistic != NULL &&
          e.statistic_bytecode == 85 &&
          e.statistic_error.status == MC_PLAYER_STATISTIC_NULL_REFERENCE);
    CHECK(row("JUMP", 1, p, &e));
    for (int c = 0; c < 11; ++c) {
        CHECK(reset_actor(f, p));
        if (c == 0)
            fluid_cells(f, 8);
        if (c == 1)
            p->base.bx = true;
        if (c == 2 || c == 3 || c == 9)
            f->states[0]->data->blocks[boxes_index(8, 8, 8)] = 65;
        if (c == 4 || c == 7 || c == 8)
            p->base.aX = true;
        const double x = c == 5 ? .25 : c == 6 ? .26 : c == 7 ? .005 : c == 8 ? .004999999 : 1;
        const double y = c == 9 ? HUGE_VAL : c == 3 ? -.01 : c == 2 ? .005 : 2;
        if (c == 10) {
            CHECK(mc_falling_entity_allocate(p->classes, &mount, NULL) == MC_ENTITY_OK &&
                  mc_falling_entity_construct(mount, f->worlds[0], 8, 8, 8, 1, NULL) ==
                      MC_ENTITY_OK);
            p->base.aH = mount;
        }
        const mc_entity_status status =
            mc_player_movement_statistics(p, c == 10 ? NULL : &context, x, y, 0, &e);
        p->base.aH = NULL;
        CHECK((status == MC_ENTITY_OK && e.cause == MC_PLAYER_TRAVEL_NONE) ||
              (status == MC_ENTITY_STATE_ERROR && e.cause == MC_PLAYER_TRAVEL_STATISTIC &&
               e.statistic != NULL));
        printf("CMP STAT %d %s %" PRId32 "\n", c,
               e.cause == MC_PLAYER_TRAVEL_STATISTIC ? e.statistic_field : "-",
               e.cause == MC_PLAYER_TRAVEL_STATISTIC ? e.statistic_amount : 0);
    }
    /* Actual source-null projections on a real constructed Player. Lifetime
     * ledger/World owner remain held; restore before any assertion/teardown. */
    CHECK(reset_actor(f, p));
    mc_world_runtime *world = p->base.world;
    p->base.world = NULL;
    const mc_entity_status null_ladder = mc_living_on_ladder(p, &context, &value, &e);
    p->base.world = world;
    CHECK(null_ladder == MC_ENTITY_WORLD_FAILED && e.ladder_bytecode == 34 &&
          e.cause == MC_PLAYER_TRAVEL_NULL_REFERENCE);
    CHECK(reset_actor(f, p));
    p->base.bx = true;
    p->base.bq = true;
    p->base.world = NULL;
    const mc_entity_status null_water = mc_living_travel(p, &context, 0, 0, &e);
    p->base.world = world;
    CHECK(null_water == MC_ENTITY_OK);
    CHECK(row("NULLWATER", 0, p, &e));
    CHECK(reset_actor(f, p));
    p->base.world = NULL;
    const mc_entity_status null_dry = mc_living_travel(p, &context, 0, 0, &e);
    p->base.world = world;
    CHECK(null_dry == MC_ENTITY_WORLD_FAILED && e.living_bytecode == 132 &&
          e.environment.bytecode == 23);
    CHECK(reset_actor(f, p));
    mc_entity_bounds *bounds = p->bounds;
    p->bounds = NULL;
    const mc_entity_status null_bounds = mc_living_on_ladder(p, &context, &value, &e);
    p->bounds = bounds;
    CHECK(null_bounds == MC_ENTITY_STATE_ERROR && e.ladder_bytecode == 12);
    CHECK(mc_living_jump(NULL, &e) == MC_ENTITY_INVALID_ARGUMENT &&
          mc_living_on_ladder(p, &context, NULL, &e) == MC_ENTITY_INVALID_ARGUMENT);
    mc_entity *raw = NULL;
    CHECK(mc_local_player_allocate(p->classes, &raw, NULL) == MC_ENTITY_OK);
    const mc_entity_status raw_status = mc_living_jump(raw, &e);
    mc_entity_release(raw);
    CHECK(raw_status == MC_ENTITY_INVALID_PHASE);
    CHECK(reset_actor(f, p));
    const unsigned int csr = _mm_getcsr();
    _mm_setcsr(csr | 0x8040u);
    const mc_entity_status jump_fp = mc_living_jump(p, &e);
    const mc_entity_status travel_fp = mc_living_travel(p, &context, 0, 0, &e);
    _mm_setcsr(csr);
    CHECK(jump_fp == MC_ENTITY_OK && travel_fp == MC_ENTITY_UNSUPPORTED_FP &&
          e.living_bytecode == 132);
    /* Missing code context fails only after the actual base jump store. */
    p->base.velocity_y = -7;
    CHECK(mc_player_jump(p, NULL, &e) == MC_ENTITY_STATE_ERROR &&
          e.cause == MC_PLAYER_TRAVEL_NAMESPACE && e.player_bytecode == 5 &&
          p->base.velocity_y == 0.41999998688697815);
    ok = true;
cleanup:
    mc_entity_release(mount);
    return ok;
}

static bool rounding(void) {
    bool ok = false;
    const uint32_t floats[] = {0,          0x80000000, 1,          0x80000001, 0x3effffff,
                               0x3f000000, 0x3f000001, 0xbeffffff, 0xbf000000, 0xbf000001,
                               0x4affffff, 0x4b000000, 0x4effffff, 0x4f000000, 0xcf000000,
                               0x7f7fffff, 0x7f800000, 0xff800000, 0x7fc00123, 0x7f800789};
    const uint64_t doubles[] = {0,
                                UINT64_C(0x8000000000000000),
                                1,
                                UINT64_C(0x3fdfffffffffffff),
                                UINT64_C(0x3fe0000000000000),
                                UINT64_C(0x3fe0000000000001),
                                UINT64_C(0xbfdfffffffffffff),
                                UINT64_C(0xbfe0000000000000),
                                UINT64_C(0xbfe0000000000001),
                                UINT64_C(0x432fffffffffffff),
                                UINT64_C(0x4330000000000000),
                                UINT64_C(0x43dfffffffffffff),
                                UINT64_C(0x43e0000000000000),
                                UINT64_C(0xc3e0000000000000),
                                UINT64_C(0x7ff0000000000000),
                                UINT64_C(0xfff0000000000000),
                                UINT64_C(0x7ff8000000000123)};
    for (size_t i = 0; i < COUNT(floats); ++i) {
        float v;
        memcpy(&v, &floats[i], 4);
        printf("JDK F %08" PRIx32 " %" PRId32 "\n", floats[i], mc_travel_round_float(v));
    }
    for (size_t i = 0; i < COUNT(doubles); ++i) {
        double v;
        memcpy(&v, &doubles[i], 8);
        printf("JDK D %016" PRIx64 " %" PRId64 "\n", doubles[i], mc_travel_round_double(v));
    }
    uint64_t state = UINT64_C(0x0123456789abcdef);
    for (unsigned int i = 0; i < 2048; ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        const uint32_t fb = (uint32_t)state;
        float fv;
        double dv;
        memcpy(&fv, &fb, sizeof(fv));
        memcpy(&dv, &state, sizeof(dv));
        printf("JDK F %08" PRIx32 " %" PRId32 "\n", fb, mc_travel_round_float(fv));
        printf("JDK D %016" PRIx64 " %" PRId64 "\n", state, mc_travel_round_double(dv));
    }
    CHECK(mc_travel_round_float(.5f) == 1 && mc_travel_round_float(-.5f) == 0 &&
          mc_travel_round_double(.5) == 1 && mc_travel_round_double(-.5) == 0);
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
    mc_entity *p = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_movement_input *input = NULL;
    const uint16_t title[] = {'T', 'r', 'a', 'v', 'e', 'l'};
    if (argc != 3 || strcmp(argv[1], "player-travel") != 0 ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    resources *r = &f->sources[0];
    r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1};
    r->streams[1] = r->streams[2] = (stream){.bytes = language};
    r->api = (mc_registry_resources){r, open_resource, close_resource, ready_resource};
    const mc_registry_options options = {MC_REGISTRY_CLIENT, f->profile, f->materials, 0, &r->api};
    CHECK(mc_registry_classes_allocate(&options, &f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_world_runtime_classes_create(MC_WORLD_INFO_CLIENT, f->profile, &trig, &f->classes[0],
                                          NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_world_runtime_classes_bind_registry(f->classes[0], f->registry[0], NULL) ==
          MC_REGISTRY_OK);
    CHECK(mc_entity_classes_create(MC_ENTITY_CLIENT, f->profile, NULL, &f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_world_runtime_classes_bind_entities(f->classes[0], f->entity_classes[0]) ==
          MC_ENTITY_OK);
    CHECK(mc_registry_classes_initialize_blocks(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(boxes_open_store(f, 0, root) && boxes_create_world(f, 0));
    CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &sessions, NULL) ==
          MC_PLAYER_OK);
    CHECK(mc_player_session_create(sessions, NULL, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], &p, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(p, f->worlds[0], NULL, session, 0, NULL, NULL) == MC_PLAYER_OK);
    CHECK(mc_movement_input_create(&input, NULL) == MC_MOVEMENT_INPUT_OK &&
          mc_local_player_set_movement_input(p, input, NULL) == MC_PLAYER_OK);
    CHECK(travel_cases(f, p) && helpers(f, p) && rounding());
    result = 0;
cleanup:
    mc_entity_release(p);
    mc_movement_input_retire(&input, NULL);
    mc_player_session_retire(&session);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !cleanup_fixture(f))
        return 1;
    if (result == 0 && printf("Player travel: %u checks\n", checks) < 0)
        return 1;
    return result;
}
