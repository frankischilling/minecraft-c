include_guard(GLOBAL)

# These Inventory children call actual Stack durability/statistic producers,
# so they live with gameplay rather than adding a reverse mc_inventory link.
target_sources(mc_simulation PRIVATE
    src/entity/player/damage.c
    src/entity/player/death.c
    src/inventory/runtime/armor.c
    src/inventory/runtime/scatter.c)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_executable(mc_test_player_death tests/entity/player/test_death.c)
    target_include_directories(mc_test_player_death PRIVATE src src/platform/time)
    target_link_libraries(mc_test_player_death PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_death COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_player_death.py"
        --executable "$<TARGET_FILE:mc_test_player_death>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-death-contracts")
    set_tests_properties(entity.player_death PROPERTIES
        LABELS "public;integration;entity;inventory;client;ownership" TIMEOUT 180)
    add_executable(mc_test_player_hurt_original tests/entity/player/test_hurt_original.c)
    target_include_directories(mc_test_player_hurt_original PRIVATE src/platform/time)
    target_link_libraries(mc_test_player_hurt_original PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.player_hurt COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_hurt/run_native.py"
        --executable "$<TARGET_FILE:mc_test_player_hurt_original>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/player-hurt-contracts")
    set_tests_properties(entity.player_hurt PROPERTIES
        LABELS "public;integration;entity;inventory;client;ownership" TIMEOUT 180)
    add_test(NAME tools.player_hurt_packet COMMAND Python3::Interpreter
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_hurt/test_packet.py")
    add_test(NAME tools.player_hurt_packet_optimized COMMAND Python3::Interpreter -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/player_hurt/test_packet.py")
    set_tests_properties(tools.player_hurt_packet tools.player_hurt_packet_optimized PROPERTIES
        LABELS "public;unit;tools;entity" TIMEOUT 60)
endif()
