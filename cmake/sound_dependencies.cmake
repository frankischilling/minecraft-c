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

    # Apply the reviewed build and codec changes in a private copy.
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

    # The Bark lookup packs a possibly negative lower index. Multiplication is
    # defined for the codec's bounded block sizes; left-shifting it is not.
    file(SHA256 "${vorbis_source}/lib/psy.c" psy_sha256)
    if(NOT psy_sha256 STREQUAL
        "e8ccec1fba7d00961d5c67fec30b488c912f0ad7842ec7b1aeab987b97744b6e")
        message(FATAL_ERROR "Pinned libvorbis psychoacoustic source does not match the reviewed source")
    endif()
    file(READ "${vorbis_source}/lib/psy.c" psy)
    set(old_bark "    p->bark[i]=((lo-1)<<16)+(hi-1);")
    set(new_bark "    p->bark[i]=(lo-1)*65536L+(hi-1);")
    string(FIND "${psy}" "${old_bark}" bark_offset)
    if(bark_offset EQUAL -1)
        message(FATAL_ERROR "Pinned libvorbis Bark packing branch is missing")
    endif()
    string(REPLACE "${old_bark}" "${new_bark}" psy "${psy}")
    # At the last noise band, del is zero but the next table access is still
    # evaluated. Reuse the endpoint instead of reading beyond noiseoff's row.
    set(old_noise_endpoint "        p->vi->noiseoff[j][inthalfoc+1]*del;")
    set(new_noise_endpoint
        "        p->vi->noiseoff[j][inthalfoc < P_BANDS-1 ? inthalfoc+1 : inthalfoc]*del;")
    string(FIND "${psy}" "${old_noise_endpoint}" noise_endpoint_offset)
    if(noise_endpoint_offset EQUAL -1)
        message(FATAL_ERROR "Pinned libvorbis noise interpolation branch is missing")
    endif()
    string(REPLACE "${old_noise_endpoint}" "${new_noise_endpoint}" psy "${psy}")
    file(CONFIGURE OUTPUT "${compat_source}/lib/psy.c" CONTENT "${psy}"
        @ONLY NEWLINE_STYLE LF)

    # The decode search table intentionally places bits in the high unsigned
    # word. Convert before shifting so setting bit 31 remains defined C.
    file(SHA256 "${vorbis_source}/lib/sharedbook.c" sharedbook_sha256)
    if(NOT sharedbook_sha256 STREQUAL
        "a883767bf16552f5cf8686b58b86e55819dcb63487c5307fee5d45820b836a40")
        message(FATAL_ERROR "Pinned libvorbis codebook source does not match the reviewed source")
    endif()
    file(READ "${vorbis_source}/lib/sharedbook.c" sharedbook)
    set(old_codeword "          ogg_uint32_t word=i<<(32-c->dec_firsttablen);")
    set(new_codeword "          ogg_uint32_t word=(ogg_uint32_t)i<<(32-c->dec_firsttablen);")
    string(FIND "${sharedbook}" "${old_codeword}" codeword_offset)
    if(codeword_offset EQUAL -1)
        message(FATAL_ERROR "Pinned libvorbis decode search table branch is missing")
    endif()
    string(REPLACE "${old_codeword}" "${new_codeword}" sharedbook "${sharedbook}")
    file(CONFIGURE OUTPUT "${compat_source}/lib/sharedbook.c" CONTENT "${sharedbook}"
        @ONLY NEWLINE_STYLE LF)
    add_subdirectory("${compat_source}" "${vorbis_binary}/build" EXCLUDE_FROM_ALL)

    # Upstream's CMake omits the configure-script check used by os.h. Strict
    # C17 needs the actual declaration instead of an implicit alloca function.
    if(UNIX)
        include(CheckIncludeFile)
        check_include_file(alloca.h MC_VORBIS_HAVE_ALLOCA_H)
        if(NOT MC_VORBIS_HAVE_ALLOCA_H)
            message(FATAL_ERROR "libvorbis requires the native alloca.h header on this platform")
        endif()
        foreach(dependency IN ITEMS vorbis vorbisfile vorbisenc)
            target_compile_definitions(${dependency} PRIVATE HAVE_ALLOCA_H=1)
        endforeach()
    endif()

    foreach(dependency IN ITEMS ogg vorbis vorbisfile vorbisenc)
        target_compile_options(${dependency} PRIVATE
            $<TARGET_PROPERTY:mc_sanitizers,INTERFACE_COMPILE_OPTIONS>)
    endforeach()
endfunction()

mc_add_sound_dependencies()
