include_guard(GLOBAL)
option(MC_ENABLE_CLIENT_ACTIVE_NOTICE_TESTS "Run finite native active Notice controls" OFF)
set(MC_CLIENT_ACTIVE_NOTICE_LINUX_GAME_DIR "" CACHE PATH
    "Explicit existing actual getpwuid .minecraft directory for no-World Linux components")
if(BUILD_TESTING AND TARGET mc_client_runtime)
    add_executable(mc_test_active_notice_cpu tests/client/gui/test_active_notice_cpu.c)
    add_executable(mc_test_active_notice tests/client/gui/test_active_notice.c
        tests/client/world/test_load_world_foreign_loading.c
        tests/client/world/test_render_global_entities.c
        tests/client/world/test_refresh_entity.c
        tests/client/world/test_renderer_focus.c)
    if(WIN32)
        target_link_libraries(mc_test_active_notice_cpu PRIVATE opengl32)
    else()
        find_package(OpenGL REQUIRED)
        target_link_libraries(mc_test_active_notice_cpu PRIVATE OpenGL::GL)
    endif()
    foreach(_mc_notice_target IN ITEMS mc_test_active_notice_cpu mc_test_active_notice)
        target_include_directories(${_mc_notice_target} PRIVATE src src/platform/time)
        target_link_libraries(${_mc_notice_target} PRIVATE mc_client_runtime mc_build_options)
        if(CMAKE_CXX_COMPILER_LOADED)
            set_property(TARGET ${_mc_notice_target} PROPERTY LINKER_LANGUAGE CXX)
        endif()
        if(WIN32 AND TARGET OpenAL)
            add_custom_command(TARGET ${_mc_notice_target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:OpenAL>"
                    "$<TARGET_FILE_DIR:${_mc_notice_target}>")
        endif()
    endforeach()
    unset(_mc_notice_target)
    add_test(NAME client.active_notice_cpu COMMAND mc_test_active_notice_cpu
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/client/startup/fixtures")
    set_tests_properties(client.active_notice_cpu PROPERTIES
        LABELS "public;integration;client;ownership" TIMEOUT 60)
    if(MC_ENABLE_CLIENT_ACTIVE_NOTICE_TESTS)
        if(MC_ENABLE_CLIENT_DISPLAY_TESTS AND MC_APPLICATION_STARTUP_IMPORTED_RESOURCES)
            if(WIN32)
                set(_mc_notice_modes components rounded tutorial)
                set(_mc_notice_linux_arguments)
            elseif(MC_CLIENT_ACTIVE_NOTICE_LINUX_GAME_DIR)
                set(_mc_notice_modes components rounded)
                set(_mc_notice_linux_arguments --linux-game-dir "${MC_CLIENT_ACTIVE_NOTICE_LINUX_GAME_DIR}")
            else()
                message(STATUS "Linux Notice graphical controls require explicit actual account directory; HOME is not source user.home")
                set(_mc_notice_modes)
            endif()
            foreach(_mc_notice_mode IN LISTS _mc_notice_modes)
                add_test(NAME "client.active_notice_${_mc_notice_mode}"
                    COMMAND Python3::Interpreter -B
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_active_notice.py"
                        --executable "$<TARGET_FILE:mc_test_active_notice>"
                        --assets "${MC_APPLICATION_STARTUP_IMPORTED_RESOURCES}"
                        --artifacts "${CMAKE_CURRENT_BINARY_DIR}/active-notice-contracts"
                        --mode "${_mc_notice_mode}" ${_mc_notice_linux_arguments})
                set_tests_properties("client.active_notice_${_mc_notice_mode}" PROPERTIES
                    LABELS "local;integration;client;graphics;ownership" TIMEOUT 480 RUN_SERIAL TRUE)
            endforeach()
            unset(_mc_notice_modes)
            unset(_mc_notice_mode)
            unset(_mc_notice_linux_arguments)
        endif()
    endif()
endif()
