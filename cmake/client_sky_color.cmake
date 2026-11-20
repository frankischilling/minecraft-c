include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE src/world/dimension/raw_temperature.c)
target_sources(mc_registry PRIVATE src/world/biome/sky_color.c)
if(UNIX)
    target_link_libraries(mc_registry PRIVATE m)
endif()
if(BUILD_TESTING)
    add_executable(mc_test_sky_leaves tests/world/dimension/test_sky_leaves.c)
    target_include_directories(mc_test_sky_leaves PRIVATE src)
    target_link_libraries(mc_test_sky_leaves PRIVATE mc_simulation mc_registry mc_resources
        mc_math_classes mc_build_options)
    add_test(NAME world.sky_leaves COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/sky_leaves/run_native.py"
        --executable "$<TARGET_FILE:mc_test_sky_leaves>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/sky-leaves")
    set_tests_properties(world.sky_leaves PROPERTIES LABELS "public;unit;world;ownership;replay"
        TIMEOUT 60)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_sky_leaves PROPERTY LINKER_LANGUAGE CXX)
    endif()
endif()
