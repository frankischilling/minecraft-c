if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_source_world_saving tests/world/runtime/saving/test_source_saving.c)
    target_include_directories(mc_test_source_world_saving PRIVATE src)
    target_link_libraries(mc_test_source_world_saving PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    add_test(NAME world.source_saving COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_source_saving.py"
        --executable "$<TARGET_FILE:mc_test_source_world_saving>"
        --source-root "${CMAKE_CURRENT_SOURCE_DIR}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-world-saving-controls")
    set_tests_properties(world.source_saving PROPERTIES
        LABELS "public;integration;world;storage;ownership;filesystem" TIMEOUT 600)
endif()
