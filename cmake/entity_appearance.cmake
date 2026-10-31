if(TARGET mc_simulation)
    target_sources(mc_simulation PRIVATE src/entity/player/appearance.c)
    if(BUILD_TESTING)
        add_executable(mc_test_entity_appearance tests/entity/player/test_appearance.c)
        target_link_libraries(mc_test_entity_appearance PRIVATE
            mc_simulation mc_c_atomics mc_build_options)
        add_test(NAME entity.appearance COMMAND mc_test_entity_appearance)
        set_tests_properties(entity.appearance PROPERTIES
            LABELS "public;unit;entity;ownership" TIMEOUT 90)
    endif()
endif()
