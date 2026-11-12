include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_player_drop_execution tests/entity/player/test_drop_execution.c)
    target_include_directories(mc_test_player_drop_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_drop_execution PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    foreach(_mc_player_drop_mode IN ITEMS scatter normal selected null-stack nullable-world absent
            math-missing-scatter math-missing-normal math-limit-scatter math-limit-normal preflight
            lease-source callback-capture native-close scope-drift scope-capacity fresh-world)
        add_test(NAME "entity.player_drop_execution_${_mc_player_drop_mode}" COMMAND
            Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_drop_execution.py"
            --mode "${_mc_player_drop_mode}"
            --executable "$<TARGET_FILE:mc_test_player_drop_execution>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-drop-execution-controls")
        set_tests_properties("entity.player_drop_execution_${_mc_player_drop_mode}" PROPERTIES
            LABELS "public;integration;entity;world;ownership;math" TIMEOUT 240)
    endforeach()
    unset(_mc_player_drop_mode)
endif()
