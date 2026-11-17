include_guard(GLOBAL)

if(BUILD_TESTING)
    foreach(_dispenser_mode IN ITEMS normal optimized)
        if(_dispenser_mode STREQUAL "optimized")
            set(_dispenser_options -O -B)
        else()
            set(_dispenser_options -B)
        endif()
        add_test(NAME tools.dispenser_gameplay_${_dispenser_mode} COMMAND Python3::Interpreter
            ${_dispenser_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_dispenser_gameplay.py" --self-test)
        set_tests_properties(tools.dispenser_gameplay_${_dispenser_mode} PROPERTIES
            LABELS "public;unit;tools;client;input;world" TIMEOUT 60)
    endforeach()
    unset(_dispenser_mode)
    unset(_dispenser_options)
endif()

if(BUILD_TESTING AND WIN32 AND TARGET mc_client_runtime)
    option(MC_ENABLE_CLIENT_DISPENSER_CROSS_OWNER_TESTS
        "Run hidden monitor-2 Dispenser cross-owner diagnostic cleanup" OFF)
    add_executable(mc_test_source_dispenser_cross_owner
        tests/client/gui/test_source_dispenser_cross_owner.c)
    target_include_directories(mc_test_source_dispenser_cross_owner PRIVATE src src/platform/time)
    target_link_libraries(mc_test_source_dispenser_cross_owner PRIVATE
        mc_client_runtime mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_dispenser_cross_owner PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(TARGET OpenAL)
        add_custom_command(TARGET mc_test_source_dispenser_cross_owner POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_source_dispenser_cross_owner>")
    endif()
    if(MC_ENABLE_CLIENT_DISPENSER_CROSS_OWNER_TESTS AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.dispenser_cross_owner COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_dispenser_cross_owner.py"
            --executable "$<TARGET_FILE:mc_test_source_dispenser_cross_owner>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/dispenser-cross-owner-controls" --order both)
        # Both sequential children own a 600-second capture and checked reap.
        set_tests_properties(client.dispenser_cross_owner PROPERTIES TIMEOUT 1440
            LABELS "local;integration;client;input;ownership"
            RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop AND TARGET mc_client_tile_renderer_draw)
    option(MC_ENABLE_CLIENT_DISPENSER_GAMEPLAY_TESTS
        "Run visible monitor-2 loaded Dispenser activation, transfer and save/reselection" OFF)
    add_executable(mc_test_dispenser tests/client/test_dispenser.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_dispenser PRIVATE src src/platform/time)
    target_link_libraries(mc_test_dispenser PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_dispenser PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_dispenser POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_dispenser>")
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_DISPENSER_GAMEPLAY_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.dispenser_gameplay COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_dispenser_gameplay.py"
            --executable "$<TARGET_FILE:mc_test_dispenser>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/dispenser-gameplay-captures")
        set_tests_properties(client.dispenser_gameplay PROPERTIES TIMEOUT 720
            LABELS "local;integration;client;input;graphics;world;storage;ownership"
            RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
