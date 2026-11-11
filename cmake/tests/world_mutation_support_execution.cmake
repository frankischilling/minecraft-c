include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_world_mutation_support_execution
        tests/world/runtime/world_access/test_mutation_support_execution.c)
    target_include_directories(mc_test_world_mutation_support_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_mutation_support_execution PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    add_test(NAME world.mutation_support_execution COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_mutation_support_execution.py"
        --executable "$<TARGET_FILE:mc_test_world_mutation_support_execution>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-mutation-support-controls")
    set_tests_properties(world.mutation_support_execution PROPERTIES
        LABELS "public;integration;world;storage;ownership;filesystem" TIMEOUT 240)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_mutation_support_python_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_mutation_support_python_options -O)
        endif()
        add_test(NAME "tools.mutation_support_execution_${mode}" COMMAND Python3::Interpreter
            ${_mc_mutation_support_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_mutation_support_execution.py" --self-test)
        set_tests_properties("tools.mutation_support_execution_${mode}" PROPERTIES
            LABELS "public;unit;world;storage;ownership;tools" TIMEOUT 60)
    endforeach()
endif()
