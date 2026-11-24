# Prepared actual CV/Block/NW state; no display or imported game assets.
if(BUILD_TESTING AND TARGET mc_client_render_global)
    add_executable(mc_test_render_blocks_faces tests/client/render/test_render_blocks_faces.c)
    target_include_directories(mc_test_render_blocks_faces PRIVATE src src/platform/time)
    target_link_libraries(mc_test_render_blocks_faces PRIVATE
        mc_client_render_global mc_simulation mc_c_atomics mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_blocks_faces PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_blocks_faces COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/render_blocks_faces/run_native.py"
        --executable "$<TARGET_FILE:mc_test_render_blocks_faces>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/render-blocks-faces")
    set_tests_properties(client.render_blocks_faces PROPERTIES
        LABELS "public;unit;client;graphics;ownership;replay" TIMEOUT 120)
endif()
