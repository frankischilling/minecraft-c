include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_client_runtime)
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_executable(mc_test_source_sign_event_ownership
        tests/client/gui/test_source_sign_event_ownership.c)
    target_include_directories(mc_test_source_sign_event_ownership PRIVATE src src/platform/time)
    target_link_libraries(mc_test_source_sign_event_ownership PRIVATE
        mc_client_runtime mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_sign_event_ownership PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_source_sign_event_ownership POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_source_sign_event_ownership>")
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.sign_event_ownership COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_sign_event_ownership.py"
            --executable "$<TARGET_FILE:mc_test_source_sign_event_ownership>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/sign-event-ownership")
        set_tests_properties(client.sign_event_ownership PROPERTIES
            LABELS "public;integration;client;graphics;input;statistics;ownership" TIMEOUT 210
            RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
