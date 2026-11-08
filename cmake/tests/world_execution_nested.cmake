include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_client_render_global)
    add_executable(mc_test_world_execution_nested
        tests/world/runtime/world_access/test_execution_nested.c)
    target_include_directories(mc_test_world_execution_nested PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_execution_nested PRIVATE
        mc_client_render_global mc_simulation mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_world_execution_nested PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME world.execution_nested COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_execution_nested.py"
        --kind nested --executable "$<TARGET_FILE:mc_test_world_execution_nested>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-nested-controls")
    set_tests_properties(world.execution_nested PROPERTIES
        LABELS "public;integration;world;client;ownership" TIMEOUT 420)

    add_executable(mc_test_activation_execution
        tests/world/runtime/world_access/test_activation_execution.c)
    target_include_directories(mc_test_activation_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_activation_execution PRIVATE
        mc_client_render_global mc_simulation mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_activation_execution PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME world.activation_execution COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_execution_nested.py"
        --kind activation --executable "$<TARGET_FILE:mc_test_activation_execution>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-nested-controls")
    set_tests_properties(world.activation_execution PROPERTIES
        LABELS "public;integration;world;client;ownership" TIMEOUT 420)
endif()

if(BUILD_TESTING AND TARGET mc_simulation)
    foreach(_mc_nested_kind IN ITEMS loader success)
        add_executable("mc_test_world_execution_nested_${_mc_nested_kind}"
            "tests/world/runtime/world_access/test_execution_nested_${_mc_nested_kind}.c")
        target_include_directories("mc_test_world_execution_nested_${_mc_nested_kind}"
            PRIVATE src src/platform/time)
        target_link_libraries("mc_test_world_execution_nested_${_mc_nested_kind}" PRIVATE
            mc_simulation mc_persistence mc_c_atomics mc_build_options)
        add_test(NAME "world.execution_nested_${_mc_nested_kind}" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_execution_nested.py"
            --kind "${_mc_nested_kind}"
            --executable "$<TARGET_FILE:mc_test_world_execution_nested_${_mc_nested_kind}>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-nested-controls")
        set_tests_properties("world.execution_nested_${_mc_nested_kind}" PROPERTIES
            LABELS "public;integration;world;storage;ownership;filesystem" TIMEOUT 420)
    endforeach()
    unset(_mc_nested_kind)
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    foreach(_mc_nested_mode IN ITEMS normal optimized)
        set(_mc_nested_python -B)
        if(_mc_nested_mode STREQUAL "optimized")
            list(APPEND _mc_nested_python -O)
        endif()
        add_test(NAME "tools.world_execution_nested_${_mc_nested_mode}" COMMAND
            Python3::Interpreter ${_mc_nested_python}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_execution_nested.py" --self-test)
        set_tests_properties("tools.world_execution_nested_${_mc_nested_mode}" PROPERTIES
            LABELS "public;unit;tools;world;ownership" TIMEOUT 90)
    endforeach()
    unset(_mc_nested_python)
    unset(_mc_nested_mode)
endif()
