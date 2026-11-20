if(BUILD_TESTING AND TARGET mc_client_particles_draw)
    add_executable(mc_test_effect_renderer_draw tests/client/particles/test_render.c)
    target_include_directories(mc_test_effect_renderer_draw PRIVATE src)
    target_link_libraries(mc_test_effect_renderer_draw PRIVATE
        mc_client_particles_draw mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_effect_renderer_draw PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.effect_renderer_draw_cpu COMMAND mc_test_effect_renderer_draw)
    set_tests_properties(client.effect_renderer_draw_cpu PROPERTIES
        LABELS "public;unit;client;entity;ownership" TIMEOUT 90)
endif()
