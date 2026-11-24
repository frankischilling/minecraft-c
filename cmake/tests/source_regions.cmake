add_executable(mc_test_source_regions tests/world/storage/test_source_regions.c)
target_link_libraries(mc_test_source_regions PRIVATE mc_persistence mc_build_options)
add_test(NAME storage.source_regions COMMAND Python3::Interpreter -B
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/storage/test_source_regions.py"
    --executable "$<TARGET_FILE:mc_test_source_regions>"
    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-regions-controls")
set_tests_properties(storage.source_regions PROPERTIES
    LABELS "public;integration;storage;ownership;filesystem" TIMEOUT 180)
