include_guard(GLOBAL)

# EW belongs to BlockAccess and World path requests as well as rendering.
target_sources(mc_simulation PRIVATE
    src/world/runtime/chunk_cache.c
    src/world/runtime/path.c
    src/world/path/common.c
    src/world/path/path.c
    src/world/path/point.c
    src/world/path/point_map.c
    src/world/path/heap.c
    src/world/path/clearance.c
    src/world/path/neighbors.c
    src/world/path/finder.c)

if(BUILD_TESTING)
    add_executable(mc_test_path tests/world/runtime/test_path.c)
    target_include_directories(mc_test_path PRIVATE src src/core/java)
    target_link_libraries(mc_test_path PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME world.path COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_path.py"
        --executable $<TARGET_FILE:mc_test_path>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-path-contracts")
    set_tests_properties(world.path PROPERTIES
        LABELS "public;unit;world;entity;ownership" TIMEOUT 420)
endif()
