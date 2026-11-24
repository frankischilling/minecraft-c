include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_world_math_proof_execution
        tests/world/runtime/world_access/test_math_proof_execution.c)
    target_include_directories(mc_test_world_math_proof_execution PRIVATE src src/platform/time)
    # This translation unit compiles the unchanged actual Math class body, like
    # the core Math ownership control. Counting delegates add no alternate body.
    target_link_libraries(mc_test_world_math_proof_execution PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_build_options)
    foreach(_mc_math_proof_mode IN ITEMS require note forward)
        add_test(NAME "world.math_proof_execution_${_mc_math_proof_mode}" COMMAND
            Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_math_proof_execution.py"
            --mode "${_mc_math_proof_mode}"
            --executable "$<TARGET_FILE:mc_test_world_math_proof_execution>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-math-proof-controls")
        set_tests_properties("world.math_proof_execution_${_mc_math_proof_mode}" PROPERTIES
            LABELS "public;unit;world;math;ownership" TIMEOUT 120)
    endforeach()
    unset(_mc_math_proof_mode)
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    foreach(_mc_math_proof_python_mode IN ITEMS normal optimized)
        set(_mc_math_proof_python_flags -B)
        if(_mc_math_proof_python_mode STREQUAL "optimized")
            list(APPEND _mc_math_proof_python_flags -O)
        endif()
        add_test(NAME "tools.world_math_proof_execution_${_mc_math_proof_python_mode}" COMMAND
            Python3::Interpreter ${_mc_math_proof_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_math_proof_execution.py" --self-test)
        set_tests_properties("tools.world_math_proof_execution_${_mc_math_proof_python_mode}" PROPERTIES
            LABELS "public;unit;tools;world;math;ownership" TIMEOUT 90)
    endforeach()
    unset(_mc_math_proof_python_flags)
    unset(_mc_math_proof_python_mode)
endif()
