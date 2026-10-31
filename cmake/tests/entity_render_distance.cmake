if(BUILD_TESTING AND TARGET mc_simulation)
    add_executable(mc_test_entity_render_distance
        tests/entity/render_distance/test_render_distance.c)
    target_link_libraries(mc_test_entity_render_distance PRIVATE
        mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME entity.render_distance COMMAND mc_test_entity_render_distance)
    set_tests_properties(entity.render_distance PROPERTIES
        LABELS "public;unit;entity;ownership" TIMEOUT 60)
endif()
