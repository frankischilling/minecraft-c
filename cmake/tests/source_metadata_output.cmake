if(BUILD_TESTING)
    add_executable(mc_test_source_metadata_output
        tests/world/storage/test_source_metadata_output.c
        tests/world/storage/java_output_native.c)
    target_include_directories(mc_test_source_metadata_output PRIVATE src)
    target_link_libraries(mc_test_source_metadata_output PRIVATE
        mc_simulation mc_persistence mc_java_outputs mc_compressed_output
        mc_platform_time mc_build_options)
    if(MSVC)
        target_link_options(mc_test_source_metadata_output PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_source_metadata_output.map")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(mc_test_source_metadata_output PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_source_metadata_output.map")
    endif()
    add_test(NAME storage.source_metadata_output COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_source_metadata_output.py"
        --executable "$<TARGET_FILE:mc_test_source_metadata_output>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-metadata-output-controls")
    set_tests_properties(storage.source_metadata_output PROPERTIES
        LABELS "public;integration;storage;ownership;filesystem" TIMEOUT 600)
endif()
