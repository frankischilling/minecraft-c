if(MC_BUILD_CLIENT)
    target_sources(mc_client_texture_effects PRIVATE src/client/render_engine/effect_upload.c)
    target_sources(mc_client_render_engine PRIVATE src/client/render_engine/chunk_statistics.c)
    target_link_libraries(mc_client_texture_effects PRIVATE mc_client_render_textures)
endif()
if(MC_BUILD_CLIENT AND BUILD_TESTING)
    add_executable(mc_test_effect_upload tests/client/texture_effects/test_effect_upload.c)
    target_include_directories(mc_test_effect_upload PRIVATE src src/platform/time)
    target_link_libraries(mc_test_effect_upload PRIVATE mc_client_texture_effects
        mc_client_source_resources mc_client_application_owner mc_simulation mc_resources
        ZLIB::ZLIBSTATIC mc_c_atomics mc_build_options)
    if(MC_ENABLE_CLIENT_DISPLAY_TESTS)
        add_test(NAME client.effect_upload COMMAND Python3::Interpreter -B
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/tools/client/test_effect_upload.py"
            --executable "$<TARGET_FILE:mc_test_effect_upload>"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/effect-upload-controls")
        set_tests_properties(client.effect_upload PROPERTIES
            LABELS "public;integration;client;graphics;ownership" TIMEOUT 180)
    endif()
endif()
