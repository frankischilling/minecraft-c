if(BUILD_TESTING AND TARGET mc_client_render_global AND TARGET mc_client_entity_renderer)
    add_executable(mc_test_render_global_recenter tests/client/world/test_render_global_recenter.c)
    target_include_directories(mc_test_render_global_recenter PRIVATE src src/platform/time)
    target_link_libraries(mc_test_render_global_recenter PRIVATE
        mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
        mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_global_recenter PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_global_recenter COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_global_recenter/run_native.py"
        --executable "$<TARGET_FILE:mc_test_render_global_recenter>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-global-recenter")
    set_tests_properties(client.render_global_recenter PROPERTIES
        LABELS "public;unit;client;world;ownership;replay" TIMEOUT 120)
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.render_global_recenter_device COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_global_recenter/run_native.py"
            --executable "$<TARGET_FILE:mc_test_render_global_recenter>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-global-recenter-device" --device)
        set_tests_properties(client.render_global_recenter_device PROPERTIES
            LABELS "public;integration;client;world;graphics;ownership;replay" TIMEOUT 180)
    endif()
endif()
