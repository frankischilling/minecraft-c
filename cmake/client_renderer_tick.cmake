if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE src/client/world/entity_renderer/tick.c)
    if(BUILD_TESTING)
        add_executable(mc_test_renderer_tick tests/client/world/test_renderer_tick.c)
        target_include_directories(mc_test_renderer_tick PRIVATE src src/client src/platform/time)
        target_link_libraries(mc_test_renderer_tick PRIVATE
            mc_client_entity_renderer mc_simulation mc_math_classes mc_c_atomics mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_renderer_tick PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME client.renderer_tick COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/renderer_tick/run_native.py"
            --executable "$<TARGET_FILE:mc_test_renderer_tick>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/renderer-tick")
        set_tests_properties(client.renderer_tick PROPERTIES
            LABELS "public;unit;client;rendering;ownership;replay" TIMEOUT 90)
    endif()
endif()
