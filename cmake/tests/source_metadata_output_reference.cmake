option(MC_ENABLE_SOURCE_METADATA_OUTPUT_REFERENCE_TESTS
    "Enable prepared original source metadata output reference tests" OFF)
if(MC_ENABLE_SOURCE_METADATA_OUTPUT_REFERENCE_TESTS)
    if(NOT MC_REFERENCE_CLIENT_JAR OR NOT MC_REFERENCE_SERVER_JAR)
        message(FATAL_ERROR
            "Source metadata reference tests require both verified original JAR paths")
    endif()
    find_package(Java 17 EXACT REQUIRED COMPONENTS Runtime Development)
    get_filename_component(_mc_metadata_java "${Java_JAVA_EXECUTABLE}" REALPATH)
    get_filename_component(_mc_metadata_java_bin "${_mc_metadata_java}" DIRECTORY)
    get_filename_component(_mc_metadata_jdk "${_mc_metadata_java_bin}" DIRECTORY)
    add_test(NAME reference.source_metadata_output
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/source_metadata_output/capture.py"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --server-jar "${MC_REFERENCE_SERVER_JAR}"
            --jdk-home "${_mc_metadata_jdk}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-source-metadata-output")
    set_tests_properties(reference.source_metadata_output PROPERTIES
        LABELS "reference;original;storage;filesystem" TIMEOUT 600)
    add_test(NAME reference.source_metadata_output_faults
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/source_metadata_output/faults/capture.py"
            --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
            --client-jar "${MC_REFERENCE_CLIENT_JAR}"
            --jdk-home "${_mc_metadata_jdk}"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/reference-source-metadata-output-faults")
    set_tests_properties(reference.source_metadata_output_faults PROPERTIES
        LABELS "reference;original;storage;authored-faults" TIMEOUT 600)
    unset(_mc_metadata_java)
    unset(_mc_metadata_java_bin)
    unset(_mc_metadata_jdk)
endif()
