# Additive tests only; YC implementation/source registration belongs to its owner.
include_guard(GLOBAL)
if(BUILD_TESTING AND TARGET mc_client_source_screens)
    add_executable(mc_test_source_sign_screen tests/client/gui/test_source_sign_screen.c)
    target_include_directories(mc_test_source_sign_screen PRIVATE src src/platform/time)
    target_link_libraries(mc_test_source_sign_screen PRIVATE mc_client_source_screens
        mc_client_application_owner mc_client_display mc_resources mc_simulation mc_math_classes mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_sign_screen PROPERTY LINKER_LANGUAGE CXX)
    endif()
    add_test(NAME client.sign_screen_cpu COMMAND mc_test_source_sign_screen)
    set_tests_properties(client.sign_screen_cpu PROPERTIES
        LABELS "public;unit;client;ownership" TIMEOUT 60)
    add_executable(mc_test_source_sign_screen_dirty tests/client/gui/test_source_sign_screen_dirty.c)
    target_include_directories(mc_test_source_sign_screen_dirty PRIVATE src src/platform/time)
    target_link_libraries(mc_test_source_sign_screen_dirty PRIVATE mc_client_source_screens
        mc_client_application_owner mc_client_display mc_resources mc_simulation mc_math_classes mc_build_options)
    if(CMAKE_CXX_COMPILER_LOADED)
        set_property(TARGET mc_test_source_sign_screen_dirty PROPERTY LINKER_LANGUAGE CXX)
    endif()
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_test(NAME client.sign_screen_dirty COMMAND Python3::Interpreter -B
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_sign_screen_dirty.py"
        --executable "$<TARGET_FILE:mc_test_source_sign_screen_dirty>"
        --work-dir "${CMAKE_CURRENT_BINARY_DIR}/sign-screen-dirty")
    set_tests_properties(client.sign_screen_dirty PROPERTIES
        LABELS "public;integration;client;world;ownership" TIMEOUT 120)
    set(MC_SIGN_SCREEN_SDK_RESOURCES "" CACHE PATH
        "Explicit authored SDK two-icon resource tree for optional YC real-input component controls")
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_SIGN_SCREEN_SDK_RESOURCES)
        add_test(NAME client.sign_screen_sdk COMMAND mc_test_source_sign_screen
            --sdk "${MC_SIGN_SCREEN_SDK_RESOURCES}")
        set_tests_properties(client.sign_screen_sdk PROPERTIES
            LABELS "public;integration;client;graphics;ownership" TIMEOUT 180
            RESOURCE_LOCK mc_desktop_focus)
    endif()
    if(WIN32 AND TARGET OpenAL)
        foreach(_sign_screen_test IN ITEMS mc_test_source_sign_screen mc_test_source_sign_screen_dirty)
            add_custom_command(TARGET ${_sign_screen_test} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:${_sign_screen_test}>")
        endforeach()
    endif()
endif()
