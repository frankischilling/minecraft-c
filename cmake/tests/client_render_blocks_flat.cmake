include_guard(GLOBAL)

# Whole entered CV flat body with genuine authored World/EW/Block/NW owners.
if(BUILD_TESTING AND TARGET mc_client_render_global AND TARGET mc_client_entity_renderer)
    add_executable(mc_test_render_blocks_flat tests/client/render/test_render_blocks_flat.c)
    target_include_directories(mc_test_render_blocks_flat PRIVATE src src/platform/time)
    target_link_libraries(mc_test_render_blocks_flat PRIVATE
        mc_client_entity_renderer mc_client_render_global mc_client_singleplayer_controller
        mc_simulation mc_resources mc_math_classes mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_blocks_flat PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_blocks_flat COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_blocks_flat/run_native.py"
        --executable "$<TARGET_FILE:mc_test_render_blocks_flat>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-blocks-flat")
    set_tests_properties(client.render_blocks_flat PROPERTIES
        LABELS "public;integration;client;world;ownership;replay" TIMEOUT 180)
endif()
