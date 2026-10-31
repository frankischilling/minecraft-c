# Actual CPU-side os.c()V and os.a()V, after both existing owner targets.
include_guard(GLOBAL)
if(NOT TARGET mc_client_singleplayer_controller OR NOT TARGET mc_game_settings)
    message(FATAL_ERROR "Singleplayer tick requires canonical controller and SoundManager owners")
endif()
target_sources(mc_client_singleplayer_controller PRIVATE
    src/client/controller/singleplayer_tick.c)
target_link_libraries(mc_client_singleplayer_controller PRIVATE mc_game_settings)

if(BUILD_TESTING)
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_executable(mc_test_singleplayer_tick tests/client/controller/test_singleplayer_tick.c)
    target_include_directories(mc_test_singleplayer_tick PRIVATE src/platform/time)
    target_link_libraries(mc_test_singleplayer_tick PRIVATE
        mc_client_singleplayer_controller mc_game_settings mc_c_atomics mc_build_options)
    add_test(NAME client.singleplayer_tick
        COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/controller_tick/run_native.py"
            --executable "$<TARGET_FILE:mc_test_singleplayer_tick>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/singleplayer-tick")
    set_tests_properties(client.singleplayer_tick PROPERTIES
        LABELS "public;unit;client;ownership;replay" TIMEOUT 60)
endif()
