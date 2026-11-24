if(BUILD_TESTING AND TARGET mc_client_source_screen_events AND TARGET mc_sound_backend)
    add_executable(mc_test_source_screen_events tests/client/gui/test_source_screen_events.c)
    target_include_directories(mc_test_source_screen_events PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_source_screen_events PRIVATE mc_client_source_screen_events
        mc_sound_backend mc_resources mc_c_atomics SDL3::SDL3-static mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_screen_events PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.source_screen_events COMMAND mc_test_source_screen_events)
    set_tests_properties(client.source_screen_events PROPERTIES
        LABELS "public;unit;client;input;ownership" TIMEOUT 120)
endif()
