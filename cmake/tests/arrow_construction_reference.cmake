include_guard(GLOBAL)

if(BUILD_TESTING)
    add_executable(mc_test_arrow_construction_reference
        tests/entity/test_arrow_construction_reference.c)
    target_include_directories(mc_test_arrow_construction_reference PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_construction_reference PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.arrow_construction_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/arrow_construction/run_native.py"
        --executable "$<TARGET_FILE:mc_test_arrow_construction_reference>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-construction-reference-scenes")
    set_tests_properties(entity.arrow_construction_reference PROPERTIES
        LABELS "public;integration;entity;reference;random;ownership" TIMEOUT 300)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_arrow_reference_python_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_arrow_reference_python_options -O)
        endif()
        add_test(NAME "tools.arrow_construction_reference_${mode}"
            COMMAND Python3::Interpreter ${_mc_arrow_reference_python_options}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/arrow_construction/test_packet.py")
        set_tests_properties("tools.arrow_construction_reference_${mode}" PROPERTIES
            LABELS "public;unit;entity;reference;random;tools" TIMEOUT 90)
    endforeach()
endif()
