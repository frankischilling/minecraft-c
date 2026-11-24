include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_client_particles_draw)
    add_executable(mc_test_explosion_draw tests/client/particles/test_explosion_draw.c)
    target_include_directories(mc_test_explosion_draw PRIVATE src src/core/java src/platform/time)
    target_link_libraries(mc_test_explosion_draw PRIVATE mc_client_particles_draw mc_simulation
        mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_explosion_draw PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.explosion_draw_cpu COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_explosion_draw.py"
        --executable "$<TARGET_FILE:mc_test_explosion_draw>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/explosion-draw-controls")
    set_tests_properties(client.explosion_draw_cpu PROPERTIES
        LABELS "public;integration;client;entity;world;ownership" TIMEOUT 240)
endif()
