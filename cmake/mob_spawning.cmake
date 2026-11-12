include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE
    src/entity/mob/spawning.c
    src/world/runtime/chunk_random.c
    src/world/runtime/collision/spawning.c)

if(BUILD_TESTING)
    add_executable(mc_test_mob_spawning tests/entity/test_mob_spawning.c)
    target_include_directories(mc_test_mob_spawning PRIVATE src src/core/java)
    target_link_libraries(mc_test_mob_spawning PRIVATE
        mc_simulation mc_resources mc_c_atomics mc_build_options)
    add_test(NAME entity.mob_spawning COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_mob_spawning.py"
        --executable $<TARGET_FILE:mc_test_mob_spawning>
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/mob-spawning-contracts")
    set_tests_properties(entity.mob_spawning PROPERTIES
        LABELS "public;unit;entity;world;ownership" TIMEOUT 180)
endif()
