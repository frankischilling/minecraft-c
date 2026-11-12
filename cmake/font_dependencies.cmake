include(FetchContent)

# Configure the exact unmodified source; source overrides remain caller-verified.
function(mc_add_font_dependencies)
    if(TARGET freetype)
        message(FATAL_ERROR "FreeType target was defined before its pinned source")
    endif()
    FetchContent_Declare(mc_freetype
        URL https://download.savannah.gnu.org/releases/freetype/freetype-2.13.2.tar.xz
        URL_HASH SHA256=12991c4e55c506dd7f9b765933e62fd2be2e06d421505d7950a132e4f1bb484d
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR __mc_deferred_configuration)
    FetchContent_MakeAvailable(mc_freetype)
    FetchContent_GetProperties(mc_freetype SOURCE_DIR font_source BINARY_DIR font_binary)
    # Upstream's old minimum needs this scoped CMake4 compatibility admission.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
    set(BUILD_SHARED_LIBS OFF)
    set(SKIP_INSTALL_ALL ON)
    set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
    set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
    add_subdirectory("${font_source}" "${font_binary}" EXCLUDE_FROM_ALL)
    # Instrument actual upstream objects without exporting project-only targets.
    # Final native owners receive sanitizer link flags through mc_build_options.
    target_compile_options(freetype PRIVATE
        $<TARGET_PROPERTY:mc_sanitizers,INTERFACE_COMPILE_OPTIONS>)
endfunction()

mc_add_font_dependencies()
