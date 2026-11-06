# CPU controls run no window, File, asset, Java or audio operation. Device
# controls are separately opt-in and require actual startup resources/SDK input.
if(BUILD_TESTING AND TARGET mc_client_render_global)
    add_executable(mc_test_render_global_cpu tests/client/world/test_render_global_cpu.c)
    target_include_directories(mc_test_render_global_cpu PRIVATE src)
    target_link_libraries(mc_test_render_global_cpu PRIVATE mc_client_render_global mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_global_cpu PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_global_cpu COMMAND mc_test_render_global_cpu)
    set_tests_properties(client.render_global_cpu PROPERTIES
        LABELS "public;unit;client;ownership" TIMEOUT 180)
    if(TARGET mc_test_application_startup)
        add_executable(mc_test_render_global_device tests/client/world/test_render_global_device.c)
        get_target_property(_mc_rg_links mc_test_application_startup LINK_LIBRARIES)
        target_link_libraries(mc_test_render_global_device PRIVATE
            mc_client_render_global ${_mc_rg_links})
        if(WIN32)
            target_link_libraries(mc_test_render_global_device PRIVATE opengl32)
        else()
            find_package(OpenGL REQUIRED)
            target_link_libraries(mc_test_render_global_device PRIVATE OpenGL::GL)
        endif()
        unset(_mc_rg_links)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_render_global_device PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_render_global_device POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:OpenAL>" "$<TARGET_FILE_DIR:mc_test_render_global_device>")
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS)
            if(WIN32)
                add_test(NAME client.render_global_device COMMAND Python3::Interpreter -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_render_global_device.py"
                    --executable "$<TARGET_FILE:mc_test_render_global_device>"
                    --resources "${_mc_startup_resource_root}"
                    --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}")
            else()
                add_test(NAME client.render_global_device COMMAND mc_test_render_global_device
                    "${_mc_startup_resource_root}" "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}")
            endif()
            set_tests_properties(client.render_global_device PROPERTIES
                LABELS "integration;client;startup;filesystem;graphics;ownership" TIMEOUT 240)
        endif()
    endif()
    foreach(_mc_rg_test IN ITEMS mc_test_render_global_cpu mc_test_render_global_device)
        if(TARGET ${_mc_rg_test})
            if(MSVC)
                target_link_options(${_mc_rg_test} PRIVATE
                    "/MAP:${CMAKE_CURRENT_BINARY_DIR}/${_mc_rg_test}.map")
            else()
                target_link_options(${_mc_rg_test} PRIVATE
                    "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/${_mc_rg_test}.map")
            endif()
        endif()
    endforeach()
    unset(_mc_rg_test)
endif()
