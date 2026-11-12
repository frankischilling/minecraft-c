# Add after the existing normal startup test and its platform/dependency links.
if(BUILD_TESTING AND TARGET mc_test_application_startup AND TARGET mc_client_sdk_runtime)
    add_executable(mc_test_application_startup_boundary
        tests/client/startup/test_application_startup.c
        tests/client/startup/controller_boundary_observer.c)
    target_compile_definitions(mc_test_application_startup_boundary PRIVATE
        MC_STARTUP_CONTROLLER_BOUNDARY=1)
    get_target_property(_mc_boundary_links mc_test_application_startup LINK_LIBRARIES)
    target_link_libraries(mc_test_application_startup_boundary PRIVATE ${_mc_boundary_links})
    unset(_mc_boundary_links)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_application_startup_boundary PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(MSVC)
        target_link_options(mc_test_application_startup_boundary PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_application_startup_boundary.map")
        target_link_options(mc_test_application_startup PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_application_startup.map")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(mc_test_application_startup_boundary PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_application_startup_boundary.map")
        target_link_options(mc_test_application_startup PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_application_startup.map")
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_application_startup_boundary POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:OpenAL>" "$<TARGET_FILE_DIR:mc_test_application_startup_boundary>")
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS)
        add_test(NAME client.application_startup_session_gl_error
            COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_application_startup_tail.py"
                --executable "$<TARGET_FILE:mc_test_application_startup_boundary>"
                --resources "${_mc_startup_resource_root}"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/startup-tail-window-session-gl-error"
                --mode window-session-gl-error)
        set_tests_properties(client.application_startup_session_gl_error PROPERTIES
            LABELS "integration;client;startup;controllers;graphics;ownership" TIMEOUT 240)
    endif()
endif()
