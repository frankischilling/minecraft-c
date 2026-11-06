if(TARGET mc_client_render_engine)
    target_sources(mc_client_render_engine PRIVATE src/client/render_engine/image_buffer.c)
    if(BUILD_TESTING)
        add_executable(mc_test_image_buffer tests/client/render_engine/test_image_buffer.c)
        target_include_directories(mc_test_image_buffer PRIVATE src)
        target_link_libraries(mc_test_image_buffer PRIVATE
            mc_client_render_engine mc_c_atomics Threads::Threads mc_build_options)
        add_test(NAME client.image_buffer COMMAND mc_test_image_buffer)
        set_tests_properties(client.image_buffer PROPERTIES
            LABELS "public;unit;client;images;ownership" TIMEOUT 90)
    endif()
endif()
