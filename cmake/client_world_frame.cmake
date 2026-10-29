include_guard(GLOBAL)
if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE src/client/world/entity_renderer/world_frame.c)
    if(BUILD_TESTING)
        add_executable(mc_test_source_world_frame tests/client/world/test_world_frame.c)
        target_include_directories(mc_test_source_world_frame PRIVATE src src/platform/time)
        target_link_libraries(mc_test_source_world_frame PRIVATE
            mc_client_entity_renderer mc_client_singleplayer_controller mc_simulation
            mc_resources mc_math_classes mc_c_atomics mc_build_options)
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.source_world_frame COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/world_frame/run_native.py"
                --executable "$<TARGET_FILE:mc_test_source_world_frame>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-world-frame")
            set_tests_properties(client.source_world_frame PROPERTIES
                LABELS "public;integration;client;world;graphics;ownership" TIMEOUT 120)
        endif()
    endif()
endif()
