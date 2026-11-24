include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE src/world/runtime/colors.c)
target_link_libraries(mc_simulation PRIVATE mc_math_classes)
if(BUILD_TESTING)
    add_executable(mc_test_world_colors tests/world/runtime/test_colors.c)
    target_link_libraries(mc_test_world_colors PRIVATE mc_simulation mc_build_options)
    add_test(NAME world.colors COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/colors/run_native.py"
        --executable "$<TARGET_FILE:mc_test_world_colors>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-colors")
    set_tests_properties(world.colors PROPERTIES
        LABELS "public;unit;world;client;ownership;replay" TIMEOUT 90)
endif()
