include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE src/world/runtime/client_tick_state.c)

if(BUILD_TESTING)
    add_executable(mc_test_world_client_tick_state tests/world/runtime/test_client_tick_state.c)
    target_link_libraries(mc_test_world_client_tick_state PRIVATE mc_simulation mc_build_options)
    add_test(NAME world.client_tick_state COMMAND mc_test_world_client_tick_state)
    set_tests_properties(world.client_tick_state PROPERTIES
        LABELS "public;unit;world;ownership" TIMEOUT 60)
endif()

if(TARGET mc_test_world_selection_loop)
    target_sources(mc_test_world_selection_loop PRIVATE tests/client/world/test_refresh_entity.c)
endif()
