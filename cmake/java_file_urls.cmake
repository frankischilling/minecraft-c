# Include after mc_java_files, mc_core, mc_client_sound_pool and
# mc_platform_files are defined. No HTTP/XML/device dependency enters here.
if(NOT TARGET mc_java_file_urls)
    target_link_libraries(mc_world_info PRIVATE mc_c_atomics)
    target_sources(mc_core PRIVATE src/core/java/ascii_operand.c)
    target_sources(mc_platform_files PRIVATE src/platform/files/url_path.c)
    add_library(mc_java_file_urls STATIC src/core/java/url.c)
    target_include_directories(mc_java_file_urls PUBLIC include PRIVATE src)
    target_link_libraries(mc_java_file_urls PUBLIC mc_java_files
        PRIVATE mc_core mc_platform_files mc_build_options)
    target_link_libraries(mc_client_sound_files PRIVATE mc_platform_files)
    target_link_libraries(mc_client_sound_pool PUBLIC mc_java_file_urls
        PRIVATE Threads::Threads mc_build_options)
endif()
