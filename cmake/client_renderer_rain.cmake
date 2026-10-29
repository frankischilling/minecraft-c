include_guard(GLOBAL)
if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE src/client/world/entity_renderer/rain_tick.c)
    if(BUILD_TESTING)
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
