if(TARGET mc_simulation)
    target_sources(mc_simulation PRIVATE src/entity/living/view.c)
    target_link_libraries(mc_simulation PRIVATE mc_math_classes)
    if(BUILD_TESTING)
        add_executable(mc_test_living_view tests/entity/living/test_view.c)
        target_include_directories(mc_test_living_view PRIVATE src)
        if(MSVC)
            # Standard C numeric-only fixture scanner; bounded conversions below.
            target_compile_definitions(mc_test_living_view PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
        target_link_libraries(mc_test_living_view PRIVATE
            mc_simulation mc_resources mc_math_classes mc_client_singleplayer_controller
            mc_c_atomics mc_build_options)
        add_test(NAME entity.living_view COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/entity/test_living_view.py"
            --executable "$<TARGET_FILE:mc_test_living_view>"
            --vectors "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/entity/view/vectors.tsv")
        set_tests_properties(entity.living_view PROPERTIES
            LABELS "public;unit;entity;ownership;filesystem" TIMEOUT 100)
    endif()
endif()
