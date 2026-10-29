include_guard(GLOBAL)
if(TARGET mc_client_source_screens)
    target_sources(mc_client_source_screens PRIVATE src/client/gui/source_pause_screen.c)
    target_link_libraries(mc_client_source_screens PRIVATE mc_simulation)
    if(BUILD_TESTING)
        add_executable(mc_test_pause_screen tests/client/gui/test_pause_screen.c)
        target_include_directories(mc_test_pause_screen PRIVATE src src/platform/time)
        target_link_libraries(mc_test_pause_screen PRIVATE mc_client_source_screens
            mc_client_application_owner mc_resources mc_math_classes mc_simulation mc_build_options)
        add_test(NAME client.pause_screen COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/pause_screen/run_native.py"
            --executable "$<TARGET_FILE:mc_test_pause_screen>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/pause-screen")
        set_tests_properties(client.pause_screen PROPERTIES LABELS "public;unit;client;ownership;replay"
            TIMEOUT 60)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET mc_test_pause_screen PROPERTY LINKER_LANGUAGE CXX)
        endif()
    endif()
endif()
if(TARGET mc_client_source_screen_drawing)
    target_sources(mc_client_source_screen_drawing PRIVATE src/client/gui/source_pause_drawing.c)
endif()
