if(TARGET mc_client_application_owner)
    target_sources(mc_client_application_owner PRIVATE
        src/client/startup/application_queries.c)
    target_link_libraries(mc_client_application_owner PUBLIC mc_game_settings)
endif()

if(BUILD_TESTING AND TARGET mc_test_chunk_cache)
    add_executable(mc_test_render_block_colors tests/client/world/test_render_block_colors.c)
    target_include_directories(mc_test_render_block_colors PRIVATE src src/platform/time)
    target_link_libraries(mc_test_render_block_colors PRIVATE
        mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
        mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_block_colors PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_block_colors COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_block_colors/run_native.py"
        --mode color --executable "$<TARGET_FILE:mc_test_render_block_colors>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-block-colors")
    set_tests_properties(client.render_block_colors PROPERTIES
        LABELS "public;integration;client;world;ownership;replay" TIMEOUT 120)
    add_test(NAME client.tall_grass_tint COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/tall_grass_tint/run_native.py"
        --executable "$<TARGET_FILE:mc_test_render_block_colors>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/tall-grass-tint-reference")
    set_tests_properties(client.tall_grass_tint PROPERTIES
        LABELS "public;integration;client;world;ownership;replay" TIMEOUT 120)
    add_test(NAME tools.tall_grass_tint_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/tall_grass_tint" -p test_packet.py)
    set_tests_properties(tools.tall_grass_tint_packet PROPERTIES
        LABELS "public;unit;tools;client;world;reference" TIMEOUT 60)
endif()

if(BUILD_TESTING AND TARGET mc_client_application_owner)
    add_executable(mc_test_application_queries tests/client/startup/test_application_queries.c)
    target_include_directories(mc_test_application_queries PRIVATE src src/client src/platform/time)
    target_link_libraries(mc_test_application_queries PRIVATE
        mc_client_application_owner mc_game_settings mc_c_atomics mc_build_options)
    add_test(NAME client.application_queries COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_block_colors/run_native.py"
        --mode ao --executable "$<TARGET_FILE:mc_test_application_queries>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/application-queries")
    set_tests_properties(client.application_queries PROPERTIES
        LABELS "public;unit;client;ownership;replay" TIMEOUT 90)
endif()
