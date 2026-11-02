include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE src/entity/particles/explosion.c)
if(BUILD_TESTING)
    add_test(NAME tools.explosion_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/explosion_particle/test_packet.py")
    set_tests_properties(tools.explosion_packet PROPERTIES
        LABELS "public;unit;tools;entity" TIMEOUT 60)
    add_executable(mc_test_explosion_particle tests/entity/particles/test_explosion.c)
    target_include_directories(mc_test_explosion_particle PRIVATE src src/platform/time src/core/java)
    target_link_libraries(mc_test_explosion_particle PRIVATE mc_simulation mc_resources
        mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.explosion_particle COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_explosion_particle.py"
        --executable "$<TARGET_FILE:mc_test_explosion_particle>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/explosion-particle-controls")
    set_tests_properties(entity.explosion_particle PROPERTIES
        LABELS "public;integration;client;entity;ownership;math" TIMEOUT 240)
endif()
