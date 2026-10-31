# Exact source n World attachment/rebuild and DK constructor/position owners.
if(TARGET mc_client_render_global)
    target_sources(mc_client_render_global PRIVATE
        src/client/world/render_global_world.c
        src/client/world/render_global_mark.c
        src/client/world/source_chunk_renderer.c
        src/client/world/render_global_update.c
        src/client/world/render_global_update_sort.c
        src/client/world/render_global_submit.c
        src/client/world/render_global_entities.c
        src/client/world/render_global_submit_access.c
        src/client/world/render_global_submit_draw.c
        src/client/world/render_global_occlusion.c
        src/client/world/source_chunk_compile.c
        src/client/render/blocks/access.c
        src/client/render/blocks/block.c
        src/client/render/blocks/bounds.c
        src/client/render/blocks/render.c
        src/client/render/blocks/color.c
        src/client/render/blocks/faces.c
        src/client/render/blocks/flat.c
        src/client/render/blocks/ambient_occlusion.c)
    target_link_libraries(mc_client_render_global PUBLIC mc_client_render_manager_cpu
        mc_client_tile_renderer_cpu mc_client_application_owner mc_client_entity_renderer)
endif()
