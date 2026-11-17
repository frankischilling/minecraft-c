include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_core)
    add_executable(mc_test_string_lower_hex tests/core/java/test_string_lower_hex.c)
    target_link_libraries(mc_test_string_lower_hex PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_string_lower_hex COMMAND mc_test_string_lower_hex)
    set_tests_properties(core.java_string_lower_hex PROPERTIES
        LABELS "public;unit;java;text" TIMEOUT 90)
endif()
