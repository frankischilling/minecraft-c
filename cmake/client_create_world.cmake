include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_CREATE_WORLD_TESTS "Run the native Create World/save/reselection scenario" OFF)
if(TARGET mc_client_source_screens)
    target_sources(mc_client_source_screens PRIVATE
        src/client/gui/source_text_field.c src/client/gui/source_create_world.c)
    target_link_libraries(mc_client_source_screens PRIVATE mc_client_display mc_client_saves)
    target_sources(mc_client_source_screen_events PRIVATE src/client/gui/source_clipboard.c)
    target_sources(mc_client_source_screen_drawing PRIVATE src/client/gui/source_create_world_drawing.c)
    if(BUILD_TESTING)
        add_executable(mc_test_text_field tests/client/gui/test_text_field.c)
        target_include_directories(mc_test_text_field PRIVATE src src/platform/time)
        target_link_libraries(mc_test_text_field PRIVATE mc_client_source_screens
            mc_client_application_owner mc_resources mc_c_atomics mc_build_options)
        add_test(NAME client.text_field COMMAND mc_test_text_field)
        set_tests_properties(client.text_field PROPERTIES LABELS "public;unit;client;ownership" TIMEOUT 180)
        add_executable(mc_test_create_world tests/client/gui/test_create_world.c)
        target_include_directories(mc_test_create_world PRIVATE src src/platform/time)
        target_link_libraries(mc_test_create_world PRIVATE
            mc_client_runtime mc_client_saves mc_build_options)
        add_test(NAME client.create_world COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_create_world.py"
            --executable "$<TARGET_FILE:mc_test_create_world>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/create-world-controls")
        set_tests_properties(client.create_world PROPERTIES LABELS "public;unit;client;ownership;storage"
            TIMEOUT 180)
        foreach(_target IN ITEMS mc_test_text_field mc_test_create_world)
            if(CMAKE_CXX_COMPILER_LOADED)
                set_property(TARGET ${_target} PROPERTY LINKER_LANGUAGE CXX)
            endif()
            if(WIN32 AND TARGET OpenAL)
                add_custom_command(TARGET ${_target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                        "$<TARGET_FILE_DIR:${_target}>")
            endif()
        endforeach()
        unset(_target)
        if(TARGET mc_test_world_selection_loop)
            add_executable(mc_test_create_world_loop tests/client/gameplay/test_create_world.c
                tests/client/world/test_load_world_foreign_loading.c
                tests/client/world/test_render_global_entities.c
                tests/client/world/test_refresh_entity.c
                tests/client/world/test_renderer_focus.c)
            target_include_directories(mc_test_create_world_loop PRIVATE src src/platform/time)
            target_link_libraries(mc_test_create_world_loop PRIVATE
                mc_client_runtime mc_client_saves mc_build_options)
            if(CMAKE_CXX_COMPILER_LOADED)
                set_property(TARGET mc_test_create_world_loop PROPERTY LINKER_LANGUAGE CXX)
            endif()
            if(WIN32 AND TARGET OpenAL)
                add_custom_command(TARGET mc_test_create_world_loop POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                        "$<TARGET_FILE_DIR:mc_test_create_world_loop>")
            endif()
            if(WIN32 AND MC_ENABLE_CLIENT_CREATE_WORLD_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
               AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
                add_test(NAME client.create_world_loop COMMAND Python3::Interpreter -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_create_world_loop.py"
                    --executable "$<TARGET_FILE:mc_test_create_world_loop>"
                    --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                    --artifacts "${CMAKE_CURRENT_BINARY_DIR}/create-world-loop-captures")
                set_tests_properties(client.create_world_loop PROPERTIES
                    LABELS "local;integration;client;input;graphics;generation;storage;ownership"
                    TIMEOUT 1200 RUN_SERIAL TRUE)
            endif()
        endif()
    endif()
endif()
