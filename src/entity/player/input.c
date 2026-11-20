#include "mc/entity/player/input.h"
#include "player_internal.h"

static mc_player_status fail(mc_player_input_error *error, mc_player_status status,
                             const char *operation, const char *class_name, const char *method,
                             const char *descriptor, unsigned int pc) {
    error->status = status;
    error->operation = operation;
    error->site = (mc_player_site){class_name, method, descriptor, pc};
    return status;
}

static bool hold_player(mc_entity *player, const char *method, const char *descriptor,
                        unsigned int null_pc, mc_player_input_error *error) {
    if (player == NULL) {
        fail(error, MC_PLAYER_NULL_REFERENCE, "local Player input null receiver", "dc", method,
             descriptor, null_pc);
        return false;
    }
    if (!mc_entity_is_kind(player, MC_ENTITY_CLASS_KIND_LOCAL_PLAYER)) {
        fail(error, MC_PLAYER_INVALID_ARGUMENT, "local Player input concrete owner", NULL, NULL,
             NULL, 0);
        return false;
    }
    error->entity = mc_entity_retain(player);
    if (error->entity != MC_ENTITY_OK) {
        fail(error, MC_PLAYER_ENTITY_FAILED, "local Player input Entity hold", NULL, NULL, NULL, 0);
        return false;
    }
    return true;
}

static mc_player_status child_failure(mc_player_input_error *error, const char *method,
                                      const char *descriptor, unsigned int pc) {
    mc_player_status status = MC_PLAYER_INVALID_ARGUMENT;
    switch (error->input.status) {
        case MC_MOVEMENT_INPUT_SOURCE_EXCEPTION:
            status = MC_PLAYER_NULL_REFERENCE;
            break;
        case MC_MOVEMENT_INPUT_LIMIT:
            status = MC_PLAYER_LIMIT;
            break;
        case MC_MOVEMENT_INPUT_ALLOCATION_FAILED:
            status = MC_PLAYER_ALLOCATION_FAILED;
            break;
        default:
            break;
    }
    return fail(error, status, error->input.operation, "dc", method, descriptor, pc);
}

mc_player_status mc_local_player_set_movement_input(mc_entity *player, mc_movement_input *input,
                                                    mc_player_input_error *out) {
    mc_player_input_error error = {.input = {.named_index = -1}};
    if (player == NULL)
        fail(&error, MC_PLAYER_NULL_REFERENCE, "local Player input field receiver",
             "net/minecraft/client/Minecraft", "a", "(Lfd;Ljava/lang/String;Lgs;)V", 205);
    else if (hold_player(player, NULL, NULL, 0, &error)) {
        if (input != NULL && mc_movement_input_retain(input, &error.input) != MC_MOVEMENT_INPUT_OK)
            fail(&error,
                 error.input.status == MC_MOVEMENT_INPUT_LIMIT ? MC_PLAYER_LIMIT
                                                               : MC_PLAYER_INVALID_ARGUMENT,
                 error.input.operation, NULL, NULL, NULL, 0);
        else {
            mc_movement_input *previous = player->local_player->fields.input;
            player->local_player->fields.input = input;
            (void)mc_movement_input_retire(&previous, NULL);
        }
        mc_entity_release(player);
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_player_status mc_local_player_input_key(mc_entity *player, int32_t code, bool pressed,
                                           mc_player_input_error *out) {
    mc_player_input_error error = {.input = {.named_index = -1}};
    if (hold_player(player, "a", "(IZ)V", 1, &error)) {
        mc_movement_input *input = player->local_player->fields.input;
        if (input == NULL)
            fail(&error, MC_PLAYER_NULL_REFERENCE, "local Player key null input", "dc", "a",
                 "(IZ)V", 6);
        else if (mc_movement_input_key(input, code, pressed, &error.input) != MC_MOVEMENT_INPUT_OK)
            child_failure(&error, "a", "(IZ)V", 6);
        mc_entity_release(player);
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_player_status mc_local_player_input_reset(mc_entity *player, mc_player_input_error *out) {
    mc_player_input_error error = {.input = {.named_index = -1}};
    if (hold_player(player, "o_", "()V", 1, &error)) {
        mc_movement_input *input = player->local_player->fields.input;
        if (input == NULL)
            fail(&error, MC_PLAYER_NULL_REFERENCE, "local Player reset null input", "dc", "o_",
                 "()V", 4);
        else if (mc_movement_input_reset(input, &error.input) != MC_MOVEMENT_INPUT_OK)
            child_failure(&error, "o_", "()V", 4);
        mc_entity_release(player);
    }
    if (out != NULL)
        *out = error;
    return error.status;
}

mc_player_status mc_local_player_is_sneaking(mc_entity *player, bool *out,
                                             mc_player_input_error *out_error) {
    mc_player_input_error error = {.input = {.named_index = -1}};
    if (out == NULL)
        fail(&error, MC_PLAYER_INVALID_ARGUMENT, "local Player sneaking output", NULL, NULL, NULL,
             0);
    else if (hold_player(player, "t", "()Z", 1, &error)) {
        mc_movement_input_state state;
        if (!mc_movement_input_read_state(player->local_player->fields.input, &state))
            fail(&error, MC_PLAYER_NULL_REFERENCE, "local Player sneaking null input", "dc", "t",
                 "()Z", 4);
        else
            *out = state.fields.sneak && !player->local_player->fields.player.u;
        mc_entity_release(player);
    }
    if (out_error != NULL)
        *out_error = error;
    return error.status;
}
