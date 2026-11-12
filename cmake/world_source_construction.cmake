# Retained source HC/fm/ld bridge inside the existing canonical World constructor.
if(TARGET mc_simulation)
    target_sources(mc_simulation PRIVATE src/world/runtime/source_construction.c)
    if(BUILD_TESTING)
        add_executable(mc_test_source_world_construction
            tests/world/runtime/test_source_world_construction.c)
        target_link_libraries(mc_test_source_world_construction PRIVATE mc_simulation
            mc_persistence mc_c_atomics mc_build_options)
        add_test(NAME world.source_construction COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_source_world_construction.py"
            --executable "$<TARGET_FILE:mc_test_source_world_construction>")
        set_tests_properties(world.source_construction PROPERTIES
            LABELS "public;integration;world;storage;ownership" TIMEOUT 180)
    endif()
endif()
