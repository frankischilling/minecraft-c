include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_world_scheduling_execution
        tests/world/runtime/test_scheduling_execution.c)
    target_link_libraries(mc_test_world_scheduling_execution PRIVATE
        mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.scheduling_execution COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_scheduling_execution.py"
        --executable "$<TARGET_FILE:mc_test_world_scheduling_execution>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/retained-scheduling-contracts")
    set_tests_properties(world.scheduling_execution PROPERTIES
        LABELS "public;integration;world;storage;ownership;filesystem" TIMEOUT 120)
endif()
