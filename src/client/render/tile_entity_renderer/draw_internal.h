#ifndef MC_CLIENT_TILE_ENTITY_RENDERER_DRAW_INTERNAL_H
#define MC_CLIENT_TILE_ENTITY_RENDERER_DRAW_INTERNAL_H

#include "entity/construction_internal.h"
#include "internal.h"
#include "mc/client/render/tile_entity_renderer/draw.h"
#include "mc/entity/factory/factory.h"
#include "mc/world/storage/source_chunk_loader.h"

struct mc_client_tile_renderer_draw_call {
    mc_client_tile_renderer_draw_state state;
    mc_client_tile_renderer_draw_error error, cleanup;
    mc_client_tile_renderer_draw_limits limits;
    mc_client_manager_draw_services services;
    mc_client_tile_renderer_dispatcher *dispatcher;
    mc_client_tile_renderer *renderer;
    mc_tile_entity *tile_entity;
    mc_world_runtime *brightness_world;
    mc_client_render_engine_classes *graphics;
    mc_entity *preview, *factory_result, *map_value_pending;
    mc_tile_entity_name *names[3];
    mc_world_brightness_error brightness;
    mc_world_source_chunk_error source_chunk;
    mc_entity_factory_error factory;
    mc_entity_error entity;
    mc_entity_position_rotation_error pose;
    mc_client_render_manager_draw_call *manager_call;
    mc_client_manager_draw_error manager_error, manager_cleanup;
    mc_client_manager_error manager_owner, manager_owner_cleanup;
    mc_client_manager_consumer manager_consumer;
};

void tile_draw_at(mc_client_tile_renderer_draw_call *, const char *source, const char *descriptor,
                  int32_t pc);
bool tile_draw_fail(mc_client_tile_renderer_draw_call *, mc_client_tile_renderer_status,
                    const char *operation, bool native);
bool tile_draw_snapshot(mc_client_tile_renderer_draw_call *, mc_tile_entity_snapshot *, int32_t);
bool tile_draw_graphics(mc_client_tile_renderer_draw_call *);
bool tile_draw_display(mc_client_tile_renderer_draw_call *, bool returned);
bool tile_draw_fp(mc_client_tile_renderer_draw_call *, int32_t);
bool tile_draw_spawner(mc_client_tile_renderer_draw_call *);
/* Actual AG25..63 cache/factory subbody, entered on the held AG/Tile graph.
 * CPU controls exercise this prefix independently, never credit a GL draw. */
bool tile_draw_spawner_cache(mc_client_tile_renderer_draw_call *);
void tile_draw_manager_receipt(mc_client_tile_renderer_draw_call *);

#endif
