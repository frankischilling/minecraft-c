include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE src/entity/mount/mount.c)

if(BUILD_TESTING)
    add_executable(mc_test_entity_mount tests/entity/mount/test_mount.c)
    target_include_directories(mc_test_entity_mount PRIVATE src src/platform/time)
    target_link_libraries(mc_test_entity_mount PRIVATE mc_simulation mc_resources
        mc_c_atomics mc_build_options)
    add_test(NAME entity.mount COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_player_drop.py"
        --action mount --executable "$<TARGET_FILE:mc_test_entity_mount>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/entity-mount-controls")
    set_tests_properties(entity.mount PROPERTIES
        LABELS "public;integration;client;entity;world;ownership" TIMEOUT 180)
endif()
