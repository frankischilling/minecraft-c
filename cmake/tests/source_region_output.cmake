if(BUILD_TESTING)
    add_executable(mc_test_source_region_output
        tests/world/storage/test_source_region_output.c
        tests/world/storage/region_output_raf_native.c)
    target_include_directories(mc_test_source_region_output PRIVATE src)
    # The direct canonical RAF object supplies the real provider with a narrow
    # native close observation/control; its archive copy is not extracted.
    target_link_libraries(mc_test_source_region_output PRIVATE
        mc_persistence mc_java_outputs mc_platform_time mc_build_options)
    if(MSVC)
        target_link_options(mc_test_source_region_output PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_source_region_output.map")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_link_options(mc_test_source_region_output PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_source_region_output.map")
    endif()
    add_test(NAME storage.source_region_output COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_source_region_output.py"
        --executable "$<TARGET_FILE:mc_test_source_region_output>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-region-output-controls")
    set_tests_properties(storage.source_region_output PROPERTIES
        LABELS "public;integration;storage;ownership;filesystem" TIMEOUT 180)
endif()
