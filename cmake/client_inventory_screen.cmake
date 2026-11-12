include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_INVENTORY_TESTS "Run genuine saved-World SDK Inventory acceptance" OFF)

target_sources(mc_registry PRIVATE src/item/render/name.c)
target_sources(mc_inventory PRIVATE
    src/inventory/container/interaction.c
    src/inventory/container/interaction_core.c
    src/inventory/container/interaction_click.c
    src/inventory/container/interaction_errors.c
    src/inventory/container/slot_source.c)
target_sources(mc_simulation PRIVATE src/entity/player/container_interaction.c)

target_sources(mc_client_screen_classes PRIVATE src/client/gui/source_inventory_classes.c)
target_include_directories(mc_client_screen_classes PRIVATE src)
target_link_libraries(mc_client_screen_classes PRIVATE mc_client_notice)

if(TARGET mc_client_source_screens)
    target_sources(mc_client_source_screens PRIVATE src/client/gui/source_inventory_screen.c)
    target_include_directories(mc_client_source_screens PRIVATE src)
    target_link_libraries(mc_client_source_screens PRIVATE
        mc_client_notice mc_simulation mc_math_classes mc_client_singleplayer_controller)
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    add_executable(mc_test_inventory_loop tests/client/gameplay/test_inventory.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_inventory_loop PRIVATE src src/platform/time)
    target_link_libraries(mc_test_inventory_loop PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_inventory_loop PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_inventory_loop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_inventory_loop>")
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_INVENTORY_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.inventory_loop COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_inventory_loop.py"
            --executable "$<TARGET_FILE:mc_test_inventory_loop>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/inventory-loop-captures")
        set_tests_properties(client.inventory_loop PROPERTIES
            LABELS "local;integration;client;input;graphics;inventory;ownership"
            TIMEOUT 420 RESOURCE_LOCK mc_desktop_focus)
        add_test(NAME client.inventory_helmet_loop COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_inventory_loop.py"
            --executable "$<TARGET_FILE:mc_test_inventory_loop>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/inventory-loop-captures" --helmet)
        set_tests_properties(client.inventory_helmet_loop PROPERTIES
            LABELS "local;integration;client;input;graphics;inventory;ownership"
            TIMEOUT 420 RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(mc_test_item_name tests/item/render/test_name.c)
    target_include_directories(mc_test_item_name PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_item_name PRIVATE mc_simulation mc_resources
        mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME item.name COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/inventory/test_interaction.py"
        --executable "$<TARGET_FILE:mc_test_item_name>" --kind item-name
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/inventory-controls")
    add_executable(mc_test_container_interaction tests/inventory/container_interaction.c)
    target_include_directories(mc_test_container_interaction PRIVATE src)
    target_link_libraries(mc_test_container_interaction PRIVATE mc_inventory mc_resources mc_build_options)
    add_test(NAME inventory.container_interaction COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/inventory/test_interaction.py"
        --executable "$<TARGET_FILE:mc_test_container_interaction>" --kind container
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/inventory-controls")
    set_tests_properties(item.name inventory.container_interaction PROPERTIES
        LABELS "public;integration;inventory;item;ownership" TIMEOUT 240)
    add_executable(mc_test_container_interaction_original tests/inventory/container_interaction_original.c)
    target_include_directories(mc_test_container_interaction_original PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_container_interaction_original PRIVATE mc_simulation mc_resources
        mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME inventory.container_interaction_original COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/inventory/container_interaction/run_native.py"
        --executable "$<TARGET_FILE:mc_test_container_interaction_original>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/container-original-controls")
    set_tests_properties(inventory.container_interaction_original PROPERTIES
        LABELS "public;integration;inventory;entity;ownership;math;replay" TIMEOUT 240)
    add_test(NAME tools.container_interaction_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/inventory/container_interaction/test_packet.py")
    set_tests_properties(tools.container_interaction_packet PROPERTIES
        LABELS "public;unit;tools;inventory" TIMEOUT 60)
    if(TARGET mc_client_source_screens)
        add_executable(mc_test_source_inventory tests/client/gui/test_source_inventory.c)
        target_include_directories(mc_test_source_inventory PRIVATE src src/platform/time)
        target_link_libraries(mc_test_source_inventory PRIVATE mc_client_source_screens
            mc_client_source_screen_events mc_client_application_owner mc_resources
            mc_math_classes mc_simulation mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_source_inventory PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME client.inventory_screen_cpu COMMAND mc_test_source_inventory)
        set_tests_properties(client.inventory_screen_cpu PROPERTIES
            LABELS "public;unit;client;inventory;ownership" TIMEOUT 60)
    endif()
    if(TARGET mc_client_entity_renderer)
        add_executable(mc_test_player_draw_prefix tests/client/render/player_draw_controls.c)
        target_include_directories(mc_test_player_draw_prefix PRIVATE src src/core/java src/platform/time)
        target_link_libraries(mc_test_player_draw_prefix PRIVATE mc_client_entity_renderer
            mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
        add_test(NAME client.player_draw_prefix COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_player_draw_prefix.py"
            --executable "$<TARGET_FILE:mc_test_player_draw_prefix>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-draw-prefix-controls")
        set_tests_properties(client.player_draw_prefix PROPERTIES
            LABELS "public;integration;client;entity;graphics;ownership" TIMEOUT 240)
    endif()
endif()

if(TARGET mc_client_source_screen_drawing)
    target_sources(mc_client_source_screen_drawing PRIVATE
        src/client/gui/source_inventory_drawing.c
        src/client/gui/source_inventory_drawing_quads.c
        src/client/gui/source_inventory_quad_kernels.c
        src/client/gui/source_inventory_drawing_preview.c)
    target_include_directories(mc_client_source_screen_drawing PRIVATE src)
    target_link_libraries(mc_client_source_screen_drawing PRIVATE
        mc_client_entity_renderer mc_client_render_manager_cpu mc_math_classes mc_simulation)
    if(BUILD_TESTING)
        add_executable(mc_test_inventory_quads tests/client/gui/test_inventory_quads.c)
        target_include_directories(mc_test_inventory_quads PRIVATE src)
        target_link_libraries(mc_test_inventory_quads PRIVATE mc_client_source_screen_drawing mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_inventory_quads PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME client.inventory_quads_cpu COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/inventory_drawing/run_native.py"
            --executable "$<TARGET_FILE:mc_test_inventory_quads>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/inventory-quad-controls")
        set_tests_properties(client.inventory_quads_cpu PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 240)
        add_test(NAME tools.inventory_drawing_packet COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/inventory_drawing/test_packet.py")
        set_tests_properties(tools.inventory_drawing_packet PROPERTIES
            LABELS "public;unit;tools;client;graphics" TIMEOUT 60)
    endif()
endif()

if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE src/client/render/living_draw/player.c
        src/client/gui/gui_ingame_pumpkin.c)
    if(BUILD_TESTING)
        add_executable(mc_test_pumpkin_geometry tests/client/gui/test_pumpkin_geometry.c)
        target_include_directories(mc_test_pumpkin_geometry PRIVATE src)
        target_link_libraries(mc_test_pumpkin_geometry PRIVATE mc_client_entity_renderer mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_pumpkin_geometry PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME client.pumpkin_geometry_cpu COMMAND mc_test_pumpkin_geometry)
        set_tests_properties(client.pumpkin_geometry_cpu PROPERTIES
            LABELS "public;unit;client;graphics;ownership" TIMEOUT 60)
    endif()
endif()
