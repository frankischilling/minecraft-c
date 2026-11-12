include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_metadata_set tests/world/runtime/test_metadata_set.c)
    target_include_directories(mc_test_metadata_set PRIVATE src src/core/java)
    target_link_libraries(mc_test_metadata_set PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME world.metadata_set COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_metadata_set.py"
        --executable "$<TARGET_FILE:mc_test_metadata_set>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/metadata-set-controls")
    set_tests_properties(world.metadata_set PROPERTIES
        LABELS "public;integration;world;ownership" TIMEOUT 180)
endif()
