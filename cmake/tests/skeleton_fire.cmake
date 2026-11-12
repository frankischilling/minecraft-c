include_guard(GLOBAL)

if(BUILD_TESTING)
    add_executable(mc_test_skeleton_fire tests/entity/test_skeleton_fire.c)
    target_include_directories(mc_test_skeleton_fire PRIVATE src src/platform/time)
    target_link_libraries(mc_test_skeleton_fire PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.skeleton_fire COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_skeleton_fire.py"
        --executable "$<TARGET_FILE:mc_test_skeleton_fire>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/skeleton-fire-controls")
    set_tests_properties(entity.skeleton_fire PROPERTIES
        LABELS "public;integration;entity;world;ownership;random" TIMEOUT 300)

    foreach(mode IN ITEMS normal optimized)
        set(_mc_skeleton_python_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_skeleton_python_options -O)
        endif()
        add_test(NAME "tools.skeleton_fire_controls_${mode}"
            COMMAND Python3::Interpreter ${_mc_skeleton_python_options}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_skeleton_fire.py"
                --self-test)
        set_tests_properties("tools.skeleton_fire_controls_${mode}" PROPERTIES
            LABELS "public;unit;entity;ownership;tools" TIMEOUT 90)
    endforeach()

    add_executable(mc_test_skeleton_fire_reference tests/entity/test_skeleton_fire_reference.c)
    target_include_directories(mc_test_skeleton_fire_reference PRIVATE src src/platform/time)
    target_link_libraries(mc_test_skeleton_fire_reference PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.skeleton_fire_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/skeleton_fire/run_native.py"
        --executable "$<TARGET_FILE:mc_test_skeleton_fire_reference>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/skeleton-fire-reference-scenes")
    set_tests_properties(entity.skeleton_fire_reference PROPERTIES
        LABELS "public;integration;entity;reference;random;ownership" TIMEOUT 300)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_skeleton_reference_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_skeleton_reference_options -O)
        endif()
        add_test(NAME "tools.skeleton_fire_reference_${mode}"
            COMMAND Python3::Interpreter ${_mc_skeleton_reference_options}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/skeleton_fire/test_packet.py")
        set_tests_properties("tools.skeleton_fire_reference_${mode}" PROPERTIES
            LABELS "public;unit;entity;reference;random;tools" TIMEOUT 90)
    endforeach()
endif()
