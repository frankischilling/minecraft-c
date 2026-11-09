include_guard(GLOBAL)

if(BUILD_TESTING AND TARGET mc_client_runtime)
    add_executable(mc_test_arrow_world_tick_cleanup
        tests/client/gameplay/test_arrow_world_tick_cleanup.c)
    target_include_directories(mc_test_arrow_world_tick_cleanup PRIVATE src src/platform/time)
    target_link_libraries(mc_test_arrow_world_tick_cleanup PRIVATE
        mc_client_runtime mc_client_saves mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_arrow_world_tick_cleanup PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        # Keep the runtime current even when its import library does not change
        # and the executable therefore does not need to relink.
        add_custom_target(mc_arrow_world_tick_cleanup_audio
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_arrow_world_tick_cleanup>"
            DEPENDS "$<TARGET_FILE:OpenAL>"
            VERBATIM)
        add_dependencies(mc_test_arrow_world_tick_cleanup mc_arrow_world_tick_cleanup_audio)
    endif()
endif()

if(BUILD_TESTING AND Python3_Interpreter_FOUND)
    add_test(NAME tools.arrow_world_tick_cleanup_transport COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_arrow_world_tick_cleanup.py" --self-test)
    add_test(NAME tools.arrow_world_tick_cleanup_transport_optimized COMMAND Python3::Interpreter -B -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_arrow_world_tick_cleanup.py" --self-test)
    set_tests_properties(tools.arrow_world_tick_cleanup_transport
        tools.arrow_world_tick_cleanup_transport_optimized PROPERTIES
        LABELS "public;unit;tools;client;ownership" TIMEOUT 90)
    if(TARGET mc_test_arrow_world_tick_cleanup)
        foreach(_mc_arrow_sdk_mode IN ITEMS source-failure counter-retry)
            add_test(NAME client.arrow_world_tick_cleanup_${_mc_arrow_sdk_mode}
                COMMAND Python3::Interpreter -B
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_arrow_world_tick_cleanup.py"
                    --mode "${_mc_arrow_sdk_mode}"
                    --executable "$<TARGET_FILE:mc_test_arrow_world_tick_cleanup>"
                    --project-root "${CMAKE_CURRENT_SOURCE_DIR}"
                    --bundle "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/arrow_world_tick_cleanup_bundle.json"
                    --work-dir "${CMAKE_CURRENT_BINARY_DIR}/arrow-world-tick-cleanup")
            set_tests_properties(client.arrow_world_tick_cleanup_${_mc_arrow_sdk_mode} PROPERTIES
                LABELS "public;integration;client;entity;world;ownership;arrow"
                TIMEOUT 420 RESOURCE_LOCK world_query_controls)
        endforeach()
        unset(_mc_arrow_sdk_mode)
    endif()
endif()
