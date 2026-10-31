include(FetchContent)

# Client libraries remain outside the shared simulation and server link graph.
function(mc_add_client_dependencies)
    if(NOT TARGET zlibstatic OR NOT TARGET ZLIB::ZLIBSTATIC)
        message(FATAL_ERROR "Client image decoding requires the project's pinned static zlib")
    endif()
    if(TARGET SDL3::SDL3-static OR TARGET png_static)
        message(FATAL_ERROR "Client dependency targets were defined before their pinned sources")
    endif()

    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_UNINSTALL OFF CACHE BOOL "" FORCE)
    set(SDL_VIDEO ON CACHE BOOL "" FORCE)
    set(SDL_OPENGL ON CACHE BOOL "" FORCE)
    set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
    set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
    set(SDL_GPU OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER OFF CACHE BOOL "" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_JOYSTICK ON CACHE BOOL "" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI ON CACHE BOOL "" FORCE)
    set(SDL_VIRTUAL_JOYSTICK ON CACHE BOOL "" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
    set(SDL_TRAY OFF CACHE BOOL "" FORCE)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # The native SDK already requires X11 at link time. Give SDL the same
        # process lifetime for Xlib's global state instead of unloading it.
        set(SDL_X11_SHARED OFF CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(mc_sdl3
        URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.tar.gz
        URL_HASH SHA256=9c75cf16330322c217dedd2e0609f1124f1b54b8633e763467b4684d0f4334a3
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(mc_sdl3)

    # libpng asks for package ZLIB and target ZLIB::ZLIB. Both must resolve to
    # the already configured mc_zlib owner, including its offline override.
    file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/zlib-config.cmake" [=[
if(NOT TARGET zlibstatic OR NOT TARGET ZLIB::ZLIBSTATIC)
    message(FATAL_ERROR "The pinned static zlib target is unavailable")
endif()
if(TARGET ZLIB::ZLIB)
    get_target_property(_mc_zlib_owner ZLIB::ZLIB ALIASED_TARGET)
    if(NOT _mc_zlib_owner STREQUAL "zlibstatic")
        message(FATAL_ERROR "ZLIB::ZLIB does not refer to the project's static zlib")
    endif()
else()
    add_library(ZLIB::ZLIB ALIAS zlibstatic)
endif()
set(ZLIB_VERSION "1.3.2")
set(ZLIB_VERSION_STRING "1.3.2")
set(ZLIB_FOUND TRUE)
]=])
    file(WRITE "${CMAKE_FIND_PACKAGE_REDIRECTS_DIR}/zlib-config-version.cmake" [=[
set(PACKAGE_VERSION "1.3.2")
if(PACKAGE_FIND_VERSION VERSION_GREATER PACKAGE_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
endif()
if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
    set(PACKAGE_VERSION_EXACT TRUE)
endif()
]=])

    # Populate before configuration so every platform uses the same checked-in
    # libpng configuration header, independently of an installed AWK program.
    FetchContent_Declare(mc_libpng
        URL https://sourceforge.net/projects/libpng/files/libpng16/1.6.59/libpng-1.6.59.tar.gz/download
        URL_HASH SHA256=86a3e4b501f7f50e392c4e456ea158893e9a595b1c63eb88c7bb9f6cf8772dad
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR __mc_deferred_configuration)
    FetchContent_MakeAvailable(mc_libpng)
    FetchContent_GetProperties(mc_libpng SOURCE_DIR png_source BINARY_DIR png_binary)
    set(PNG_STATIC ON CACHE BOOL "" FORCE)
    set(PNG_SHARED OFF CACHE BOOL "" FORCE)
    set(PNG_FRAMEWORK OFF CACHE BOOL "" FORCE)
    set(PNG_TESTS OFF CACHE BOOL "" FORCE)
    set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
    set(PNG_LIBCONF_HEADER "${png_source}/scripts/pnglibconf.h.prebuilt" CACHE FILEPATH "" FORCE)
    set(SKIP_INSTALL_ALL ON)
    add_subdirectory("${png_source}" "${png_binary}" EXCLUDE_FROM_ALL)

    # SDL exports its build-tree target even with installation disabled. Keep
    # project-only sanitizer targets out of that export's link interface.
    # Final client executables obtain sanitizer link flags from mc_build_options.
    target_compile_options(SDL3-static PRIVATE
        $<TARGET_PROPERTY:mc_sanitizers,INTERFACE_COMPILE_OPTIONS>)
    target_compile_options(png_static PRIVATE
        $<TARGET_PROPERTY:mc_sanitizers,INTERFACE_COMPILE_OPTIONS>)
endfunction()

mc_add_client_dependencies()
