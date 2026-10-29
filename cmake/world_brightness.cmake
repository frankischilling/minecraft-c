include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE src/world/runtime/brightness.c)

if(BUILD_TESTING)
    add_executable(mc_test_world_brightness tests/world/runtime/test_brightness.c)
    target_link_libraries(mc_test_world_brightness PRIVATE mc_simulation mc_build_options)
    add_test(NAME world.brightness COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/brightness/run_native.py"
        --executable "$<TARGET_FILE:mc_test_world_brightness>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-brightness")
    set_tests_properties(world.brightness PROPERTIES
        LABELS "public;unit;world;ownership;replay" TIMEOUT 60)
endif()
