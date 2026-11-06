include_guard(GLOBAL)
if(TARGET mc_client_source_screens)
    target_sources(mc_client_source_screens PRIVATE src/client/gui/source_pause_screen.c
        src/client/gui/source_death_screen.c)
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
if(BUILD_TESTING AND TARGET mc_client_source_screen_events)
    add_executable(mc_test_death_screen tests/client/gui/test_death_screen.c)
    target_include_directories(mc_test_death_screen PRIVATE src src/platform/time)
    target_link_libraries(mc_test_death_screen PRIVATE mc_client_source_screen_events
        mc_client_application_owner mc_resources mc_math_classes mc_simulation mc_build_options)
    add_test(NAME client.death_screen COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_death_screen.py"
        --executable "$<TARGET_FILE:mc_test_death_screen>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/death-screen-contracts")
    set_tests_properties(client.death_screen PROPERTIES
        LABELS "public;integration;client;gui;ownership" TIMEOUT 180)
    add_test(NAME tools.death_screen_packet COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/death_screen/test_packet.py")
    add_test(NAME tools.death_screen_packet_optimized COMMAND Python3::Interpreter -O -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/reference/client/death_screen/test_packet.py")
    set_tests_properties(tools.death_screen_packet tools.death_screen_packet_optimized PROPERTIES
        LABELS "public;unit;tools;client;gui" TIMEOUT 60)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_death_screen PROPERTY LINKER_LANGUAGE CXX)
    endif()
endif()
if(TARGET mc_client_source_screen_drawing)
    target_sources(mc_client_source_screen_drawing PRIVATE src/client/gui/source_pause_drawing.c
        src/client/gui/source_death_drawing.c)
endif()
