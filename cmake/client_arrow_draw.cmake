include_guard(GLOBAL)
if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE src/client/render/arrow_draw.c)
    if(BUILD_TESTING AND TARGET mc_client_application_startup AND TARGET mc_sound_backend)
        add_executable(mc_test_arrow_draw tests/client/render/test_arrow_draw.c)
        target_include_directories(mc_test_arrow_draw PRIVATE src src/platform/time)
        target_link_libraries(mc_test_arrow_draw PRIVATE mc_client_entity_renderer
            mc_simulation mc_math_classes mc_client_application_startup mc_sound_backend
            mc_client_assets mc_resources mc_c_atomics mc_build_options)
        if(WIN32)
            target_link_libraries(mc_test_arrow_draw PRIVATE opengl32)
        else()
            find_package(OpenGL REQUIRED)
            target_link_libraries(mc_test_arrow_draw PRIVATE OpenGL::GL)
        endif()
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_arrow_draw PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_arrow_draw POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_arrow_draw>")
        endif()
        add_test(NAME client.arrow_draw_cpu COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_arrow_draw.py"
            --executable "$<TARGET_FILE:mc_test_arrow_draw>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-draw-cpu" --cpu-only)
        set_tests_properties(client.arrow_draw_cpu PROPERTIES
            LABELS "public;integration;client;graphics;ownership" TIMEOUT 180)
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.arrow_draw COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_arrow_draw.py"
                --executable "$<TARGET_FILE:mc_test_arrow_draw>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-draw-contracts")
            set_tests_properties(client.arrow_draw PROPERTIES
                LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 600)
        endif()
        add_test(NAME tools.arrow_draw_controls COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_arrow_draw.py" --self-test)
        set_tests_properties(tools.arrow_draw_controls PROPERTIES
            LABELS "public;unit;tools;client;graphics" TIMEOUT 60)
    endif()
endif()
