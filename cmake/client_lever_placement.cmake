include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_LEVER_PLACEMENT_TESTS
    "Run native Lever placement, circuit switching and save/reselection" OFF)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.lever_gui_controls COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_lever_controls.py")
    set_tests_properties(tools.lever_gui_controls PROPERTIES
        LABELS "public;unit;tools;client;storage" TIMEOUT 60)
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    add_executable(mc_test_lever_placement
        tests/client/gameplay/test_lever_placement.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_lever_placement PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" src src/platform/time)
    target_link_libraries(mc_test_lever_placement PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_lever_placement PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_lever_placement POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_lever_placement>")
    endif()
    if(WIN32 AND Python3_Interpreter_FOUND AND MC_ENABLE_CLIENT_LEVER_PLACEMENT_TESTS
       AND MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.lever_placement_loop COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_lever_loop.py"
            --executable "$<TARGET_FILE:mc_test_lever_placement>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/lever-placement-captures")
        set_tests_properties(client.lever_placement_loop PROPERTIES
            LABELS "local;integration;client;input;graphics;storage;ownership"
            TIMEOUT 300 RUN_SERIAL TRUE RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
