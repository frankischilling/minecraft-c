#include "mc/client/controller/attack.h"

mc_entity_status mc_client_singleplayer_attack_begin(mc_client_singleplayer_controller *controller,
                                                     mc_registry_classes *registry,
                                                     mc_math_classes *math, mc_entity *player,
                                                     mc_entity *target, mc_player_attack_call **out,
                                                     mc_player_attack_error *error) {
    if (controller == NULL || player == NULL) {
        if (error != NULL)
            *error = (mc_player_attack_error){.status = MC_ENTITY_STATE_ERROR,
                                              .operation = controller == NULL
                                                               ? "Attack null controller invocation"
                                                               : "Attack null Player invocation",
                                              .site = {"ob", "b", "(Lgs;Lsn;)V", 2},
                                              .source_exception = true,
                                              .exception_class = "java/lang/NullPointerException"};
        return MC_ENTITY_STATE_ERROR;
    }
    return mc_local_player_attack_begin(registry, math, player, target, out, error);
}
