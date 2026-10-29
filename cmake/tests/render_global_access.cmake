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
endif()
