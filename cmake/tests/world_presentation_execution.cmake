include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_world_presentation_execution
        tests/world/runtime/world_access/test_presentation_execution.c)
    target_include_directories(mc_test_world_presentation_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_presentation_execution PRIVATE
        mc_simulation mc_persistence mc_c_atomics mc_build_options)
    foreach(_mc_presentation_mode IN ITEMS
        sound-scope-drift
        sound-native-stop
        sound-live
        sound-empty
        sound-held-live
        sound-lease
        sound-nodes
        sound-capture-refusal
        sound-admission
        particle-scope-drift
        particle-native-stop
        particle-live
        particle-empty
        particle-held-live
        particle-lease
        particle-nodes
        particle-capture-refusal
        particle-admission
        particle-legacy-scope-drift
        particle-bare-scope-drift
        particle-legacy-live
        particle-bare-live
        particle-legacy-lease
        particle-bare-lease
        particle-legacy-null-lease
        particle-bare-null-native-stop
        particle-legacy-admission
        particle-bare-admission
        entity-detailed-scope-drift
        entity-detailed-native-stop
        entity-detailed-live
        entity-detailed-admission
        entity-detailed-null-scope-drift
        entity-bare-scope-drift
        entity-bare-native-stop
        entity-bare-live
        entity-bare-admission
        entity-bare-null-scope-drift)
        add_test(NAME "world.presentation_execution_${_mc_presentation_mode}" COMMAND
            Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_presentation_execution.py"
            --mode "${_mc_presentation_mode}"
            --executable "$<TARGET_FILE:mc_test_world_presentation_execution>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-presentation-controls")
        set_tests_properties("world.presentation_execution_${_mc_presentation_mode}" PROPERTIES
            LABELS "public;integration;world;ownership;filesystem"
            RESOURCE_LOCK world_presentation_execution TIMEOUT 240)
    endforeach()
    unset(_mc_presentation_mode)
    foreach(_mc_presentation_optimized IN ITEMS normal optimized)
        if(_mc_presentation_optimized STREQUAL "optimized")
            set(_mc_presentation_python_flags -B -O)
        else()
            set(_mc_presentation_python_flags -B)
        endif()
        add_test(NAME "world.presentation_controller_${_mc_presentation_optimized}" COMMAND
            Python3::Interpreter ${_mc_presentation_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_presentation_execution.py"
            --self-test)
        set_tests_properties("world.presentation_controller_${_mc_presentation_optimized}" PROPERTIES
            LABELS "public;unit;world;ownership" TIMEOUT 30)
    endforeach()
    unset(_mc_presentation_optimized)
    unset(_mc_presentation_python_flags)
endif()
