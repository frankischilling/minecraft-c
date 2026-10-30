include_guard(GLOBAL)
if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE
        src/client/world/entity_renderer/overlay_setup.c
        src/client/gui/gui_ingame_draw.c)
    if(BUILD_TESTING AND TARGET mc_test_application_startup)
        add_executable(mc_test_gui_ingame_draw tests/client/gui/test_gui_ingame_draw.c)
        get_target_property(_mc_hud_links mc_test_application_startup LINK_LIBRARIES)
        target_link_libraries(mc_test_gui_ingame_draw PRIVATE
            mc_client_entity_renderer mc_simulation mc_math_classes ${_mc_hud_links})
        unset(_mc_hud_links)
        target_include_directories(mc_test_gui_ingame_draw PRIVATE src src/platform/time)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_gui_ingame_draw PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_gui_ingame_draw POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_gui_ingame_draw>")
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.gui_ingame_draw COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_gui_ingame_draw.py"
                --executable "$<TARGET_FILE:mc_test_gui_ingame_draw>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/gui-ingame-draw")
            set_tests_properties(client.gui_ingame_draw PROPERTIES
                LABELS "public;integration;client;gui;graphics;ownership;replay" TIMEOUT 180)
        endif()
    endif()
endif()
