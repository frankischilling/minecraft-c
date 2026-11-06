# These actual-window/default-directory modes have the same explicit platform
# opt-in and fresh child APPDATA policy as the existing normal session profile.
# POSIX real-home filesystem effects require separate execution admission.
if(BUILD_TESTING AND MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS AND
   MC_ENABLE_CLIENT_DISPLAY_TESTS AND TARGET mc_test_application_startup)
    foreach(_mc_effect_mode IN ITEMS constructor-limit list-limit)
        add_test(NAME "client.application_startup_effects_${_mc_effect_mode}"
            COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_application_startup_tail.py"
                --executable "$<TARGET_FILE:mc_test_application_startup>"
                --resources "${_mc_startup_resource_root}"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/startup-effects-${_mc_effect_mode}"
                --mode "window-effects-${_mc_effect_mode}")
        set_tests_properties("client.application_startup_effects_${_mc_effect_mode}" PROPERTIES
            LABELS "integration;client;startup;filesystem;effects;ownership" TIMEOUT 240)
    endforeach()
    unset(_mc_effect_mode)
endif()
