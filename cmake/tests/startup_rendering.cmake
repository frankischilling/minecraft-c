# These actual-window/default-directory controls require the same explicit
# platform admission as the existing session profile. POSIX real-home effects
# require separate execution admission; no default HOME override is invented.
if(BUILD_TESTING AND MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS AND
   MC_ENABLE_CLIENT_DISPLAY_TESTS AND TARGET mc_test_application_startup)
    foreach(_mc_render_mode IN ITEMS render-global-limit particle-constructor-limit)
        add_test(NAME "client.application_startup_${_mc_render_mode}"
            COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_application_startup_tail.py"
                --executable "$<TARGET_FILE:mc_test_application_startup>"
                --resources "${_mc_startup_resource_root}"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/startup-${_mc_render_mode}"
                --mode "window-${_mc_render_mode}")
        set_tests_properties("client.application_startup_${_mc_render_mode}" PROPERTIES
            LABELS "integration;client;startup;filesystem;rendering;ownership" TIMEOUT 240)
    endforeach()
    unset(_mc_render_mode)
endif()
