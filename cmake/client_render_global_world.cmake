# Exact source n World attachment/rebuild and DK constructor/position owners.
if(TARGET mc_client_render_global)
    target_sources(mc_client_render_global PRIVATE
        src/client/world/render_global_world.c
        src/client/world/render_global_mark.c
        src/client/world/source_chunk_renderer.c
        src/client/world/chunk_cache.c
        src/client/world/render_global_update.c
        src/client/world/render_global_update_sort.c
        src/client/world/source_chunk_compile.c)
    target_link_libraries(mc_client_render_global PUBLIC mc_client_render_manager_cpu)
endif()
