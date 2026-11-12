# Actual native GL3.3 context and canonical NW/bounds owner. No imported assets
# are required by these authored geometry and ownership controls.
if(BUILD_TESTING AND MC_BUILD_CLIENT AND TARGET mc_client_render_textures AND
   TARGET mc_client_sdk_runtime AND TARGET mc_client_event_transport)
    # The test includes the real Display TU; its actual body resolves the same
    # methods used by the linked engine/NW objects.
    add_executable(mc_test_source_lists tests/client/display/test_source_lists.c)
    target_include_directories(mc_test_source_lists PRIVATE include src src/client/display)
    target_link_libraries(mc_test_source_lists PRIVATE mc_client_render_textures
        mc_client_sdk_runtime mc_client_event_transport mc_build_options)
    if(UNIX)
        target_link_libraries(mc_test_source_lists PRIVATE m)
    endif()
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_lists PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(MSVC)
        target_link_options(mc_test_source_lists PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_source_lists.map")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(mc_test_source_lists PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_source_lists.map")
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.source_lists_device COMMAND mc_test_source_lists)
        set_tests_properties(client.source_lists_device PROPERTIES
            LABELS "integration;client;graphics;ownership" TIMEOUT 180)
    endif()
endif()
