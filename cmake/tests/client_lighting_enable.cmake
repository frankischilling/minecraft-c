if(BUILD_TESTING AND TARGET mc_client_render_textures)
    add_executable(mc_test_lighting_enable tests/client/render_engine/test_lighting_enable.c)
    target_include_directories(mc_test_lighting_enable PRIVATE src)
    target_link_libraries(mc_test_lighting_enable PRIVATE
        mc_client_render_textures mc_entity_classes mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_lighting_enable PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.lighting_enable_cpu COMMAND mc_test_lighting_enable --cpu)
    set_tests_properties(client.lighting_enable_cpu PROPERTIES
        LABELS "public;unit;client;ownership" TIMEOUT 30)
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.lighting_enable_controls COMMAND mc_test_lighting_enable --controls)
        set_tests_properties(client.lighting_enable_controls PROPERTIES
            LABELS "public;integration;client;graphics;ownership" TIMEOUT 30)
        add_test(NAME client.lighting_enable COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/lighting_enable/run_native.py"
            --executable "$<TARGET_FILE:mc_test_lighting_enable>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lighting-enable")
        set_tests_properties(client.lighting_enable PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 120)
    endif()
endif()
