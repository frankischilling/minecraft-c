include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_WORKBENCH_TESTS "Run genuine saved-World SDK crafting acceptance" OFF)
option(MC_ENABLE_CLIENT_WORKBENCH_AUTO_CLOSE_TESTS
    "Run saved-World SDK and authored automatic Workbench close acceptance" OFF)
option(MC_ENABLE_CLIENT_WORKBENCH_OPEN_SHUTDOWN_TESTS
    "Run saved-World SDK open Workbench native shutdown acceptance" OFF)

if(TARGET mc_client_source_screens)
    target_sources(mc_client_source_screens PRIVATE src/client/gui/source_workbench_screen.c)
endif()
if(TARGET mc_client_source_screen_drawing)
    target_sources(mc_client_source_screen_drawing PRIVATE src/client/gui/source_workbench_drawing.c)
endif()

if(BUILD_TESTING)
    add_test(NAME tools.workbench_open_shutdown_packet COMMAND Python3::Interpreter -B
        -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/workbench_open_shutdown"
        -p test_packet.py)
    set_tests_properties(tools.workbench_open_shutdown_packet PROPERTIES
        LABELS "public;unit;tools;inventory;client;ownership" TIMEOUT 60)
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
    add_test(NAME tools.workbench_auto_close_packet COMMAND Python3::Interpreter -B
        -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/inventory/workbench_auto_close"
        -p test_packet.py)
    set_tests_properties(tools.workbench_auto_close_packet PROPERTIES
        LABELS "public;unit;tools;inventory;client" TIMEOUT 60)
    if(TARGET mc_client_source_screens)
        add_executable(mc_test_player_terminal_inventory
            tests/entity/player/test_terminal_inventory.c)
        target_include_directories(mc_test_player_terminal_inventory PRIVATE
            src src/core/java src/platform/time)
        target_link_libraries(mc_test_player_terminal_inventory PRIVATE
            mc_client_source_screens mc_client_source_screen_events mc_client_application_owner
            mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_player_terminal_inventory PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME entity.player_terminal_inventory COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/inventory/test_terminal_inventory.py"
            --executable "$<TARGET_FILE:mc_test_player_terminal_inventory>" --scope player
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-terminal-contracts")
        set_tests_properties(entity.player_terminal_inventory PROPERTIES
            LABELS "public;integration;entity;client;inventory;ownership" TIMEOUT 90)
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
    add_executable(mc_test_workbench_open_shutdown
        tests/client/gameplay/test_workbench_open_shutdown.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_workbench_open_shutdown PRIVATE src src/platform/time)
    target_link_libraries(mc_test_workbench_open_shutdown PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_workbench_open_shutdown PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_workbench_open_shutdown POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_workbench_open_shutdown>")
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_WORKBENCH_OPEN_SHUTDOWN_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        foreach(_mc_shutdown_profile IN ITEMS clean container player world)
            add_test(NAME "client.workbench_open_shutdown_${_mc_shutdown_profile}"
                COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_workbench_open_shutdown.py"
                --executable "$<TARGET_FILE:mc_test_workbench_open_shutdown>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/workbench-open-shutdown-captures"
                --profile "${_mc_shutdown_profile}")
            set_tests_properties("client.workbench_open_shutdown_${_mc_shutdown_profile}" PROPERTIES
                LABELS "local;integration;client;input;graphics;inventory;ownership"
                TIMEOUT 420 RESOURCE_LOCK mc_desktop_focus)
        endforeach()
        unset(_mc_shutdown_profile)
    endif()
    add_executable(mc_test_workbench_auto_close_failures
        tests/client/gameplay/test_workbench_auto_close_failures.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_workbench_auto_close_failures PRIVATE src src/platform/time)
    target_link_libraries(mc_test_workbench_auto_close_failures PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_workbench_auto_close_failures PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_workbench_auto_close_failures POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_workbench_auto_close_failures>")
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_WORKBENCH_AUTO_CLOSE_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.workbench_auto_close_failures COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_workbench_auto_close_failures.py"
            --executable "$<TARGET_FILE:mc_test_workbench_auto_close_failures>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/workbench-auto-close-failure-captures")
        set_tests_properties(client.workbench_auto_close_failures PROPERTIES
            LABELS "local;integration;client;input;graphics;inventory;ownership"
            TIMEOUT 600 RESOURCE_LOCK mc_desktop_focus)
    endif()
    add_executable(mc_test_workbench_auto_close_loop
        tests/client/gameplay/test_workbench_auto_close.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_workbench_auto_close_loop PRIVATE src src/platform/time)
    target_link_libraries(mc_test_workbench_auto_close_loop PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_workbench_auto_close_loop PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_workbench_auto_close_loop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_workbench_auto_close_loop>")
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_WORKBENCH_AUTO_CLOSE_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        foreach(_mc_auto_close_mode IN ITEMS removed distance)
            add_test(NAME "client.workbench_auto_close_${_mc_auto_close_mode}"
                COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_workbench_auto_close_loop.py"
                --executable "$<TARGET_FILE:mc_test_workbench_auto_close_loop>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/workbench-auto-close-captures"
                --mode "${_mc_auto_close_mode}")
            set_tests_properties("client.workbench_auto_close_${_mc_auto_close_mode}" PROPERTIES
                LABELS "local;integration;client;input;graphics;inventory;ownership"
                TIMEOUT 420 RESOURCE_LOCK mc_desktop_focus)
        endforeach()
        unset(_mc_auto_close_mode)
    endif()
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
