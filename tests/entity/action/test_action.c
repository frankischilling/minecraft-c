#define MC_PLAYER_MOVEMENT_MAIN mc_action_movement_fixture_main
#include "../movement/test_player_movement.c"
#undef MC_PLAYER_MOVEMENT_MAIN
#include "../../../src/item/stack/source_internal.h"
#include "mc/entity/action/action.h"
#include "mc/entity/minecart/minecart.h"
#include "mc/entity/tick/environment.h"
#include <xmmintrin.h>

static bool action_reference;

static bool reset_action(fixture *f, mc_entity *p, mc_movement_input *input) {
    bool ok = false;
    movement_air(f, 0);
    movement_floor(f, 0);
    CHECK(movement_input(p, 8.5, 8, 8.5));
    p->base.by = 0;
    p->base.aX = true;
    p->base.bt = 1;
    p->base.bj = 0;
    p->base.private_b = 1;
    p->base.yaw = 0;
    p->base.aT = 0;
    p->base.br = 0;
    p->base.velocity_x = p->base.velocity_y = p->base.velocity_z = 0.0;
    mc_living_entity_fields *v = &p->local_player->fields.living;
    v->Y = 20;
    v->E = 20;
    v->V = false;
    v->an = 0;
    v->aw = 0;
    v->ax = 0;
    v->ay = 0;
    v->az = false;
    v->ag = .5f;
    v->X = -1;
    v->ao = 10.5;
    v->ap = 8;
    v->aq = 8.5;
    v->ar = 10;
    v->as = 30;
    mc_player_fields *s = &p->local_player->fields.player;
    s->u = false;
    s->j = false;
    s->k = 0;
    s->h = -1;
    s->i = .125f;
    f->worlds[0]->fields.q = 1;
    f->worlds[0]->fields.B = false;
    const mc_movement_input_fields fields = {0};
    CHECK(mc_movement_input_set_fields(input, &fields, NULL) == MC_MOVEMENT_INPUT_OK);
    CHECK(mc_local_player_set_movement_input(p, input, NULL) == MC_PLAYER_OK);
    CHECK(mc_entity_bounds_pool_reset(p->classes, NULL) == MC_ENTITY_OK &&
          mc_entity_vector_pool_reset(p->classes, NULL) == MC_ENTITY_OK);
    mc_java_random_reseed(p->random, 38);
    ok = true;
cleanup:
    return ok;
}

static bool row_action(mc_entity *p, mc_entity *other, mc_entity *cart, mc_entity *item,
                       int scenario, bool complete) {
    if (!action_reference)
        return true;
    const mc_living_entity_fields *v = &p->local_player->fields.living;
    const mc_player_fields *s = &p->local_player->fields.player;
    printf("ACTION %d %d %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
           " %016" PRIx64,
           scenario, complete ? 1 : 0, movement_double_bits(p->base.x),
           movement_double_bits(p->base.y), movement_double_bits(p->base.z),
           movement_double_bits(p->base.velocity_x), movement_double_bits(p->base.velocity_y),
           movement_double_bits(p->base.velocity_z));
    printf(" %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %d %08" PRIx32
           " %d %" PRId32,
           movement_float_bits(p->base.yaw), movement_float_bits(p->base.aT),
           movement_float_bits(v->aw), movement_float_bits(v->ax), movement_float_bits(v->ay),
           v->az ? 1 : 0, movement_float_bits(v->X), s->j ? 1 : 0, s->k);
    printf(" %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %" PRId32 " %" PRId32 " %" PRId32
           " %d %d %d %08" PRIx32 " %" PRId32,
           movement_float_bits(s->h), movement_float_bits(s->i), movement_float_bits(v->ag), v->Y,
           p->base.by, v->an, p->base.aX ? 1 : 0, p->base.aY ? 1 : 0, p->base.aZ ? 1 : 0,
           movement_float_bits(p->base.bj), p->base.private_b);
    printf(" %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64
           " %016" PRIx64,
           movement_double_bits(p->bounds->min_x), movement_double_bits(p->bounds->min_y),
           movement_double_bits(p->bounds->min_z), movement_double_bits(p->bounds->max_x),
           movement_double_bits(p->bounds->max_y), movement_double_bits(p->bounds->max_z));
    mc_entity *actors[] = {other, cart, item};
    for (size_t i = 0; i < COUNT(actors); ++i)
        printf(" %016" PRIx64 " %016" PRIx64 " %016" PRIx64,
               movement_double_bits(actors[i]->base.velocity_x),
               movement_double_bits(actors[i]->base.velocity_y),
               movement_double_bits(actors[i]->base.velocity_z));
    printf(" %012" PRIx64 "\n", p->random->state);
    return !ferror(stdout);
}

static bool action_cases(fixture *f, mc_entity *p, mc_entity *other, mc_entity *cart,
                         mc_entity *item, mc_movement_input *input) {
    bool ok = false;
    mc_player_action_error error;
    const mc_player_action_context context = {
        .travel = {.code_registry = f->registry[0], .trig = &trig}};
    for (int scenario = 0; scenario < 28; ++scenario) {
        CHECK(reset_action(f, p, input));
        mc_entity *actors[] = {other, cart, item};
        for (size_t i = 0; i < COUNT(actors); ++i) {
            CHECK(mc_entity_set_position(actors[i], 14, 8, 14) == MC_ENTITY_OK);
            actors[i]->base.velocity_x = actors[i]->base.velocity_z = 0;
            actors[i]->base.velocity_y = -0.0;
            actors[i]->base.be = false;
            actors[i]->base.br = 0;
        }
        CHECK(mc_entity_item_pickup_delay_set(item, 3) == MC_ENTITY_OK);
        mc_living_entity_fields *v = &p->local_player->fields.living;
        mc_player_fields *s = &p->local_player->fields.player;
        if (scenario == 1) {
            f->worlds[0]->fields.q = 0;
            v->Y = 19;
            p->base.bt = 20;
        }
        if (scenario == 2)
            v->Y = 0;
        if (scenario == 3)
            s->u = true;
        if (scenario == 4)
            v->V = true;
        if (scenario >= 2 && scenario <= 4) {
            const mc_movement_input_fields fields = {.strafe = 1, .forward = -1, .jump = true};
            CHECK(mc_movement_input_set_fields(input, &fields, NULL) == MC_MOVEMENT_INPUT_OK);
        }
        if (scenario == 5 || scenario == 6) {
            v->an = 2;
            p->base.yaw = 350;
            p->base.aT = 10;
            if (scenario == 6)
                f->states[0]->data->blocks[boxes_index(9, 8, 8)] = 1;
        }
        if (scenario == 7) {
            s->j = true;
            s->k = 6;
        }
        if (scenario == 8) {
            CHECK(mc_entity_set_position(other, 9.1, 8, 8.5) == MC_ENTITY_OK);
        }
        if (scenario == 9) {
            CHECK(mc_entity_set_position(cart, 9.1, 8, 8.5) == MC_ENTITY_OK);
        }
        if (scenario == 10) {
            CHECK(mc_entity_set_position(item, 9.1, 8, 8.5) == MC_ENTITY_OK);
        }
        if (scenario == 11) {
            CHECK(mc_entity_set_position(item, 9.1, 8, 8.5) == MC_ENTITY_OK &&
                  mc_entity_item_pickup_delay_set(item, 0) == MC_ENTITY_OK);
        }
        if (scenario == 12) {
            const mc_movement_input_fields fields = {.jump = true};
            CHECK(mc_movement_input_set_fields(input, &fields, NULL) == MC_MOVEMENT_INPUT_OK);
        }
        if (scenario == 13) {
            p->base.velocity_x = .5;
            const mc_movement_input_fields fields = {.strafe = .5f, .forward = 1};
            CHECK(mc_movement_input_set_fields(input, &fields, NULL) == MC_MOVEMENT_INPUT_OK);
        }
        if (scenario == 14) {
            CHECK(mc_local_player_set_movement_input(p, NULL, NULL) == MC_PLAYER_OK);
            s->j = true;
            s->k = 2;
        }
        if (scenario == 15) {
            v->an = 2;
            const uint64_t bits = UINT64_C(0x7ff0000000000000);
            memcpy(&v->as, &bits, 8);
        }
        if (scenario == 16) {
            v->an = 2;
            const uint64_t bits = UINT64_C(0x7ff8123456789abc);
            memcpy(&v->ar, &bits, 8);
        }
        if (scenario == 17 || scenario == 18 || scenario == 23 || scenario == 26) {
            CHECK(mc_entity_set_position(other, 9.1, 8, 8.9) == MC_ENTITY_OK);
        }
        if (scenario == 19 || scenario == 20 || scenario == 21 || scenario == 22 ||
            scenario == 27) {
            CHECK(mc_entity_set_position(cart, 9.1, 8, 8.9) == MC_ENTITY_OK);
        }
        if (scenario == 18)
            other->base.br = 1;
        if (scenario == 20)
            cart->base.br = .5f;
        if (scenario == 21)
            f->worlds[0]->fields.B = true;
        if (scenario == 22)
            cart->base.be = true;
        if (scenario == 23)
            other->base.be = true;
        if (scenario == 24)
            CHECK(mc_entity_set_position(other, 8.50001, 8, 8.5) == MC_ENTITY_OK);
        if (scenario == 25)
            CHECK(mc_entity_set_position(cart, 8.50001, 8, 8.5) == MC_ENTITY_OK);
        if (scenario == 26 || scenario == 27) {
            const uint32_t bits = UINT32_C(0x7fc12345);
            memcpy(scenario == 26 ? &other->base.br : &cart->base.br, &bits, 4);
        }
        const bool expected =
            !(scenario == 11 || scenario == 12 || scenario == 13 || scenario == 14);
        const mc_java_random item_random_before = *item->random;
        const mc_entity_status status = mc_player_action(p, &context, NULL, &error);
        if ((status == MC_ENTITY_OK) != expected)
            fprintf(stderr, "action%d status%d %s gs%u ls%u\n", scenario, (int)status,
                    error.operation == NULL ? "-" : error.operation, error.player_bytecode,
                    error.living_bytecode);
        if ((status == MC_ENTITY_OK) != expected &&
            error.child_kind == MC_PLAYER_ACTION_CHILD_TRAVEL)
            fprintf(stderr, "travel %s, move %s collision %s\n",
                    error.travel.operation == NULL ? "-" : error.travel.operation,
                    error.travel.movement.operation == NULL ? "-" : error.travel.movement.operation,
                    error.travel.movement.collision.nearby.query.operation == NULL
                        ? "-"
                        : error.travel.movement.collision.nearby.query.operation);
        CHECK((status == MC_ENTITY_OK) == expected && p->holds >= 1 &&
              f->worlds[0]->active_calls == 0 && f->worlds[0]->local_leases == 0);
        if (expected)
            CHECK(error.child_kind == MC_PLAYER_ACTION_CHILD_NONE &&
                  error.travel.status == MC_ENTITY_OK &&
                  error.travel.registry.initializer_pc == -1 && error.nearby_index == SIZE_MAX);
        CHECK(row_action(p, other, cart, item, scenario, status == MC_ENTITY_OK));
        if (scenario == 1)
            CHECK(v->Y == 20 && p->base.by == 10);
        if (scenario == 5)
            CHECK(v->an == 1 && p->base.yaw == 0 && p->base.aT == 20 && p->base.x == 9.5);
        if (scenario == 6)
            CHECK(p->base.y == 9.0);
        if (scenario == 7)
            CHECK(s->k == 7 && v->X == .875f && s->j);
        if (scenario == 11) {
            mc_item_stack_fields incoming, stored;
            mc_inventory_player_state inventory_state;
            mc_java_random expected_random = item_random_before;
            (void)mc_java_random_next_float(&expected_random);
            (void)mc_java_random_next_float(&expected_random);
            CHECK(error.child_kind == MC_PLAYER_ACTION_CHILD_PICKUP &&
                  error.pickup_bytecode == 123 && error.player_bytecode == 242 &&
                  error.contact_bytecode == 2 && error.pickup.notification_bytecode == 4 &&
                  error.pickup.insertion_returned && error.pickup.inserted && !item->base.be &&
                  item->random->state == expected_random.state);
            CHECK(mc_item_stack_fields_get(item->stack, &incoming) == MC_ITEM_STACK_OK &&
                  incoming.count == 0 &&
                  mc_inventory_player_read_state(p->local_player->fields.player.c,
                                                 &inventory_state) &&
                  inventory_state.main_slots[0] != item->stack &&
                  mc_item_stack_fields_get(inventory_state.main_slots[0], &stored) ==
                      MC_ITEM_STACK_OK &&
                  stored.count == 5 && stored.animation == 5);
            /* This extra native-only case stays excluded from original rows.
             * Restore the subsequent scenes' actual previous setup. */
            CHECK(mc_inventory_player_set(p->local_player->fields.player.c, 0, NULL, NULL) ==
                      MC_INVENTORY_OK &&
                  mc_item_stack_source_count_set(item->stack, 5) == MC_ITEM_STACK_OK);
            *item->random = item_random_before;
        }
        if (scenario == 12)
            CHECK(error.living_bytecode == 414 && p->base.velocity_y == 0.41999998688697815);
        if (scenario == 12 || scenario == 13)
            CHECK(error.child_kind == MC_PLAYER_ACTION_CHILD_TRAVEL &&
                  error.travel.statistic != NULL && error.travel.statistic_field != NULL &&
                  strcmp(error.travel.statistic_field, scenario == 12 ? "u" : "p") == 0 &&
                  error.travel.statistic_amount == (scenario == 12 ? 1 : 55));
        if (scenario == 14)
            CHECK(error.input_bytecode == 9 && s->k == 3 && v->X == .375f);
    }
    action_reference = false;
    CHECK(reset_action(f, p, input));
    p->local_player->fields.living.an = 2;
    p->local_player->fields.living.ar = 1080;
    const mc_player_action_limits cap = {1};
    CHECK(mc_player_action(p, &context, &cap, &error) == MC_ENTITY_LIMIT &&
          error.living_bytecode == 117 && error.loop_work == 1 &&
          p->local_player->fields.living.an == 2);
    CHECK(reset_action(f, p, input));
    mc_inventory_player *inventory = p->local_player->fields.player.c;
    p->local_player->fields.player.c = NULL;
    f->worlds[0]->fields.q = 0;
    p->local_player->fields.living.Y = 19;
    p->base.bt = 20;
    const mc_entity_status missing = mc_player_action(p, &context, NULL, &error);
    p->local_player->fields.player.c = inventory;
    CHECK(missing == MC_ENTITY_STATE_ERROR && error.player_bytecode == 41 &&
          p->local_player->fields.living.Y == 20 && p->local_player->fields.player.h == -1);
    CHECK(reset_action(f, p, input));
    mc_item_stack *stack = NULL;
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 262, 1, 0) == MC_ITEM_STACK_OK &&
          mc_item_stack_source_animation_set(stack, 5) == MC_ITEM_STACK_OK &&
          mc_inventory_player_set(inventory, 0, stack, NULL) == MC_INVENTORY_OK);
    CHECK(mc_player_action(p, &context, NULL, &error) == MC_ENTITY_OK);
    mc_item_stack_fields fields;
    CHECK(mc_item_stack_fields_get(stack, &fields) == MC_ITEM_STACK_OK && fields.animation == 4);
    CHECK(mc_inventory_player_set(inventory, 0, NULL, NULL) == MC_INVENTORY_OK);
    mc_item_stack_release(stack);
    /* The candidate f() admission must not admit the different moving a(sn)
     * virtual. Original yl.a(sn) returns the argument permanent bounds. */
    CHECK(reset_action(f, p, input));
    CHECK(mc_entity_set_position(cart, 9.1, 8, 8.5) == MC_ENTITY_OK &&
          mc_entity_set_position(other, 9.6, 8, 8.5) == MC_ENTITY_OK);
    mc_world_collision_list_view boxes = {0};
    mc_world_collision_error collision;
    CHECK(mc_world_runtime_collision_boxes(f->worlds[0], cart, cart->bounds, &boxes, &collision) ==
              MC_WORLD_STORAGE_UNSUPPORTED_STATE &&
          collision.world_bytecode == 260 && boxes.identity == NULL &&
          f->worlds[0]->active_calls == 0 && f->worlds[0]->local_leases == 0);
    ok = true;
cleanup:
    return ok;
}

static bool input_cases(fixture *f, mc_entity *p, mc_movement_input *input) {
    bool ok = false;
    mc_player_action_error error;
    const int32_t counters[] = {0, 6, 7, 8, -2, INT32_MAX, INT32_MIN, INT32_MAX, 6};
    const bool active[] = {true, true, true, true, true, true, true, false, true};
    for (size_t i = 0; i < COUNT(counters); ++i) {
        CHECK(reset_action(f, p, input));
        mc_player_fields *s = &p->local_player->fields.player;
        mc_living_entity_fields *v = &p->local_player->fields.living;
        s->j = active[i];
        s->k = counters[i];
        const mc_movement_input_fields fields = {.strafe = -.25f, .forward = .75f, .jump = true};
        CHECK(mc_movement_input_set_fields(input, &fields, NULL) == MC_MOVEMENT_INPUT_OK);
        if (i == 8)
            CHECK(mc_local_player_set_movement_input(p, NULL, NULL) == MC_PLAYER_OK);
        const mc_entity_status status = mc_local_player_action_input(p, &error);
        CHECK(status == (i == 8 ? MC_ENTITY_STATE_ERROR : MC_ENTITY_OK));
        CHECK(i == 8 || (v->aw == -.25f && v->ax == .75f && v->az));
        if (action_reference)
            printf("INPUT %zu %d %d %" PRId32 " %08" PRIx32 " %08" PRIx32 " %08" PRIx32 " %d\n", i,
                   status == MC_ENTITY_OK ? 1 : 0, s->j ? 1 : 0, s->k, movement_float_bits(v->X),
                   movement_float_bits(v->aw), movement_float_bits(v->ax), v->az ? 1 : 0);
    }
    CHECK(reset_action(f, p, input));
    p->local_player->fields.player.j = true;
    p->local_player->fields.player.k = 6;
    const unsigned int fp = _mm_getcsr();
    _mm_setcsr((fp & ~0x6000u) | 0x2000u);
    const mc_entity_status failure = mc_local_player_action_input(p, &error);
    _mm_setcsr(fp);
    CHECK(failure == MC_ENTITY_UNSUPPORTED_FP && error.swing_bytecode == 49 &&
          p->local_player->fields.player.k == 7 && p->local_player->fields.living.X == -1);
    CHECK(mc_local_player_action_input(NULL, &error) == MC_ENTITY_INVALID_ARGUMENT &&
          mc_player_action(NULL, NULL, NULL, &error) == MC_ENTITY_INVALID_ARGUMENT);
    ok = true;
cleanup:
    return ok;
}

int main(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_entity *p = NULL, *other = NULL, *cart = NULL, *item = NULL;
    mc_item_stack *stack = NULL;
    mc_movement_input *input = NULL;
    char root[8192];
    const uint16_t title[] = {'A', 'c', 't', 'i', 'o', 'n'};
    if (argc != 3 ||
        (strcmp(argv[1], "player-action") != 0 &&
         strcmp(argv[1], "player-action-reference") != 0) ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    action_reference = strcmp(argv[1], "player-action-reference") == 0;
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
    CHECK(mc_local_player_allocate(f->entity_classes[0], &p, NULL) == MC_ENTITY_OK &&
          mc_local_player_construct(p, f->worlds[0], NULL, session, 0, NULL, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], &other, NULL) == MC_ENTITY_OK);
    f->entities[f->entity_count++] = other;
    CHECK(mc_local_player_construct(other, f->worlds[0], NULL, session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    CHECK(mc_minecart_entity_allocate(f->entity_classes[0], &cart, NULL) == MC_ENTITY_OK);
    f->entities[f->entity_count++] = cart;
    CHECK(mc_minecart_entity_construct(cart, f->worlds[0], 14, 8, 14, 0, NULL) == MC_ENTITY_OK);
    CHECK(mc_item_stack_allocate(&stack) == MC_ITEM_STACK_OK &&
          mc_item_stack_construct(stack, 262, 5, 7) == MC_ITEM_STACK_OK);
    CHECK(mc_item_entity_allocate(f->entity_classes[0], &item, NULL) == MC_ENTITY_OK);
    f->entities[f->entity_count++] = item;
    CHECK(mc_item_entity_construct(item, f->worlds[0], 14, 8, 14, stack, NULL) == MC_ENTITY_OK);
    CHECK(mc_entity_set_position(other, 14, 8, 14) == MC_ENTITY_OK);
    mc_entity *actors[] = {other, cart, item};
    bool inserted;
    for (size_t i = 0; i < COUNT(actors); ++i)
        CHECK(mc_world_runtime_insert_entity(f->worlds[0], actors[i], &inserted, NULL) ==
                  MC_WORLD_STORAGE_OK &&
              inserted);
    CHECK(mc_movement_input_create(&input, NULL) == MC_MOVEMENT_INPUT_OK &&
          input_cases(f, p, input) && action_cases(f, p, other, cart, item, input));
    result = 0;
cleanup:
    (void)mc_movement_input_retire(&input, NULL);
    mc_item_stack_release(stack);
    mc_entity_release(p);
    mc_player_session_retire(&session);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !cleanup_fixture(f))
        return 1;
    if (result == 0 && printf("Player action: %u checks\n", checks) < 0)
        return 1;
    return result;
}
