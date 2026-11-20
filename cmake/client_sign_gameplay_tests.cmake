# Finite Sign gameplay integration and reader controls.
include_guard(GLOBAL)

if(BUILD_TESTING)
    foreach(_sign_gameplay_mode IN ITEMS normal optimized)
        if(_sign_gameplay_mode STREQUAL "optimized")
            set(_sign_gameplay_options -O -B)
        else()
            set(_sign_gameplay_options -B)
        endif()
        add_test(NAME tools.sign_gameplay_${_sign_gameplay_mode} COMMAND Python3::Interpreter
            ${_sign_gameplay_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_sign_gameplay.py" --controls)
        set_tests_properties(tools.sign_gameplay_${_sign_gameplay_mode} PROPERTIES
            LABELS "public;unit;tools;client;input;world" TIMEOUT 60)
    endforeach()
    unset(_sign_gameplay_mode)
    unset(_sign_gameplay_options)
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop AND TARGET mc_client_tile_renderer_draw)
    option(MC_ENABLE_CLIENT_SIGN_GAMEPLAY_TESTS
        "Run visible monitor-2 finite Sign VC/editor/Done/save/reselection" OFF)
    add_executable(mc_test_sign_gameplay tests/client/gameplay/test_sign_editor.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_sign_gameplay PRIVATE src src/platform/time)
    target_link_libraries(mc_test_sign_gameplay PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_sign_gameplay PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_SIGN_GAMEPLAY_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.sign_gameplay COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_sign_gameplay.py"
            --executable "$<TARGET_FILE:mc_test_sign_gameplay>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/sign-gameplay-captures")
        set_tests_properties(client.sign_gameplay PROPERTIES TIMEOUT 720
            LABELS "local;integration;client;input;graphics;world;storage;ownership"
            RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
