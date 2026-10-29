if(BUILD_TESTING AND TARGET mc_client_render_global)
    add_executable(mc_test_render_global_submit_cpu tests/client/world/test_render_global_submit_cpu.c)
    target_include_directories(mc_test_render_global_submit_cpu PRIVATE src src/platform/time)
    target_link_libraries(mc_test_render_global_submit_cpu PRIVATE
        mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
        mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_global_submit_cpu PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_global_submit_cpu COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_global_submit_cpu/run_native.py"
        --executable "$<TARGET_FILE:mc_test_render_global_submit_cpu>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-global-submit-cpu")
    set_tests_properties(client.render_global_submit_cpu PROPERTIES
        LABELS "public;unit;client;world;ownership;replay" TIMEOUT 120)
endif()
