if(MC_BUILD_CLIENT AND TARGET mc_client_application_startup AND TARGET mc_client_source_screens)
    add_library(mc_client_source_screen_events STATIC src/client/gui/source_screen_events.c)
    target_include_directories(mc_client_source_screen_events PUBLIC include src/client/gui PRIVATE src/client)
    target_link_libraries(mc_client_source_screen_events PUBLIC mc_client_source_screens
        mc_client_application_startup mc_client_display mc_client_render_textures
        PRIVATE mc_build_options)
endif()
