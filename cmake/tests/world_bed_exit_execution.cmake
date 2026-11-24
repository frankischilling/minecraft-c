include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_world_bed_exit_execution
        tests/world/runtime/world_access/test_bed_exit_execution.c)
    target_include_directories(mc_test_world_bed_exit_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_bed_exit_execution PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    add_test(NAME world.bed_exit_execution COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_bed_exit_execution.py"
        --executable "$<TARGET_FILE:mc_test_world_bed_exit_execution>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-bed-exit-controls")
    set_tests_properties(world.bed_exit_execution PROPERTIES
        LABELS "public;integration;world;storage;ownership;filesystem" TIMEOUT 240)
endif()
