include_guard(GLOBAL)

# Production scalar bodies are registered by the main simulation source list.
if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_world_block_scalar_execution
        tests/world/runtime/world_access/test_block_scalar_execution.c)
    target_include_directories(mc_test_world_block_scalar_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_block_scalar_execution PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    if(Python3_Interpreter_FOUND)
        foreach(_mc_scalar_mode IN ITEMS values metadata blank admission loader-error
                completion-block completion-metadata completion-set open-descendant)
            add_test(NAME "world.block_scalar_execution_${_mc_scalar_mode}" COMMAND
                Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_block_scalar_execution.py"
                --mode "${_mc_scalar_mode}"
                --executable "$<TARGET_FILE:mc_test_world_block_scalar_execution>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-block-scalar-controls")
            set_tests_properties("world.block_scalar_execution_${_mc_scalar_mode}" PROPERTIES
                LABELS "public;integration;world;storage;ownership;filesystem"
                RESOURCE_LOCK world_block_scalar_execution TIMEOUT 240)
        endforeach()
        unset(_mc_scalar_mode)
    endif()
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    foreach(_mc_scalar_python_mode IN ITEMS normal optimized)
        set(_mc_scalar_python_flags -B)
        if(_mc_scalar_python_mode STREQUAL "optimized")
            list(APPEND _mc_scalar_python_flags -O)
        endif()
        add_test(NAME "tools.world_block_scalar_execution_${_mc_scalar_python_mode}" COMMAND
            Python3::Interpreter ${_mc_scalar_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_block_scalar_execution.py" --self-test)
        set_tests_properties("tools.world_block_scalar_execution_${_mc_scalar_python_mode}" PROPERTIES
            LABELS "public;unit;tools;world;ownership" TIMEOUT 90)
    endforeach()
    unset(_mc_scalar_python_flags)
    unset(_mc_scalar_python_mode)
endif()
