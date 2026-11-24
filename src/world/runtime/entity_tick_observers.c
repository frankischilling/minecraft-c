#include "dedicated/entity_index.h"
#include "entity_tick_internal.h"

bool wt_observe_removed(wt_scope *s, mc_entity *entity, unsigned int pc) {
    s->error.bytecode = pc;
    for (size_t i = 0;; ++i) {
        mc_world_access *access = NULL;
        bool present;
        mc_world_storage_error cause = {0};
        if (!mc_world_access_next(s->world, i, 7, 20, 25, &access, &present,
                                  &s->error.world_access_bytecode, &cause))
            return wt_cause(s, cause);
        if (!present)
            break;
        s->error.world_access_bytecode = 29;
        if (!wt_work(s, pc)) {
            mc_world_access_release_held(access);
            return false;
        }
        /* The source local remains independently held by the outer loop. */
        const mc_world_storage_status status = mc_world_access_call_entity_removed(
            access, s->world, entity, &cause, &s->error.world_access_failure);
        mc_world_access_release_held(access);
        if (status != MC_WORLD_STORAGE_OK)
            return wt_cause(s, mc_world_access_callback_error(status, cause));
    }
    s->error.world_access_bytecode = 0;
    if (s->world->construction.entry != MC_WORLD_RUNTIME_SERVER_DEDICATED)
        return true;
    mc_world_entity_index *index = s->world->dedicated.G;
    s->error.dedicated_bytecode = 10;
    if (entity == NULL)
        return wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                       "Dedicated removed Entity ID receiver");
    const int32_t id = entity->base.id;
    s->error.dedicated_bytecode = 14;
    bool present;
    mc_world_entity_index_error cause;
    if (mc_world_entity_index_remove(index, id, &present, NULL, &cause) != MC_WORLD_STORAGE_OK) {
        s->error.entity.status = cause.entity_status;
        s->error.index_bytecode = cause.bytecode;
        return wt_cause(s, cause.cause);
    }
    s->error.dedicated_bytecode = s->error.index_bytecode = 0;
    return true;
}
