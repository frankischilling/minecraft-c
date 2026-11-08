include_guard(GLOBAL)

# runtime_execution.c belongs to the main production mc_simulation source list,
# including BUILD_TESTING=OFF; do not register a duplicate test-only source here.

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_world_kernel_execution
        tests/world/runtime/world_access/test_kernel_execution.c)
    target_include_directories(mc_test_world_kernel_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_kernel_execution PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    foreach(_mc_kernel_mode IN ITEMS empty source native first-owner)
        add_test(NAME "world.kernel_execution_${_mc_kernel_mode}" COMMAND
            Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_kernel_execution.py"
            --mode "${_mc_kernel_mode}"
            --executable "$<TARGET_FILE:mc_test_world_kernel_execution>"
            --bundle "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/kernel_execution_bundle.json"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-kernel-controls")
        set_tests_properties("world.kernel_execution_${_mc_kernel_mode}" PROPERTIES
            LABELS "public;integration;world;storage;ownership;filesystem"
            RESOURCE_LOCK world_kernel_execution TIMEOUT 420)
    endforeach()
    unset(_mc_kernel_mode)
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    foreach(_mc_kernel_python_mode IN ITEMS normal optimized)
        set(_mc_kernel_python_flags -B)
        if(_mc_kernel_python_mode STREQUAL "optimized")
            list(APPEND _mc_kernel_python_flags -O)
        endif()
        add_test(NAME "tools.world_kernel_execution_${_mc_kernel_python_mode}" COMMAND
            Python3::Interpreter ${_mc_kernel_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_kernel_execution.py" --self-test)
        set_tests_properties("tools.world_kernel_execution_${_mc_kernel_python_mode}" PROPERTIES
            LABELS "public;unit;tools;world;ownership" TIMEOUT 90)
    endforeach()
    unset(_mc_kernel_python_flags)
    unset(_mc_kernel_python_mode)
endif()
