include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_ATTACK_TESTS "Run native saved-World picking and Player attack" OFF)

target_sources(mc_simulation PRIVATE src/entity/player/attack.c
    src/entity/player/ally_alert.c
    src/entity/player/ally_alert_occurrences.c
    src/entity/player/combat_callbacks.c src/item/combat/combat.c)
if(TARGET mc_client_singleplayer_controller)
    target_sources(mc_client_singleplayer_controller PRIVATE src/client/controller/attack.c)
endif()

if(BUILD_TESTING)
    add_executable(mc_test_item_combat tests/item/combat/test_combat.c)
    target_include_directories(mc_test_item_combat PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_item_combat PRIVATE mc_simulation mc_resources
        mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME item.combat COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/item/test_combat.py"
        --executable "$<TARGET_FILE:mc_test_item_combat>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-combat-controls")
    set_tests_properties(item.combat PROPERTIES
        LABELS "public;integration;item;entity;ownership;math" TIMEOUT 240)
    add_test(NAME tools.held_item_attack_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/held_item_attack/test_packet.py")
    set_tests_properties(tools.held_item_attack_packet PROPERTIES
        LABELS "public;unit;tools;entity;item" TIMEOUT 60)
    add_test(NAME tools.item_durability_overlay_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/item_durability_overlay/test_packet.py")
    set_tests_properties(tools.item_durability_overlay_packet PROPERTIES
        LABELS "public;unit;tools;client;graphics;item" TIMEOUT 60)
    if(TARGET mc_test_inventory_block)
        add_test(NAME client.item_durability_overlay_cpu COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_item_durability_overlay.py"
            --executable "$<TARGET_FILE:mc_test_inventory_block>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/item-durability-overlay-controls")
        set_tests_properties(client.item_durability_overlay_cpu PROPERTIES
            LABELS "public;integration;client;item;graphics;ownership;replay" TIMEOUT 180)
    endif()
    if(TARGET mc_client_entity_renderer)
        add_executable(mc_test_held_item_geometry tests/client/render/test_held_item_geometry.c)
        target_include_directories(mc_test_held_item_geometry PRIVATE src src/core/java src/platform/time)
        target_link_libraries(mc_test_held_item_geometry PRIVATE mc_client_entity_renderer
            mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
        add_test(NAME client.held_item_geometry_cpu COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_held_item_geometry.py"
            --executable "$<TARGET_FILE:mc_test_held_item_geometry>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/held-item-geometry-controls")
        set_tests_properties(client.held_item_geometry_cpu PROPERTIES
            LABELS "public;integration;client;entity;graphics;ownership;replay" TIMEOUT 240)
        add_test(NAME tools.held_item_geometry_packet COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/held_item_geometry/test_packet.py")
        set_tests_properties(tools.held_item_geometry_packet PROPERTIES
            LABELS "public;unit;tools;client;graphics" TIMEOUT 120)
    endif()
    add_executable(mc_test_inventory_source_set tests/inventory/player/test_source_set.c)
    target_include_directories(mc_test_inventory_source_set PRIVATE src)
    target_link_libraries(mc_test_inventory_source_set PRIVATE mc_inventory mc_build_options)
    add_test(NAME inventory.player_source_set COMMAND mc_test_inventory_source_set)
    set_tests_properties(inventory.player_source_set PROPERTIES
        LABELS "public;unit;inventory;ownership" TIMEOUT 30)
    add_test(NAME tools.attack_autosave COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_attack_autosave.py")
    set_tests_properties(tools.attack_autosave PROPERTIES
        LABELS "public;unit;tools;client;world" TIMEOUT 60)
    add_test(NAME tools.attack_inventory COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_attack_inventory.py")
    set_tests_properties(tools.attack_inventory PROPERTIES
        LABELS "public;unit;tools;client;world;inventory" TIMEOUT 60)
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
    add_executable(mc_test_player_held_attack tests/entity/player/test_held_attack.c)
    target_include_directories(mc_test_player_held_attack PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_player_held_attack PRIVATE mc_simulation mc_resources
        mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.player_held_attack COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_held_item_attack.py"
        --executable "$<TARGET_FILE:mc_test_player_held_attack>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/held-item-attack-controls")
    set_tests_properties(entity.player_held_attack PROPERTIES
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
            foreach(profile IN ITEMS held-dirt iron-sword-break)
                add_test(NAME client.attack_${profile}_loop COMMAND Python3::Interpreter -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_attack_loop.py"
                    --executable "$<TARGET_FILE:mc_test_attack_loop>"
                    --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                    --artifacts "${CMAKE_CURRENT_BINARY_DIR}/attack-loop-captures"
                    --profile ${profile})
                set_tests_properties(client.attack_${profile}_loop PROPERTIES
                    LABELS "local;integration;client;input;graphics;entity;ownership"
                    TIMEOUT 420 RESOURCE_LOCK mc_desktop_focus)
            endforeach()
        endif()
    endif()
endif()
