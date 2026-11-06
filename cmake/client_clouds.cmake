include_guard(GLOBAL)
if(BUILD_TESTING AND TARGET mc_client_render_global AND TARGET mc_test_application_startup)
    add_executable(mc_test_render_global_clouds tests/client/world/test_render_global_clouds.c)
    get_target_property(_mc_cloud_links mc_test_application_startup LINK_LIBRARIES)
    target_link_libraries(mc_test_render_global_clouds PRIVATE
        mc_client_render_global mc_simulation mc_math_classes ${_mc_cloud_links})
    unset(_mc_cloud_links)
    target_include_directories(mc_test_render_global_clouds PRIVATE src src/platform/time)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_global_clouds PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_render_global_clouds POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_render_global_clouds>")
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.render_global_clouds COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_render_global_clouds.py"
            --executable "$<TARGET_FILE:mc_test_render_global_clouds>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-global-clouds")
        set_tests_properties(client.render_global_clouds PROPERTIES
            LABELS "public;integration;client;world;graphics;ownership;replay" TIMEOUT 180)
    endif()
endif()
