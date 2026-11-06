# Canonical SDK device create calls and the actual vy/Cursor owners.
if(MC_BUILD_CLIENT AND TARGET mc_client_sdk_runtime AND TARGET mc_client_display AND
   TARGET mc_client_render_engine)
    target_sources(mc_client_sdk_runtime PRIVATE src/client/display/sdk_input_create.c)
    add_library(mc_client_startup_input STATIC
        src/client/display/sdk_cursor.c
        src/client/display/sdk_cursor_platform.c
        src/client/input/mouse_helper.c)
    target_include_directories(mc_client_startup_input PUBLIC include src
        PRIVATE src/platform/time)
    target_link_libraries(mc_client_startup_input PUBLIC
        mc_client_sdk_runtime mc_client_display mc_client_render_engine mc_c_atomics
        PRIVATE mc_platform_time mc_build_options)
    if(WIN32)
        target_link_libraries(mc_client_startup_input PRIVATE user32)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(NOT TARGET X11::X11)
            find_package(X11 REQUIRED)
        endif()
        target_link_libraries(mc_client_startup_input PRIVATE X11::X11)
    endif()
    if(TARGET mc_client_application_startup)
        target_sources(mc_client_application_startup PRIVATE
            src/client/startup/application_startup_input.c)
        target_link_libraries(mc_client_application_startup PUBLIC mc_client_startup_input)
    endif()
endif()
