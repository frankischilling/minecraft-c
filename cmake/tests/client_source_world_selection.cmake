if(BUILD_TESTING AND TARGET mc_client_source_screens)
    add_executable(mc_test_source_world_selection tests/client/gui/test_source_world_selection.c)
    target_include_directories(mc_test_source_world_selection PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_source_world_selection PRIVATE mc_client_source_screens
        mc_client_application_owner mc_client_saves mc_resources mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_world_selection PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.source_world_selection
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_source_world_selection.py"
            --executable "$<TARGET_FILE:mc_test_source_world_selection>")
    set_tests_properties(client.source_world_selection PROPERTIES
        LABELS "public;client;resource;ownership" TIMEOUT 180)
endif()
