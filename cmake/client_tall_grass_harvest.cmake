include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_TALL_GRASS_HARVEST_TESTS "Run the saved-World TallGrass/Seeds SDK scene" OFF)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.tall_grass_harvest_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/block/tall_grass_harvest" -p test_packet.py)
    set_tests_properties(tools.tall_grass_harvest_packet PROPERTIES
        LABELS "public;unit;tools;reference;block;client" TIMEOUT 60)
    if(TARGET mc_test_block_harvest)
        add_test(NAME client.tall_grass_harvest_controller COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_tall_grass_harvest_controller.py"
            --executable "$<TARGET_FILE:mc_test_block_harvest>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/tall-grass-controller-controls")
        set_tests_properties(client.tall_grass_harvest_controller PROPERTIES
            LABELS "public;unit;client;world;controller;item;ownership;replay" TIMEOUT 180)
    endif()
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    add_executable(mc_test_tall_grass_harvest tests/client/gameplay/test_tall_grass_harvest.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_tall_grass_harvest PRIVATE src src/platform/time)
    target_link_libraries(mc_test_tall_grass_harvest PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_tall_grass_harvest PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_tall_grass_harvest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_tall_grass_harvest>")
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_TALL_GRASS_HARVEST_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.tall_grass_harvest COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_tall_grass_harvest.py"
            --executable "$<TARGET_FILE:mc_test_tall_grass_harvest>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/tall-grass-harvest-captures")
        set_tests_properties(client.tall_grass_harvest PROPERTIES
            LABELS "local;integration;client;input;graphics;world;ownership;inventory"
            TIMEOUT 360 RUN_SERIAL TRUE RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
