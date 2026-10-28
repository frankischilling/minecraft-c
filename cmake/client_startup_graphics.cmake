if(MC_BUILD_CLIENT AND TARGET mc_client_application_startup AND TARGET mc_client_gl_diagnostics)
    target_sources(mc_client_application_startup PRIVATE
        src/client/startup/application_startup_graphics.c)
    target_link_libraries(mc_client_application_startup PUBLIC mc_client_gl_diagnostics)
endif()
