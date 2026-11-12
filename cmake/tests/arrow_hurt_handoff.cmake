include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_arrow_hurt_handoff tests/world/runtime/test_arrow_hurt_handoff.c)
    target_include_directories(mc_test_arrow_hurt_handoff PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_hurt_handoff PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    foreach(_mc_handoff_mode IN ITEMS
            source-sound source-unknown source-exact-budget native-close scope-drift
            workspace-limit fatal-publication reject wolf impact-null-list retire-provenance)
        add_test(NAME "world.arrow_hurt_handoff_${_mc_handoff_mode}" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_arrow_hurt_handoff.py"
            --mode "${_mc_handoff_mode}" --executable "$<TARGET_FILE:mc_test_arrow_hurt_handoff>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-hurt-handoff-controls")
        set_tests_properties("world.arrow_hurt_handoff_${_mc_handoff_mode}" PROPERTIES
            LABELS "public;integration;world;entity;ownership" TIMEOUT 300)
    endforeach()
    unset(_mc_handoff_mode)
endif()
