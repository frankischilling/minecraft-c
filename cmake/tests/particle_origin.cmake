if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_particle_origin tests/entity/particles/test_origin.c)
    target_link_libraries(mc_test_particle_origin PRIVATE
        mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.particle_origin COMMAND mc_test_particle_origin)
    set_tests_properties(entity.particle_origin PROPERTIES
        LABELS "public;unit;entity;ownership" TIMEOUT 60)
endif()
