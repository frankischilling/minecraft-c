include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_arrow_tick_failures tests/world/runtime/test_arrow_tick_failures.c)
    target_include_directories(mc_test_arrow_tick_failures PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_tick_failures PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
endif()

if(BUILD_TESTING AND TARGET mc_test_arrow_tick_failures AND Python3_Interpreter_FOUND)
    foreach(_mc_arrow_failure_mode IN ITEMS
            null-world code-null math-null math-failed math-cold admission-fp)
        string(REPLACE "-" "_" _mc_arrow_failure_name "${_mc_arrow_failure_mode}")
        add_test(NAME entity.arrow_tick_${_mc_arrow_failure_name}
            COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_arrow_tick_failures.py"
                --mode "${_mc_arrow_failure_mode}"
                --executable "$<TARGET_FILE:mc_test_arrow_tick_failures>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-tick-failure-scenes")
        set_tests_properties(entity.arrow_tick_${_mc_arrow_failure_name} PROPERTIES
            TIMEOUT 300 RESOURCE_LOCK world_query_controls
            LABELS "public;integration;world;entity;ownership")
    endforeach()
    unset(_mc_arrow_failure_mode)
    unset(_mc_arrow_failure_name)
    add_test(NAME tools.arrow_tick_failures_protocol
        COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_arrow_tick_failures.py" --self-test)
    add_test(NAME tools.arrow_tick_failures_protocol_optimized
        COMMAND Python3::Interpreter -B -O
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_arrow_tick_failures.py" --self-test)
    set_tests_properties(tools.arrow_tick_failures_protocol
        tools.arrow_tick_failures_protocol_optimized PROPERTIES
        TIMEOUT 90 LABELS "public;unit;tools;world;entity")
endif()
