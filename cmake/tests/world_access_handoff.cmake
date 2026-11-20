include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_client_render_global)
    add_executable(mc_test_world_access_handoff tests/world/runtime/world_access/test_handoff.c)
    target_include_directories(mc_test_world_access_handoff PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_access_handoff PRIVATE
        mc_client_render_global mc_simulation mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_world_access_handoff PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME world.access_handoff COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_execution_ownership.py"
        --kind handoff --executable "$<TARGET_FILE:mc_test_world_access_handoff>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-execution-controls")
    set_tests_properties(world.access_handoff PROPERTIES
        LABELS "public;integration;world;client;ownership" TIMEOUT 300)
    add_executable(mc_test_world_execution_children
        tests/world/runtime/world_access/test_execution_children.c)
    target_include_directories(mc_test_world_execution_children PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_execution_children PRIVATE
        mc_client_render_global mc_simulation mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_world_execution_children PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME world.execution_children COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_execution_ownership.py"
        --kind children --executable "$<TARGET_FILE:mc_test_world_execution_children>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-execution-controls")
    set_tests_properties(world.execution_children PROPERTIES
        LABELS "public;integration;world;client;ownership" TIMEOUT 300)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_execution_python -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_execution_python -O)
        endif()
        add_test(NAME "tools.world_execution_ownership_${mode}" COMMAND Python3::Interpreter ${_mc_execution_python}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_execution_ownership.py" --self-test)
        set_tests_properties("tools.world_execution_ownership_${mode}" PROPERTIES
            LABELS "public;unit;tools;world;ownership" TIMEOUT 90)
    endforeach()
endif()
