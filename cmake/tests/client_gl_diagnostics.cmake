if(BUILD_TESTING AND TARGET mc_client_gl_diagnostics)
    add_executable(mc_test_application_gl_diagnostics
        tests/client/startup/test_application_gl_diagnostics.c)
    target_link_libraries(mc_test_application_gl_diagnostics PRIVATE
        mc_client_gl_diagnostics mc_build_options)
    add_test(NAME client.application_gl_diagnostics_cpu
        COMMAND mc_test_application_gl_diagnostics)
    set_tests_properties(client.application_gl_diagnostics_cpu PROPERTIES
        LABELS "public;unit;client;ownership" TIMEOUT 120)

    # Exact Display.c included once by the control, like existing source FP
    # controls. Do not link another Display/diagnostic archive body here.
    add_executable(mc_test_application_gl_diagnostics_device
        tests/client/startup/test_application_gl_diagnostics_device.c
        src/client/startup/application_gl_diagnostics.c
        src/client/startup/glu_error_text.c)
    target_include_directories(mc_test_application_gl_diagnostics_device PRIVATE
        include src src/client/display)
    target_link_libraries(mc_test_application_gl_diagnostics_device PRIVATE
        mc_core mc_image SDL3::SDL3-static mc_client_event_transport
        mc_client_sdk_runtime mc_build_options)
    if(UNIX)
        target_link_libraries(mc_test_application_gl_diagnostics_device PRIVATE m)
    endif()
    option(MC_ENABLE_APPLICATION_GL_DIAGNOSTIC_DEVICE_TESTS
        "Run actual GL single-error diagnostic controls" OFF)
    if(MC_ENABLE_APPLICATION_GL_DIAGNOSTIC_DEVICE_TESTS)
        add_test(NAME client.application_gl_diagnostics_device
            COMMAND mc_test_application_gl_diagnostics_device)
        set_tests_properties(client.application_gl_diagnostics_device PROPERTIES
            LABELS "integration;client;graphics;ownership" TIMEOUT 180)
    endif()
endif()
