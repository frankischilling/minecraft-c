# Canonical Display TU with actual cached ARB functions and real Ge buffers.
if(BUILD_TESTING AND MC_BUILD_CLIENT AND TARGET mc_client_render_textures AND
   TARGET mc_client_sdk_runtime AND TARGET mc_client_event_transport)
    add_executable(mc_test_occlusion_queries tests/client/display/test_occlusion_queries.c)
    target_include_directories(mc_test_occlusion_queries PRIVATE include src src/client/display)
    target_link_libraries(mc_test_occlusion_queries PRIVATE mc_client_render_textures
        mc_client_sdk_runtime mc_client_event_transport mc_build_options)
    if(UNIX)
        target_link_libraries(mc_test_occlusion_queries PRIVATE m)
    endif()
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_occlusion_queries PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.occlusion_queries_device COMMAND mc_test_occlusion_queries)
        set_tests_properties(client.occlusion_queries_device PROPERTIES
            LABELS "integration;client;graphics;ownership" TIMEOUT 120)
    endif()
endif()
