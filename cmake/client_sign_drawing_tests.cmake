# Authored direct YC device component controls, separate from original packets.
include_guard(GLOBAL)
if(BUILD_TESTING AND TARGET mc_client_runtime AND TARGET mc_client_source_screens)
    add_executable(mc_test_source_sign_drawing tests/client/gui/test_source_sign_drawing.c)
    target_include_directories(mc_test_source_sign_drawing PRIVATE src src/platform/time)
    target_link_libraries(mc_test_source_sign_drawing PRIVATE
        mc_client_runtime mc_client_saves mc_client_source_screens mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_sign_drawing PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32)
        target_link_libraries(mc_test_source_sign_drawing PRIVATE opengl32)
        if(TARGET OpenAL)
            add_custom_command(TARGET mc_test_source_sign_drawing POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:mc_test_source_sign_drawing>")
        endif()
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(mc_test_source_sign_drawing PRIVATE OpenGL::GL)
    endif()
    add_test(NAME tools.source_sign_drawing_controls COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_source_sign_drawing.py" --controls)
    add_test(NAME tools.source_sign_drawing_controls_optimized COMMAND Python3::Interpreter -B -O
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_source_sign_drawing.py" --controls)
    set_tests_properties(tools.source_sign_drawing_controls tools.source_sign_drawing_controls_optimized
        PROPERTIES LABELS "public;unit;tools;client" TIMEOUT 60)
    if((WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux") AND MC_ENABLE_CLIENT_DISPLAY_TESTS
       AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        add_test(NAME client.source_sign_drawing COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_source_sign_drawing.py"
            --executable "$<TARGET_FILE:mc_test_source_sign_drawing>"
            --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
            --artifacts "${CMAKE_CURRENT_BINARY_DIR}/source-sign-drawing-captures")
        set_tests_properties(client.source_sign_drawing PROPERTIES
            LABELS "local;integration;client;graphics;ownership;world" TIMEOUT 1500
            RESOURCE_LOCK mc_desktop_focus)
    endif()
endif()
