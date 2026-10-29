# Actual da/fu namespace is CPU-only, independently releasable by MC classes.
add_library(mc_client_screen_classes STATIC src/client/gui/source_screen_classes.c)
target_include_directories(mc_client_screen_classes PUBLIC include src/client/gui)
target_link_libraries(mc_client_screen_classes PUBLIC mc_simulation mc_registry
    PRIVATE Threads::Threads mc_build_options)

# Source splash streams use the canonical ClassStream/Reader, not diagnostic GUI.
if(TARGET mc_client_image_io)
    add_library(mc_client_source_screens STATIC
        src/client/gui/source_screen.c src/client/gui/source_menu.c
        src/client/gui/source_widgets.c src/client/gui/source_screen_initialize.c)
    target_include_directories(mc_client_source_screens PUBLIC include src/client/gui PRIVATE src/client)
    target_link_libraries(mc_client_source_screens PUBLIC mc_client_screen_classes
        PRIVATE mc_client_image_io mc_client_source_resources mc_client_notice mc_platform_time
        mc_simulation mc_core mc_c_atomics Threads::Threads mc_build_options)
endif()
