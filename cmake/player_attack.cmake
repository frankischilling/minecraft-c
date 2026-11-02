include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_ATTACK_TESTS "Run native saved-World picking and Player attack" OFF)

target_sources(mc_simulation PRIVATE src/entity/player/attack.c
    src/entity/player/combat_callbacks.c)
if(TARGET mc_client_singleplayer_controller)
    target_sources(mc_client_singleplayer_controller PRIVATE src/client/controller/attack.c)
endif()

if(BUILD_TESTING)
    add_test(NAME tools.attack_autosave COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_attack_autosave.py")
    set_tests_properties(tools.attack_autosave PROPERTIES
        LABELS "public;unit;tools;client;world" TIMEOUT 60)
    add_test(NAME tools.player_attack_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_attack/test_packet.py")
    set_tests_properties(tools.player_attack_packet PROPERTIES
        LABELS "public;unit;tools;entity" TIMEOUT 60)
    add_executable(mc_test_player_attack tests/entity/player/test_attack.c)
    target_include_directories(mc_test_player_attack PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_player_attack PRIVATE mc_simulation mc_resources
        mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.player_attack COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_attack.py"
        --executable "$<TARGET_FILE:mc_test_player_attack>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-attack-controls")
    set_tests_properties(entity.player_attack PROPERTIES
        LABELS "public;integration;client;entity;world;ownership;math" TIMEOUT 240)
    if(TARGET mc_test_world_selection_loop)
        add_executable(mc_test_attack_loop tests/client/gameplay/test_attack.c
            src/world/runtime/world_tick.c
            tests/client/world/test_load_world_foreign_loading.c
            tests/client/world/test_render_global_entities.c
            tests/client/world/test_refresh_entity.c
            tests/client/world/test_renderer_focus.c)
        target_compile_definitions(mc_test_attack_loop PRIVATE MC_WORLD_TICK_AUTOSAVE_TEST_OBSERVER)
        target_include_directories(mc_test_attack_loop PRIVATE src src/platform/time)
        target_link_libraries(mc_test_attack_loop PRIVATE
            mc_client_runtime mc_client_saves mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_attack_loop PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_attack_loop POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_attack_loop>")
        endif()
        if(WIN32 AND MC_ENABLE_CLIENT_ATTACK_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
           AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
            add_test(NAME client.attack_loop COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_attack_loop.py"
                --executable "$<TARGET_FILE:mc_test_attack_loop>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/attack-loop-captures")
            set_tests_properties(client.attack_loop PROPERTIES
                LABELS "local;integration;client;input;graphics;entity;ownership"
                TIMEOUT 420 RESOURCE_LOCK mc_desktop_focus)
        endif()
    endif()
endif()
