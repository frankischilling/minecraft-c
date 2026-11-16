include_guard(GLOBAL)

if(TARGET mc_client_tile_renderer_cpu AND TARGET mc_client_entity_renderer)
    add_library(mc_client_tile_renderer_draw STATIC
        src/client/render/tile_entity_renderer/draw.c
        src/client/render/tile_entity_renderer/spawner.c)
    target_include_directories(mc_client_tile_renderer_draw PUBLIC include src src/platform/time)
    target_link_libraries(mc_client_tile_renderer_draw PUBLIC mc_client_tile_renderer_cpu
        mc_client_entity_renderer mc_math_classes PRIVATE mc_build_options)
    if(TARGET mc_client_render_global)
        target_link_libraries(mc_client_render_global PUBLIC mc_client_tile_renderer_draw)
    endif()
    if(BUILD_TESTING)
        add_executable(mc_test_tile_entity_draw
            tests/client/render/tile_entity_renderer/test_draw.c)
        target_include_directories(mc_test_tile_entity_draw PRIVATE src src/platform/time)
        target_link_libraries(mc_test_tile_entity_draw PRIVATE mc_client_tile_renderer_draw mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_tile_entity_draw PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME client.tile_entity_draw COMMAND mc_test_tile_entity_draw)
        set_tests_properties(client.tile_entity_draw PROPERTIES
            LABELS "public;unit;client;world;rendering;ownership" TIMEOUT 90)
    endif()
endif()

if(BUILD_TESTING)
    add_test(NAME tools.spawner_frame_controls COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/test_spawner_frame.py" --controls)
    set_tests_properties(tools.spawner_frame_controls PROPERTIES
        LABELS "public;unit;tools;client;world;rendering" TIMEOUT 90)
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    option(MC_ENABLE_CLIENT_SPAWNER_TESTS "Run visible monitor-2 saved MobSpawner frames" OFF)
    add_executable(mc_test_spawner_frame tests/client/test_spawner_frame_sdk.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_spawner_frame PRIVATE src src/platform/time)
    target_link_libraries(mc_test_spawner_frame PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_spawner_frame PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_spawner_frame POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_spawner_frame>")
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_SPAWNER_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.spawner_frame COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/test_spawner_frame.py"
            --executable "$<TARGET_FILE:mc_test_spawner_frame>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/spawner-frame-captures")
        set_tests_properties(client.spawner_frame PROPERTIES TIMEOUT 420
            LABELS "local;integration;client;graphics;world;ownership"
            RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
