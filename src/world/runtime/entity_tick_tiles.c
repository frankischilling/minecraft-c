#include "entity_tick_internal.h"
#include "tile_entities/tick_internal.h"
#include "tile_entities/tile_entities_internal.h"

static bool tile_result(wt_scope *s, bool ok) {
    if (ok) {
        s->error.child = MC_WORLD_ENTITY_TICK_CHILD_NONE;
        return true;
    }
    return wt_cause(s, s->error.child_error.tile_entity.cause);
}

bool wt_tile_active(wt_scope *s) {
    wt_at(s, "g()V", 350);
    s->error.loop_bytecode = 350;
    s->world->fields.L = true;
    mc_live_tile_iterator iterator;
    s->error.loop_bytecode = 357;
    s->error.child = MC_WORLD_ENTITY_TICK_CHILD_TILE_ENTITY;
    if (!tile_result(s, mc_live_tile_iterator_begin_held(s->world, s->world->fields.c, &iterator,
                                                         &s->error.child_error.tile_entity)))
        return false;
    for (;;) {
        bool has;
        s->error.loop_bytecode = 364;
        s->error.child = MC_WORLD_ENTITY_TICK_CHILD_TILE_ENTITY;
        if (!tile_result(s, mc_live_tile_iterator_has_next_held(s->world, &iterator, &has,
                                                                &s->error.child_error.tile_entity)))
            return false;
        if (!has)
            break;
        s->error.tile_index = iterator.cursor;
        if (!wt_work(s, 373))
            return false;
        mc_tile_entity *tile = NULL;
        s->error.loop_bytecode = 373;
        s->error.child = MC_WORLD_ENTITY_TICK_CHILD_TILE_ENTITY;
        if (!tile_result(s, mc_live_tile_iterator_next_held(s->world, &iterator, &tile,
                                                            &s->error.child_error.tile_entity)))
            return false;
        bool ok = true;
        s->error.loop_bytecode = 383;
        if (tile == NULL)
            ok = wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                         "World active null TileEntity invalid receiver");
        else if (!tile->invalid) {
            s->error.loop_bytecode = 390;
            s->error.tile_bytecode = 0;
            if (tile->kind == MC_TILE_ENTITY_CHEST) {
                /* Actual js inherits the literal-return ow.n_()V. */
            } else if (tile->kind == MC_TILE_ENTITY_SPAWNER)
                ok = wt_spawner(s, tile);
            else if (tile->kind == MC_TILE_ENTITY_PISTON) {
                s->error.child = MC_WORLD_ENTITY_TICK_CHILD_PISTON;
                const mc_world_storage_status status = mc_tile_entity_piston_tick(
                    tile, s->context == NULL ? NULL : s->context->piston_move_limits,
                    &s->error.child_error.piston);
                if (status != MC_WORLD_STORAGE_OK)
                    ok = wt_fail(s, status, "World actual Piston tick child");
                else
                    s->error.child = MC_WORLD_ENTITY_TICK_CHILD_NONE;
            } else
                ok = wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                             "World actual concrete TileEntity tick dependency");
        }
        if (ok && tile->invalid) {
            s->error.loop_bytecode = 401;
            s->error.child = MC_WORLD_ENTITY_TICK_CHILD_TILE_ENTITY;
            ok = tile_result(s, mc_live_tile_iterator_remove_held(
                                    s->world, &iterator, &s->error.child_error.tile_entity));
            if (ok) {
                const int32_t x = mc_java_i32_shr(tile->x, 4);
                const int32_t z = mc_java_i32_shr(tile->z, 4);
                mc_chunk_state *state;
                s->error.loop_bytecode = 419;
                ok = wt_get(s, x, z, 419, &state);
                if (ok && state != NULL && !state->blank) {
                    mc_world_store *store = s->world->fields.w;
                    mc_world_chunk *chunk = mc_world_store_chunk_object(store, state);
                    mc_world_storage_error cause = {0};
                    if (chunk == NULL)
                        ok = wt_fail(s, MC_WORLD_STORAGE_UNSUPPORTED_STATE,
                                     "World active actual normal Chunk header");
                    else if (mc_world_store_lease_chunk(store, state, &cause) !=
                             MC_WORLD_STORAGE_OK)
                        ok = wt_cause(s, cause);
                    else {
                        s->error.loop_bytecode = 446;
                        s->error.child = MC_WORLD_ENTITY_TICK_CHILD_TILE_ENTITY;
                        ok = tile_result(s, mc_live_tile_chunk_remove_held(
                                                chunk, tile->x & 15, tile->y, tile->z & 15,
                                                &s->error.child_error.tile_entity));
                        (void)mc_world_store_release_chunk(store, state, NULL);
                    }
                }
            }
        }
        mc_tile_entity_release(tile);
        if (!ok)
            return false; /* No source finally restores L. */
    }
    s->error.loop_bytecode = 454;
    s->world->fields.L = false;
    s->error.loop_bytecode = 461;
    s->error.child = MC_WORLD_ENTITY_TICK_CHILD_TILE_ENTITY;
    const mc_world_storage_status status =
        mc_live_tile_entities_drain_deferred(s->world, &s->error.child_error.tile_entity);
    return tile_result(s, status == MC_WORLD_STORAGE_OK);
}
