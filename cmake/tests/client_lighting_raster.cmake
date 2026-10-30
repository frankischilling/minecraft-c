if(BUILD_TESTING AND TARGET mc_client_render_textures)
    add_executable(mc_test_nw_normals tests/client/render_engine/test_nw_normals.c)
    target_include_directories(mc_test_nw_normals PRIVATE src)
    target_link_libraries(mc_test_nw_normals PRIVATE mc_client_render_textures mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_nw_normals PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.nw_normals COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/nw_normals/run_native.py"
        --executable "$<TARGET_FILE:mc_test_nw_normals>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/nw-normals")
    set_tests_properties(client.nw_normals PROPERTIES
        LABELS "public;unit;client;ownership;replay" TIMEOUT 60)
    add_executable(mc_test_lighting_raster tests/client/display/test_lighting_raster.c)
    target_include_directories(mc_test_lighting_raster PRIVATE src)
    target_link_libraries(mc_test_lighting_raster PRIVATE mc_client_render_textures mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_lighting_raster PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.lighting_material COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/lighting_raster/run_material.py"
            --executable "$<TARGET_FILE:mc_test_lighting_raster>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lighting-material")
        set_tests_properties(client.lighting_material PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 60)
        add_test(NAME client.lighting_list_state COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/lighting_raster/run_list_state.py"
            --executable "$<TARGET_FILE:mc_test_lighting_raster>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lighting-list-state")
        set_tests_properties(client.lighting_list_state PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 90)
        add_test(NAME client.lighting_array_normals COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/lighting_raster/run_array_normals.py"
            --executable "$<TARGET_FILE:mc_test_lighting_raster>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lighting-array-normals")
        set_tests_properties(client.lighting_array_normals PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 90)
        add_test(NAME client.lighting_raster COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/lighting_raster/run_native.py"
            --executable "$<TARGET_FILE:mc_test_lighting_raster>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lighting-raster")
        set_tests_properties(client.lighting_raster PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 180)
        add_test(NAME client.lighting_raster_extended COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/lighting_raster/run_extended.py"
            --executable "$<TARGET_FILE:mc_test_lighting_raster>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lighting-raster-extended")
        set_tests_properties(client.lighting_raster_extended PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 180)
        add_test(NAME client.lighting_raster_converted COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/lighting_raster/run_converted.py"
            --executable "$<TARGET_FILE:mc_test_lighting_raster>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lighting-raster-converted")
        set_tests_properties(client.lighting_raster_converted PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 90)
    endif()
endif()
