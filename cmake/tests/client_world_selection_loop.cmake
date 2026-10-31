if(BUILD_TESTING AND TARGET mc_client_runtime)
    add_executable(mc_test_world_selection_loop tests/client/startup/test_world_selection_loop.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c)
    target_include_directories(mc_test_world_selection_loop PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_selection_loop PRIVATE mc_client_runtime
        mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_world_selection_loop PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32)
        add_custom_command(TARGET mc_test_world_selection_loop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_world_selection_loop>")
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
            add_test(NAME client.world_selection_loop COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_world_selection_loop.py"
                --executable "$<TARGET_FILE:mc_test_world_selection_loop>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/world-selection-captures")
            set_tests_properties(client.world_selection_loop PROPERTIES
                LABELS "public;integration;client;input;graphics;ownership" TIMEOUT 240)
            add_test(NAME client.world_selection_light COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_world_selection_loop.py"
                --executable "$<TARGET_FILE:mc_test_world_selection_loop>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/world-selection-light-captures"
                --mode load-light)
            set_tests_properties(client.world_selection_light PROPERTIES
                LABELS "public;integration;client;input;graphics;lighting;ownership" TIMEOUT 240)
            add_test(NAME client.world_selection_animals COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_world_selection_loop.py"
                --executable "$<TARGET_FILE:mc_test_world_selection_loop>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/world-selection-animal-captures"
                --mode animals)
            set_tests_properties(client.world_selection_animals PROPERTIES
                LABELS "public;integration;client;input;graphics;entities;ownership" TIMEOUT 900)
        endif()
    endif()
endif()
