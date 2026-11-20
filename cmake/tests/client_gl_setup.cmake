# Native GL startup controls; original parity is a separate qualification.
if(BUILD_TESTING AND MC_BUILD_CLIENT AND TARGET mc_client_display AND
   TARGET mc_client_sdk_runtime AND TARGET mc_client_event_transport)
    # Include the real Display TU exactly once; do not link mc_client_display
    # again. Existing SDK objects resolve to that same body in this executable.
    add_executable(mc_test_source_gl_setup tests/client/display/test_source_gl_setup.c)
    target_include_directories(mc_test_source_gl_setup PRIVATE include src src/client/display)
    target_link_libraries(mc_test_source_gl_setup PRIVATE
        mc_core mc_image SDL3::SDL3-static mc_client_event_transport
        mc_client_sdk_runtime mc_build_options)
    if(UNIX)
        target_link_libraries(mc_test_source_gl_setup PRIVATE m)
    endif()
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_gl_setup PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(MSVC)
        target_link_options(mc_test_source_gl_setup PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_source_gl_setup.map")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(mc_test_source_gl_setup PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_source_gl_setup.map")
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.source_gl_startup_device COMMAND mc_test_source_gl_setup)
        set_tests_properties(client.source_gl_startup_device PROPERTIES
            LABELS "integration;client;graphics;ownership" TIMEOUT 180)
    endif()
endif()
