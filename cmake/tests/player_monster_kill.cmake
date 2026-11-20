include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_player_monster_kill tests/entity/player/test_monster_kill.c)
    target_include_directories(mc_test_player_monster_kill PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_monster_kill PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_monster_kill COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_monster_kill.py"
        --executable "$<TARGET_FILE:mc_test_player_monster_kill>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-monster-kill-controls")
    set_tests_properties(entity.player_monster_kill PROPERTIES
        LABELS "public;integration;entity;client;statistics;ownership" TIMEOUT 180)
endif()
