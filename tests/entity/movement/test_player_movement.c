/* Reuse the real GAME loaded-Chunk fixture. Its named main is retained but
 * never executed by this native contract test. */
#define MC_COLLISION_BOXES_MAIN mc_player_movement_boxes_main
#include "../../world/runtime/collision/test_boxes.c"
#undef MC_COLLISION_BOXES_MAIN
#include "../../../src/entity/player/player_internal.h"
#include "mc/block/registry/step_sound.h"
#include "mc/entity/movement/movement.h"
#include "mc/entity/player/input.h"
#include <inttypes.h>

static bool reference_output;

static uint32_t movement_float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static uint64_t movement_double_bits(double value) {
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static bool movement_input(mc_entity *e, double x, double y, double z) {
    e->base.offset = e->base.y_size = e->base.bp = e->base.bk = 0;
    e->base.aX = e->base.aY = e->base.aZ = e->base.ba = false;
    e->base.bb = e->base.bc = e->base.be = false;
    e->base.bd = true;
    e->base.bq = e->base.bx = e->base.bC = false;
    e->base.bu = 5;
    e->base.bv = 0;
    e->base.aG = e->base.aH = NULL;
    return mc_entity_set_size(e, 1, 1) == MC_ENTITY_OK &&
           mc_entity_set_position(e, x, y, z) == MC_ENTITY_OK &&
           mc_entity_set_velocity(e, 0, 2) == MC_ENTITY_OK &&
           mc_entity_set_velocity(e, 1, 3) == MC_ENTITY_OK &&
           mc_entity_set_velocity(e, 2, 4) == MC_ENTITY_OK;
}

static void movement_air(fixture *f, size_t side) {
    boxes_air(f, side);
    memset(f->states[side]->data->metadata, 0, MC_CHUNK_NIBBLE_BYTES);
}

static void movement_floor(fixture *f, size_t side) {
    for (int32_t x = 6; x <= 12; ++x)
        for (int32_t z = 6; z <= 12; ++z)
            f->states[side]->data->blocks[boxes_index(x, 7, z)] = 1;
}

static bool step_text(const mc_registry_string *string, const char *text) {
    if (string == NULL || string->length != strlen(text))
        return false;
    for (size_t i = 0; i < string->length; ++i)
        if (string->units[i] != (unsigned char)text[i])
            return false;
    return true;
}

static bool sound_cases(fixture *f, size_t side) {
    bool ok = false;
    mc_registry_classes *classes = f->registry[side];
    mc_registry_block_view blocks;
    mc_step_sound_name first = {0}, second = {0}, retained = {0};
    CHECK(mc_registry_classes_inspect_blocks(classes, &blocks, NULL) == MC_REGISTRY_OK);
    const mc_block_sound *sounds[] = {blocks.m[1]->fields.by, blocks.m[20]->fields.by,
                                      blocks.m[12]->fields.by};
    static const char *const names[] = {"step.stone", "random.glass", "step.gravel"};
    static const char *const walking[] = {"step.stone", "step.stone", "step.sand"};
    for (size_t i = 0; i < COUNT(sounds); ++i) {
        float volume = 37, pitch = 37;
        CHECK(mc_registry_step_sound_volume(sounds[i], &volume, NULL) == MC_REGISTRY_OK &&
              volume == 1.0f);
        CHECK(mc_registry_step_sound_pitch(sounds[i], &pitch, NULL) == MC_REGISTRY_OK &&
              pitch == 1.0f);
        CHECK(mc_registry_step_sound_name(classes, sounds[i], true, &first, NULL) ==
                  MC_REGISTRY_OK &&
              step_text(first.string, names[i]));
        CHECK(mc_registry_step_sound_name(classes, sounds[i], true, &second, NULL) ==
                  MC_REGISTRY_OK &&
              ((first.string == second.string) == (i != 0)));
        if (reference_output && side == 0)
            printf("CMP SOUND %zu a %s %d %08" PRIx32 " %08" PRIx32 "\n", i, names[i],
                   first.string == second.string ? 1 : 0, movement_float_bits(volume),
                   movement_float_bits(pitch));
        CHECK(mc_registry_step_sound_name_release(&first, NULL) == MC_REGISTRY_OK &&
              mc_registry_step_sound_name_release(&second, NULL) == MC_REGISTRY_OK);
        CHECK(mc_registry_step_sound_name(classes, sounds[i], false, &first, NULL) ==
                  MC_REGISTRY_OK &&
              step_text(first.string, walking[i]));
        CHECK(mc_registry_step_sound_name(classes, sounds[i], false, &second, NULL) ==
                  MC_REGISTRY_OK &&
              first.string != second.string);
        if (reference_output && side == 0)
            printf("CMP SOUND %zu d %s 0 %08" PRIx32 " %08" PRIx32 "\n", i, walking[i],
                   movement_float_bits(volume), movement_float_bits(pitch));
        CHECK(mc_registry_step_sound_name_retain(classes, first.string, &retained, NULL) ==
                  MC_REGISTRY_OK &&
              retained.string == first.string);
        CHECK(mc_registry_step_sound_name_release(&first, NULL) == MC_REGISTRY_OK &&
              step_text(retained.string, walking[i]));
        CHECK(mc_registry_step_sound_name_release(&retained, NULL) == MC_REGISTRY_OK &&
              mc_registry_step_sound_name_release(&second, NULL) == MC_REGISTRY_OK);
    }
    const size_t managed = classes->managed_current_bytes, bytes = classes->allocation_bytes,
                 borrows = classes->runtime_borrows;
    for (size_t i = 0; i < 10000; ++i) {
        CHECK(mc_registry_step_sound_name(classes, sounds[i % 3], false, &first, NULL) ==
                  MC_REGISTRY_OK &&
              first.owner != NULL);
        CHECK(mc_registry_step_sound_name_release(&first, NULL) == MC_REGISTRY_OK &&
              classes->managed_current_bytes == managed && classes->allocation_bytes == bytes &&
              classes->runtime_borrows == borrows);
    }
    mc_block_sound *mutable = (mc_block_sound *)(void *)sounds[0];
    const mc_registry_string *suffix = mutable->fields.a;
    mutable->fields.a = NULL;
    const mc_registry_status null_name =
        mc_registry_step_sound_name(classes, mutable, false, &first, NULL);
    mutable->fields.a = suffix;
    CHECK(null_name == MC_REGISTRY_OK && step_text(first.string, "step.null") &&
          mc_registry_step_sound_name_release(&first, NULL) == MC_REGISTRY_OK);
    const size_t limit = classes->options.max_allocation_bytes;
    classes->options.max_allocation_bytes =
        classes->allocation_bytes + classes->managed_current_bytes;
    const mc_registry_status exhausted =
        mc_registry_step_sound_name(classes, sounds[0], false, &first, NULL);
    classes->options.max_allocation_bytes = limit;
    CHECK(exhausted == MC_REGISTRY_LIMIT && first.classes == NULL &&
          classes->managed_current_bytes == managed && classes->allocation_bytes == bytes &&
          classes->runtime_borrows == borrows);
    ok = true;
cleanup:
    (void)mc_registry_step_sound_name_release(&first, NULL);
    (void)mc_registry_step_sound_name_release(&second, NULL);
    (void)mc_registry_step_sound_name_release(&retained, NULL);
    return ok;
}

static bool movement_reference_cases(fixture *f, mc_entity *player) {
    bool ok = false;
    mc_entity_move_error error;
    mc_entity_effect_error effect;
    /* Controlled source fields and actual Chunk bytes are authored inputs,
     * not observed answers or replacements for collision/presentation methods. */
    for (int32_t scenario = 0; scenario < 6; ++scenario) {
        movement_air(f, 0);
        CHECK(movement_input(player, 8.5, 8, 8.5));
        const mc_movement_input_fields keys = {0};
        CHECK(mc_movement_input_set_fields(player->local_player->fields.input, &keys, NULL) ==
              MC_MOVEMENT_INPUT_OK);
        player->base.bj = 0;
        player->base.private_b = 0;
        player->local_player->fields.player.u = false;
        mc_java_random_reseed(f->worlds[0]->fields.r, 38);
        const uint8_t support = scenario == 0 ? 0 : scenario == 4 ? 8 : scenario == 5 ? 73 : 1;
        if (scenario == 1 || scenario == 2 || scenario == 5)
            movement_floor(f, 0);
        if (scenario != 3)
            f->states[0]->data->blocks[boxes_index(9, 7, 8)] = support;
        if (scenario == 2)
            f->states[0]->data->blocks[boxes_index(9, 8, 8)] = 78;
        if (scenario == 3)
            f->states[0]->data->blocks[boxes_index(9, 6, 8)] = 85;
        const double dx = scenario == 0 ? .25 : 1.0, dy = scenario == 0 ? .5 : -.25,
                     dz = scenario == 0 ? -.25 : 0.0;
        CHECK(mc_entity_move_in_context(player, f->registry[0], dx, dy, dz, NULL, &error) ==
              MC_ENTITY_OK);
        printf("CMP MOVE %" PRId32 " %016" PRIx64 " %016" PRIx64 " %016" PRIx64 " %08" PRIx32
               " %" PRId32 " %d %d %d %d %08" PRIx32 " %" PRId32 " %012" PRIx64 " %u\n",
               scenario, movement_double_bits(player->base.x), movement_double_bits(player->base.y),
               movement_double_bits(player->base.z), movement_float_bits(player->base.bj),
               player->base.private_b, player->base.aX ? 1 : 0, player->base.aY ? 1 : 0,
               player->base.aZ ? 1 : 0, player->base.ba ? 1 : 0,
               movement_float_bits(player->base.bk), player->base.bv, f->worlds[0]->fields.r->state,
               (unsigned int)f->states[0]->data->blocks[boxes_index(9, 7, 8)]);
    }
    CHECK(movement_input(player, 8.5, 8, 8.5));
    for (int32_t scenario = 0; scenario < 4; ++scenario) {
        player->local_player->fields.player.z = scenario - 1;
        player->local_player->fields.player.A = false;
        CHECK(mc_entity_effect_contact_portal(player, &effect) == MC_ENTITY_OK);
        printf("CMP PORTAL %" PRId32 " %" PRId32 " %d\n", scenario,
               player->local_player->fields.player.z,
               player->local_player->fields.player.A ? 1 : 0);
    }
    for (int32_t scenario = 0; scenario < 3; ++scenario) {
        player->base.bk = (float)scenario * .75f;
        CHECK(mc_entity_effect_fall_state_in_context(player, NULL, 0, true, &effect) ==
              MC_ENTITY_OK);
        printf("CMP FALL %" PRId32 " %08" PRIx32 "\n", scenario,
               movement_float_bits(player->base.bk));
    }
    for (int32_t scenario = 0; scenario < 2; ++scenario) {
        bool ordinary = true;
        player->local_player->fields.living.av = 19;
        player->local_player->fields.living.Y = scenario == 0 ? 0 : 20;
        player->base.bb = false;
        CHECK(mc_entity_effect_damage(player, NULL, scenario == 0 ? 7 : 0, &ordinary, &effect) ==
              MC_ENTITY_OK);
        printf("CMP DAMAGE %" PRId32 " %d %" PRId32 " %" PRId32 " %d\n", scenario, ordinary ? 1 : 0,
               player->local_player->fields.living.av, player->local_player->fields.living.Y,
               player->base.bb ? 1 : 0);
    }
    static const uint32_t round_inputs[] = {
        UINT32_C(0x40000000), UINT32_C(0x400051eb), UINT32_C(0x400051ec), UINT32_C(0x400051ed),
        UINT32_C(0x4affffff), UINT32_C(0x4b000000), UINT32_C(0x4b000001), UINT32_C(0x5bffffff),
        UINT32_C(0x5c000000), UINT32_C(0x5dffffff), UINT32_C(0x7f7fffff), UINT32_C(0x7f800000)};
    for (size_t scenario = 0; scenario < COUNT(round_inputs); ++scenario) {
        float distance;
        memcpy(&distance, &round_inputs[scenario], sizeof(distance));
        CHECK(mc_entity_effect_fall_in_context(player, f->registry[0], distance, &effect) ==
                  MC_ENTITY_STATE_ERROR &&
              effect.player_bytecode == 20 &&
              effect.statistic == f->registry[0]->statistics.named[8]);
        printf("JDK ROUND %zu %08" PRIx32 " %" PRId32 "\n", scenario, round_inputs[scenario],
               effect.statistic_amount);
    }
    ok = true;
cleanup:
    return ok;
}

typedef struct step_observer {
    mc_registry_classes *classes;
    size_t sounds, particles;
    float volume, pitch;
    mc_step_sound_name last;
    bool fail_sound;
} step_observer;

static mc_world_storage_status observe_sound(void *context, mc_world_runtime *world,
                                             const mc_registry_string *name, double x, double y,
                                             double z, float volume, float pitch,
                                             mc_world_storage_error *error) {
    step_observer *observer = context;
    (void)world;
    (void)x;
    (void)y;
    (void)z;
    ++observer->sounds;
    observer->volume = volume;
    observer->pitch = pitch;
    if (observer->last.classes != NULL &&
        mc_registry_step_sound_name_release(&observer->last, NULL) != MC_REGISTRY_OK)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (mc_registry_step_sound_name_retain(observer->classes, name, &observer->last, NULL) !=
        MC_REGISTRY_OK)
        return MC_WORLD_STORAGE_UNSUPPORTED_STATE;
    if (observer->fail_sound) {
        if (error != NULL)
            *error = (mc_world_storage_error){MC_WORLD_STORAGE_IO_ERROR,
                                              "authored reached sound failure", 0};
        return MC_WORLD_STORAGE_IO_ERROR;
    }
    return MC_WORLD_STORAGE_OK;
}

static bool step_cases(fixture *f, size_t side) {
    bool ok = false;
    mc_registry_block_view view;
    mc_world_step_error error;
    CHECK(mc_registry_classes_inspect_blocks(f->registry[side], &view, NULL) == MC_REGISTRY_OK);
    /* Every actual current Block identity resolves its real base body except
     * the two stateful families; Stairs' real wrapped Blocks also resolve base. */
    for (size_t id = 1; id < 256; ++id)
        if (view.m[id] != NULL && view.m[id]->kind != MC_BLOCK_REDSTONE_ORE &&
            view.m[id]->kind != MC_BLOCK_FARMLAND)
            CHECK(mc_world_runtime_entity_step(NULL, view.m[id], INT32_MIN, INT32_MAX, -1, NULL,
                                               NULL, &error) == MC_WORLD_STORAGE_OK);
    mc_block *stairs = (mc_block *)(void *)view.m[53];
    mc_block *wrapped = stairs->subtype.ss.a;
    stairs->subtype.ss.a = stairs;
    const mc_world_step_limits cap = {3};
    const mc_world_storage_status cycle =
        mc_world_runtime_entity_step(NULL, stairs, 0, 0, 0, NULL, &cap, &error);
    stairs->subtype.ss.a = wrapped;
    CHECK(cycle == MC_WORLD_STORAGE_LIMIT && error.wrapped_calls == 3 && error.bytecode == 11);
    movement_air(f, side);
    f->states[side]->data->blocks[boxes_index(8, 7, 8)] = 73;
    mc_java_random_reseed(f->worlds[side]->fields.r, 38);
    mc_java_random expected = *f->worlds[side]->fields.r;
    for (size_t draw = 0; draw < 18; ++draw)
        (void)mc_java_random_next_float(&expected);
    CHECK(mc_world_runtime_entity_step(f->worlds[side], view.m[73], 8, 7, 8, NULL, NULL, &error) ==
              MC_WORLD_STORAGE_OK &&
          mc_test_java_random_equal(&expected, f->worlds[side]->fields.r) &&
          f->states[side]->data->blocks[boxes_index(8, 7, 8)] == 74 &&
          f->worlds[side]->active_calls == 0);
    mc_java_random_reseed(f->worlds[side]->fields.r, 38);
    expected = *f->worlds[side]->fields.r;
    for (size_t draw = 0; draw < 18; ++draw)
        (void)mc_java_random_next_float(&expected);
    CHECK(mc_world_runtime_entity_step(f->worlds[side], view.m[74], 8, 7, 8, NULL, NULL, &error) ==
              MC_WORLD_STORAGE_OK &&
          mc_test_java_random_equal(&expected, f->worlds[side]->fields.r) &&
          f->states[side]->data->blocks[boxes_index(8, 7, 8)] == 74);
    movement_air(f, side);
    f->states[side]->data->blocks[boxes_index(8, 7, 8)] = 60;
    mc_java_random_reseed(f->worlds[side]->fields.r, 23);
    expected = *f->worlds[side]->fields.r;
    int32_t draw;
    CHECK(mc_java_random_next_bounded(&expected, 4, &draw));
    CHECK(mc_world_runtime_entity_step(f->worlds[side], view.m[60], 8, 7, 8, NULL, NULL, &error) ==
              MC_WORLD_STORAGE_OK &&
          mc_test_java_random_equal(&expected, f->worlds[side]->fields.r) &&
          f->states[side]->data->blocks[boxes_index(8, 7, 8)] == (draw == 0 ? 3 : 60));
    ok = true;
cleanup:
    return ok;
}

static bool player_cases(fixture *f, mc_entity *player) {
    bool ok = false, ordinary = true;
    mc_entity_effect_error effect;
    mc_entity_move_error error;
    mc_movement_input *input = NULL;
    mc_world_access *access = NULL;
    bool registered = false;
    step_observer observer = {.classes = f->registry[0]};
    const mc_world_access_callbacks callbacks = {.sound = observe_sound};
    CHECK(mc_entity_effect_walks(player, &ordinary, &effect) == MC_ENTITY_OK && ordinary);
    ordinary = true;
    CHECK(mc_entity_effect_sneaking(player, &ordinary, &effect) == MC_ENTITY_STATE_ERROR &&
          ordinary && effect.source_bytecode == 4);
    CHECK(mc_movement_input_create(&input, NULL) == MC_MOVEMENT_INPUT_OK &&
          mc_local_player_set_movement_input(player, input, NULL) == MC_PLAYER_OK);
    CHECK(mc_entity_effect_sneaking(player, &ordinary, &effect) == MC_ENTITY_OK && !ordinary);
    mc_movement_input_fields keys = {.sneak = true};
    CHECK(mc_movement_input_set_fields(input, &keys, NULL) == MC_MOVEMENT_INPUT_OK &&
          mc_entity_effect_sneaking(player, &ordinary, &effect) == MC_ENTITY_OK && ordinary);
    player->local_player->fields.player.u = true;
    CHECK(mc_entity_effect_sneaking(player, &ordinary, &effect) == MC_ENTITY_OK && !ordinary);
    player->local_player->fields.player.u = false;
    keys.sneak = false;
    CHECK(mc_movement_input_set_fields(input, &keys, NULL) == MC_MOVEMENT_INPUT_OK);
    player->local_player->fields.player.z = 1;
    CHECK(mc_entity_effect_contact_portal(player, &effect) == MC_ENTITY_OK &&
          player->local_player->fields.player.z == 10);
    player->local_player->fields.player.z = 0;
    CHECK(mc_entity_effect_contact_portal(player, &effect) == MC_ENTITY_OK &&
          player->local_player->fields.player.A);
    CHECK(movement_input(player, 8.5, 8, 8.5));
    player->base.bk = 1.75f;
    CHECK(mc_entity_effect_fall_state_in_context(player, NULL, 0, true, &effect) == MC_ENTITY_OK &&
          player->base.bk == 0.0f);
    player->base.bk = 2.0f;
    CHECK(mc_entity_effect_fall_state_in_context(player, f->registry[0], 0, true, &effect) ==
              MC_ENTITY_STATE_ERROR &&
          player->base.bk == 2.0f && effect.fall_bytecode == 18 && effect.player_bytecode == 20 &&
          effect.statistic == f->registry[0]->statistics.named[8] &&
          effect.statistic_amount == 200);
    player->local_player->fields.living.av = 19;
    player->local_player->fields.living.Y = 0;
    CHECK(mc_entity_effect_damage(player, NULL, 1, &ordinary, &effect) == MC_ENTITY_OK &&
          !ordinary && player->local_player->fields.living.av == 0 && !player->base.bb);
    player->local_player->fields.living.Y = 20;
    CHECK(mc_entity_effect_damage(player, NULL, 0, &ordinary, &effect) == MC_ENTITY_OK &&
          !ordinary);
    ordinary = true;
    CHECK(mc_entity_effect_damage_in_context(player, f->registry[0], NULL, 7, &ordinary, &effect) ==
              MC_ENTITY_STATE_ERROR &&
          ordinary && effect.player_bytecode == 152 &&
          effect.statistic == f->registry[0]->statistics.named[18] &&
          effect.statistic_amount == 7 && !player->base.bb);
    CHECK(mc_entity_effect_dead(player, &effect) == MC_ENTITY_STATE_ERROR && player->base.be &&
          effect.close_bytecode == 9);
    CHECK(movement_input(player, 8.5, 8, 8.5));
    /* Real no-clip returns before World/code statics, even with null World. */
    mc_world_runtime *saved_world = player->base.world;
    player->base.world = NULL;
    player->base.bq = true;
    const mc_entity_status noclip =
        mc_entity_move_in_context(player, NULL, .25, .5, -.25, NULL, &error);
    player->base.world = saved_world;
    CHECK(noclip == MC_ENTITY_OK && error.bytecode == 88 && player->base.x == 8.75 &&
          player->base.y == 8.5 && player->base.z == 8.25);
    CHECK(mc_world_access_create(&callbacks, &observer, &access, NULL) == MC_WORLD_STORAGE_OK &&
          mc_world_runtime_add_world_access(f->worlds[0], access, NULL) == MC_WORLD_STORAGE_OK);
    registered = true;
    movement_air(f, 0);
    movement_floor(f, 0);
    CHECK(movement_input(player, 8.5, 8, 8.5));
    player->base.bj = 0;
    player->base.private_b = 0;
    CHECK(mc_entity_move_in_context(player, f->registry[0], 1, -.25, 0, NULL, &error) ==
              MC_ENTITY_OK &&
          error.bytecode == 2028 && player->base.bj == .6f && player->base.private_b == 1 &&
          observer.sounds == 1 && step_text(observer.last.string, "step.stone") &&
          observer.volume == .15f && observer.pitch == 1.0f);
    CHECK(movement_input(player, 8.5, 8, 8.5));
    player->base.bj = 0;
    player->base.private_b = 0;
    observer.fail_sound = true;
    CHECK(mc_entity_move_in_context(player, f->registry[0], 1, -.25, 0, NULL, &error) ==
              MC_ENTITY_WORLD_FAILED &&
          error.bytecode == 1657 && player->base.bj == .6f && player->base.private_b == 1 &&
          observer.sounds == 2 && step_text(observer.last.string, "step.stone"));
    observer.fail_sound = false;
    bool removed = false;
    CHECK(mc_world_runtime_remove_world_access(f->worlds[0], access, &removed, NULL) ==
              MC_WORLD_STORAGE_OK &&
          removed);
    registered = false;
    CHECK(mc_world_access_free(access, NULL) == MC_WORLD_STORAGE_OK);
    access = NULL;
    CHECK(mc_registry_step_sound_name_release(&observer.last, NULL) == MC_REGISTRY_OK);
    /* Sneaking suppresses distance/sound/step before support getters. */
    CHECK(movement_input(player, 8.5, 8, 8.5));
    player->base.aX = true;
    keys.sneak = true;
    CHECK(mc_movement_input_set_fields(input, &keys, NULL) == MC_MOVEMENT_INPUT_OK);
    player->base.bj = 0;
    player->base.private_b = 0;
    CHECK(mc_entity_move_in_context(player, NULL, .2, -.25, 0, NULL, &error) == MC_ENTITY_OK &&
          player->base.bj == 0 && player->base.private_b == 0);
    CHECK(player->holds == 1 && f->worlds[0]->active_calls == 0 && f->worlds[0]->local_leases == 0);
    ok = true;
cleanup:
    if (registered) {
        bool cleanup_removed = false;
        (void)mc_world_runtime_remove_world_access(f->worlds[0], access, &cleanup_removed, NULL);
    }
    (void)mc_world_access_free(access, NULL);
    (void)mc_registry_step_sound_name_release(&observer.last, NULL);
    (void)mc_movement_input_retire(&input, NULL);
    return ok;
}

#ifndef MC_PLAYER_MOVEMENT_MAIN
#define MC_PLAYER_MOVEMENT_MAIN main
#endif
int MC_PLAYER_MOVEMENT_MAIN(int argc, char **argv) {
    int result = 1;
    fixture *f = NULL;
    mc_player_session_classes *sessions = NULL;
    mc_player_session *session = NULL;
    mc_player_string *username = NULL;
    mc_entity *player = NULL;
    char root[8192];
    const uint16_t title[] = {'M', 'o', 'v', 'e'}, name[] = {'W', 'a', 'l', 'k'};
    if (argc != 3 ||
        (strcmp(argv[1], "player-movement") != 0 &&
         strcmp(argv[1], "player-movement-reference") != 0) ||
        !decode_path(argv[2], root, sizeof(root)) || !mc_math_trig_init(&trig))
        return 1;
    reference_output = strcmp(argv[1], "player-movement-reference") == 0;
    f = calloc(1, sizeof(*f));
    CHECK(f != NULL && mc_world_regions_create(NULL, &f->regions, NULL) == MC_WORLD_STORAGE_OK);
    CHECK(mc_materials_create(&f->materials));
    CHECK(mc_java_random_profile_create(8u * 1024u * 1024u, &f->profile) ==
          MC_JAVA_RANDOM_PROFILE_OK);
    CHECK(mc_world_info_name_copy((mc_nbt_string){title, COUNT(title)}, SIZE_MAX, &f->name) ==
          MC_WORLD_INFO_OK);
    for (size_t side = 0; side < 2; ++side) {
        resources *r = &f->sources[side];
        r->streams[0] = (stream){.bytes = language, .length = sizeof(language) - 1u};
        r->streams[1] = r->streams[2] = (stream){.bytes = language};
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
        CHECK(sound_cases(f, side) && step_cases(f, side));
    }
    CHECK(mc_registry_classes_initialize_statistics(f->registry[0], NULL) == MC_REGISTRY_OK);
    CHECK(mc_player_session_classes_create(f->entity_classes[0], f->registry[0], &sessions, NULL) ==
          MC_PLAYER_OK);
    CHECK(mc_player_string_create(name, COUNT(name), &username) == MC_PLAYER_OK);
    CHECK(mc_player_session_create(sessions, username, NULL, &session, NULL) == MC_PLAYER_OK);
    CHECK(mc_local_player_allocate(f->entity_classes[0], &player, NULL) == MC_ENTITY_OK);
    CHECK(mc_local_player_construct(player, f->worlds[0], NULL, session, 0, NULL, NULL) ==
          MC_PLAYER_OK);
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    username = NULL;
    CHECK(player_cases(f, player));
    if (reference_output)
        CHECK(movement_reference_cases(f, player));
    result = 0;
cleanup:
    mc_entity_release(player);
    mc_player_session_retire(&session);
    mc_player_string_release(username);
    if (mc_player_session_classes_free(&sessions, NULL) != MC_PLAYER_OK || !cleanup_fixture(f))
        return 1;
    if (result == 0 && printf("PASS: %u Player movement/step/sound checks\n", checks) < 0)
        return 1;
    return result;
}
