if(BUILD_TESTING AND TARGET mc_client_tile_renderer_cpu)
    add_executable(mc_test_tile_entity_renderer tests/client/render/test_tile_entity_renderer.c)
    target_include_directories(mc_test_tile_entity_renderer PRIVATE src)
    target_link_libraries(mc_test_tile_entity_renderer PRIVATE
        mc_client_tile_renderer_cpu mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_tile_entity_renderer PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.tile_entity_renderer COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/tile_entity_renderer/run_native.py"
        --executable "$<TARGET_FILE:mc_test_tile_entity_renderer>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/tile-entity-renderer")
    set_tests_properties(client.tile_entity_renderer PROPERTIES
        LABELS "public;unit;client;ownership;replay" TIMEOUT 300)

    add_executable(mc_test_tile_entity_renderer_world
        tests/client/render/test_tile_entity_renderer_world.c)
    target_include_directories(mc_test_tile_entity_renderer_world PRIVATE src)
    target_link_libraries(mc_test_tile_entity_renderer_world PRIVATE
        mc_client_tile_renderer_cpu mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_tile_entity_renderer_world PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.tile_entity_renderer_world COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/tile_entity_renderer/run_world.py"
        --executable "$<TARGET_FILE:mc_test_tile_entity_renderer_world>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/tile-entity-renderer-world" --controls)
    set_tests_properties(client.tile_entity_renderer_world PROPERTIES
        LABELS "public;unit;client;ownership;replay" TIMEOUT 240)

    add_executable(mc_test_tile_entity_renderer_frame
        tests/client/render/test_tile_entity_renderer_frame.c)
    target_include_directories(mc_test_tile_entity_renderer_frame PRIVATE src)
    target_link_libraries(mc_test_tile_entity_renderer_frame PRIVATE
        mc_client_tile_renderer_cpu mc_build_options)
    add_executable(mc_test_tile_entity_renderer_lifetime
        tests/client/render/test_tile_entity_renderer_lifetime.c)
    target_include_directories(mc_test_tile_entity_renderer_lifetime PRIVATE src)
    target_link_libraries(mc_test_tile_entity_renderer_lifetime PRIVATE
        mc_client_tile_renderer_cpu mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_tile_entity_renderer_frame
            mc_test_tile_entity_renderer_lifetime PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.tile_entity_renderer_frame COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/tile_entity_renderer/run_frame.py"
        --executable "$<TARGET_FILE:mc_test_tile_entity_renderer_frame>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/tile-entity-renderer-frame")
    set_tests_properties(client.tile_entity_renderer_frame PROPERTIES
        LABELS "public;unit;client;ownership;replay" TIMEOUT 120)
    add_test(NAME client.tile_entity_renderer_lifetime
        COMMAND mc_test_tile_entity_renderer_lifetime)
    set_tests_properties(client.tile_entity_renderer_lifetime PROPERTIES
        LABELS "public;unit;client;ownership" TIMEOUT 90)
endif()
