include_guard(GLOBAL)
if(TARGET mc_client_entity_renderer)
    target_sources(mc_client_entity_renderer PRIVATE
        src/client/world/entity_renderer/renderer_fog_state.c)
    if(BUILD_TESTING)
        add_executable(mc_test_renderer_fog_state tests/client/world/test_renderer_fog_state.c)
        target_include_directories(mc_test_renderer_fog_state PRIVATE src src/platform/time)
        target_link_libraries(mc_test_renderer_fog_state PRIVATE
            mc_client_entity_renderer mc_client_singleplayer_controller mc_simulation
            mc_resources mc_math_classes mc_c_atomics mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_renderer_fog_state PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.renderer_fog_state COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/renderer_fog_state/run_native.py"
                --executable "$<TARGET_FILE:mc_test_renderer_fog_state>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/renderer-fog-state")
            set_tests_properties(client.renderer_fog_state PROPERTIES
                LABELS "public;integration;client;world;graphics;ownership;replay" TIMEOUT 120)
        endif()
    endif()
    if(TARGET mc_client_render_global)
        target_sources(mc_client_entity_renderer PRIVATE src/client/world/frustum_culling.c)
        target_compile_definitions(mc_client_entity_renderer PRIVATE MC_SOURCE_HAS_RENDER_GLOBAL=1)
        target_link_libraries(mc_client_entity_renderer PRIVATE mc_client_render_global)
        target_sources(mc_client_render_global PRIVATE src/client/world/render_global_sky_draw.c)
        if(BUILD_TESTING)
            add_executable(mc_test_render_global_sky_cpu tests/client/world/test_render_global_sky_cpu.c)
            target_include_directories(mc_test_render_global_sky_cpu PRIVATE src src/platform/time)
            target_link_libraries(mc_test_render_global_sky_cpu PRIVATE
                mc_client_render_global mc_client_application_owner mc_math_classes
                mc_c_atomics mc_build_options)
            if(CMAKE_CXX_COMPILER_LOADED)
                set_property(TARGET mc_test_render_global_sky_cpu PROPERTY LINKER_LANGUAGE CXX)
            endif()
            add_test(NAME client.render_global_sky_cpu COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_global_sky/run_cpu.py"
                --executable "$<TARGET_FILE:mc_test_render_global_sky_cpu>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-global-sky-cpu")
            set_tests_properties(client.render_global_sky_cpu PROPERTIES
                LABELS "public;unit;client;world;ownership;replay" TIMEOUT 90)
            if(TARGET mc_test_application_startup)
                add_executable(mc_test_render_global_sky_device
                    tests/client/world/test_render_global_sky_device.c)
                get_target_property(_mc_sky_links mc_test_application_startup LINK_LIBRARIES)
                target_link_libraries(mc_test_render_global_sky_device PRIVATE
                    mc_client_render_global mc_simulation mc_math_classes ${_mc_sky_links})
                unset(_mc_sky_links)
                if(CMAKE_CXX_COMPILER_LOADED)
                    set_property(TARGET mc_test_render_global_sky_device PROPERTY LINKER_LANGUAGE CXX)
                endif()
                if(WIN32 AND TARGET OpenAL)
                    add_custom_command(TARGET mc_test_render_global_sky_device POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                            "$<TARGET_FILE_DIR:mc_test_render_global_sky_device>")
                endif()
                if(WIN32 AND MC_ENABLE_CLIENT_DISPLAY_TESTS AND
                   MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS AND
                   NOT "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}" STREQUAL "")
                    add_test(NAME client.render_global_sky_device COMMAND Python3::Interpreter -B
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_global_sky/run_device.py"
                        --executable "$<TARGET_FILE:mc_test_render_global_sky_device>"
                        --resources "${_mc_startup_resource_root}"
                        --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-global-sky-device")
                    set_tests_properties(client.render_global_sky_device PROPERTIES
                        LABELS "reference;local;integration;client;world;graphics;ownership;replay"
                        TIMEOUT 240)
                endif()
            endif()
            add_executable(mc_test_frustum_culling tests/client/world/test_frustum_culling.c)
            target_include_directories(mc_test_frustum_culling PRIVATE src)
            target_link_libraries(mc_test_frustum_culling PRIVATE
                mc_client_entity_renderer mc_client_render_global mc_math_classes
                mc_c_atomics mc_build_options)
            if(CMAKE_CXX_COMPILER_LOADED)
                set_property(TARGET mc_test_frustum_culling PROPERTY LINKER_LANGUAGE CXX)
            endif()
            if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
                add_test(NAME client.frustum_culling COMMAND Python3::Interpreter -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/frustum_culling/run_native.py"
                    --executable "$<TARGET_FILE:mc_test_frustum_culling>"
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/frustum-culling")
                set_tests_properties(client.frustum_culling PROPERTIES
                    LABELS "public;integration;client;world;graphics;ownership" TIMEOUT 120)
            endif()
        endif()
    endif()
endif()
