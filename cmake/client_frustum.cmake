include_guard(GLOBAL)
if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_render_engine PRIVATE src/client/render_engine/ge_float.c)
    target_sources(mc_client_entity_renderer PRIVATE src/client/world/frustum.c)
    if(BUILD_TESTING)
        add_executable(mc_test_source_frustum tests/client/world/test_frustum.c)
        target_include_directories(mc_test_source_frustum PRIVATE src)
        target_link_libraries(mc_test_source_frustum PRIVATE mc_client_entity_renderer
            mc_math_classes mc_c_atomics mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_source_frustum PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.source_frustum COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/frustum/run_native.py"
                --executable "$<TARGET_FILE:mc_test_source_frustum>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/source-frustum")
            set_tests_properties(client.source_frustum PROPERTIES
                LABELS "public;integration;client;world;graphics;ownership" TIMEOUT 120)
        endif()
    endif()
endif()
