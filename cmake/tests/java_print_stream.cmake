if(BUILD_TESTING)
    add_executable(mc_test_java_print_stream tests/core/java/test_print_stream.c)
    target_link_libraries(mc_test_java_print_stream PRIVATE mc_core Threads::Threads mc_build_options)
    if(UNIX)
        target_compile_definitions(mc_test_java_print_stream PRIVATE _POSIX_C_SOURCE=200809L)
    endif()
    add_test(NAME core.java_print_stream COMMAND mc_test_java_print_stream)
    set_tests_properties(core.java_print_stream PROPERTIES
        LABELS "public;unit;core;ownership;io" TIMEOUT 120)

    if(TARGET mc_client_gl_diagnostics)
        add_executable(mc_test_diagnostic_builder tests/client/startup/test_diagnostic_builder.c)
        target_link_libraries(mc_test_diagnostic_builder PRIVATE mc_client_gl_diagnostics mc_build_options)
        add_test(NAME client.diagnostic_builder COMMAND mc_test_diagnostic_builder)
        set_tests_properties(client.diagnostic_builder PROPERTIES
            LABELS "public;unit;client;ownership" TIMEOUT 120)

        # The control includes the exact Display TU and separate exact
        # PrintStream TU once. Neither archive body may also be extracted.
        add_executable(mc_test_application_gl_output_device
            tests/client/startup/test_application_gl_output_device.c
            tests/client/startup/print_stream_rebind_observer.c
            src/client/startup/application_gl_diagnostics.c
            src/client/startup/glu_error_text.c
            src/client/startup/diagnostic_builder.c
            src/client/startup/application_gl_output.c)
        target_include_directories(mc_test_application_gl_output_device PRIVATE include src src/client/display)
        target_link_libraries(mc_test_application_gl_output_device PRIVATE
            mc_core mc_simulation mc_image SDL3::SDL3-static mc_client_event_transport
            mc_client_sdk_runtime Threads::Threads mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_application_gl_output_device PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(MSVC)
            target_link_options(mc_test_application_gl_output_device PRIVATE
                "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_application_gl_output_device.map")
        elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_link_options(mc_test_application_gl_output_device PRIVATE
                "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_application_gl_output_device.map")
        endif()
        if(UNIX)
            target_compile_definitions(mc_test_application_gl_output_device PRIVATE _POSIX_C_SOURCE=200809L)
            target_link_libraries(mc_test_application_gl_output_device PRIVATE m)
        endif()
        option(MC_ENABLE_APPLICATION_GL_OUTPUT_DEVICE_TESTS
            "Run genuine nonzero GL diagnostics through canonical System.out" OFF)
        if(MC_ENABLE_APPLICATION_GL_OUTPUT_DEVICE_TESTS)
            add_test(NAME client.application_gl_output_device COMMAND mc_test_application_gl_output_device)
            set_tests_properties(client.application_gl_output_device PROPERTIES
                LABELS "integration;client;graphics;ownership;io" TIMEOUT 180)
        endif()
    endif()
endif()
