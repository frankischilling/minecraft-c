if(MC_BUILD_CLIENT)
    add_library(mc_client_particles_draw STATIC
        src/client/particles/render.c src/client/particles/render_pickup.c)
    target_include_directories(mc_client_particles_draw PUBLIC include PRIVATE src)
    target_link_libraries(mc_client_particles_draw PUBLIC mc_simulation
        mc_client_render_textures mc_client_render_manager_cpu PRIVATE mc_build_options)
    target_link_libraries(mc_client_entity_renderer PUBLIC mc_client_particles_draw)
endif()
