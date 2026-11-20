#include "../construction_internal.h"
#include "../entity_internal.h"
#include "../player/player_internal.h"
#include "construction_internal.h"

static mc_entity_status draw(mc_entity *entity, double *out, unsigned int pc,
                             mc_entity_error *error) {
    const mc_java_random_profile_status status = mc_java_math_random(entity->classes->profile, out);
    if (status == MC_JAVA_RANDOM_PROFILE_OK)
        return MC_ENTITY_OK;
    *error = (mc_entity_error){.status = MC_ENTITY_PROFILE_FAILED,
                               .operation = "LivingEntity constructor Math.random",
                               .bytecode = (uint16_t)pc,
                               .profile = status};
    return error->status;
}

mc_entity_status mc_entity_construct_living(mc_entity *entity, mc_living_entity_fields *v,
                                            unsigned int *source_pc, mc_entity_error *error) {
#define STORE(field, value, pc)                                                                    \
    do {                                                                                           \
        v->field = value;                                                                          \
        *source_pc = pc;                                                                           \
    } while (0)
    STORE(E, 20, 8);
    STORE(H, 0.0f, 13);
    STORE(I, 0.0f, 18);
    STORE(N, true, 23);
    STORE(O, mc_player_texture_literal(), 29);
    STORE(P, true, 34);
    STORE(Q, 0.0f, 39);
    STORE(R, NULL, 44);
    STORE(S, 1.0f, 49);
    STORE(T, 0, 54);
    STORE(U, 0.0f, 59);
    STORE(V, false, 64);
    STORE(ac, 0.0f, 69);
    STORE(ad, 0, 74);
    STORE(ae, 0, 79);
    STORE(ah, false, 84);
    STORE(ai, -1, 89);
    double value;
    mc_entity_status status = draw(entity, &value, 93, error);
    if (status != MC_ENTITY_OK)
        return status;
    STORE(aj, mc_local_player_aj_from_draw(value), 105);
    STORE(at, 0.0f, 110);
    STORE(au, 0, 115);
    STORE(av, 0, 120);
    STORE(az, false, 125);
    STORE(aA, 0.0f, 130);
    STORE(aB, 0.7f, 136);
    STORE(aC, 0, 141);
    STORE(Y, 10, 147);
    entity->base.aF = true;
    *source_pc = 152;
    status = draw(entity, &value, 156, error);
    if (status != MC_ENTITY_OK)
        return status;
    STORE(G, mc_local_player_g_from_draw(value), 165);
    mc_entity_source_position(entity, entity->base.x, entity->base.y, entity->base.z);
    *source_pc = 181;
    status = draw(entity, &value, 185, error);
    if (status != MC_ENTITY_OK)
        return status;
    STORE(F, mc_local_player_f_from_draw(value), 192);
    status = draw(entity, &value, 196, error);
    if (status != MC_ENTITY_OK)
        return status;
    entity->base.yaw = mc_local_player_yaw_from_draw(value);
    *source_pc = 208;
    entity->base.bp = 0.5f;
    *source_pc = 217;
#undef STORE
    return MC_ENTITY_OK;
}
