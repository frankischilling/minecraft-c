include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_ZOMBIE_DAMAGE_TESTS
    "Run local imported-resource loaded Zombie damage scene" OFF)

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    add_executable(mc_test_client_zombie_damage
        tests/client/gameplay/test_zombie_damage.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_client_zombie_damage PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" src src/platform/time)
    target_link_libraries(mc_test_client_zombie_damage PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_client_zombie_damage PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_client_zombie_damage POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_client_zombie_damage>")
    endif()
endif()

if(MC_ENABLE_CLIENT_ZOMBIE_DAMAGE_TESTS)
    if(NOT WIN32 OR NOT BUILD_TESTING OR NOT TARGET mc_test_client_zombie_damage OR
       NOT MC_ENABLE_CLIENT_DISPLAY_TESTS OR NOT MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        message(FATAL_ERROR "Zombie damage scene requires native Windows client/display tests and imported resources")
    endif()
    add_test(NAME client.zombie_damage COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_zombie_damage.py"
        --executable "$<TARGET_FILE:mc_test_client_zombie_damage>"
        --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/zombie-damage-scenes")
    set_tests_properties(client.zombie_damage PROPERTIES
        LABELS "local;integration;client;input;graphics;entity;ownership;storage"
        RESOURCE_LOCK mc_desktop_focus TIMEOUT 300)
endif()

if(BUILD_TESTING)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_zombie_fixture_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_zombie_fixture_options -O)
        endif()
        add_test(NAME "tools.zombie_damage_fixture_${mode}" COMMAND Python3::Interpreter
            ${_mc_zombie_fixture_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_zombie_damage.py" --self-test)
        set_tests_properties("tools.zombie_damage_fixture_${mode}" PROPERTIES
            LABELS "public;unit;tools;client;storage" TIMEOUT 90)
    endforeach()
endif()
