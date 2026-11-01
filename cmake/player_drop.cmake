include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_DROP_TESTS "Run the native Q/drop and save cycle" OFF)
target_sources(mc_simulation PRIVATE src/entity/player/drop.c)

if(BUILD_TESTING)
    add_test(NAME tools.client_drop_evidence COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_drop_evidence.py")
    set_tests_properties(tools.client_drop_evidence PROPERTIES LABELS "public;tools;storage")
    add_executable(mc_test_player_drop tests/entity/player/test_drop.c)
    target_include_directories(mc_test_player_drop PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_drop PRIVATE mc_simulation mc_resources
        mc_c_atomics mc_build_options)
    add_test(NAME entity.player_drop COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_drop.py"
        --executable "$<TARGET_FILE:mc_test_player_drop>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-drop-controls")
    set_tests_properties(entity.player_drop PROPERTIES
        LABELS "public;integration;client;entity;world;ownership" TIMEOUT 180)

    if(TARGET mc_test_world_selection_loop)
        add_executable(mc_test_drop_loop tests/client/gameplay/test_drop.c
            tests/client/world/test_load_world_foreign_loading.c
            tests/client/world/test_render_global_entities.c
            tests/client/world/test_refresh_entity.c
            tests/client/world/test_renderer_focus.c)
        target_include_directories(mc_test_drop_loop PRIVATE src src/platform/time)
        target_link_libraries(mc_test_drop_loop PRIVATE
            mc_client_runtime mc_client_saves mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_drop_loop PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_drop_loop POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_drop_loop>")
        endif()
        if(WIN32 AND MC_ENABLE_CLIENT_DROP_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
           AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
            add_test(NAME client.drop_loop COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_drop_loop.py"
                --executable "$<TARGET_FILE:mc_test_drop_loop>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/drop-loop-captures")
            set_tests_properties(client.drop_loop PROPERTIES
                LABELS "local;integration;client;input;graphics;storage;ownership"
                TIMEOUT 960 RUN_SERIAL TRUE)
        endif()
    endif()
endif()
