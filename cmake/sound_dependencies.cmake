include(FetchContent)

# Pure C codec libraries stay outside the shared runtime/server link graph.
function(mc_add_sound_dependencies)
    if(TARGET ogg OR TARGET Ogg::ogg OR TARGET vorbis OR TARGET vorbisfile)
        message(FATAL_ERROR "Sound codec targets were defined before their pinned sources")
    endif()
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_FRAMEWORK OFF CACHE BOOL "" FORCE)
    # Ogg's older policy range must honor this function-local test setting,
    # even when the caller's BUILD_TESTING cache entry is still untyped.
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(BUILD_TESTING OFF)
    set(INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
    set(INSTALL_CMAKE_PACKAGE_MODULE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(mc_ogg
        URL https://downloads.xiph.org/releases/ogg/libogg-1.3.6.zip
        URL_HASH SHA256=e347b59c29e8f938bb48b2fda5da02f2d5d97316c004abaa76e171cf4a2fbdd7
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR __mc_deferred_configuration)
    FetchContent_MakeAvailable(mc_ogg)
    FetchContent_GetProperties(mc_ogg SOURCE_DIR ogg_source BINARY_DIR ogg_binary)
    add_subdirectory("${ogg_source}" "${ogg_binary}" EXCLUDE_FROM_ALL)

    FetchContent_Declare(mc_vorbis
        URL https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.zip
        URL_HASH SHA256=57c8bc92d2741934b8dc939af49c2639edc44b8879cba2ec14ad3189e2814582
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR __mc_deferred_configuration)
    FetchContent_MakeAvailable(mc_vorbis)
    FetchContent_GetProperties(mc_vorbis SOURCE_DIR vorbis_source BINARY_DIR vorbis_binary)

    # Apply the reviewed build and nonseekable-chain changes in a private copy.
    # Offline source overrides remain byte-identical to the official archive.
    file(READ "${vorbis_source}/CMakeLists.txt" upstream)
    set(adapted "${upstream}")
    set(old_minimum "cmake_minimum_required(VERSION 2.8.12)")
    set(old_project "project(vorbis)")
    set(old_ogg "find_package(Ogg REQUIRED)")
    foreach(required IN ITEMS "${old_minimum}" "${old_project}" "${old_ogg}")
        string(FIND "${upstream}" "${required}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR "Pinned libvorbis build metadata does not match the reviewed source")
        endif()
    endforeach()
    string(REPLACE "${old_minimum}" "cmake_minimum_required(VERSION 3.24)" adapted "${adapted}")
    string(REPLACE "${old_project}" "project(vorbis LANGUAGES C)" adapted "${adapted}")
    string(REPLACE "${old_ogg}" "if(NOT TARGET Ogg::ogg)\n    find_package(Ogg REQUIRED)\nendif()"
        adapted "${adapted}")
    set(compat_source "${vorbis_binary}/mc-configured-source")
    file(MAKE_DIRECTORY "${compat_source}")
    file(COPY "${vorbis_source}/" DESTINATION "${compat_source}")
    file(CONFIGURE OUTPUT "${compat_source}/CMakeLists.txt" CONTENT "${adapted}"
        @ONLY NEWLINE_STYLE LF)

    # _fetch_headers already submits the last header page. Submitting it again
    # reports a false packet hole when a nonseekable stream changes logical links.
    file(SHA256 "${vorbis_source}/lib/vorbisfile.c" vorbisfile_sha256)
    if(NOT vorbisfile_sha256 STREQUAL
        "557752159e4c96e4ca339d28a4454c9d7edbf6b2bca6cbe4e8a5f8c44ef9196f")
        message(FATAL_ERROR "Pinned libvorbis chain source does not match the reviewed source")
    endif()
    file(READ "${vorbis_source}/lib/vorbisfile.c" vorbisfile)
    set(old_chain [=[          vf->current_link++;
          link=0;]=])
    set(new_chain [=[          vf->current_link++;
          /* _fetch_headers already submitted the final header page. */
          continue;]=])
    string(FIND "${vorbisfile}" "${old_chain}" chain_offset)
    if(chain_offset EQUAL -1)
        message(FATAL_ERROR "Pinned libvorbis chain branch is missing")
    endif()
    string(REPLACE "${old_chain}" "${new_chain}" vorbisfile "${vorbisfile}")
    file(CONFIGURE OUTPUT "${compat_source}/lib/vorbisfile.c" CONTENT "${vorbisfile}"
        @ONLY NEWLINE_STYLE LF)
    add_subdirectory("${compat_source}" "${vorbis_binary}/build" EXCLUDE_FROM_ALL)

    foreach(dependency IN ITEMS ogg vorbis vorbisfile vorbisenc)
        target_compile_options(${dependency} PRIVATE
            $<TARGET_PROPERTY:mc_sanitizers,INTERFACE_COMPILE_OPTIONS>)
    endforeach()
endfunction()

mc_add_sound_dependencies()
