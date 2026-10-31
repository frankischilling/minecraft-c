if(TARGET mc_entity_classes)
    target_sources(mc_entity_classes PRIVATE src/entity/hit_result/kind.c)
endif()
if(TARGET mc_simulation)
    target_sources(mc_simulation PRIVATE
        src/entity/hit_result/owner.c
        src/entity/hit_result/pickability.c
        src/entity/bounds/intercept.c)
    if(BUILD_TESTING)
        add_executable(mc_test_entity_hit_result tests/entity/hit_result/test_hit_result.c)
        target_link_libraries(mc_test_entity_hit_result PRIVATE
            mc_simulation mc_c_atomics mc_build_options)
        add_test(NAME entity.hit_result COMMAND mc_test_entity_hit_result
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/hit_result/finite.tsv")
        set_tests_properties(entity.hit_result PROPERTIES
            LABELS "public;unit;entity;ownership;replay" TIMEOUT 90)
    endif()
endif()
