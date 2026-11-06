option(MC_ENABLE_STARTUP_CURSOR_WINDOW_TESTS
    "Run real SDK input/vy cursor controls with privately imported icons" OFF)

if(BUILD_TESTING AND TARGET mc_client_startup_input)
    add_executable(mc_test_mouse_helper tests/client/input/test_mouse_helper.c)
    target_link_libraries(mc_test_mouse_helper PRIVATE
        mc_client_startup_input mc_resources mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_mouse_helper PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.mouse_helper_prefix COMMAND mc_test_mouse_helper)
    set_tests_properties(client.mouse_helper_prefix PROPERTIES
        LABELS "public;unit;client;constructor;ownership" TIMEOUT 120)

    # This test includes the actual cursor implementation once for pixel
    # inspection. Its link map must not extract the archive's cursor member.
    add_executable(mc_test_sdk_cursor_pixels tests/client/input/test_sdk_cursor_pixels.c)
    target_include_directories(mc_test_sdk_cursor_pixels PRIVATE src/platform/time)
    target_link_libraries(mc_test_sdk_cursor_pixels PRIVATE
        mc_client_startup_input mc_resources mc_platform_time mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_sdk_cursor_pixels PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(MSVC)
        target_link_options(mc_test_sdk_cursor_pixels PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_sdk_cursor_pixels.map")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(mc_test_sdk_cursor_pixels PRIVATE
            "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/mc_test_sdk_cursor_pixels.map")
    endif()

    if(MC_ENABLE_STARTUP_CURSOR_WINDOW_TESTS)
        if(NOT MC_ENABLE_CLIENT_DISPLAY_TESTS OR NOT MC_CLIENT_ASSET_IMPORT_DIR)
            message(FATAL_ERROR "Cursor window controls require real Display and explicit imported icons")
        endif()
        add_test(NAME client.mouse_helper_window COMMAND mc_test_mouse_helper
            --window "${MC_CLIENT_ASSET_IMPORT_DIR}")
        add_test(NAME client.sdk_cursor_pixels COMMAND mc_test_sdk_cursor_pixels
            "${MC_CLIENT_ASSET_IMPORT_DIR}")
        set_tests_properties(client.mouse_helper_window client.sdk_cursor_pixels PROPERTIES
            LABELS "integration;client;input;window;ownership" TIMEOUT 240)
    endif()
endif()
if(MC_ENABLE_STARTUP_CURSOR_WINDOW_TESTS AND
   (NOT BUILD_TESTING OR NOT TARGET mc_test_mouse_helper OR NOT TARGET mc_test_sdk_cursor_pixels))
    message(FATAL_ERROR "Cursor window controls require the actual client/testing targets")
endif()
