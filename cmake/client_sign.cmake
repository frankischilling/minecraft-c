include_guard(GLOBAL)

if(BUILD_TESTING)
    add_executable(mc_test_world_sign tests/world/runtime/sign/test_lifecycle.c)
    target_link_libraries(mc_test_world_sign PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.sign COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_runtime.py"
        --executable "$<TARGET_FILE:mc_test_world_sign>" --scenario world-sign
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/world-sign-contracts")
    set_tests_properties(world.sign PROPERTIES
        LABELS "public;integration;ownership;mutation" TIMEOUT 120)
endif()

if(BUILD_TESTING)
    foreach(_sign_draw_mode IN ITEMS normal optimized)
        if(_sign_draw_mode STREQUAL "optimized")
            set(_sign_draw_options -O -B)
        else()
            set(_sign_draw_options -B)
        endif()
        add_test(NAME tools.sign_draw_packet_${_sign_draw_mode} COMMAND Python3::Interpreter
            ${_sign_draw_options} "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/sign_draw/test_packet.py")
        set_tests_properties(tools.sign_draw_packet_${_sign_draw_mode} PROPERTIES
            LABELS "public;unit;tools;reference;client;graphics" TIMEOUT 180)
    endforeach()
    unset(_sign_draw_mode)
    unset(_sign_draw_options)
    foreach(_sign_packet_mode IN ITEMS normal optimized)
        if(_sign_packet_mode STREQUAL "optimized")
            set(_sign_packet_options -O -B)
        else()
            set(_sign_packet_options -B)
        endif()
        add_test(NAME tools.sign_nbt_packet_${_sign_packet_mode} COMMAND Python3::Interpreter
            ${_sign_packet_options} "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/sign_nbt/test_packet.py")
        set_tests_properties(tools.sign_nbt_packet_${_sign_packet_mode} PROPERTIES
            LABELS "public;unit;tools;reference;world" TIMEOUT 120)
    endforeach()
    unset(_sign_packet_mode)
    unset(_sign_packet_options)
    add_test(NAME tools.sign_sdk_controls COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/test_sign_sdk.py" --controls)
    set_tests_properties(tools.sign_sdk_controls PROPERTIES
        LABELS "public;unit;tools;client;world;graphics" TIMEOUT 180)
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop AND TARGET mc_client_tile_renderer_draw)
    option(MC_ENABLE_CLIENT_SIGN_TESTS "Run monitor-2 saved Sign frames and save/reselection" OFF)
    add_executable(mc_test_sign_sdk tests/client/test_sign_sdk.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    add_executable(mc_test_sign_draw_geometry tests/client/render/tile_entity_renderer/test_sign_geometry.c)
    target_include_directories(mc_test_sign_draw_geometry PRIVATE src src/platform/time)
    target_link_libraries(mc_test_sign_draw_geometry PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    if(WIN32)
        target_link_libraries(mc_test_sign_draw_geometry PRIVATE opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(mc_test_sign_draw_geometry PRIVATE OpenGL::GL)
    endif()
    target_include_directories(mc_test_sign_sdk PRIVATE src src/platform/time)
    target_link_libraries(mc_test_sign_sdk PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_sign_sdk PROPERTY LINKER_LANGUAGE CXX)
        set_property(TARGET mc_test_sign_draw_geometry PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_sign_sdk POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_sign_sdk>")
        add_custom_command(TARGET mc_test_sign_draw_geometry POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_sign_draw_geometry>")
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_SIGN_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        foreach(_sign_profile IN ITEMS standing-0 standing-4 standing-8 standing-12 wall-2 wall-3 wall-4 wall-5)
            add_test(NAME client.sign_${_sign_profile} COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/test_sign_sdk.py" --profile ${_sign_profile}
                --executable "$<TARGET_FILE:mc_test_sign_sdk>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/sign-sdk-captures")
            set_tests_properties(client.sign_${_sign_profile} PROPERTIES
                LABELS "local;integration;client;graphics;world;ownership" TIMEOUT 600
                RESOURCE_LOCK mc_desktop_focus)
        endforeach()
        unset(_sign_profile)
    endif()
endif()

if(BUILD_TESTING AND TARGET mc_client_tile_renderer_draw AND TARGET mc_client_render_global)
    add_executable(mc_test_sign_blocks tests/client/render/test_sign_blocks.c)
    target_include_directories(mc_test_sign_blocks PRIVATE src src/platform/time)
    target_link_libraries(mc_test_sign_blocks PRIVATE
        mc_client_tile_renderer_draw mc_client_entity_renderer mc_client_render_global
        mc_client_singleplayer_controller mc_simulation mc_resources mc_math_classes
        mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_sign_blocks PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.sign_blocks COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_sign_blocks.py"
        --executable "$<TARGET_FILE:mc_test_sign_blocks>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/sign-controls")
    set_tests_properties(client.sign_blocks PROPERTIES
        LABELS "public;integration;client;world;graphics;ownership" TIMEOUT 240)
endif()
