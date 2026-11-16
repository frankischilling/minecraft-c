if(BUILD_TESTING)
    add_test(NAME tools.chest_container_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/inventory/chest_container/test_packet.py")
    set_tests_properties(tools.chest_container_packet PROPERTIES
        LABELS "public;unit;tools;inventory;reference" TIMEOUT 60)
    add_test(NAME tools.chest_gui_controls COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_chest_controls.py")
    set_tests_properties(tools.chest_gui_controls PROPERTIES
        LABELS "public;unit;tools;client;inventory;storage" TIMEOUT 60)
endif()

if(BUILD_TESTING AND TARGET mc_client_render_global AND TARGET mc_client_tile_renderer_cpu)
    add_executable(mc_test_chunk_tile_eligibility tests/client/world/test_chunk_tile_eligibility.c)
    target_include_directories(mc_test_chunk_tile_eligibility PRIVATE src)
    target_link_libraries(mc_test_chunk_tile_eligibility PRIVATE mc_client_render_global
        mc_client_tile_renderer_cpu mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_chunk_tile_eligibility PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.chunk_tile_eligibility COMMAND mc_test_chunk_tile_eligibility)
    set_tests_properties(client.chunk_tile_eligibility PROPERTIES
        LABELS "public;unit;client;rendering;ownership" TIMEOUT 60)
endif()

if(BUILD_TESTING AND TARGET mc_client_render_global AND TARGET mc_client_entity_renderer)
    add_executable(mc_test_chest_texture tests/client/render/test_chest_texture.c)
    target_include_directories(mc_test_chest_texture PRIVATE src src/platform/time)
    target_link_libraries(mc_test_chest_texture PRIVATE mc_client_entity_renderer
        mc_client_render_global mc_client_singleplayer_controller mc_simulation mc_resources
        mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_chest_texture PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.chest_texture COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/render/run_chest_texture.py"
        --executable "$<TARGET_FILE:mc_test_chest_texture>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/chest-texture-controls")
    set_tests_properties(client.chest_texture PROPERTIES
        LABELS "public;unit;client;rendering;world;ownership" TIMEOUT 150)
    add_executable(mc_test_player_chest_eligibility tests/entity/player/test_chest_eligibility.c)
    target_include_directories(mc_test_player_chest_eligibility PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_chest_eligibility PRIVATE mc_client_entity_renderer
        mc_client_render_global mc_client_singleplayer_controller mc_simulation mc_resources
        mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_player_chest_eligibility PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME entity.player_chest_eligibility COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/entity/player/run_chest_eligibility.py"
        --executable "$<TARGET_FILE:mc_test_player_chest_eligibility>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-chest-eligibility-controls")
    set_tests_properties(entity.player_chest_eligibility PROPERTIES
        LABELS "public;unit;entity;client;inventory;world;ownership" TIMEOUT 150)
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    option(MC_ENABLE_CLIENT_CHEST_TESTS "Run visible monitor-2 Chest transfer/save controls" OFF)
    add_executable(mc_test_chest_loop tests/client/gameplay/test_chest.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_chest_loop PRIVATE src src/platform/time)
    target_link_libraries(mc_test_chest_loop PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_chest_loop PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_chest_loop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_chest_loop>")
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_CHEST_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        foreach(_chest IN ITEMS single double dense blocked shutdown)
            add_test(NAME "client.chest_${_chest}" COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_chest_loop.py"
                --executable "$<TARGET_FILE:mc_test_chest_loop>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/chest-captures" --profile "${_chest}")
            set_tests_properties("client.chest_${_chest}" PROPERTIES TIMEOUT 660
                LABELS "local;integration;client;input;graphics;inventory;storage;ownership"
                RESOURCE_LOCK mc_desktop_focus)
        endforeach()
        unset(_chest)
    endif()
endif()
