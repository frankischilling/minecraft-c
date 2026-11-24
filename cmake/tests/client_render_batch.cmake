include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_client_render_global)
    add_executable(mc_test_render_batch tests/client/world/test_render_batch.c)
    target_include_directories(mc_test_render_batch PRIVATE src src/platform/time)
    target_link_libraries(mc_test_render_batch PRIVATE mc_client_render_global mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_batch PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_batch_cpu COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_batch/run_native.py"
        --executable "$<TARGET_FILE:mc_test_render_batch>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-batch-cpu")
    set_tests_properties(client.render_batch_cpu PROPERTIES
        LABELS "public;unit;client;ownership;replay" TIMEOUT 180)
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.render_batch_device COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_batch/run_native.py"
            --executable "$<TARGET_FILE:mc_test_render_batch>" --device
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-batch-device")
        set_tests_properties(client.render_batch_device PROPERTIES
            LABELS "public;integration;client;graphics;ownership;replay" TIMEOUT 180)
    endif()
endif()
