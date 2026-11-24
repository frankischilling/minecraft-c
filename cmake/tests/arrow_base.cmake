include_guard(GLOBAL)

if(BUILD_TESTING)
    add_executable(mc_test_math_execution tests/core/math/test_execution.c)
    target_link_libraries(mc_test_math_execution PRIVATE mc_core mc_build_options)
    add_test(NAME core.math_execution COMMAND mc_test_math_execution)
    set_tests_properties(core.math_execution PROPERTIES LABELS "public;unit;math;ownership" TIMEOUT 90)

    add_executable(mc_test_arrow_base tests/entity/test_arrow_base.c)
    target_include_directories(mc_test_arrow_base PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_base PRIVATE mc_simulation mc_math_classes mc_c_atomics mc_build_options)
    add_test(NAME entity.arrow_base COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_arrow_base.py"
        --executable "$<TARGET_FILE:mc_test_arrow_base>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-base-controls")
    set_tests_properties(entity.arrow_base PROPERTIES LABELS "public;integration;entity;world;math;ownership" TIMEOUT 240)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_arrow_base_python -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_arrow_base_python -O)
        endif()
        add_test(NAME "tools.arrow_base_controls_${mode}" COMMAND Python3::Interpreter ${_mc_arrow_base_python}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_arrow_base.py" --self-test)
        set_tests_properties("tools.arrow_base_controls_${mode}" PROPERTIES LABELS "public;unit;tools;entity" TIMEOUT 90)
    endforeach()
endif()
