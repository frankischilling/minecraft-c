# Class namespaces remain CPU-only; actual caller construction is client-only.
if(TARGET mc_client_application_owner)
    target_link_libraries(mc_client_application_owner PRIVATE
        mc_client_gui_ingame mc_client_screen_classes)
endif()
if(MC_BUILD_CLIENT AND TARGET mc_client_application_startup)
    target_sources(mc_client_application_startup PRIVATE
        src/client/startup/application_startup_gui_ingame.c)
    target_link_libraries(mc_client_application_startup PUBLIC mc_client_gui_ingame)
endif()
