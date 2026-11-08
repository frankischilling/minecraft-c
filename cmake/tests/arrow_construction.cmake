include_guard(GLOBAL)

if(BUILD_TESTING)
    add_executable(mc_test_arrow_construction tests/entity/test_arrow_construction.c)
    target_include_directories(mc_test_arrow_construction PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_construction PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.arrow_construction COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_arrow_construction.py"
        --executable "$<TARGET_FILE:mc_test_arrow_construction>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-construction-controls")
    set_tests_properties(entity.arrow_construction PROPERTIES
        LABELS "public;integration;entity;world;ownership" TIMEOUT 240)

    foreach(mode IN ITEMS normal optimized)
        set(_mc_arrow_python_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_arrow_python_options -O)
        endif()
        add_test(NAME "tools.arrow_construction_controls_${mode}"
            COMMAND Python3::Interpreter ${_mc_arrow_python_options}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_arrow_construction.py"
                --self-test)
        set_tests_properties("tools.arrow_construction_controls_${mode}" PROPERTIES
            LABELS "public;unit;entity;ownership;tools" TIMEOUT 90)
    endforeach()
endif()
