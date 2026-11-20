include_guard(GLOBAL)
option(MC_ENABLE_STRING_CASE_REFERENCE_TESTS "Compare complete simple folds and UTF16 comparison with local JDK17" OFF)

if(BUILD_TESTING AND TARGET mc_core)
    add_executable(mc_test_string_case tests/core/java/test_string_case.c)
    target_link_libraries(mc_test_string_case PRIVATE mc_core mc_build_options)
    add_test(NAME core.java_string_case COMMAND mc_test_string_case)
    set_tests_properties(core.java_string_case PROPERTIES LABELS "public;unit;java;text" TIMEOUT 30)
    if(MC_ENABLE_STRING_CASE_REFERENCE_TESTS OR MC_ENABLE_JAVA_REFERENCE_TESTS)
        find_package(Java 17 COMPONENTS Runtime Development REQUIRED)
        find_package(Python3 3.10 COMPONENTS Interpreter REQUIRED)
        add_test(NAME core.java_string_case_reference COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/core/string_case/capture_reference.py"
            --executable "$<TARGET_FILE:mc_test_string_case>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/string-case-reference"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}")
        set_tests_properties(core.java_string_case_reference PROPERTIES
            LABELS "reference;integration;java;text" TIMEOUT 180)
    endif()
endif()
