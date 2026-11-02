include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_WORKBENCH_TESTS "Run genuine saved-World SDK crafting acceptance" OFF)

if(TARGET mc_client_source_screens)
    target_sources(mc_client_source_screens PRIVATE src/client/gui/source_workbench_screen.c)
endif()
if(TARGET mc_client_source_screen_drawing)
    target_sources(mc_client_source_screen_drawing PRIVATE src/client/gui/source_workbench_drawing.c)
endif()

if(BUILD_TESTING)
    add_test(NAME block.pressure_plate_inventory_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/block/pressure_plate_inventory/test_packet.py")
    set_tests_properties(block.pressure_plate_inventory_packet PROPERTIES
        LABELS "public;unit;tools;block;graphics" TIMEOUT 60)
    add_test(NAME tools.player_preview_caught_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/player_preview_caught/test_packet.py")
    set_tests_properties(tools.player_preview_caught_packet PROPERTIES
        LABELS "public;unit;tools;client;graphics" TIMEOUT 60)
    add_test(NAME tools.workbench_drawing_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/workbench_drawing/test_packet.py")
    set_tests_properties(tools.workbench_drawing_packet PROPERTIES
        LABELS "public;unit;tools;client;graphics" TIMEOUT 60)
    add_test(NAME tools.workbench_interaction_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/inventory/workbench_interaction/test_packet.py")
    set_tests_properties(tools.workbench_interaction_packet PROPERTIES
        LABELS "public;unit;tools;inventory" TIMEOUT 60)
    if(TARGET mc_client_source_screens)
        add_executable(mc_test_source_workbench tests/client/gui/test_source_workbench.c)
        target_include_directories(mc_test_source_workbench PRIVATE src)
        target_link_libraries(mc_test_source_workbench PRIVATE mc_client_source_screens
            mc_client_source_screen_events mc_client_application_owner mc_resources
            mc_math_classes mc_simulation mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_source_workbench PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME client.workbench_screen_cpu COMMAND mc_test_source_workbench)
        set_tests_properties(client.workbench_screen_cpu PROPERTIES
            LABELS "public;unit;client;inventory;ownership" TIMEOUT 60)
        add_executable(mc_test_workbench_interaction_original
            tests/inventory/workbench_interaction_original.c)
        target_include_directories(mc_test_workbench_interaction_original PRIVATE
            src src/core/java src/platform/time)
        target_link_libraries(mc_test_workbench_interaction_original PRIVATE
            mc_client_source_screens mc_client_source_screen_events mc_client_application_owner
            mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_workbench_interaction_original PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME inventory.workbench_interaction_original COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/inventory/workbench_interaction/run_native.py"
            --executable "$<TARGET_FILE:mc_test_workbench_interaction_original>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/workbench-original-controls")
        set_tests_properties(inventory.workbench_interaction_original PROPERTIES
            LABELS "public;integration;inventory;client;ownership;math;replay" TIMEOUT 300)
    endif()
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    add_executable(mc_test_workbench_loop tests/client/gameplay/test_workbench.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_workbench_loop PRIVATE src src/platform/time)
    target_link_libraries(mc_test_workbench_loop PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_workbench_loop PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_workbench_loop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_workbench_loop>")
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_WORKBENCH_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.workbench_loop COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_workbench_loop.py"
            --executable "$<TARGET_FILE:mc_test_workbench_loop>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/workbench-loop-captures")
        set_tests_properties(client.workbench_loop PROPERTIES
            LABELS "local;integration;client;input;graphics;inventory;ownership"
            TIMEOUT 420 RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
