include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_TORCH_PLACEMENT_TESTS "Run native Torch placement and support-removal save cycles" OFF)

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    add_executable(mc_test_torch_placement tests/client/gameplay/test_torch_placement.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_torch_placement PRIVATE src src/platform/time)
    target_link_libraries(mc_test_torch_placement PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_torch_placement PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_torch_placement POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_torch_placement>")
    endif()
    if(WIN32 AND Python3_Interpreter_FOUND AND MC_ENABLE_CLIENT_TORCH_PLACEMENT_TESTS
       AND MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        foreach(_torch_mode IN ITEMS placement support-removal)
            if(_torch_mode STREQUAL "placement")
                set(_torch_name client.torch_placement)
            else()
                set(_torch_name client.torch_support_removal)
            endif()
            add_test(NAME "${_torch_name}" COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_torch_placement_loop.py"
                --executable "$<TARGET_FILE:mc_test_torch_placement>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/torch-placement-captures"
                --mode "${_torch_mode}")
            set_tests_properties("${_torch_name}" PROPERTIES
                LABELS "local;integration;client;input;graphics;storage;ownership"
                TIMEOUT 300 RUN_SERIAL TRUE RESOURCE_LOCK mc_desktop_focus)
        endforeach()
        unset(_torch_name)
        unset(_torch_mode)
    endif()
endif()
