include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE src/entity/particles/smoke.c src/entity/particles/flame.c)

if(BUILD_TESTING)
    add_test(NAME tools.torch_particles_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/torch_particles" -p test_packet.py)
    set_tests_properties(tools.torch_particles_packet PROPERTIES
        LABELS "public;unit;tools;entity" TIMEOUT 60)
    add_executable(mc_test_torch_particle tests/entity/particles/test_torch.c)
    target_include_directories(mc_test_torch_particle PRIVATE src src/platform/time src/core/java)
    target_link_libraries(mc_test_torch_particle PRIVATE mc_simulation mc_resources
        mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    add_test(NAME entity.torch_particle COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/torch_particles/run_native.py"
        --executable "$<TARGET_FILE:mc_test_torch_particle>" --mode ticks
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/torch-particle-controls")
    set_tests_properties(entity.torch_particle PROPERTIES
        LABELS "public;integration;client;entity;ownership;math" TIMEOUT 240)
endif()

if(BUILD_TESTING AND TARGET mc_client_particles_draw)
    add_executable(mc_test_torch_particle_draw tests/client/particles/test_torch_draw.c)
    target_include_directories(mc_test_torch_particle_draw PRIVATE src src/platform/time src/core/java)
    target_link_libraries(mc_test_torch_particle_draw PRIVATE mc_client_particles_draw mc_simulation
        mc_resources mc_persistence mc_c_atomics mc_math_classes mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_torch_particle_draw PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.torch_particle_draw_cpu COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/torch_particles/run_native.py"
        --executable "$<TARGET_FILE:mc_test_torch_particle_draw>" --mode draw
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/torch-particle-draw-controls")
    set_tests_properties(client.torch_particle_draw_cpu PROPERTIES
        LABELS "public;integration;client;entity;world;ownership" TIMEOUT 240)
endif()
