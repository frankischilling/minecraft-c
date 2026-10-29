if(BUILD_TESTING AND TARGET mc_client_render_global)
    option(MC_REQUIRE_TERRAIN_OCCLUSION_TEST
        "Require supported-device occlusion and all 35 terrain submission rows" OFF)
    add_test(NAME tools.submit_capability COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_submit_capability.py")
    set_tests_properties(tools.submit_capability PROPERTIES
        LABELS "public;unit;tools;client;replay" TIMEOUT 30)
    add_executable(mc_test_render_global_occlusion tests/client/world/test_render_global_occlusion.c)
    target_include_directories(mc_test_render_global_occlusion PRIVATE src src/platform/time)
    target_link_libraries(mc_test_render_global_occlusion PRIVATE
        mc_client_render_global mc_simulation mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_global_occlusion PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_global_occlusion COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_global_occlusion/run_native.py"
        --executable "$<TARGET_FILE:mc_test_render_global_occlusion>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-global-occlusion")
    set_tests_properties(client.render_global_occlusion PROPERTIES
        LABELS "public;unit;client;world;ownership;replay" TIMEOUT 90)
    add_executable(mc_test_render_global_submit tests/client/world/test_render_global_submit.c)
    target_include_directories(mc_test_render_global_submit PRIVATE src src/platform/time)
    target_link_libraries(mc_test_render_global_submit PRIVATE
        mc_client_render_global mc_simulation mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_global_submit PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        set(mc_submit_capability_arguments)
        if(MC_REQUIRE_TERRAIN_OCCLUSION_TEST)
            list(APPEND mc_submit_capability_arguments --require-occlusion)
        endif()
        add_test(NAME client.render_global_submit COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_global_submit/run_native.py"
            ${mc_submit_capability_arguments}
            --executable "$<TARGET_FILE:mc_test_render_global_submit>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-global-submit")
        set_tests_properties(client.render_global_submit PROPERTIES
            LABELS "public;integration;client;world;graphics;ownership;replay" TIMEOUT 240)
    endif()
endif()
