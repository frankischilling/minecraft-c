include_guard(GLOBAL)
target_sources(mc_simulation PRIVATE src/entity/particles/redstone.c)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.redstone_particles_packet COMMAND Python3::Interpreter -B -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/redstone_particles" -p test_packet.py)
    set_tests_properties(tools.redstone_particles_packet PROPERTIES
        LABELS "public;unit;tools;entity;reference" TIMEOUT 90)
endif()

if(BUILD_TESTING AND TARGET mc_client_particles_draw)
    add_executable(mc_test_redstone_particle_reference tests/entity/particles/test_redstone_reference.c)
    target_include_directories(mc_test_redstone_particle_reference PRIVATE
        src src/platform/time src/core/java)
    target_link_libraries(mc_test_redstone_particle_reference PRIVATE
        mc_client_particles_draw mc_simulation mc_resources mc_persistence
        mc_c_atomics mc_math_classes mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_redstone_particle_reference PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME entity.redstone_particle_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/redstone_particles/run_native.py"
        --executable "$<TARGET_FILE:mc_test_redstone_particle_reference>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/redstone-particle-reference-controls")
    set_tests_properties(entity.redstone_particle_reference PROPERTIES
        LABELS "public;integration;client;entity;world;ownership;math;reference" TIMEOUT 240)

    add_executable(mc_test_redstone_particle_draw tests/client/particles/test_redstone_draw.c)
    target_include_directories(mc_test_redstone_particle_draw PRIVATE
        src src/platform/time src/core/java)
    target_link_libraries(mc_test_redstone_particle_draw PRIVATE
        mc_client_particles_draw mc_simulation mc_resources mc_persistence
        mc_c_atomics mc_math_classes mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_redstone_particle_draw PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.redstone_particle_draw_cpu COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_redstone_factory.py"
        --executable "$<TARGET_FILE:mc_test_redstone_particle_draw>" --mode draw
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/redstone-particle-draw-controls")
    set_tests_properties(client.redstone_particle_draw_cpu PROPERTIES
        LABELS "public;integration;client;entity;world;ownership" TIMEOUT 90)
endif()

if(BUILD_TESTING AND TARGET mc_client_render_global)
    add_executable(mc_test_redstone_particle_factory
        tests/client/particles/test_redstone_factory.c)
    target_include_directories(mc_test_redstone_particle_factory PRIVATE
        src src/platform/time src/core/java)
    target_link_libraries(mc_test_redstone_particle_factory PRIVATE
        mc_client_render_global mc_simulation mc_resources mc_persistence
        mc_c_atomics mc_math_classes mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_redstone_particle_factory PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.redstone_particle_factory COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_redstone_factory.py"
        --executable "$<TARGET_FILE:mc_test_redstone_particle_factory>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/redstone-particle-factory-controls")
    set_tests_properties(client.redstone_particle_factory PROPERTIES
        LABELS "public;integration;client;entity;world;ownership" TIMEOUT 90)
endif()
