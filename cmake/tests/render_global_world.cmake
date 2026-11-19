# Canonical whole-App/startup + explicit World direct-constructor controls.
if(BUILD_TESTING AND TARGET mc_test_application_startup AND TARGET mc_client_render_global)
    add_executable(mc_test_render_global_world tests/client/world/test_render_global_world.c)
    get_target_property(_mc_rg_world_links mc_test_application_startup LINK_LIBRARIES)
    target_link_libraries(mc_test_render_global_world PRIVATE
        mc_client_render_global ${_mc_rg_world_links})
    unset(_mc_rg_world_links)
    if(WIN32)
        target_link_libraries(mc_test_render_global_world PRIVATE opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(mc_test_render_global_world PRIVATE OpenGL::GL)
    endif()
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_render_global_world PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.render_global_world_cpu COMMAND mc_test_render_global_world --cpu)
    set_tests_properties(client.render_global_world_cpu PROPERTIES
        LABELS "public;unit;client;ownership" TIMEOUT 30)
    if(WIN32 AND MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.render_global_world_rebuild_budget COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_render_global_world.py"
            --executable "$<TARGET_FILE:mc_test_render_global_world>"
            --resources "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/startup/fixtures"
            --authored-assets --rebuild-budget)
        set_tests_properties(client.render_global_world_rebuild_budget PROPERTIES
            LABELS "integration;client;startup;graphics;ownership"
            RUN_SERIAL TRUE TIMEOUT 240)
    endif()
    if(WIN32 AND MC_ENABLE_CLIENT_DISPLAY_TESTS AND
       MC_ENABLE_APPLICATION_STARTUP_DEFAULT_DIRECTORY_TESTS)
        add_test(NAME client.render_global_world_device COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_render_global_world.py"
            --executable "$<TARGET_FILE:mc_test_render_global_world>"
            --resources "${_mc_startup_resource_root}"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}")
        set_tests_properties(client.render_global_world_device PROPERTIES
            LABELS "integration;client;startup;filesystem;graphics;ownership"
            RUN_SERIAL TRUE TIMEOUT 240)
    endif()
    if(MSVC)
        target_link_options(mc_test_render_global_world PRIVATE
            "/MAP:${CMAKE_CURRENT_BINARY_DIR}/mc_test_render_global_world.map")
    else()
        target_link_options(mc_test_render_global_world PRIVATE
            "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/mc_test_render_global_world.map")
    endif()
endif()
