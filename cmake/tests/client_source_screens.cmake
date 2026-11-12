if(BUILD_TESTING AND TARGET mc_client_source_screens)
    add_executable(mc_test_source_screen tests/client/gui/test_source_screen.c)
    target_include_directories(mc_test_source_screen PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_source_screen PRIVATE mc_client_source_screens mc_resources
        mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_screen PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.source_screen_constructors COMMAND mc_test_source_screen)
    set_tests_properties(client.source_screen_constructors PROPERTIES
        LABELS "public;unit;client;ownership" TIMEOUT 180)
    add_test(NAME client.source_screen_resources
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_source_screen_resources.py"
            --executable "$<TARGET_FILE:mc_test_source_screen>")
    set_tests_properties(client.source_screen_resources PROPERTIES
        LABELS "public;client;resource;ownership" TIMEOUT 180)

endif()

if(BUILD_TESTING AND TARGET mc_client_source_screens)
    add_executable(mc_test_source_screen_initialize tests/client/gui/test_source_screen_initialize.c)
    target_include_directories(mc_test_source_screen_initialize PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_source_screen_initialize PRIVATE mc_client_source_screens
        mc_client_application_owner mc_resources mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_screen_initialize PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.source_screen_initialize COMMAND mc_test_source_screen_initialize)
    set_tests_properties(client.source_screen_initialize PROPERTIES LABELS "public;unit;client;ownership" TIMEOUT 180)
    add_test(NAME client.source_screen_initialize_resources
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_source_screen_resources.py"
            --executable "$<TARGET_FILE:mc_test_source_screen_initialize>")
    set_tests_properties(client.source_screen_initialize_resources PROPERTIES
        LABELS "public;client;resource;ownership" TIMEOUT 180)

endif()
