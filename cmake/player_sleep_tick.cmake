include_guard(GLOBAL)

if(BUILD_TESTING)
    foreach(mode IN ITEMS normal optimized)
        set(_mc_sleep_tick_python_options -B)
        if(mode STREQUAL "optimized")
            list(APPEND _mc_sleep_tick_python_options -O)
        endif()
        add_test(NAME "tools.player_sleep_tick_packet_${mode}" COMMAND Python3::Interpreter
            ${_mc_sleep_tick_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_sleep_tick/test_packet.py")
        add_test(NAME "tools.player_sleep_tick_controls_${mode}" COMMAND Python3::Interpreter
            ${_mc_sleep_tick_python_options}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_sleep_tick.py" --controls)
        set_tests_properties("tools.player_sleep_tick_packet_${mode}"
            "tools.player_sleep_tick_controls_${mode}" PROPERTIES
            LABELS "public;unit;entity;world;reference;tools" TIMEOUT 90)
    endforeach()
    add_executable(mc_test_player_sleep_tick tests/entity/player/test_sleep_tick.c)
    target_include_directories(mc_test_player_sleep_tick PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_sleep_tick PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_sleep_tick COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_sleep_tick.py"
        --executable "$<TARGET_FILE:mc_test_player_sleep_tick>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-sleep-tick")
    set_tests_properties(entity.player_sleep_tick PROPERTIES
        LABELS "public;integration;entity;world;reference;replay;ownership" TIMEOUT 240)
endif()

if(BUILD_TESTING AND TARGET mc_client_runtime)
    add_executable(mc_test_player_sleep_tick_cleanup
        tests/client/gameplay/test_player_sleep_tick_cleanup.c)
    target_include_directories(mc_test_player_sleep_tick_cleanup PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_sleep_tick_cleanup PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_player_sleep_tick_cleanup PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_player_sleep_tick_cleanup POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_player_sleep_tick_cleanup>")
    endif()
    add_test(NAME client.player_sleep_tick_cleanup COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_player_sleep_tick_cleanup.py"
        --executable "$<TARGET_FILE:mc_test_player_sleep_tick_cleanup>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-sleep-tick-cleanup")
    set_tests_properties(client.player_sleep_tick_cleanup PROPERTIES
        LABELS "public;integration;client;entity;world;ownership" TIMEOUT 240)
endif()
