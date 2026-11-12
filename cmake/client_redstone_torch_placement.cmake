include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_REDSTONE_TORCH_PLACEMENT_TESTS
    "Run native Redstone Torch placement, natural off transition and save cycle" OFF)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.redstone_torch_gui_controls COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_redstone_torch_controls.py")
    set_tests_properties(tools.redstone_torch_gui_controls PROPERTIES
        LABELS "public;unit;tools;client;storage" TIMEOUT 60)
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    add_executable(mc_test_redstone_torch_placement
        tests/client/gameplay/test_redstone_torch_placement.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_redstone_torch_placement PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" src src/platform/time)
    target_link_libraries(mc_test_redstone_torch_placement PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_redstone_torch_placement PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_redstone_torch_placement POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_redstone_torch_placement>")
    endif()
    if(WIN32 AND Python3_Interpreter_FOUND AND MC_ENABLE_CLIENT_REDSTONE_TORCH_PLACEMENT_TESTS
       AND MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.redstone_torch_placement_loop COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_redstone_torch_loop.py"
            --executable "$<TARGET_FILE:mc_test_redstone_torch_placement>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/redstone-torch-placement-captures")
        set_tests_properties(client.redstone_torch_placement_loop PROPERTIES
            LABELS "local;integration;client;input;graphics;storage;ownership"
            TIMEOUT 300 RUN_SERIAL TRUE RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
