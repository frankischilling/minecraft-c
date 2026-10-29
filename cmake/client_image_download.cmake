if(MC_BUILD_CLIENT AND TARGET mc_java_http_inputs AND TARGET mc_client_image_io)
    add_library(mc_client_image_downloads STATIC src/client/render_engine/image_download.c)
    target_include_directories(mc_client_image_downloads PUBLIC src/client/render_engine PRIVATE src)
    target_link_libraries(mc_client_image_downloads PUBLIC mc_client_render_engine
        mc_java_http_inputs mc_client_image_io mc_simulation mc_c_atomics
        PRIVATE mc_build_options)
    target_link_libraries(mc_client_render_global PRIVATE mc_client_image_downloads)
    if(TARGET mc_client_runtime)
        target_link_libraries(mc_client_runtime PUBLIC mc_client_image_downloads)
    endif()
endif()
