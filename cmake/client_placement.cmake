include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_PLACEMENT_TESTS "Run the native collected-Dirt placement/save cycle" OFF)

target_sources(mc_simulation PRIVATE src/world/runtime/placement.c)
add_library(mc_item_use STATIC src/item/use/use.c)
target_include_directories(mc_item_use PUBLIC include PRIVATE src)
target_link_libraries(mc_item_use PUBLIC mc_simulation PRIVATE mc_build_options)
target_sources(mc_client_singleplayer_controller PRIVATE src/client/controller/use.c)
target_link_libraries(mc_client_singleplayer_controller PUBLIC mc_item_use)

if(BUILD_TESTING)
    add_executable(mc_test_world_placement tests/world/runtime/placement/test_placement.c)
    target_include_directories(mc_test_world_placement PRIVATE src src/platform/time)
    target_link_libraries(mc_test_world_placement PRIVATE mc_simulation mc_resources
        mc_c_atomics mc_build_options)
    if(TARGET mc_client_render_global)
        add_executable(mc_test_world_placement_callbacks tests/world/runtime/placement/test_callbacks.c)
        target_include_directories(mc_test_world_placement_callbacks PRIVATE src src/platform/time)
        target_link_libraries(mc_test_world_placement_callbacks PRIVATE
            mc_client_singleplayer_controller mc_client_render_global mc_resources
            mc_c_atomics mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_world_placement_callbacks PROPERTY LINKER_LANGUAGE CXX)
        endif()
    endif()
    add_executable(mc_test_item_use tests/item/use/test_use.c)
    target_include_directories(mc_test_item_use PRIVATE src src/platform/time)
    target_link_libraries(mc_test_item_use PRIVATE mc_item_use mc_resources
        mc_c_atomics mc_build_options)
    if(TARGET mc_client_render_global)
        target_link_libraries(mc_test_item_use PRIVATE mc_client_render_global)
        target_compile_definitions(mc_test_item_use PRIVATE MC_PLACEMENT_REAL_RENDER_GLOBAL=1)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_item_use PROPERTY LINKER_LANGUAGE CXX)
        endif()
    endif()
    if(TARGET mc_client_entity_renderer)
        add_executable(mc_test_controller_use tests/client/controller/test_use.c)
        target_include_directories(mc_test_controller_use PRIVATE src src/platform/time)
        target_link_libraries(mc_test_controller_use PRIVATE mc_client_singleplayer_controller
            mc_client_entity_renderer mc_resources mc_c_atomics mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_controller_use PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_controller_use POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_controller_use>")
        endif()
    endif()
    if(WIN32 AND TARGET OpenAL)
        foreach(_target IN ITEMS mc_test_item_use mc_test_world_placement_callbacks)
            if(TARGET ${_target})
                add_custom_command(TARGET ${_target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                        "$<TARGET_FILE_DIR:${_target}>")
            endif()
        endforeach()
        unset(_target)
    endif()
    foreach(_mode IN ITEMS world callbacks item controller)
        if(_mode STREQUAL "world")
            set(_target mc_test_world_placement)
        elseif(_mode STREQUAL "callbacks")
            set(_target mc_test_world_placement_callbacks)
        elseif(_mode STREQUAL "item")
            set(_target mc_test_item_use)
        else()
            set(_target mc_test_controller_use)
        endif()
        if(TARGET ${_target})
            add_test(NAME "client.placement_${_mode}" COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_placement_components.py"
                --executable "$<TARGET_FILE:${_target}>" --mode "${_mode}"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/placement-${_mode}-controls")
            set_tests_properties("client.placement_${_mode}" PROPERTIES
                LABELS "public;integration;client;entity;world;ownership" TIMEOUT 180)
        endif()
    endforeach()
    unset(_mode)
    unset(_target)

    if(TARGET mc_test_world_selection_loop)
        add_executable(mc_test_placement_loop tests/client/gameplay/test_placement.c
            tests/client/world/test_load_world_foreign_loading.c
            tests/client/world/test_render_global_entities.c
            tests/client/world/test_refresh_entity.c
            tests/client/world/test_renderer_focus.c)
        target_include_directories(mc_test_placement_loop PRIVATE src src/platform/time)
        target_link_libraries(mc_test_placement_loop PRIVATE
            mc_client_runtime mc_client_saves mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_placement_loop PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET mc_test_placement_loop POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_placement_loop>")
        endif()
        if(WIN32 AND MC_ENABLE_CLIENT_PLACEMENT_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
           AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
            add_test(NAME client.placement_loop COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_placement_loop.py"
                --executable "$<TARGET_FILE:mc_test_placement_loop>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/placement-loop-captures")
            set_tests_properties(client.placement_loop PROPERTIES
                LABELS "local;integration;client;input;graphics;storage;ownership"
                TIMEOUT 960 RUN_SERIAL TRUE)
        endif()
    endif()
endif()
