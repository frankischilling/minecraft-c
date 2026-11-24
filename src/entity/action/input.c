#include "action_internal.h"

bool mc_action_input(mc_entity *p, mc_player_action_error *e) {
    e->source_class = "gs";
    e->source_method = "f_()V";
    mc_player_fields *player = &p->local_player->fields.player;
    if (player->j) {
        player->k = mc_java_i32_add(player->k, 1);
        if (player->k >= 8) {
            player->k = 0;
            player->j = false;
        }
    } else
        player->k = 0;
    e->swing_bytecode = 49;
    if (!mc_action_fp(e, 49))
        return false;
    p->local_player->fields.living.X = ac_fdiv(ac_i2f(player->k), 8.0f);
    e->source_class = "dc";
    struct mc_movement_input *input = p->local_player->fields.input;
    e->input_bytecode = 9;
    if (input == NULL)
        return mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Action null MovementInput strafe receiver",
                              9);
    p->local_player->fields.living.aw = input->fields.strafe;
    input = p->local_player->fields.input;
    e->input_bytecode = 20;
    if (input == NULL)
        return mc_action_fail(e, MC_ENTITY_STATE_ERROR,
                              "Action null MovementInput forward receiver", 20);
    p->local_player->fields.living.ax = input->fields.forward;
    input = p->local_player->fields.input;
    e->input_bytecode = 31;
    if (input == NULL)
        return mc_action_fail(e, MC_ENTITY_STATE_ERROR, "Action null MovementInput jump receiver",
                              31);
    p->local_player->fields.living.az = input->fields.jump;
    return true;
}

mc_entity_status mc_local_player_action_input(mc_entity *p, mc_player_action_error *out) {
    mc_player_action_error frame;
    mc_player_action_error_init(&frame);
    if (mc_action_enter(p, &frame)) {
        (void)mc_action_input(p, &frame);
        mc_entity_release(p);
    }
    return mc_action_finish(&frame, out);
}
