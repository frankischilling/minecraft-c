include_guard(GLOBAL)

target_sources(mc_simulation PRIVATE
    src/world/runtime/random_display.c
    src/world/runtime/block_random_display.c)

if(BUILD_TESTING)
    add_executable(mc_test_random_display tests/world/runtime/test_random_display.c)
    target_include_directories(mc_test_random_display PRIVATE src src/core/java)
    target_link_libraries(mc_test_random_display PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME world.random_display COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_random_display.py"
        --executable $<TARGET_FILE:mc_test_random_display>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-random-display-contracts")
    set_tests_properties(world.random_display PROPERTIES
        LABELS "public;unit;world;ownership" TIMEOUT 420)
endif()
