include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_BLOCK_SELECTION_TESTS "Run finite native SDK aiming/selected-block controls" OFF)
if(BUILD_TESTING AND TARGET mc_client_runtime)
    add_executable(mc_test_block_selection tests/client/gui/test_block_selection.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_block_selection PRIVATE src src/platform/time)
    target_link_libraries(mc_test_block_selection PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_block_selection PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_block_selection POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_block_selection>")
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_BLOCK_SELECTION_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        foreach(_mode IN ITEMS dirt snow dirt-rounded snow-rounded)
            string(REPLACE "-" "_" _name "${_mode}")
            add_test(NAME "client.block_selection_${_name}"
                COMMAND Python3::Interpreter -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_block_selection.py"
                    --executable "$<TARGET_FILE:mc_test_block_selection>"
                    --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                    --artifacts "${CMAKE_CURRENT_BINARY_DIR}/block-selection-captures"
                    --mode "${_mode}")
            set_tests_properties("client.block_selection_${_name}" PROPERTIES
                LABELS "local;integration;client;input;graphics;ownership" TIMEOUT 300 RUN_SERIAL TRUE)
        endforeach()
        unset(_mode)
        unset(_name)
    endif()
endif()
