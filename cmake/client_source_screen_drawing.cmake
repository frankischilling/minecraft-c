# Present the initialized source screen through the existing native GUI kernels.
if(TARGET mc_client_source_screens AND TARGET mc_client_gui_main_menu)
    add_library(mc_client_source_screen_drawing STATIC src/client/gui/source_screen_drawing.c)
    target_include_directories(mc_client_source_screen_drawing PUBLIC include src/client/gui
        PRIVATE src/client)
    target_link_libraries(mc_client_source_screen_drawing PUBLIC
        mc_client_source_screens mc_client_gui_main_menu
        PRIVATE mc_client_source_resources mc_game_settings mc_c_atomics mc_build_options)
endif()
