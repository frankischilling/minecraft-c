if(MC_BUILD_CLIENT AND TARGET mc_client_render_global)
    target_sources(mc_client_render_global PRIVATE src/client/world/render_global_access.c)
    target_link_libraries(mc_client_render_global PRIVATE mc_game_settings)
endif()
