# Include after the canonical RenderGlobal and application startup targets.
# No rendering source or link edge is attached to a headless target.
if(TARGET mc_client_render_global)
    target_sources(mc_client_render_global PRIVATE
        src/client/world/render_global_retire.c)
endif()
if(MC_BUILD_CLIENT AND TARGET mc_client_application_startup)
    if(NOT TARGET mc_client_render_global)
        message(FATAL_ERROR "Actual startup rendering requires the canonical RenderGlobal owner")
    endif()
    target_sources(mc_client_application_startup PRIVATE
        src/client/startup/application_startup_rendering.c)
    target_link_libraries(mc_client_application_startup PUBLIC mc_client_render_global)
endif()
