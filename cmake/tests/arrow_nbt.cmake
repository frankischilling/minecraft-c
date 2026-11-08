include_guard(GLOBAL)

target_sources(mc_simulation PRIVATE src/entity/arrow/nbt.c)

if(BUILD_TESTING)
    add_executable(mc_test_arrow_nbt tests/entity/test_arrow_nbt.c)
    target_include_directories(mc_test_arrow_nbt PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_nbt PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.arrow_nbt COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_arrow_nbt.py"
        --executable "$<TARGET_FILE:mc_test_arrow_nbt>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-nbt-controls")
    set_tests_properties(entity.arrow_nbt PROPERTIES
        LABELS "public;integration;entity;world;ownership;storage" TIMEOUT 300)

    foreach(mode IN ITEMS normal optimized)
        set(_mc_arrow_nbt_python_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_arrow_nbt_python_options -O)
        endif()
        add_test(NAME "tools.arrow_nbt_controls_${mode}"
            COMMAND Python3::Interpreter ${_mc_arrow_nbt_python_options}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_arrow_nbt.py"
                --self-test)
        set_tests_properties("tools.arrow_nbt_controls_${mode}" PROPERTIES
            LABELS "public;unit;entity;ownership;storage;tools" TIMEOUT 90)
    endforeach()
endif()
