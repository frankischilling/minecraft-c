include_guard(GLOBAL)

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    foreach(_piston_python_mode IN ITEMS normal optimized)
        if(_piston_python_mode STREQUAL "optimized")
            set(_piston_python_flags -O -B)
        else()
            set(_piston_python_flags -B)
        endif()
        add_test(NAME tools.piston_draw_packet_${_piston_python_mode}
            COMMAND Python3::Interpreter ${_piston_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/piston_draw/test_packet.py")
        set_tests_properties(tools.piston_draw_packet_${_piston_python_mode} PROPERTIES
            LABELS "public;unit;tools;client;graphics;reference" TIMEOUT 180)
        add_test(NAME tools.piston_geometry_controls_${_piston_python_mode}
            COMMAND Python3::Interpreter ${_piston_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_piston_geometry.py" --controls)
        set_tests_properties(tools.piston_geometry_controls_${_piston_python_mode} PROPERTIES
            LABELS "public;unit;tools;client;graphics;reference" TIMEOUT 180)
        add_test(NAME tools.piston_activation_packet_${_piston_python_mode}
            COMMAND Python3::Interpreter ${_piston_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/world/piston_activation/test_packet.py")
        set_tests_properties(tools.piston_activation_packet_${_piston_python_mode} PROPERTIES
            LABELS "public;unit;tools;world;reference" TIMEOUT 180)
        add_test(NAME tools.piston_mobility_packet_${_piston_python_mode}
            COMMAND Python3::Interpreter ${_piston_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/block/piston_mobility/test_packet.py")
        set_tests_properties(tools.piston_mobility_packet_${_piston_python_mode} PROPERTIES
            LABELS "public;unit;tools;world;reference" TIMEOUT 180)
        add_test(NAME tools.piston_reference_controls_${_piston_python_mode}
            COMMAND Python3::Interpreter ${_piston_python_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_piston_reference.py" --controls)
        set_tests_properties(tools.piston_reference_controls_${_piston_python_mode} PROPERTIES
            LABELS "public;unit;tools;world;reference" TIMEOUT 180)
    endforeach()
    unset(_piston_python_mode)
    unset(_piston_python_flags)
    add_executable(mc_test_piston_mobility tests/world/runtime/piston/test_piston_mobility.c)
    target_include_directories(mc_test_piston_mobility PRIVATE src)
    target_link_libraries(mc_test_piston_mobility PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.piston_mobility COMMAND mc_test_piston_mobility)
    set_tests_properties(world.piston_mobility PROPERTIES
        LABELS "public;unit;world;reference" TIMEOUT 120
        PASS_REGULAR_EXPRESSION "PASS: Piston virtual mobility; original-rows=96; native-sides=2; checks=[0-9]+; cleanup=1")
    add_executable(mc_test_piston_gameplay tests/world/runtime/piston/test_piston.c)
    target_include_directories(mc_test_piston_gameplay PRIVATE src)
    target_link_libraries(mc_test_piston_gameplay PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.piston_gameplay COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_piston_gameplay.py"
        --executable "$<TARGET_FILE:mc_test_piston_gameplay>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/piston-gameplay-controls")
    set_tests_properties(world.piston_gameplay PROPERTIES
        LABELS "public;integration;world;ownership" TIMEOUT 300)
    add_executable(mc_test_piston_reference tests/world/runtime/piston/test_piston_reference.c)
    target_include_directories(mc_test_piston_reference PRIVATE src)
    target_link_libraries(mc_test_piston_reference PRIVATE mc_simulation mc_c_atomics mc_build_options)
    add_test(NAME world.piston_reference COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/test_piston_reference.py"
        --executable "$<TARGET_FILE:mc_test_piston_reference>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/piston-reference-controls")
    set_tests_properties(world.piston_reference PROPERTIES
        LABELS "public;integration;world;ownership;reference" TIMEOUT 300)
    foreach(_piston_sdk_mode IN ITEMS normal optimized)
        if(_piston_sdk_mode STREQUAL "optimized")
            set(_piston_sdk_flags -O -B)
        else()
            set(_piston_sdk_flags -B)
        endif()
        add_test(NAME tools.piston_sdk_controls_${_piston_sdk_mode}
            COMMAND Python3::Interpreter ${_piston_sdk_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/test_piston_sdk.py" --controls)
        set_tests_properties(tools.piston_sdk_controls_${_piston_sdk_mode} PROPERTIES
            LABELS "public;unit;tools;client;world;graphics" TIMEOUT 180)
    endforeach()
    unset(_piston_sdk_mode)
    unset(_piston_sdk_flags)
endif()

if(BUILD_TESTING AND TARGET mc_client_tile_renderer_draw AND TARGET mc_client_render_global)
    foreach(_piston_kind IN ITEMS blocks draw_gates)
        if(_piston_kind STREQUAL "blocks")
            set(_piston_source tests/client/render/test_piston_blocks.c)
            set(_piston_driver_kind blocks)
        else()
            set(_piston_source tests/client/render/tile_entity_renderer/test_piston_draw.c)
            set(_piston_driver_kind draw)
        endif()
        add_executable(mc_test_piston_${_piston_kind} ${_piston_source})
        target_include_directories(mc_test_piston_${_piston_kind} PRIVATE src src/platform/time)
        target_link_libraries(mc_test_piston_${_piston_kind} PRIVATE
            mc_client_tile_renderer_draw mc_client_entity_renderer mc_client_render_global
            mc_client_singleplayer_controller mc_simulation mc_resources mc_math_classes
            mc_c_atomics mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_piston_${_piston_kind} PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME client.piston_${_piston_kind} COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_piston_blocks.py"
            --executable "$<TARGET_FILE:mc_test_piston_${_piston_kind}>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/piston-controls" --kind ${_piston_driver_kind})
        set_tests_properties(client.piston_${_piston_kind} PROPERTIES
            LABELS "public;integration;client;world;graphics;ownership" TIMEOUT 240)
    endforeach()
    unset(_piston_kind)
    unset(_piston_source)
    unset(_piston_driver_kind)
endif()

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop AND TARGET mc_client_tile_renderer_draw)
    option(MC_ENABLE_CLIENT_PISTON_TESTS "Run monitor-2 original-asset Piston draws and empty-front input" OFF)
    add_executable(mc_test_piston_sdk tests/client/test_piston_sdk.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_piston_sdk PRIVATE src src/platform/time)
    target_link_libraries(mc_test_piston_sdk PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    add_executable(mc_test_piston_draw_geometry tests/client/render/tile_entity_renderer/test_piston_geometry.c)
    target_include_directories(mc_test_piston_draw_geometry PRIVATE src src/platform/time)
    target_link_libraries(mc_test_piston_draw_geometry PRIVATE mc_client_runtime mc_client_saves mc_build_options)
    if(WIN32)
        target_link_libraries(mc_test_piston_draw_geometry PRIVATE opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(mc_test_piston_draw_geometry PRIVATE OpenGL::GL)
    endif()
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_piston_sdk PROPERTY LINKER_LANGUAGE CXX)
        set_property(TARGET mc_test_piston_draw_geometry PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_PISTON_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.piston_draw_geometry COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_piston_geometry.py"
            --executable "$<TARGET_FILE:mc_test_piston_draw_geometry>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/piston-geometry-captures")
        add_test(NAME client.piston_empty_front COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/test_piston_sdk.py" --profile empty-front
            --executable "$<TARGET_FILE:mc_test_piston_sdk>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/piston-sdk-captures")
        set_tests_properties(client.piston_draw_geometry client.piston_empty_front PROPERTIES
            LABELS "local;integration;client;graphics;world;ownership" TIMEOUT 600
            RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
