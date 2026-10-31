# Include after mc_simulation and the optional client image libraries exist.
# The shared fd.u tail is also required by the existing headless Player body.
target_sources(mc_simulation PRIVATE src/world/runtime/spawn.c)
if(MC_BUILD_CLIENT)
    target_sources(mc_client_image_io PRIVATE src/client/images/class_url.c)
    target_link_libraries(mc_client_image_io PRIVATE mc_resources)
    add_library(mc_client_texture_effects STATIC
        src/client/texture_effects/effect.c src/client/texture_effects/construct.c
        src/client/texture_effects/portal.c src/client/texture_effects/flowing_water.c
        src/client/texture_effects/flowing_lava.c src/client/texture_effects/fire.c
        src/client/texture_effects/compass.c src/client/texture_effects/watch.c
        src/client/texture_effects/resource_image.c src/client/render_engine/effects.c)
    target_include_directories(mc_client_texture_effects PUBLIC include PRIVATE src src/platform/time)
    target_link_libraries(mc_client_texture_effects PUBLIC mc_client_notice mc_client_render_engine
        PRIVATE mc_simulation mc_client_image_io mc_resources mc_build_options)
endif()
