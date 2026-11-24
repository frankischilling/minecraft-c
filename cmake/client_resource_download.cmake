# Include after canonical Java/File URL and actual App/Settings targets.
# This module has no startup/U publication and no headless transport edge.
if(MC_BUILD_CLIENT AND NOT TARGET mc_client_resource_download)
    include(cmake/resource_download_dependencies.cmake)
    mc_add_resource_download_dependencies()
    add_library(mc_java_http_inputs STATIC src/core/java/http_input.c
        src/core/java/url_data_input.c)
    target_include_directories(mc_java_http_inputs PUBLIC include PRIVATE src)
    target_link_libraries(mc_java_http_inputs PUBLIC mc_java_file_urls
        PRIVATE CURL::libcurl mc_storage Threads::Threads mc_c_atomics mc_build_options)
    add_library(mc_java_xml_documents STATIC src/core/java/xml_dom.c)
    target_include_directories(mc_java_xml_documents PUBLIC include src/core/java PRIVATE src)
    target_link_libraries(mc_java_xml_documents PUBLIC mc_java_http_inputs
        PRIVATE expat::expat mc_platform_files mc_build_options)
    add_library(mc_client_resource_download STATIC
        src/client/resources/download_thread_owner.c
        src/client/resources/download_thread_run.c
        src/client/resources/download_thread_scan.c
        src/client/resources/resource_registration.c)
    target_include_directories(mc_client_resource_download PUBLIC src include
        PRIVATE src/client src/platform/time)
    target_link_libraries(mc_client_resource_download PUBLIC mc_client_application_owner
        mc_game_settings mc_java_outputs mc_java_xml_documents
        PRIVATE mc_client_notice Threads::Threads mc_c_atomics mc_build_options)
endif()
