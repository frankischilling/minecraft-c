include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE
    src/world/runtime/world_tick_random.c
    src/world/runtime/chunk_tick.c
    src/world/runtime/mutation/random_blocks.c
    src/entity/lightning/construction.c)

if(BUILD_TESTING)
    add_executable(mc_test_fixed_manager_constructor
        tests/world/dimension/test_fixed_manager_constructor.c)
    target_include_directories(mc_test_fixed_manager_constructor PRIVATE src src/core/java)
    target_link_libraries(mc_test_fixed_manager_constructor PRIVATE
        mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.fixed_manager_constructor COMMAND mc_test_fixed_manager_constructor)
    set_tests_properties(world.fixed_manager_constructor PROPERTIES
        LABELS "public;unit;world;ownership" TIMEOUT 60)

    add_executable(mc_test_lightning_construction tests/entity/test_lightning_construction.c)
    target_include_directories(mc_test_lightning_construction PRIVATE src src/core/java)
    target_link_libraries(mc_test_lightning_construction PRIVATE
        mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.lightning_construction COMMAND mc_test_lightning_construction)
    set_tests_properties(entity.lightning_construction PROPERTIES
        LABELS "public;unit;entity;world;ownership" TIMEOUT 60)
    add_test(NAME entity.lightning_construction_probe COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_lightning_construction.py"
        --executable $<TARGET_FILE:mc_test_lightning_construction>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lightning-construction-contracts")
    set_tests_properties(entity.lightning_construction_probe PROPERTIES
        LABELS "public;unit;entity;world;ownership" TIMEOUT 60)

    add_executable(mc_test_chunk_tick tests/world/runtime/test_chunk_tick.c)
    target_include_directories(mc_test_chunk_tick PRIVATE src src/core/java)
    target_link_libraries(mc_test_chunk_tick PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME world.chunk_tick COMMAND mc_test_chunk_tick "${CMAKE_CURRENT_BINARY_DIR}/chunk-tick")
    set_tests_properties(world.chunk_tick PROPERTIES
        LABELS "public;unit;world;ownership" TIMEOUT 120)
    add_executable(mc_test_random_blocks tests/world/runtime/test_random_blocks.c)
    target_include_directories(mc_test_random_blocks PRIVATE src src/core/java)
    target_link_libraries(mc_test_random_blocks PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    string(HEX "${CMAKE_CURRENT_BINARY_DIR}/world-random-blocks" _mc_random_blocks_root)
    add_test(NAME world.random_blocks COMMAND mc_test_random_blocks
        world-random-blocks "${_mc_random_blocks_root}")
    set_tests_properties(world.random_blocks PROPERTIES
        LABELS "public;unit;world;ownership" TIMEOUT 180)
    add_test(NAME world.random_blocks_probe COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_random_blocks.py"
        --executable $<TARGET_FILE:mc_test_random_blocks>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-random-block-probes")
    set_tests_properties(world.random_blocks_probe PROPERTIES
        LABELS "public;unit;world;ownership" TIMEOUT 420)

    add_executable(mc_test_world_random_tick tests/world/runtime/test_world_random_tick.c)
    target_include_directories(mc_test_world_random_tick PRIVATE src src/core/java)
    target_link_libraries(mc_test_world_random_tick PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME world.random_tick COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_random_tick.py"
        --executable $<TARGET_FILE:mc_test_world_random_tick>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-random-tick-contracts")
    set_tests_properties(world.random_tick PROPERTIES
        LABELS "public;unit;world;ownership" TIMEOUT 300)
endif()
