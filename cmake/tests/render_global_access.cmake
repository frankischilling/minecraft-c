if(BUILD_TESTING AND TARGET mc_client_render_global)
    add_executable(mc_test_render_global_access tests/client/world/test_render_global_access.c)
    target_include_directories(mc_test_render_global_access PRIVATE src src/platform/time)
    target_link_libraries(mc_test_render_global_access PRIVATE
        mc_client_render_global mc_simulation mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_global_access PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_global_access COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/world/run_world_access.py"
        --executable "$<TARGET_FILE:mc_test_render_global_access>")
    set_tests_properties(client.render_global_access PROPERTIES
        LABELS "public;unit;client;ownership;world;filesystem" TIMEOUT 180)
    add_test(NAME client.sky_changed COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/sky_changed/run_native.py"
        --executable "$<TARGET_FILE:mc_test_render_global_access>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/sky-changed")
    set_tests_properties(client.sky_changed PROPERTIES
        LABELS "public;unit;client;ownership;world;replay" TIMEOUT 180)
    add_executable(mc_test_render_global_update_sort tests/client/world/test_render_global_update_sort.c)
    target_include_directories(mc_test_render_global_update_sort PRIVATE src src/platform/time)
    target_link_libraries(mc_test_render_global_update_sort PRIVATE mc_client_render_global mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_global_update_sort PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_global_update_sort COMMAND mc_test_render_global_update_sort --check
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_global_update_sort/cases.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_global_update_sort/expected.txt")
    set_tests_properties(client.render_global_update_sort PROPERTIES
        LABELS "public;unit;client;world;replay" TIMEOUT 90)
    add_executable(mc_test_render_global_update tests/client/world/test_render_global_update.c)
    target_include_directories(mc_test_render_global_update PRIVATE src src/platform/time)
    target_link_libraries(mc_test_render_global_update PRIVATE mc_client_render_global
        mc_simulation mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_global_update PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_global_update COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_global_update/run_native.py"
        --executable "$<TARGET_FILE:mc_test_render_global_update>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-global-update")
    set_tests_properties(client.render_global_update PROPERTIES
        LABELS "public;unit;client;world;ownership;replay" TIMEOUT 120)
    if(TARGET mc_test_source_resources)
        add_test(NAME client.lighting COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/lighting/run_native.py"
            --executable "$<TARGET_FILE:mc_test_source_resources>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/lighting")
        set_tests_properties(client.lighting PROPERTIES
            LABELS "public;unit;client;graphics;ownership;replay" TIMEOUT 120)
    endif()
    if(TARGET mc_client_entity_renderer)
        add_executable(mc_test_chunk_cache tests/client/world/test_chunk_cache.c)
        target_include_directories(mc_test_chunk_cache PRIVATE src src/platform/time)
        target_link_libraries(mc_test_chunk_cache PRIVATE
            mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
            mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_chunk_cache PROPERTY LINKER_LANGUAGE CXX)
        endif()
        add_test(NAME client.chunk_cache COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/chunk_cache/run_native.py"
            --executable "$<TARGET_FILE:mc_test_chunk_cache>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/chunk-cache")
        set_tests_properties(client.chunk_cache PROPERTIES
            LABELS "public;integration;client;world;ownership;replay" TIMEOUT 180)
        add_executable(mc_test_chunk_compile tests/client/world/test_chunk_compile.c)
        target_include_directories(mc_test_chunk_compile PRIVATE src src/platform/time)
        target_link_libraries(mc_test_chunk_compile PRIVATE
            mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
            mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_chunk_compile PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
            add_test(NAME client.chunk_compile COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_chunk_compile.py"
                --executable "$<TARGET_FILE:mc_test_chunk_compile>"
                --work-dir "${CMAKE_CURRENT_BINARY_DIR}/chunk-compile")
            set_tests_properties(client.chunk_compile PROPERTIES
                LABELS "public;integration;client;world;graphics;ownership" TIMEOUT 180)
        endif()
    endif()
endif()
