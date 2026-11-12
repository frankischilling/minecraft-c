#include "active_internal.h"

bool mc_active_container(mc_active_scope *s, mc_container *container, bool *out,
                         mc_player_tick_error *e) {
    mc_container_state state;
    e->container_bytecode = 0;
    if (!mc_container_read_state(container, &state) || state.side != MC_REGISTRY_CLIENT)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                              "Active Player actual held Container owner");
    if (state.kind == MC_CONTAINER_PLAYER) {
        *out = true;
        return true;
    } /* Actual aa.b(gs): iconst_1/ireturn. */
    if (state.kind != MC_CONTAINER_WORKBENCH)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                              "Active Player unsupported Container eligibility descendant");
    e->source_class = "it";
    e->source_method = "b(Lgs;)Z";
    e->container_bytecode = 16;
    int32_t id;
    if (state.world == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                              "Workbench eligibility null World receiver");
    if (!mc_active_world_result(
            e, mc_world_runtime_block_get(state.world, state.x, state.y, state.z, &id, &e->world),
            "Workbench eligibility actual World block read"))
        return false;
    e->container_bytecode = 19;
    if (!mc_active_registry(s, e))
        return false;
    const mc_block *block;
    if (!mc_active_registry_result(
            e, mc_registry_block_static_get(s->registry, "az", &block, &e->registry)))
        return false;
    e->container_bytecode = 22;
    const mc_block_fields *fields = mc_block_fields_get(block);
    if (fields == NULL)
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                              "Workbench eligibility live Block.az receiver");
    if (id != fields->bn) {
        *out = false;
        return true;
    }
    /* Separate field loads follow the query, as in it30..58. The actual
     * constructed Container remains live/exclusive; no copied World result. */
    if (!mc_container_read_state(container, &state))
        return mc_active_fail(e, MC_ENTITY_STATE_ERROR,
                              "Workbench eligibility live coordinate fields");
    e->container_bytecode = 39;
    if (!mc_active_fp(e))
        return false;
    const double x = at_add((double)state.x, .5), y = at_add((double)state.y, .5),
                 z = at_add((double)state.z, .5);
    e->container_bytecode = 58;
    const double dx = at_sub(s->player->base.x, x), dy = at_sub(s->player->base.y, y),
                 dz = at_sub(s->player->base.z, z);
    const double square = at_add(at_add(at_mul(dx, dx), at_mul(dy, dy)), at_mul(dz, dz));
    *out = !(square > 64.0);
    return true;
}
