include_guard(GLOBAL)
if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE
        src/client/world/entity_renderer/rain_tick.c
        src/client/world/entity_renderer/precipitation.c)
    if(BUILD_TESTING)
        add_executable(mc_test_precipitation tests/client/world/test_precipitation.c)
        target_include_directories(mc_test_precipitation PRIVATE src src/platform/time)
        target_link_libraries(mc_test_precipitation PRIVATE
            mc_client_entity_renderer mc_client_singleplayer_controller mc_simulation
            mc_resources mc_math_classes mc_c_atomics mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_precipitation PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.precipitation COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_precipitation.py"
                --executable "$<TARGET_FILE:mc_test_precipitation>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/precipitation")
            set_tests_properties(client.precipitation PROPERTIES
                LABELS "public;integration;client;weather;graphics;ownership" TIMEOUT 120)
        endif()
        add_executable(mc_test_renderer_rain tests/client/world/test_renderer_rain.c)
        target_include_directories(mc_test_renderer_rain PRIVATE src src/platform/time)
        target_link_libraries(mc_test_renderer_rain PRIVATE
            mc_client_entity_renderer mc_game_settings mc_simulation mc_build_options)
        add_test(NAME client.renderer_rain COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/renderer_rain/run_native.py"
            --executable "$<TARGET_FILE:mc_test_renderer_rain>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/renderer-rain")
        set_tests_properties(client.renderer_rain PROPERTIES
            LABELS "public;unit;client;weather;ownership;replay" TIMEOUT 60)
    endif()
endif()
