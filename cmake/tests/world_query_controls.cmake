include_guard(GLOBAL)

# Owned runtime bodies belong to the main mc_simulation source list, including
# BUILD_TESTING=OFF. This module adds only native controls and their controllers.
if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_world_kernel_loader_execution
        tests/world/runtime/world_access/test_kernel_loader_execution.c)
    target_include_directories(mc_test_world_kernel_loader_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_kernel_loader_execution PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    add_executable(mc_test_world_tile_lookup_execution
        tests/world/runtime/world_access/test_tile_lookup_execution.c)
    target_include_directories(mc_test_world_tile_lookup_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_tile_lookup_execution PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    add_executable(mc_test_world_collision_query_execution
        tests/world/runtime/world_access/test_collision_query_execution.c)
    target_include_directories(mc_test_world_collision_query_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_collision_query_execution PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
endif()

if(BUILD_TESTING AND TARGET mc_test_world_kernel_loader_execution
        AND TARGET mc_test_world_tile_lookup_execution
        AND TARGET mc_test_world_collision_query_execution AND Python3_Interpreter_FOUND)
    foreach(_mc_query_kind IN ITEMS loader lookup collision)
        if(_mc_query_kind STREQUAL "loader")
            set(_mc_query_target mc_test_world_kernel_loader_execution)
            set(_mc_query_modes error null-error)
            set(_mc_query_name kernel_loader_execution)
        elseif(_mc_query_kind STREQUAL "lookup")
            set(_mc_query_target mc_test_world_tile_lookup_execution)
            set(_mc_query_modes map lazy armed held nested)
            set(_mc_query_name tile_lookup_execution)
        else()
            set(_mc_query_target mc_test_world_collision_query_execution)
            set(_mc_query_modes id-first metadata-first domain loader-error loader-null-error armed held)
            set(_mc_query_name collision_query_execution)
        endif()
        foreach(_mc_query_mode IN LISTS _mc_query_modes)
            add_test(NAME "world.${_mc_query_name}_${_mc_query_mode}" COMMAND
                Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_world_query_controls.py"
                --kind "${_mc_query_kind}" --mode "${_mc_query_mode}"
                --executable "$<TARGET_FILE:${_mc_query_target}>"
                --bundle "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/world_query_controls_bundle.json"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-query-controls")
            set_tests_properties("world.${_mc_query_name}_${_mc_query_mode}" PROPERTIES
                LABELS "public;integration;world;storage;ownership;filesystem"
                RESOURCE_LOCK world_query_controls TIMEOUT 420)
        endforeach()
    endforeach()
    unset(_mc_query_kind)
    unset(_mc_query_target)
    unset(_mc_query_modes)
    unset(_mc_query_name)
    unset(_mc_query_mode)
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    foreach(_mc_query_python_mode IN ITEMS normal optimized)
        set(_mc_query_python_flags -B)
        if(_mc_query_python_mode STREQUAL "optimized")
            list(APPEND _mc_query_python_flags -O)
        endif()
        add_test(NAME "tools.world_query_controls_${_mc_query_python_mode}" COMMAND
            Python3::Interpreter ${_mc_query_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_world_query_controls.py" --self-test)
        set_tests_properties("tools.world_query_controls_${_mc_query_python_mode}" PROPERTIES
            LABELS "public;unit;tools;world;ownership" TIMEOUT 90)
    endforeach()
    unset(_mc_query_python_flags)
    unset(_mc_query_python_mode)
endif()
