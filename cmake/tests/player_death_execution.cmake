include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_player_death_execution tests/entity/player/test_death_execution.c)
    target_include_directories(mc_test_player_death_execution PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_death_execution PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    set(_mc_player_death_modes empty full40 notch41 fail-apple math-missing math-limit nullable-world
        preflight lease-source native-stop callback-capture scope-capacity)
    foreach(_mc_player_death_slot RANGE 1 40)
        list(APPEND _mc_player_death_modes "fail-${_mc_player_death_slot}")
    endforeach()
    foreach(_mc_player_death_mode IN LISTS _mc_player_death_modes)
        add_test(NAME "entity.player_death_execution_${_mc_player_death_mode}" COMMAND
            Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_death_execution.py"
            --mode "${_mc_player_death_mode}"
            --executable "$<TARGET_FILE:mc_test_player_death_execution>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-death-execution-controls")
        set_tests_properties("entity.player_death_execution_${_mc_player_death_mode}" PROPERTIES
            LABELS "public;integration;entity;world;ownership;math" TIMEOUT 240)
    endforeach()
    unset(_mc_player_death_mode)
    unset(_mc_player_death_slot)
    unset(_mc_player_death_modes)
endif()
