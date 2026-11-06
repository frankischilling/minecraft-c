if(BUILD_TESTING)
    add_test(NAME tools.startup_executable_fingerprint
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_startup_fingerprint.py")
    set_tests_properties(tools.startup_executable_fingerprint PROPERTIES
        LABELS "public;unit;client;filesystem" TIMEOUT 45)
endif()
