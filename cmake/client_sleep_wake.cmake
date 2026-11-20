include_guard(GLOBAL)

if(BUILD_TESTING)
    foreach(suffix IN ITEMS normal optimized)
        set(_mc_sleep_wake_python_options -B)
        if(suffix STREQUAL "optimized")
            list(APPEND _mc_sleep_wake_python_options -O)
        endif()
        add_test(NAME "tools.player_sleep_wake_packet_${suffix}" COMMAND Python3::Interpreter
            ${_mc_sleep_wake_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_sleep_wake/test_packet.py")
        set_tests_properties("tools.player_sleep_wake_packet_${suffix}" PROPERTIES
            LABELS "public;unit;tools;entity;world;reference" TIMEOUT 90)
        add_test(NAME "tools.player_sleep_wake_controls_${suffix}" COMMAND Python3::Interpreter
            ${_mc_sleep_wake_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_sleep_wake.py" --controls)
        set_tests_properties("tools.player_sleep_wake_controls_${suffix}" PROPERTIES
            LABELS "public;unit;tools;entity;reference" TIMEOUT 90)
    endforeach()

    add_executable(mc_test_player_sleep_wake tests/entity/player/test_sleep_wake.c)
    target_include_directories(mc_test_player_sleep_wake PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_sleep_wake PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_sleep_wake COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_sleep_wake.py"
        --executable "$<TARGET_FILE:mc_test_player_sleep_wake>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-sleep-wake")
    set_tests_properties(entity.player_sleep_wake PROPERTIES
        LABELS "public;integration;entity;world;ownership;reference;replay" TIMEOUT 240)

    foreach(name IN ITEMS bed_state sleep_tick)
        add_executable("mc_test_world_${name}" "tests/world/runtime/test_${name}.c")
        target_include_directories("mc_test_world_${name}" PRIVATE src src/platform/time)
        target_link_libraries("mc_test_world_${name}" PRIVATE
            mc_simulation mc_resources mc_c_atomics mc_build_options)
        add_test(NAME "world.${name}" COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_${name}.py"
            --executable "$<TARGET_FILE:mc_test_world_${name}>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/${name}-contracts")
        set_tests_properties("world.${name}" PROPERTIES
            LABELS "public;integration;world;entity;ownership" TIMEOUT 240)
    endforeach()
endif()
