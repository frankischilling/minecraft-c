include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_simulation AND Python3_Interpreter_FOUND)
    add_executable(mc_test_arrow_hurt_player tests/world/runtime/test_arrow_hurt_player.c)
    target_include_directories(mc_test_arrow_hurt_player PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_hurt_player PRIVATE
        mc_simulation mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    foreach(_mc_arrow_player_mode IN ITEMS live fatal fatal-source wake wake-source-null-list
            wake-bed-foot wake-bed-foot-source wake-bed-ladder wake-bed-ladder-source
            wake-bed-ladder-removal-source wake-bed-torch wake-bed-torch-source
            wake-bed-torch-removal-source)
        add_test(NAME "world.arrow_hurt_player_${_mc_arrow_player_mode}" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_arrow_hurt_player.py"
            --mode "${_mc_arrow_player_mode}" --executable "$<TARGET_FILE:mc_test_arrow_hurt_player>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-hurt-player-controls")
        set_tests_properties("world.arrow_hurt_player_${_mc_arrow_player_mode}" PROPERTIES
            LABELS "public;integration;world;entity;ownership" TIMEOUT 300)
    endforeach()
    unset(_mc_arrow_player_mode)
endif()
