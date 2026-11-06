include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_DEATH_RESPAWN_TESTS "Run the local imported-resource Death/Respawn scene" OFF)
target_sources(mc_simulation PRIVATE src/world/runtime/respawn.c)
if(BUILD_TESTING)
    add_executable(mc_test_world_respawn tests/world/runtime/test_respawn.c)
    target_include_directories(mc_test_world_respawn PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_respawn PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME world.respawn COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_respawn.py"
        --executable "$<TARGET_FILE:mc_test_world_respawn>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-respawn-contracts")
    set_tests_properties(world.respawn PROPERTIES
        LABELS "public;integration;world;entity;ownership" TIMEOUT 180)
endif()
if(MC_ENABLE_CLIENT_DEATH_RESPAWN_TESTS)
    if(NOT WIN32 OR NOT BUILD_TESTING OR NOT TARGET mc_client_runtime OR
       NOT MC_ENABLE_CLIENT_DISPLAY_TESTS OR NOT MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        message(FATAL_ERROR "Death/Respawn scene requires native Windows client/display tests and imported resources")
    endif()
    add_executable(mc_test_death_respawn tests/client/startup/test_world_selection_loop.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_compile_definitions(mc_test_death_respawn PRIVATE MC_TEST_DEATH_RESPAWN=1)
    target_include_directories(mc_test_death_respawn PRIVATE src src/platform/time)
    target_link_libraries(mc_test_death_respawn PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_death_respawn PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_custom_command(TARGET mc_test_death_respawn POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
            "$<TARGET_FILE_DIR:mc_test_death_respawn>")
    add_test(NAME client.death_respawn COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_death_respawn.py"
        --executable "$<TARGET_FILE:mc_test_death_respawn>"
        --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/death-respawn-scenes")
    set_tests_properties(client.death_respawn PROPERTIES
        LABELS "public;integration;client;input;graphics;entity;ownership"
        RESOURCE_LOCK "mc_desktop_focus" TIMEOUT 210)
endif()
