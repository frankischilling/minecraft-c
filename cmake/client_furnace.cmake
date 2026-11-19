include_guard(GLOBAL)

target_sources(mc_inventory PRIVATE src/inventory/furnace.c src/inventory/furnace_container.c)
target_sources(mc_simulation PRIVATE src/inventory/furnace_tile.c
    src/world/runtime/mutation/furnace/furnace.c
    src/world/runtime/tile_entities/furnace_tick.c)

if(TARGET mc_client_source_screens)
    target_sources(mc_client_source_screens PRIVATE src/client/gui/source_furnace_screen.c)
endif()
if(TARGET mc_client_source_screen_drawing)
    target_sources(mc_client_source_screen_drawing PRIVATE src/client/gui/source_furnace_drawing.c)
endif()

if(BUILD_TESTING)
    add_executable(mc_test_furnace_owned_lazy
        tests/world/runtime/world_access/test_furnace_owned_lazy.c)
    target_include_directories(mc_test_furnace_owned_lazy PRIVATE src src/platform/time)
    target_link_libraries(mc_test_furnace_owned_lazy PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    foreach(_furnace_lazy_mode IN ITEMS direct-success direct-failure ray-success ray-failure)
        add_test(NAME world.furnace_owned_lazy_${_furnace_lazy_mode} COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_furnace_owned_lazy.py"
            --executable "$<TARGET_FILE:mc_test_furnace_owned_lazy>" --mode "${_furnace_lazy_mode}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/furnace-owned-lazy-controls")
        set_tests_properties(world.furnace_owned_lazy_${_furnace_lazy_mode} PROPERTIES
            LABELS "public;integration;world;inventory;ownership" TIMEOUT 240)
    endforeach()
    unset(_furnace_lazy_mode)
    add_test(NAME tools.furnace_owned_lazy_controls COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_furnace_owned_lazy.py" --controls)
    set_tests_properties(tools.furnace_owned_lazy_controls PROPERTIES
        LABELS "public;unit;tools;inventory;ownership" TIMEOUT 90)
    add_executable(mc_test_furnace_core tests/world/furnace/test_core.c)
    add_executable(mc_test_furnace_inventory tests/inventory/furnace.c)
    add_executable(mc_test_furnace_tick tests/world/runtime/tile_entities/test_furnace_tick.c)
    add_executable(mc_test_furnace_lifecycle tests/world/furnace/test_lifecycle.c)
    foreach(_furnace_mode IN ITEMS core inventory tick lifecycle)
        target_include_directories(mc_test_furnace_${_furnace_mode} PRIVATE src src/platform/time)
        target_link_libraries(mc_test_furnace_${_furnace_mode} PRIVATE
            mc_simulation mc_resources mc_c_atomics mc_build_options)
        add_test(NAME world.furnace_${_furnace_mode} COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_furnace.py"
            --executable "$<TARGET_FILE:mc_test_furnace_${_furnace_mode}>"
            --mode "${_furnace_mode}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/furnace-${_furnace_mode}-controls")
        set_tests_properties(world.furnace_${_furnace_mode} PROPERTIES
            LABELS "public;integration;world;inventory;ownership" TIMEOUT 240)
    endforeach()
    unset(_furnace_mode)
    add_executable(mc_test_furnace_output tests/entity/player/test_furnace_output.c)
    target_include_directories(mc_test_furnace_output PRIVATE src src/platform/time)
    target_link_libraries(mc_test_furnace_output PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_furnace_output COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_furnace.py"
        --executable "$<TARGET_FILE:mc_test_furnace_output>" --mode output
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/furnace-output-controls")
    set_tests_properties(entity.player_furnace_output PROPERTIES
        LABELS "public;integration;entity;inventory;statistics;ownership" TIMEOUT 240)
    add_test(NAME tools.furnace_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/inventory/furnace/test_packet.py")
    add_test(NAME tools.furnace_lifecycle_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/furnace_lifecycle/test_packet.py")
    add_test(NAME tools.furnace_save_readback_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/furnace_save_readback/test_packet.py")
    add_test(NAME tools.furnace_gui_controls COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/test_furnace_gui.py" --controls)
    set_tests_properties(tools.furnace_packet tools.furnace_lifecycle_packet
        tools.furnace_save_readback_packet tools.furnace_gui_controls PROPERTIES
        LABELS "public;unit;tools;inventory;reference" TIMEOUT 90)
endif()

if(BUILD_TESTING AND TARGET mc_client_source_screens)
    add_executable(mc_test_source_furnace tests/client/test_furnace_screen.c)
    target_include_directories(mc_test_source_furnace PRIVATE src src/platform/time)
    target_link_libraries(mc_test_source_furnace PRIVATE mc_client_source_screens
        mc_client_application_owner mc_resources mc_math_classes mc_simulation mc_build_options)
    if(TARGET mc_client_source_screen_drawing)
        target_link_libraries(mc_test_source_furnace PRIVATE mc_client_source_screen_drawing)
        target_compile_definitions(mc_test_source_furnace PRIVATE MC_TEST_FURNACE_DRAWING=1)
    endif()
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_furnace PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.furnace_screen_cpu COMMAND mc_test_source_furnace)
    set_tests_properties(client.furnace_screen_cpu PROPERTIES
        LABELS "public;unit;client;inventory;ownership" TIMEOUT 90)
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    option(MC_ENABLE_CLIENT_FURNACE_TESTS "Run visible monitor-2 Furnace smelting/save controls" OFF)
    add_executable(mc_test_furnace_loop tests/client/test_furnace_screen_sdk.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_furnace_loop PRIVATE src src/platform/time)
    target_link_libraries(mc_test_furnace_loop PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_furnace_loop PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_FURNACE_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.furnace_loop COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/test_furnace_gui.py"
            --executable "$<TARGET_FILE:mc_test_furnace_loop>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/furnace-captures")
        set_tests_properties(client.furnace_loop PROPERTIES TIMEOUT 660
            LABELS "local;integration;client;input;graphics;inventory;storage;ownership"
            RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
