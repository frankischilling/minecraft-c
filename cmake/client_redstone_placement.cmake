include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_REDSTONE_PLACEMENT_TESTS
    "Run native unpowered Redstone Item placement and save/reselection" OFF)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.redstone_placement_controls COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_redstone_placement_loop.py"
        --self-test)
    set_tests_properties(tools.redstone_placement_controls PROPERTIES
        LABELS "public;unit;tools;client;reference" TIMEOUT 60)
endif()

# The SDK case uses the genuine World-selection runtime.
if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    add_executable(mc_test_redstone_placement
        tests/client/gameplay/redstone/test_placement.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_redstone_placement PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" src src/platform/time)
    target_link_libraries(mc_test_redstone_placement PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_redstone_placement PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_redstone_placement POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_redstone_placement>")
    endif()
    if(WIN32 AND Python3_Interpreter_FOUND AND MC_ENABLE_CLIENT_REDSTONE_PLACEMENT_TESTS
       AND MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.redstone_placement_loop COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_redstone_placement_loop.py"
            --executable "$<TARGET_FILE:mc_test_redstone_placement>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/redstone-placement-captures")
        set_tests_properties(client.redstone_placement_loop PROPERTIES
            LABELS "local;integration;client;input;graphics;storage;ownership"
            TIMEOUT 300 RUN_SERIAL TRUE RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
