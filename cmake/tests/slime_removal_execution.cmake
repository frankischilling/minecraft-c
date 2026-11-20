include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_slime_removal_execution
        tests/world/runtime/test_slime_removal_execution.c)
    target_include_directories(mc_test_slime_removal_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_slime_removal_execution PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    foreach(_mc_slime_owned_mode IN ITEMS ready sticky-math scope-drift native-close legacy-release
            owned-release owned-final-release child-allocation)
        add_test(NAME "entity.slime_removal_execution_${_mc_slime_owned_mode}" COMMAND
            Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_slime_removal_execution.py"
            --mode "${_mc_slime_owned_mode}"
            --executable "$<TARGET_FILE:mc_test_slime_removal_execution>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/slime-removal-execution-controls")
        set_tests_properties("entity.slime_removal_execution_${_mc_slime_owned_mode}" PROPERTIES
            LABELS "public;integration;entity;world;ownership;math" TIMEOUT 300)
    endforeach()
    unset(_mc_slime_owned_mode)
endif()
