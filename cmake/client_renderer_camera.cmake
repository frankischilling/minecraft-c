if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE src/client/world/entity_renderer/camera.c)
    if(BUILD_TESTING AND TARGET mc_client_render_global)
        add_executable(mc_test_renderer_camera tests/client/world/test_renderer_camera.c)
        target_include_directories(mc_test_renderer_camera PRIVATE src src/platform/time)
        target_link_libraries(mc_test_renderer_camera PRIVATE mc_client_entity_renderer
            mc_client_render_global mc_client_singleplayer_controller mc_simulation
            mc_resources mc_math_classes mc_c_atomics mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_renderer_camera PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.renderer_camera COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/renderer_camera/run_native.py"
                --executable "$<TARGET_FILE:mc_test_renderer_camera>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/renderer-camera")
            set_tests_properties(client.renderer_camera PROPERTIES
                LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 120)
        endif()
    endif()
endif()
