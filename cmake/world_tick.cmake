include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE
    src/world/runtime/world_tick.c
    src/world/runtime/world_tick_scheduled.c)

if(BUILD_TESTING)
    add_executable(mc_test_world_tick tests/world/runtime/test_world_tick.c)
    target_link_libraries(mc_test_world_tick PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.tick COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/tick/run.py"
        --native $<TARGET_FILE:mc_test_world_tick>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-tick")
    set_tests_properties(world.tick PROPERTIES LABELS "public;unit;world;ownership" TIMEOUT 120)
endif()
