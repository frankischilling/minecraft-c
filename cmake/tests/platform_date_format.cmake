add_executable(mc_test_date_format tests/platform/time/test_date_format.c)
target_include_directories(mc_test_date_format PRIVATE src/platform/time)
target_link_libraries(mc_test_date_format PRIVATE mc_platform_date_format mc_build_options)
add_test(NAME platform.date_format_default COMMAND mc_test_date_format
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/platform/date_format/vectors.tsv")
set_tests_properties(platform.date_format_default PROPERTIES LABELS "public;unit" TIMEOUT 60)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(NAME platform.date_format_utc COMMAND mc_test_date_format
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/platform/date_format/vectors.tsv")
    set_tests_properties(platform.date_format_utc PROPERTIES LABELS "public;unit" TIMEOUT 60
        ENVIRONMENT "TZ=UTC")
    add_test(NAME platform.date_format_unsupported_locale COMMAND mc_test_date_format --reject-locale)
    set_tests_properties(platform.date_format_unsupported_locale PROPERTIES LABELS "public;unit"
        TIMEOUT 60 ENVIRONMENT "LC_ALL=de_DE.UTF-8")
    add_test(NAME platform.date_format_unsupported_zone COMMAND mc_test_date_format --reject-zone)
    set_tests_properties(platform.date_format_unsupported_zone PROPERTIES LABELS "public;unit"
        TIMEOUT 60 ENVIRONMENT "TZ=Europe/Berlin")
elseif(WIN32)
    add_test(NAME platform.date_format_unsupported_zone COMMAND mc_test_date_format --reject-zone)
    set_tests_properties(platform.date_format_unsupported_zone PROPERTIES LABELS "public;unit"
        TIMEOUT 60 ENVIRONMENT "TZ=UTC")
endif()
if(MC_ENABLE_JAVA_REFERENCE_TESTS)
    find_package(Java 17 REQUIRED COMPONENTS Runtime Development)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME platform.date_format_reference
        COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/platform/test_date_format_reference.py"
            --java "${Java_JAVA_EXECUTABLE}" --javac "${Java_JAVAC_EXECUTABLE}"
            --native "$<TARGET_FILE:mc_test_date_format>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/date-format-reference")
    set_tests_properties(platform.date_format_reference PROPERTIES LABELS "reference;runtime"
        TIMEOUT 120)
endif()
