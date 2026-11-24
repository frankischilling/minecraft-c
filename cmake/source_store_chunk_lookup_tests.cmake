include_guard(GLOBAL)
if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_source_store_chunk_lookup tests/world/storage/test_chunk_lookup.c)
    target_include_directories(mc_test_source_store_chunk_lookup PRIVATE src)
    target_link_libraries(mc_test_source_store_chunk_lookup PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    add_test(NAME world.source_store_chunk_lookup COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_store_chunk_lookup.py"
        --executable "$<TARGET_FILE:mc_test_source_store_chunk_lookup>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-store-chunk-lookup-controls")
    set_tests_properties(world.source_store_chunk_lookup PROPERTIES
        LABELS "public;integration;world;storage;ownership;filesystem" TIMEOUT 300)
endif()
