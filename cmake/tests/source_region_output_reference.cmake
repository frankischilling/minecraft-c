option(MC_ENABLE_SOURCE_REGION_OUTPUT_REFERENCE_TESTS
    "Enable prepared original source region output reference tests" OFF)
if(MC_ENABLE_SOURCE_REGION_OUTPUT_REFERENCE_TESTS)
    if(NOT MC_REFERENCE_CLIENT_JAR)
        message(FATAL_ERROR
            "Source region output reference tests require the verified original client JAR")
    endif()
    find_package(Java 17 EXACT REQUIRED COMPONENTS Runtime Development)
    get_filename_component(_mc_region_output_java "${Java_JAVA_EXECUTABLE}" REALPATH)
    get_filename_component(_mc_region_output_java_bin "${_mc_region_output_java}" DIRECTORY)
    get_filename_component(_mc_region_output_jdk "${_mc_region_output_java_bin}" DIRECTORY)
    add_test(NAME reference.source_region_output
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/source_region_output/capture.py"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --jdk-home "${_mc_region_output_jdk}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-source-region-output")
    set_tests_properties(reference.source_region_output PROPERTIES
        LABELS "reference;original;storage;filesystem;ownership" TIMEOUT 600)
    unset(_mc_region_output_java)
    unset(_mc_region_output_java_bin)
    unset(_mc_region_output_jdk)
endif()
