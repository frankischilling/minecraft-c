if(TARGET mc_client_application_startup AND TARGET mc_client_source_screen_drawing AND
   TARGET mc_client_source_screen_events AND TARGET mc_sound_backend)
    add_library(mc_client_runtime STATIC
        src/client/window/frame.c src/client/window/frame_platform.c
        src/client/runtime/bootstrap.c src/client/runtime/menu_loop.c
        src/client/runtime/gameplay_input.c src/client/runtime/world_tick.c
        src/client/gui/achievement_notice_draw.c)
    target_include_directories(mc_client_runtime PUBLIC src/client/runtime
        PRIVATE src src/client src/platform/time)
    target_link_libraries(mc_client_runtime PUBLIC mc_client_application_startup
        mc_client_source_screen_drawing mc_client_source_screen_events mc_client_texture_effects
        mc_sound_backend mc_resources mc_c_atomics
        PRIVATE mc_client_canvas mc_client_event_transport mc_client_tile_renderer_cpu mc_platform_time mc_build_options)
    if(WIN32)
        target_link_libraries(mc_client_runtime PRIVATE user32 gdi32)
    else()
        target_link_libraries(mc_client_runtime PRIVATE X11::X11)
    endif()
    target_link_libraries(minecraft-client PRIVATE mc_client_runtime)
    target_compile_definitions(minecraft-client PRIVATE MC_CLIENT_SOURCE_MENU_RUNTIME=1)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET minecraft-client PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32)
        add_custom_command(TARGET minecraft-client POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:minecraft-client>")
    endif()
    if(BUILD_TESTING)
        add_executable(mc_test_standalone_canvas tests/client/window/test_standalone_canvas.c)
        target_include_directories(mc_test_standalone_canvas PRIVATE src include)
        target_link_libraries(mc_test_standalone_canvas PRIVATE mc_client_runtime mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_standalone_canvas PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_executable(mc_test_frame_canvas tests/client/window/test_frame_canvas.c)
        target_include_directories(mc_test_frame_canvas PRIVATE src include)
        target_link_libraries(mc_test_frame_canvas PRIVATE mc_client_runtime mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_frame_canvas PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.frame_canvas_owner COMMAND mc_test_frame_canvas)
            set_tests_properties(client.frame_canvas_owner PROPERTIES
                LABELS "public;integration;client;graphics;ownership" TIMEOUT 60)
        endif()
        if(WIN32)
            foreach(_mc_canvas_target IN ITEMS mc_test_frame_canvas mc_test_standalone_canvas)
                add_custom_command(TARGET ${_mc_canvas_target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>" "$<TARGET_FILE_DIR:${_mc_canvas_target}>")
            endforeach()
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
            foreach(_mc_canvas_profile IN ITEMS hidden visible visible-fp-position visible-fp-ungrab
                visible-reactivation)
                add_test(NAME "client.standalone_canvas_${_mc_canvas_profile}" COMMAND Python3::Interpreter -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_standalone_canvas.py"
                    --executable "$<TARGET_FILE:mc_test_standalone_canvas>"
                    --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/standalone-canvas-contracts"
                    --profile "${_mc_canvas_profile}")
                set_tests_properties("client.standalone_canvas_${_mc_canvas_profile}" PROPERTIES
                    LABELS "local;integration;client;graphics;input;ownership" TIMEOUT 180
                    RUN_SERIAL TRUE)
            endforeach()
            unset(_mc_canvas_profile)
        endif()
        add_executable(mc_test_menu_loop tests/client/startup/test_menu_loop.c)
        target_include_directories(mc_test_menu_loop PRIVATE src src/platform/time)
        target_link_libraries(mc_test_menu_loop PRIVATE mc_client_runtime mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_menu_loop PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32)
            add_custom_command(TARGET mc_test_menu_loop POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_menu_loop>")
            if(MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
                add_test(NAME client.source_menu_loop COMMAND Python3::Interpreter -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_menu_loop.py"
                    --executable "$<TARGET_FILE:mc_test_menu_loop>"
                    --client "$<TARGET_FILE:minecraft-client>"
                    --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}")
                set_tests_properties(client.source_menu_loop PROPERTIES
                    LABELS "public;integration;client;input;graphics;ownership" TIMEOUT 210
                    RESOURCE_LOCK mc_desktop_focus)
            endif()
        endif()
    endif()
endif()
