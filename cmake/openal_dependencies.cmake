include(FetchContent)

# Enable the dependency's linker/runtime language in the including directory.
# This file is included only by the client audio graph.
enable_language(CXX)

# The C API and shared device runtime belong only to the client audio target.
# OpenAL Soft's implementation is third-party C++20; project runtime code is C17.
function(mc_add_openal_dependencies)
    if(TARGET OpenAL OR TARGET OpenAL::OpenAL OR TARGET alsoft.common OR TARGET alsoft.fmt)
        message(FATAL_ERROR "OpenAL targets were defined before their pinned sources")
    endif()

    # Normal function-local variables override dependency defaults without
    # changing the parent project's cache or its other dependencies.
    set(LIBTYPE SHARED)
    set(ALSOFT_UTILS OFF)
    set(ALSOFT_NO_CONFIG_UTIL ON)
    set(ALSOFT_EXAMPLES OFF)
    set(ALSOFT_TESTS OFF)
    set(ALSOFT_INSTALL OFF)
    set(ALSOFT_INSTALL_CONFIG OFF)
    set(ALSOFT_INSTALL_HRTF_DATA OFF)
    set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF)
    set(ALSOFT_INSTALL_EXAMPLES OFF)
    set(ALSOFT_INSTALL_UTILS OFF)
    set(ALSOFT_BUILD_ROUTER OFF)
    set(ALSOFT_UPDATE_BUILD_VERSION OFF)
    set(ALSOFT_ENABLE_MODULES OFF)
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
    set(ALSOFT_EAX OFF)

    # Native Windows/Linux output and built-in loopback stay available. Avoid
    # unrelated optional transports and their package/runtime dependencies.
    set(ALSOFT_BACKEND_COREAUDIO OFF)
    set(ALSOFT_BACKEND_JACK OFF)
    set(ALSOFT_BACKEND_OBOE OFF)
    set(ALSOFT_BACKEND_OPENSL OFF)
    set(ALSOFT_BACKEND_OSS OFF)
    set(ALSOFT_BACKEND_PORTAUDIO OFF)
    set(ALSOFT_BACKEND_SDL2 OFF)
    set(ALSOFT_BACKEND_SDL3 OFF)
    set(ALSOFT_BACKEND_SNDIO OFF)
    set(ALSOFT_BACKEND_SOLARIS OFF)
    set(ALSOFT_BACKEND_WAVE OFF)
    set(CMAKE_DISABLE_FIND_PACKAGE_SDL3 TRUE)

    FetchContent_Declare(mc_openal
        URL https://openal-soft.org/openal-releases/openal-soft-1.25.2.tar.bz2
        URL_HASH SHA256=1dbaac44e7579d5bc8847ca8db4b2e8b9fd3961041f35ee20def4958301e1089
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR __mc_deferred_configuration)
    FetchContent_MakeAvailable(mc_openal)
    FetchContent_GetProperties(mc_openal SOURCE_DIR openal_source BINARY_DIR openal_binary)

    # Catch stale offline overrides before compiling a different API/build.
    file(SHA256 "${openal_source}/CMakeLists.txt" openal_cmake_sha256)
    if(NOT openal_cmake_sha256 STREQUAL
        "76e61f8148997f098e39375fe408cbc86d10de2574d610512f8a74fad08e9b6d")
        message(FATAL_ERROR "Pinned OpenAL Soft build metadata does not match version 1.25.2")
    endif()
    file(SHA256 "${openal_source}/include/AL/al.h" openal_header_sha256)
    if(NOT openal_header_sha256 STREQUAL
        "c239b51b32ec10214c3e0a3678876795133150909001ee6cc338d89993ea3c14")
        message(FATAL_ERROR "Pinned OpenAL Soft C header does not match version 1.25.2")
    endif()

    # Clang 18/libstdc++ 13 rejects optional<array<UpsampleParams,4>>::emplace()
    # through is_constructible, despite the same aggregate's valid value
    # initialization. Pass that explicitly value-initialized array instead.
    # Its fields are floats, zero-initialized float arrays and a trivially
    # copyable BandSplitter with four zero-initialized float members. Copying
    # this value preserves those defaults before the following assignments.
    # Change only a private build copy; archive and offline overrides stay intact.
    file(SHA256 "${openal_source}/core/filters/splitter.h" splitter_sha256)
    if(NOT splitter_sha256 STREQUAL
        "b652747ba11ff9de4595a81b6cbeff8ffe54650bac3ba42809588858fd538e60")
        message(FATAL_ERROR "Pinned OpenAL Soft BandSplitter defaults do not match the reviewed source")
    endif()
    set(old_initializer "        auto &upsampler = mUpsampler.emplace();")
    set(new_initializer
        "        auto &upsampler = mUpsampler.emplace(std::array<UpsampleParams, NumLines>{});")
    set(compat_source "${openal_binary}/mc-configured-source")
    file(MAKE_DIRECTORY "${compat_source}")
    # Keep the corrected files' timestamps stable on a no-change reconfigure.
    file(COPY "${openal_source}/" DESTINATION "${compat_source}"
        REGEX "/alc/effects/(chorus|distortion|fshifter|pshifter)\\.cpp$" EXCLUDE)
    foreach(effect_pin IN ITEMS
        "chorus.cpp,0eed1eb01b6bf4486d6cc8f8303c50cf9a3a7ef32c7ceb627fa67a4b6cbc3fc8"
        "distortion.cpp,a7a9d764f49332348ae49852a8b94b6b2498179f938515c6ac6a2ed60a59d445"
        "fshifter.cpp,b9b6935b4e468f0bb4d99adad0f15d8d9a9f9ffdde10f51e0b67142614136c1a"
        "pshifter.cpp,32a1c0b66eebdd91e785b11b555a8c0169fdf508cb87970e684271fd467985c4")
        string(REPLACE "," ";" effect_fields "${effect_pin}")
        list(GET effect_fields 0 effect_file)
        list(GET effect_fields 1 expected_sha256)
        file(SHA256 "${openal_source}/alc/effects/${effect_file}" actual_sha256)
        if(NOT actual_sha256 STREQUAL expected_sha256)
            message(FATAL_ERROR "Pinned OpenAL Soft ${effect_file} initializer differs from the reviewed source")
        endif()
        file(READ "${openal_source}/alc/effects/${effect_file}" effect)
        string(FIND "${effect}" "${old_initializer}" initializer_offset)
        if(initializer_offset EQUAL -1)
            message(FATAL_ERROR "Pinned OpenAL Soft ${effect_file} initializer expression is missing")
        endif()
        string(REPLACE "${old_initializer}" "${new_initializer}" effect "${effect}")
        set(effect
            "/* minecraft-c build compatibility, 2026-10-25: explicit aggregate value initialization for Clang 18. */\n${effect}")
        file(CONFIGURE OUTPUT "${compat_source}/alc/effects/${effect_file}" CONTENT "${effect}"
            @ONLY NEWLINE_STYLE LF)
    endforeach()
    add_subdirectory("${compat_source}" "${openal_binary}/build" EXCLUDE_FROM_ALL)
    get_target_property(openal_type OpenAL TYPE)
    if(NOT openal_type STREQUAL "SHARED_LIBRARY")
        message(FATAL_ERROR "Client audio requires the shared OpenAL Soft runtime")
    endif()

    # Preserve upstream diagnostics; only the project's sanitizer interface is
    # shared with the dependency and its two compiled support targets.
    if(TARGET mc_sanitizers)
        foreach(target IN ITEMS OpenAL alsoft.common alsoft.fmt)
            target_link_libraries(${target} PRIVATE mc_sanitizers)
        endforeach()
    endif()
endfunction()

mc_add_openal_dependencies()
