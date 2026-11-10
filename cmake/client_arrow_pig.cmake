include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_ARROW_PIG_TESTS
    "Run local imported-resource saved Arrow/Pig causal scene" OFF)

if(BUILD_TESTING AND TARGET mc_test_world_selection_loop)
    add_executable(mc_test_client_arrow_pig
        tests/client/gameplay/test_arrow_pig.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    target_include_directories(mc_test_client_arrow_pig PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}" src src/platform/time)
    target_link_libraries(mc_test_client_arrow_pig PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_client_arrow_pig PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_client_arrow_pig POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_client_arrow_pig>")
    endif()
endif()

if(MC_ENABLE_CLIENT_ARROW_PIG_TESTS)
    if(NOT WIN32 OR NOT BUILD_TESTING OR NOT TARGET mc_test_client_arrow_pig OR
       NOT MC_ENABLE_CLIENT_DISPLAY_TESTS OR NOT MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        message(FATAL_ERROR "Arrow/Pig scene requires native Windows client/display tests and imported resources")
    endif()
    add_test(NAME client.arrow_pig COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_arrow_pig.py"
        --executable "$<TARGET_FILE:mc_test_client_arrow_pig>"
        --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-pig-scenes")
    set_tests_properties(client.arrow_pig PROPERTIES
        LABELS "local;integration;client;input;graphics;entity;ownership;storage"
        RESOURCE_LOCK mc_desktop_focus TIMEOUT 300)
endif()

if(BUILD_TESTING)
    foreach(_mc_arrow_pig_mode IN ITEMS normal optimized)
        set(_mc_arrow_pig_flags -B)
        if(_mc_arrow_pig_mode STREQUAL "optimized")
            list(APPEND _mc_arrow_pig_flags -O)
        endif()
        add_test(NAME "tools.arrow_pig_fixture_${_mc_arrow_pig_mode}" COMMAND Python3::Interpreter
            ${_mc_arrow_pig_flags}
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_arrow_pig.py" --self-test)
        set_tests_properties("tools.arrow_pig_fixture_${_mc_arrow_pig_mode}" PROPERTIES
            LABELS "public;unit;tools;client;storage" TIMEOUT 90)
    endforeach()
    unset(_mc_arrow_pig_flags)
    unset(_mc_arrow_pig_mode)
endif()
