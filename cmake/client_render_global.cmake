# Include after the actual Engine/textures, cx and Simulation targets exist.
# No source in this module is attached to Core, World or headless libraries.
if(MC_BUILD_CLIENT)
    if(NOT TARGET mc_client_render_textures OR NOT TARGET mc_client_cx)
        message(FATAL_ERROR "Actual RenderGlobal requires canonical ge/nw/Display/cx owners")
    endif()
    add_library(mc_client_render_global STATIC
        src/client/world/render_global_owner.c
        src/client/world/render_global_construct.c
        src/client/world/render_batch.c
        src/client/world/render_global_stars.c
        src/client/world/render_global_sky.c)
    target_include_directories(mc_client_render_global PUBLIC include src PRIVATE src/platform/time)
    target_link_libraries(mc_client_render_global PUBLIC mc_client_render_textures mc_client_cx
        PRIVATE mc_simulation mc_core mc_build_options)
endif()
