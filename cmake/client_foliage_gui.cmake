include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_FOLIAGE_GUI_TESTS "Run native Sapling/Leaves GUI draws with imported assets" OFF)
set(MC_CLIENT_FOLIAGE_GUI_LINUX_GAME_DIR "" CACHE PATH
    "Explicit isolated actual getpwuid game directory for foliage GUI controls")

if(BUILD_TESTING AND TARGET mc_client_runtime)
    add_executable(mc_test_foliage_gui tests/client/render/test_foliage_gui.c)
    target_include_directories(mc_test_foliage_gui PRIVATE src src/platform/time)
    target_link_libraries(mc_test_foliage_gui PRIVATE mc_client_runtime mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_foliage_gui PROPERTY LINKER_LANGUAGE CXX)
    endif()
    if(WIN32 AND TARGET OpenAL)
        add_custom_command(TARGET mc_test_foliage_gui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                "$<TARGET_FILE_DIR:mc_test_foliage_gui>")
    endif()
    if(MC_ENABLE_CLIENT_FOLIAGE_GUI_TESTS AND MC_ENABLE_CLIENT_DISPLAY_TESTS
        AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
        set(_foliage_host_arguments)
        if(NOT WIN32 AND MC_CLIENT_FOLIAGE_GUI_LINUX_GAME_DIR)
            list(APPEND _foliage_host_arguments --linux-game-dir "${MC_CLIENT_FOLIAGE_GUI_LINUX_GAME_DIR}")
        endif()
        if(WIN32 OR MC_CLIENT_FOLIAGE_GUI_LINUX_GAME_DIR)
            add_test(NAME client.foliage_gui_components COMMAND Python3::Interpreter -B
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_foliage_gui.py"
                --executable "$<TARGET_FILE:mc_test_foliage_gui>"
                --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                --artifacts "${CMAKE_CURRENT_BINARY_DIR}/foliage-gui-contracts"
                --mode components ${_foliage_host_arguments})
            set_tests_properties(client.foliage_gui_components PROPERTIES
                LABELS "local;integration;client;graphics;ownership" TIMEOUT 240
                RUN_SERIAL TRUE RESOURCE_LOCK mc_desktop_focus)
        endif()
        unset(_foliage_host_arguments)
    endif()
endif()
