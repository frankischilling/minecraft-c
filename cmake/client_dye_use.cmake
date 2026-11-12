include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_DYE_TESTS "Run the native saved-Crop Dye input and save cycle" OFF)

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    add_executable(mc_test_dye_loop tests/client/gameplay/test_dye.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_dye_loop PRIVATE src src/platform/time)
    target_link_libraries(mc_test_dye_loop PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_dye_loop PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_dye_loop POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_dye_loop>")
    endif()
    if(WIN32 AND Python3_Interpreter_FOUND AND MC_ENABLE_CLIENT_DYE_TESTS
       AND MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.dye_loop COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_dye_loop.py"
            --executable "$<TARGET_FILE:mc_test_dye_loop>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/dye-loop-captures")
        set_tests_properties(client.dye_loop PROPERTIES
            LABELS "local;integration;client;input;graphics;storage;ownership"
            TIMEOUT 300 RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
